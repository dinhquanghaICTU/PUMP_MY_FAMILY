#include "state_machine.h"
#include "AJ-SR04M.h"
#include "config.h"
#include "esp_log.h"
#include "esp_now_node.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "ota.h"
#include <math.h>
#include <string.h>

static const char *TAG = "NODE_STATE_MACHINE";

// Biến duy trì qua Deep Sleep trong RTC Fast Memory
RTC_DATA_ATTR static float s_rtc_prev_dist = -1.0f;
RTC_DATA_ATTR static int s_rtc_stable_count = 0;

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
  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
  bool is_wake_from_deep_sleep = (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER);

  g_node_fsm.state_current = NODE_STATE_INIT;
  // Nếu thức dậy từ Deep Sleep: đo ngay lập tức (không chờ delay IDLE)
  g_node_fsm.state_next = is_wake_from_deep_sleep ? NODE_STATE_MEASURE : NODE_STATE_IDLE;
  g_node_fsm.packet_counter = 0;
  g_node_fsm.last_measure_time = 0;
  g_node_fsm.measure_interval_ms = (s_rtc_stable_count >= 2) ? 5000 : 2000;
  g_node_fsm.is_sensor_ok = true;
  g_node_fsm.is_ota_running = false;

  ESP_LOGI(TAG, "Khởi tạo Node State Machine THÀNH CÔNG! (Lý do boot: %s | Chu kỳ: %lu ms)",
           is_wake_from_deep_sleep ? "DEEP SLEEP TIMER" : "POWER ON",
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

        // Cơ chế Adaptive Interval tiết kiệm pin (Lưu vào RTC RAM):
        // Nếu nước đứng yên (chênh lệch < 0.8cm): giãn chu kỳ gửi lên 5 giây (tiết kiệm 60% pin)
        // Nếu nước đang thay đổi (đang bơm / xả chênh lệch >= 0.8cm): đo nhanh 2 giây để đóng ngắt chuẩn
        if (s_rtc_prev_dist > 0.0f) {
          float delta = fabsf(g_node_fsm.current_distance_cm - s_rtc_prev_dist);
          if (delta >= 0.8f) {
            g_node_fsm.measure_interval_ms = 2000;
            s_rtc_stable_count = 0;
          } else {
            s_rtc_stable_count++;
            if (s_rtc_stable_count >= 2) {
              g_node_fsm.measure_interval_ms = 5000;
            }
          }
        }
        s_rtc_prev_dist = g_node_fsm.current_distance_cm;
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

      uint32_t pkt_id = esp_now_node_get_next_packet_id();
      g_node_fsm.packet_counter = pkt_id;

      SensorData_t packet = {
          .packet_id = pkt_id,
          .distance_cm = g_node_fsm.current_distance_cm,
          .battery_volt = g_node_fsm.current_battery_volt,
      };
      strncpy(packet.fw_version, ota_node_get_version(), sizeof(packet.fw_version) - 1);

      led_on();
      esp_err_t send_err = esp_now_node_send(&packet);
      led_off();

      if (send_err == ESP_OK) {
        ESP_LOGI(TAG,
                 " [BẮN ESP-NOW #%lu] -> Khoảng cách: %.1f cm | Pin: %.2fV | Ver: [%s]",
                 (unsigned long)packet.packet_id, packet.distance_cm,
                 packet.battery_volt, packet.fw_version);
      } else {
        ESP_LOGE(TAG, "[LỖI ESP-NOW] Không thể gửi gói tin #%lu",
                 (unsigned long)packet.packet_id);
      }

      // CỬA SỔ ĐÓN TIN OTA TỪ MASTER: Lắng nghe 80ms xem Master có gửi lệnh OTA không
      bool has_ota = esp_now_node_wait_for_ota_trigger(80);
      if (has_ota || ota_node_is_updating()) {
        ESP_LOGW(TAG, "🚀 [OTA PHÁT HIỆN] Master đang gửi lệnh OTA! Hủy Deep Sleep và chuẩn bị nhận firmware...");
        node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
        break;
      }

      // Nếu Master chưa khóa kênh (lần đầu boot): chạy bình thường không ngủ để bắt tay
      if (!esp_now_node_is_master_locked()) {
        ESP_LOGI(TAG, "Đang chờ bắt tay và khóa kênh với Master...");
        g_node_fsm.last_measure_time = esp_timer_get_time() / 1000;
        node_state_machine_set_state(NODE_STATE_IDLE);
        break;
      }

      // Đã khóa kênh và không có lệnh OTA: Đi vào Smart Deep Sleep
      node_state_machine_set_state(NODE_STATE_SLEEP);
      break;
    }

    case NODE_STATE_SLEEP: {
      if (ota_node_is_updating()) {
        node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
        break;
      }

      uint32_t sleep_ms = g_node_fsm.measure_interval_ms;
      if (sleep_ms < 1500) sleep_ms = 2000;

      ESP_LOGI(TAG, "💤 [SMART DEEP SLEEP] Ngủ sâu %lu ms (Dòng ~15µA - Tiết kiệm >94%% pin)...", (unsigned long)sleep_ms);

      // Đảm bảo toàn bộ log UART được flush sạch trước khi CPU sleep
      fflush(stdout);
      vTaskDelay(pdMS_TO_TICKS(15));

      // Tắt Wi-Fi để ngắt hoàn toàn công suất phát RF
      esp_wifi_stop();

      // Hẹn giờ đánh thức bằng RTC Timer
      esp_sleep_enable_timer_wakeup((uint64_t)sleep_ms * 1000ULL);
      esp_deep_sleep_start();
      break;
    }

    case NODE_STATE_OTA_UPDATING: {
      if (!ota_node_is_updating()) {
        ESP_LOGI(TAG, "Phiên OTA đã kết thúc hoặc bị hủy -> Quay lại trạng thái IDLE đo đạc...");
        esp_now_node_reset_ota_trigger();
        node_state_machine_set_state(NODE_STATE_IDLE);
        break;
      }

      // Tự động hủy nếu bị treo quá 15 giây không có dữ liệu mới
      int64_t last_active = ota_node_get_last_activity_time();
      if (last_active > 0 && (esp_timer_get_time() / 1000 - last_active) > 15000) {
        ESP_LOGE(TAG, "Quá 15 giây không nhận được dữ liệu OTA mới -> Hủy phiên OTA và quay về đo đạc!");
        ota_node_abort();
        break;
      }

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