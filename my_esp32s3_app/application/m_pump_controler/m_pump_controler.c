#include "m_pump_controler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt.h"
#include "node_esp.h"
#include "relay.h"
#include "wifi.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "APP_PUMP_CONTROLER";

static m_controler_pump_t s_pump_ctx = {
    .mode = MODE_PUMP_AUTO,
    .state_current = STATE_PUMP_IDLE,
    .state_next = STATE_PUMP_IDLE,
    .is_pump_on = false,
    .child_lock = false,

    .tank_height_cm = 120.0f,
    .sensor_offset_cm = 25.0f,
    .min_water_percent = 25,
    .max_water_percent = 95,
    .max_runtime_sec = 2700,

    .current_distance_cm = -1.0f,
    .current_percent = 0.0f,
    .node_battery_volt = 0.0f,
    .node_rssi = 0,

    .runtime_counter_sec = 0,
    .last_node_seen_sec = 999999,
};

static TaskHandle_t s_pump_task_handle = NULL;
float m_pump_controler_calculate_percent(float distance_cm) {
  if (distance_cm <= 0.0f) {
    return -1.0f;
  }

  float full_dist = s_pump_ctx.sensor_offset_cm;
  float empty_dist = s_pump_ctx.sensor_offset_cm + s_pump_ctx.tank_height_cm;

  if (distance_cm <= full_dist) {
    return 100.0f;
  }
  if (distance_cm >= empty_dist) {
    return 0.0f;
  }

  float pct = ((empty_dist - distance_cm) / (empty_dist - full_dist)) * 100.0f;
  if (pct < 0.0f)
    pct = 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;

  return roundf(pct * 10.0f) / 10.0f;
}
static void pump_controler_task(void *pvParam) {
  ESP_LOGI(TAG, "Task điều khiển bơm (m_pump_controler) đã khởi động!");

  TickType_t last_wake_time = xTaskGetTickCount();

  while (1) {
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));

    SensorData_t sensor_data;
    bool has_data = node_esp_get_latest_data(&sensor_data);
    uint32_t seconds_since_last = node_esp_get_seconds_since_last_packet();

    s_pump_ctx.last_node_seen_sec = seconds_since_last;

    if (has_data && seconds_since_last < 60) {
      s_pump_ctx.current_distance_cm = sensor_data.distance_cm;
      s_pump_ctx.node_battery_volt = sensor_data.battery_volt;
      s_pump_ctx.current_percent =
          m_pump_controler_calculate_percent(sensor_data.distance_cm);
    } else {
      s_pump_ctx.current_percent = -1.0f;
    }

    s_pump_ctx.is_pump_on = relay_is_on();

    if (s_pump_ctx.is_pump_on) {
      s_pump_ctx.runtime_counter_sec++;
    } else {
      s_pump_ctx.runtime_counter_sec = 0;
    }

    if (s_pump_ctx.is_pump_on &&
        s_pump_ctx.runtime_counter_sec >= s_pump_ctx.max_runtime_sec) {
      ESP_LOGE(TAG,
               "[CẢNH BÁO BẢO VỆ] Máy bơm chạy liên tục quá %lu giây! Tự "
               "ngắt khẩn cấp.",
               (unsigned long)s_pump_ctx.max_runtime_sec);
      relay_turn_off();
      s_pump_ctx.state_current = STATE_PUMP_ERROR_TIMEOUT;

      app_mqtt_publish(
          "pump/family/status",
          "{\"event\":\"pump_alert\",\"error\":\"TIMEOUT_MAX_RUNTIME\","
          "\"message\":\"Bơm chạy quá giờ, đã ngắt an toàn\"}",
          1, 0);
      continue;
    }

    // Khi mất mạng (Wi-Fi hoặc MQTT), tự động thoát chế độ Auto sang Manual và Tắt khóa trẻ em
    bool is_online = wifi_is_connected() && app_mqtt_is_connected();
    if (!is_online) {
      if (s_pump_ctx.mode == MODE_PUMP_AUTO || s_pump_ctx.child_lock) {
        ESP_LOGW(TAG, "📡 [MẤT KẾT NỐI MẠNG] Tự động THOÁT chế độ AUTO -> MANUAL và MỞ KHÓA nút cứng tủ điện!");
        s_pump_ctx.mode = MODE_PUMP_MANUAL;
        s_pump_ctx.child_lock = false;
      }
    }

    if (s_pump_ctx.mode == MODE_PUMP_AUTO && seconds_since_last > 120 &&
        s_pump_ctx.is_pump_on) {
      ESP_LOGW(TAG, "[CẢNH BÁO] Mất tín hiệu Node Bể > 120s khi đang bơm "
                    "Auto -> Tạm dừng bơm an toàn!");
      relay_turn_off();
      s_pump_ctx.state_current = STATE_PUMP_ERROR_NODE_LOST;
      continue;
    }

    if (s_pump_ctx.mode == MODE_PUMP_AUTO && !s_pump_ctx.child_lock &&
        s_pump_ctx.state_current != STATE_PUMP_ERROR_TIMEOUT) {

      if (s_pump_ctx.current_percent >= 0.0f) {

        if (s_pump_ctx.current_percent <= (float)s_pump_ctx.min_water_percent &&
            !s_pump_ctx.is_pump_on) {
          ESP_LOGW(TAG, "[TỰ ĐỘNG BẬT BƠM] Mức nước thấp (%.1f%% <= %d%%)",
                   s_pump_ctx.current_percent, s_pump_ctx.min_water_percent);
          relay_turn_on();
          s_pump_ctx.state_current = STATE_PUMP_RUNNING;
        }

        if (s_pump_ctx.current_percent >= (float)s_pump_ctx.max_water_percent &&
            s_pump_ctx.is_pump_on) {
          ESP_LOGI(TAG, "[TỰ ĐỘNG TẮT BƠM] Bể đã đầy nước (%.1f%% >= %d%%)",
                   s_pump_ctx.current_percent, s_pump_ctx.max_water_percent);
          relay_turn_off();
          s_pump_ctx.state_current = STATE_PUMP_IDLE;
        }
      }
    }

    if (s_pump_ctx.is_pump_on) {
      s_pump_ctx.state_current = STATE_PUMP_RUNNING;
    } else if (s_pump_ctx.state_current != STATE_PUMP_ERROR_TIMEOUT &&
               s_pump_ctx.state_current != STATE_PUMP_ERROR_NODE_LOST) {
      s_pump_ctx.state_current = STATE_PUMP_IDLE;
    }

    static int s_telemetry_tick = 0;
    if (++s_telemetry_tick >= 2) {
      s_telemetry_tick = 0;
      char stat_json[256];
      snprintf(
          stat_json, sizeof(stat_json),
          "{\"pump\":%d,\"mode\":\"%s\",\"water_percent\":%.1f,\"distance_cm\":"
          "%.1f,\"battery\":%.2f,\"runtime\":%lu,\"child_lock\":%d,\"state\":"
          "\"%s\"}",
          s_pump_ctx.is_pump_on ? 1 : 0,
          s_pump_ctx.mode == MODE_PUMP_AUTO ? "auto" : "manual",
          s_pump_ctx.current_percent, s_pump_ctx.current_distance_cm,
          s_pump_ctx.node_battery_volt,
          (unsigned long)s_pump_ctx.runtime_counter_sec,
          s_pump_ctx.child_lock ? 1 : 0,
          s_pump_ctx.state_current == STATE_PUMP_RUNNING
              ? "RUNNING"
              : (s_pump_ctx.state_current == STATE_PUMP_ERROR_TIMEOUT
                     ? "ERROR_TIMEOUT"
                     : (s_pump_ctx.state_current == STATE_PUMP_ERROR_NODE_LOST
                            ? "ERROR_NODE_LOST"
                            : "IDLE")));
      app_mqtt_publish("pump/family/status", stat_json, 1, 0);
    }
  }
}

