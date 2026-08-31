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

static const char *TAG = "NODE_STATE_MACHINE";

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
  g_node_fsm.state_next = state;
}

void node_state_machine_task(void *pvParam) {
  ESP_LOGI(TAG, "Node State Machine Task đang chạy...");

  while (1) {
    g_node_fsm.state_current = g_node_fsm.state_next;

    switch (g_node_fsm.state_current) {

    case NODE_STATE_INIT: {
      node_state_machine_set_state(NODE_STATE_IDLE);
      break;
    }

    case NODE_STATE_IDLE: {

      int64_t now_ms = esp_timer_get_time() / 1000;

      if (now_ms - g_node_fsm.last_measure_time >=
          g_node_fsm.measure_interval_ms) {
        node_state_machine_set_state(NODE_STATE_MEASURE);
      }
      break;
    }

    case NODE_STATE_MEASURE: {
      float measured_dist = 0.0f;

      esp_err_t err = aj_sr04m_read_filtered_distance(&measured_dist, 3);

      if (err == ESP_OK) {
        g_node_fsm.current_distance_cm = measured_dist;
        g_node_fsm.is_sensor_ok = true;
      } else {
        ESP_LOGW(TAG, "Cảm biến siêu âm đọc lỗi: %s", esp_err_to_name(err));
        g_node_fsm.current_distance_cm = -1.0f;
        g_node_fsm.is_sensor_ok = false;
      }

      g_node_fsm.current_battery_volt = 4.15f;

      node_state_machine_set_state(NODE_STATE_SEND_DATA);
      break;
    }

    case NODE_STATE_SEND_DATA: {
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

      ESP_LOGW(TAG, "===> ĐANG NẠP OTA QUA ESP-NOW (TẠM DỪNG ĐO)...");
      vTaskDelay(pdMS_TO_TICKS(500));
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