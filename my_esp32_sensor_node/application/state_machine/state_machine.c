#include "state_machine.h"
#include "AJ-SR04M.h"
#include "config.h"
#include "esp_log.h"
#include "esp_now_node.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "ota.h"
#include <math.h>

static const char *TAG = "NODE_STATE_MACHINE";

static float s_prev_reported_dist = -1.0f;
static int s_stable_count = 0;

node_state_machine_t g_node_fsm = {
    .state_current = NODE_STATE_INIT,
    .state_next = NODE_STATE_INIT,
    .packet_counter = 0,
    .current_distance_cm = 0.0f,
    .current_battery_volt = 4.10f,
    .last_measure_time = 0,
    .measure_interval_ms = SENSOR_SEND_INTERVAL_MS,
    .is_sensor_ok = true,
    .is_ota_running = false,
};

void node_state_machine_init(void) {
  g_node_fsm.state_current = NODE_STATE_INIT;
  g_node_fsm.state_next = NODE_STATE_IDLE;
  g_node_fsm.packet_counter = 0;
  g_node_fsm.last_measure_time = 0;
  g_node_fsm.measure_interval_ms = SENSOR_SEND_INTERVAL_MS;
  g_node_fsm.is_sensor_ok = true;
  g_node_fsm.is_ota_running = false;

  ESP_LOGI(TAG, "Khởi tạo Node State Machine THÀNH CÔNG! (Chu kỳ đo: %lu ms)",
           (unsigned long)g_node_fsm.measure_interval_ms);
}

void node_state_machine_set_state(node_state_t state) {
  if (ota_node_is_updating() && state != NODE_STATE_OTA_UPDATING) {
    return; // Đang OTA, không cho phép đổi sang trạng thái đo đạc
  }
  g_node_fsm.state_next = state;
}

void node_state_machine_task(void *pvParam) {
  ESP_LOGI(TAG, "Node State Machine Task đang chạy...");

  while (1) {
    if (ota_node_is_updating()) {
      int64_t now_ms = esp_timer_get_time() / 1000;
      int64_t last_act = ota_node_get_last_activity_time();
      if (last_act > 0 && (now_ms - last_act > 30000)) {
        ESP_LOGE(TAG, "⏰ [OTA TIMEOUT] Quá 30 giây không nhận được dữ liệu từ Master! Tự động hủy OTA và khôi phục đo đạc...");
        ota_node_abort();
        continue;
      }
      g_node_fsm.state_current = NODE_STATE_OTA_UPDATING;
      g_node_fsm.state_next = NODE_STATE_OTA_UPDATING;
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    g_node_fsm.state_current = g_node_fsm.state_next;

    switch (g_node_fsm.state_current) {

      /*
        case này sẽ chờ 50ms để chuyển qua case NODE_STATE_IDLE

      */

    case NODE_STATE_INIT: {
      node_state_machine_set_state(NODE_STATE_IDLE);
      break;
    }
    /*
      case này tính ra time hiện tại và so sánh với time trước đó nếu lớn hơn
      thì chuyển qua case NODE_STATE_MEASURE
    */
    case NODE_STATE_IDLE: {

      int64_t now_ms = esp_timer_get_time() / 1000;

      if (now_ms - g_node_fsm.last_measure_time >=
          g_node_fsm.measure_interval_ms) {
        node_state_machine_set_state(NODE_STATE_MEASURE);
      }
      break;
    }
    /*
      case này sẽ lấy khoảng cách từ cảm biến và gán cho current_distance_cm
      sử dụng bộ lọc Trimmed Mean + EMA 7 mẫu
    */
    case NODE_STATE_MEASURE: {
      if (ota_node_is_updating()) {
        node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
        break;
      }

      float measured_dist = 0.0f;

      // Đo 7 mẫu loại bỏ ngoại lai min/max và lọc EMA
      esp_err_t err = aj_sr04m_read_filtered_distance(&measured_dist, 7);

      if (ota_node_is_updating()) {
        node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
        break;
      }

      if (err == ESP_OK) {
        g_node_fsm.current_distance_cm = measured_dist;
        g_node_fsm.is_sensor_ok = true;

        // Cơ chế Adaptive Interval tiết kiệm pin:
        // Nếu nước đứng yên (chênh lệch < 0.8cm): giãn chu kỳ gửi lên 5 giây (tiết kiệm 60% pin)
        // Nếu nước đang thay đổi (đang bơm / xả chênh lệch >= 0.8cm): đo nhanh 2 giây để đóng ngắt chuẩn
        if (s_prev_reported_dist > 0.0f) {
          float delta = fabsf(g_node_fsm.current_distance_cm - s_prev_reported_dist);
          if (delta >= 0.8f) {
            g_node_fsm.measure_interval_ms = 2000;
            s_stable_count = 0;
          } else {
            s_stable_count++;
            if (s_stable_count >= 2) {
              g_node_fsm.measure_interval_ms = 5000;
            }
          }
        }
        s_prev_reported_dist = g_node_fsm.current_distance_cm;
      } else {
        ESP_LOGW(TAG, "Cảm biến siêu âm đọc lỗi: %s", esp_err_to_name(err));
        g_node_fsm.current_distance_cm = -1.0f;
        g_node_fsm.is_sensor_ok = false;
        g_node_fsm.measure_interval_ms = 2000; // Đang lỗi: thử lại sau 2s
      }
      // Pin hiện tại
      g_node_fsm.current_battery_volt = 4.15f;

      node_state_machine_set_state(NODE_STATE_SEND_DATA);
      break;
    }
    /*
      gửi đống data vừa rồi qua esp now

    */
    case NODE_STATE_SEND_DATA: {
      if (ota_node_is_updating()) {
        node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
        break;
      }

      g_node_fsm.packet_counter++;

      SensorData_t packet = {
          .packet_id = g_node_fsm.packet_counter,
          .distance_cm = g_node_fsm.current_distance_cm,
          .battery_volt = g_node_fsm.current_battery_volt,
      };

      led_on();
      esp_err_t send_err = esp_now_node_send(&packet);
      led_off();

      if (send_err == ESP_OK) {
        ESP_LOGI(TAG,
                 " [BẮN ESP-NOW #%lu] -> Khoảng cách: %.1f cm | Pin: %.2fV",
                 (unsigned long)packet.packet_id, packet.distance_cm,
                 packet.battery_volt);
      } else {
        ESP_LOGE(TAG, "[LỖI ESP-NOW] Không thể gửi gói tin #%lu",
                 (unsigned long)packet.packet_id);
      }

      g_node_fsm.last_measure_time = esp_timer_get_time() / 1000;
      node_state_machine_set_state(NODE_STATE_IDLE);
      break;
    }

    case NODE_STATE_OTA_UPDATING: {
      ESP_LOGW(TAG, "===> ĐANG NẠP OTA QUA ESP-NOW (TẠM DỪNG ĐO VÀ KHÔNG BẮN DỮ LIỆU)...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      break;
    }

    case NODE_STATE_ERROR: {
      ESP_LOGE(TAG, "Hệ thống gặp sự cố, thử lại sau 2 giây...");
      vTaskDelay(pdMS_TO_TICKS(2000));
      node_state_machine_set_state(NODE_STATE_IDLE);
      break;
    }

    default:
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}