esp_err_t m_pump_controler_init(void) {
  relay_init();

  if (s_pump_task_handle == NULL) {
    BaseType_t ret = xTaskCreate(pump_controler_task, "pump_ctrl_task", 4096,
                                 NULL, 5, &s_pump_task_handle);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Không thể khởi tạo pump_controler_task!");
      return ESP_FAIL;
    }
  }

  ESP_LOGI(
      TAG,
      "Khởi tạo Module Điều Khiển Bơm THÀNH CÔNG! (Auto: Min %d%% / Max %d%%)",
      s_pump_ctx.min_water_percent, s_pump_ctx.max_water_percent);
  return ESP_OK;
}

const m_controler_pump_t *m_pump_controler_get_context(void) {
  return &s_pump_ctx;
}

void m_pump_controler_set_mode(m_mode_pump_t mode) {
  s_pump_ctx.mode = mode;
  ESP_LOGI(TAG, "Chuyển chế độ hoạt động bơm: %s",
           mode == MODE_PUMP_AUTO ? "TỰ ĐỘNG (AUTO)" : "THỦ CÔNG (MANUAL)");
}

void m_pump_controler_set_pump(bool turn_on) {
  if (s_pump_ctx.child_lock && turn_on) {
    ESP_LOGW(TAG, "Khóa trẻ em đang bật! Bỏ qua lệnh bật bơm.");
    return;
  }
  if (turn_on) {
    relay_turn_on();
    s_pump_ctx.state_current = STATE_PUMP_RUNNING;
  } else {
    relay_turn_off();
    s_pump_ctx.state_current = STATE_PUMP_IDLE;
  }
}

void m_pump_controler_toggle_pump(void) {
  bool is_online = wifi_is_connected() && app_mqtt_is_connected();
  if (is_online && s_pump_ctx.mode == MODE_PUMP_AUTO) {
    ESP_LOGW(TAG, "🤖 [CHẾ ĐỘ TỰ ĐỘNG (AUTO)] Đang kích hoạt! Đã KHÓA nút cứng tủ điện (hãy chuyển sang MANUAL trên App để điều khiển bằng tay).");
    return;
  }
  if (is_online && s_pump_ctx.child_lock) {
    ESP_LOGW(TAG, "🔒 [KHÓA TRẺ EM] Đang BẬT! Bỏ qua thao tác bấm nút.");
    return;
  }
  relay_toggle();
}

void m_pump_controler_set_child_lock(bool enable) {
  s_pump_ctx.child_lock = enable;
  ESP_LOGI(TAG, "Khóa trẻ em: %s", enable ? "BẬT (LOCKED)" : "TẮT (UNLOCKED)");
}

void m_pump_controler_set_config(float tank_height_cm, float sensor_offset_cm,
                                 int min_pct, int max_pct,
                                 uint32_t max_runtime_sec) {
  if (tank_height_cm > 0.0f)
    s_pump_ctx.tank_height_cm = tank_height_cm;
  if (sensor_offset_cm >= 0.0f)
    s_pump_ctx.sensor_offset_cm = sensor_offset_cm;
  if (min_pct >= 0 && min_pct <= 100)
    s_pump_ctx.min_water_percent = min_pct;
  if (max_pct >= 0 && max_pct <= 100)
    s_pump_ctx.max_water_percent = max_pct;
  if (max_runtime_sec > 0)
    s_pump_ctx.max_runtime_sec = max_runtime_sec;

  ESP_LOGI(TAG,
           "Đã cập nhật cấu hình bể: Cao=%.1fcm, Offset=%.1fcm, Auto=[%d%% - "
           "%d%%], MaxRun=%lus",
           s_pump_ctx.tank_height_cm, s_pump_ctx.sensor_offset_cm,
           s_pump_ctx.min_water_percent, s_pump_ctx.max_water_percent,
           (unsigned long)s_pump_ctx.max_runtime_sec);
}

void m_pump_controler_clear_error(void) {
  s_pump_ctx.state_current = STATE_PUMP_IDLE;
  s_pump_ctx.runtime_counter_sec = 0;
  ESP_LOGI(TAG, "Đã xóa toàn bộ cảnh báo lỗi máy bơm!");
}