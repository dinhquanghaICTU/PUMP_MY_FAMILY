#include "m_pump_controler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt.h"
#include "node_esp.h"
#include "ota.h"
#include "relay.h"
#include "wifi.h"
#include "m_state_machine.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "APP_PUMP_CONTROLER";

static ota_config_t s_pending_ota_cfg = {0};
static volatile bool s_has_pending_ota = false;

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

    if (has_data && seconds_since_last < 20) {
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

    // Quản lý trạng thái Online / Offline:
    // Khi mất mạng: Cho phép bấm nút cứng thủ công tại tủ điện, KHÔNG ghi đè chế độ trong Flash
    // Khi có lại mạng: Tự động khôi phục chính xác chế độ (AUTO / MANUAL) đã lưu trong Flash NVS!
    static bool s_was_online = true;
    bool is_online = wifi_is_connected() && app_mqtt_is_connected();
    if (!is_online && s_was_online) {
      s_was_online = false;
      ESP_LOGW(TAG, "📡 [MẤT KẾT NỐI MẠNG] Mở khóa nút cứng tủ điện để bấm tay cục bộ (giữ nguyên cấu hình trong Flash)...");
      s_pump_ctx.child_lock = false;
    } else if (is_online && !s_was_online) {
      s_was_online = true;
      nvs_handle_t nvs;
      if (nvs_open("system_cfg", NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t saved_mode = 1;
        if (nvs_get_u8(nvs, "pump_mode", &saved_mode) == ESP_OK) {
          s_pump_ctx.mode = (saved_mode == 1) ? MODE_PUMP_AUTO : MODE_PUMP_MANUAL;
          ESP_LOGI(TAG, "🌐 [CÓ LẠI MẠNG] Đã khôi phục lại chế độ hoạt động từ Flash NVS: [%s]!",
                   s_pump_ctx.mode == MODE_PUMP_AUTO ? "TỰ ĐỘNG (AUTO)" : "THỦ CÔNG (MANUAL)");
        }
        nvs_close(nvs);
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

    // =========================================================================
    // XỬ LÝ HÀNG ĐỢI SMART AUTO OTA: CHỜ NƯỚC ĐẦY BỂ RỒI MỚI NẠP FIRMWARE
    // =========================================================================
    if (s_has_pending_ota) {
      // 1. Kiểm tra nếu nước đã đầy (hoặc cảm biến đọc >= max_water_percent)
      if (s_pump_ctx.current_percent >= (float)s_pump_ctx.max_water_percent) {
        ESP_LOGI(TAG, "🎉 [SMART AUTO OTA] Nước bể đã ĐẦY (%.1f%% >= %d%%)! Tắt bơm và bắt đầu nạp OTA...",
                 s_pump_ctx.current_percent, s_pump_ctx.max_water_percent);
        relay_turn_off();
        s_pump_ctx.is_pump_on = false;
        s_pump_ctx.state_current = STATE_PUMP_IDLE;

        char start_buf[256];
        snprintf(start_buf, sizeof(start_buf),
                 "{\"event\":\"ota_progress\",\"target\":\"%s\",\"status\":\"in_progress\","
                 "\"percent\":0,\"message\":\"Bể đã đầy nước 100%%! Bắt đầu nạp firmware...\"}",
                 s_pending_ota_cfg.target);
        app_mqtt_publish("pump/family/status", start_buf, 1, 0);

        vTaskDelay(pdMS_TO_TICKS(2000)); // Chờ 2s ổn định relay và dòng nước

        s_has_pending_ota = false;
        m_state_machine_set_state(STATE_OTA);
        ota_start(&s_pending_ota_cfg);
        continue;
      } else if (!s_pump_ctx.is_pump_on && s_pump_ctx.mode == MODE_PUMP_AUTO &&
                 s_pump_ctx.state_current != STATE_PUMP_ERROR_TIMEOUT &&
                 s_pump_ctx.state_current != STATE_PUMP_ERROR_NODE_LOST) {
        // Nếu bơm đang tắt mà nước chưa đầy: Chủ động bật bơm lên để bơm đầy nước
        ESP_LOGI(TAG, "🤖 [SMART AUTO OTA] Tự động bật máy bơm để làm đầy bể trước khi nạp OTA...");
        relay_turn_on();
        s_pump_ctx.is_pump_on = true;
        s_pump_ctx.state_current = STATE_PUMP_RUNNING;
      }
    }

    static int s_telemetry_tick = 2; // Bắn ngay telemetry chứa version trên tick đầu tiên sau boot
    if (++s_telemetry_tick >= 2) {
      s_telemetry_tick = 0;
      char stat_json[320];
      snprintf(
          stat_json, sizeof(stat_json),
          "{\"pump\":%d,\"mode\":\"%s\",\"water_percent\":%.1f,\"distance_cm\":"
          "%.1f,\"battery\":%.2f,\"runtime\":%lu,\"child_lock\":%d,\"tank_online\":%d,\"state\":"
          "\"%s\",\"version\":\"%s\",\"tank_version\":\"%s\"}",
          s_pump_ctx.is_pump_on ? 1 : 0,
          s_pump_ctx.mode == MODE_PUMP_AUTO ? "auto" : "manual",
          s_pump_ctx.current_percent, s_pump_ctx.current_distance_cm,
          s_pump_ctx.node_battery_volt,
          (unsigned long)s_pump_ctx.runtime_counter_sec,
          s_pump_ctx.child_lock ? 1 : 0,
          (has_data && seconds_since_last < 20) ? 1 : 0,
          s_pump_ctx.state_current == STATE_PUMP_RUNNING
              ? "RUNNING"
              : (s_pump_ctx.state_current == STATE_PUMP_ERROR_TIMEOUT
                     ? "ERROR_TIMEOUT"
                     : (s_pump_ctx.state_current == STATE_PUMP_ERROR_NODE_LOST
                            ? "ERROR_NODE_LOST"
                            : "IDLE")),
          ota_get_current_version(),
          ota_get_tank_version());
      app_mqtt_publish("pump/family/status", stat_json, 1, 0);

      // Nếu đang trong trạng thái chờ bơm đầy để OTA, định kỳ bắn event để Web Dashboard cập nhật % nước
      if (s_has_pending_ota) {
        char pend_buf[320];
        snprintf(pend_buf, sizeof(pend_buf),
                 "{\"event\":\"ota_progress\",\"target\":\"%s\",\"status\":\"pending_wait_full\","
                 "\"percent\":0,\"water_percent\":%.1f,\"target_percent\":%d,"
                 "\"message\":\"Đang ở chế độ AUTO: Đang bơm nước đầy bể (%.1f%% / %d%%) trước khi nạp Firmware...\"}",
                 s_pending_ota_cfg.target, s_pump_ctx.current_percent, s_pump_ctx.max_water_percent,
                 s_pump_ctx.current_percent, s_pump_ctx.max_water_percent);
        app_mqtt_publish("pump/family/status", pend_buf, 1, 0);
      }
    }
  }
}

esp_err_t m_pump_controler_init(void) {
  relay_init();

  // Đọc chế độ hoạt động và thông số bể đã lưu trong Flash NVS (Phục hồi sau mất điện / khởi động lại)
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READONLY, &nvs) == ESP_OK) {
    uint8_t saved_mode = 1;
    if (nvs_get_u8(nvs, "pump_mode", &saved_mode) == ESP_OK) {
      s_pump_ctx.mode = (saved_mode == 1) ? MODE_PUMP_AUTO : MODE_PUMP_MANUAL;
      ESP_LOGI(TAG, "⚡ [NVS BOOT] Khôi phục chế độ bơm từ Flash: [%s]",
               s_pump_ctx.mode == MODE_PUMP_AUTO ? "TỰ ĐỘNG (AUTO)" : "THỦ CÔNG (MANUAL)");
    } else {
      s_pump_ctx.mode = MODE_PUMP_AUTO;
    }

    uint32_t val_u32 = 0;
    if (nvs_get_u32(nvs, "tank_h_x10", &val_u32) == ESP_OK && val_u32 > 0) {
      s_pump_ctx.tank_height_cm = (float)val_u32 / 10.0f;
    }
    if (nvs_get_u32(nvs, "offset_x10", &val_u32) == ESP_OK && val_u32 > 0) {
      s_pump_ctx.sensor_offset_cm = (float)val_u32 / 10.0f;
    }
    uint8_t val_u8 = 0;
    if (nvs_get_u8(nvs, "min_pct", &val_u8) == ESP_OK && val_u8 <= 100) {
      s_pump_ctx.min_water_percent = val_u8;
    }
    if (nvs_get_u8(nvs, "max_pct", &val_u8) == ESP_OK && val_u8 <= 100) {
      s_pump_ctx.max_water_percent = val_u8;
    }
    uint8_t saved_lock = 0;
    if (nvs_get_u8(nvs, "child_lock", &saved_lock) == ESP_OK) {
      s_pump_ctx.child_lock = (saved_lock == 1);
      ESP_LOGI(TAG, "⚡ [NVS BOOT] Khôi phục Khóa trẻ em từ Flash: [%s]",
               s_pump_ctx.child_lock ? "BẬT" : "TẮT");
    }
    nvs_close(nvs);
  }

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
      "Khởi tạo Module Điều Khiển Bơm THÀNH CÔNG! [Chế độ: %s | Auto: Min %d%% / Max %d%% | Cao: %.1fcm | Offset: %.1fcm]",
      s_pump_ctx.mode == MODE_PUMP_AUTO ? "AUTO" : "MANUAL",
      s_pump_ctx.min_water_percent, s_pump_ctx.max_water_percent,
      s_pump_ctx.tank_height_cm, s_pump_ctx.sensor_offset_cm);
  return ESP_OK;
}

const m_controler_pump_t *m_pump_controler_get_context(void) {
  return &s_pump_ctx;
}

void m_pump_controler_set_mode(m_mode_pump_t mode) {
  s_pump_ctx.mode = mode;
  ESP_LOGI(TAG, "Chuyển chế độ hoạt động bơm: %s",
           mode == MODE_PUMP_AUTO ? "TỰ ĐỘNG (AUTO)" : "THỦ CÔNG (MANUAL)");

  // Lưu ngay chế độ vào Flash NVS để phục hồi khi mất điện hoặc có lại mạng
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
    nvs_set_u8(nvs, "pump_mode", (uint8_t)mode);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "💾 [NVS SAVE] Đã lưu chế độ [%s] vào Flash NVS!",
             mode == MODE_PUMP_AUTO ? "AUTO" : "MANUAL");
  }
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
  if (s_pump_ctx.child_lock) {
    ESP_LOGW(TAG, "🔒 [KHÓA TRẺ EM] Đang BẬT! Bỏ qua thao tác bấm nút (Nhấn giữ 3 giây để thoát Khóa trẻ em).");
    return;
  }
  relay_toggle();
}

void m_pump_controler_set_child_lock(bool enable) {
  s_pump_ctx.child_lock = enable;
  ESP_LOGI(TAG, "Khóa trẻ em: %s", enable ? "BẬT (LOCKED)" : "TẮT (UNLOCKED)");

  // Lưu trạng thái vào NVS Flash để nhớ sau khi mất nguồn / khởi động lại
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
    nvs_set_u8(nvs, "child_lock", enable ? 1 : 0);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "💾 [NVS SAVE] Đã lưu trạng thái Khóa trẻ em [%s] vào Flash NVS!",
             enable ? "BẬT" : "TẮT");
  }

  // Nếu đang kết nối MQTT, gửi cập nhật ngay lập tức để Web/App đồng bộ
  if (wifi_is_connected() && app_mqtt_is_connected()) {
    char stat_json[320];
    const m_controler_pump_t *ctx = &s_pump_ctx;
    snprintf(
        stat_json, sizeof(stat_json),
        "{\"pump\":%d,\"mode\":\"%s\",\"water_percent\":%.1f,\"distance_cm\":"
        "%.1f,\"battery\":%.2f,\"runtime\":%lu,\"child_lock\":%d,\"tank_online\":%d,\"state\":"
        "\"%s\",\"version\":\"%s\",\"tank_version\":\"%s\"}",
        ctx->is_pump_on ? 1 : 0,
        ctx->mode == MODE_PUMP_AUTO ? "auto" : "manual",
        ctx->current_percent, ctx->current_distance_cm,
        ctx->node_battery_volt,
        (unsigned long)ctx->runtime_counter_sec,
        ctx->child_lock ? 1 : 0,
        1,
        ctx->is_pump_on ? "RUNNING" : "IDLE",
        ota_get_current_version(),
        ota_get_tank_version());
    app_mqtt_publish("pump/family/status", stat_json, 1, 0);
  }
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

  // Lưu cấu hình vào Flash NVS
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
    nvs_set_u32(nvs, "tank_h_x10", (uint32_t)(s_pump_ctx.tank_height_cm * 10.0f));
    nvs_set_u32(nvs, "offset_x10", (uint32_t)(s_pump_ctx.sensor_offset_cm * 10.0f));
    nvs_set_u8(nvs, "min_pct", (uint8_t)s_pump_ctx.min_water_percent);
    nvs_set_u8(nvs, "max_pct", (uint8_t)s_pump_ctx.max_water_percent);
    nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "💾 [NVS SAVE] Đã lưu cấu hình kích thước bể vào Flash NVS!");
  }

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

bool m_pump_controler_is_tank_full(void) {
  // Bể được coi là đầy khi: mức nước >= max_water_percent, máy bơm đang tắt, và cảm biến đọc hợp lệ (> 0)
  return (s_pump_ctx.current_percent >= (float)s_pump_ctx.max_water_percent &&
          !s_pump_ctx.is_pump_on &&
          s_pump_ctx.current_percent > 0.0f);
}

void m_pump_controler_queue_ota(const void *ota_cfg) {
  if (!ota_cfg) return;
  memcpy(&s_pending_ota_cfg, ota_cfg, sizeof(ota_config_t));
  s_has_pending_ota = true;

  ESP_LOGW(TAG, "⏳ [SMART AUTO OTA] Đã lưu lệnh OTA vào hàng đợi! Đang chờ bơm đầy bể (Nước: %.1f%% / Đầy: %d%%)...",
           s_pump_ctx.current_percent, s_pump_ctx.max_water_percent);

  // Nếu bơm chưa chạy và mức nước chưa đầy: Chủ động bật bơm ngay để làm đầy nước
  if (!s_pump_ctx.is_pump_on && s_pump_ctx.current_percent < (float)s_pump_ctx.max_water_percent) {
    ESP_LOGI(TAG, "🚀 [SMART AUTO OTA] Tự động BẬT máy bơm để bơm đầy nước trước khi nạp Firmware!");
    relay_turn_on();
    s_pump_ctx.is_pump_on = true;
    s_pump_ctx.state_current = STATE_PUMP_RUNNING;
  }

  // Báo cáo trạng thái PENDING_WAIT_FULL lên MQTT & Web Dashboard
  char pend_buf[320];
  snprintf(pend_buf, sizeof(pend_buf),
           "{\"event\":\"ota_progress\",\"target\":\"%s\",\"status\":\"pending_wait_full\","
           "\"percent\":0,\"water_percent\":%.1f,\"target_percent\":%d,"
           "\"message\":\"Đang ở chế độ AUTO: Hệ thống đang bơm nước đầy bể (%.1f%% / %d%%) trước khi nạp Firmware...\"}",
           s_pending_ota_cfg.target, s_pump_ctx.current_percent, s_pump_ctx.max_water_percent,
           s_pump_ctx.current_percent, s_pump_ctx.max_water_percent);
  app_mqtt_publish("pump/family/status", pend_buf, 1, 0);
}

bool m_pump_controler_has_pending_ota(void) {
  return s_has_pending_ota;
}

void m_pump_controler_cancel_pending_ota(void) {
  if (s_has_pending_ota) {
    s_has_pending_ota = false;
    memset(&s_pending_ota_cfg, 0, sizeof(ota_config_t));
    ESP_LOGI(TAG, "Đã hủy lệnh OTA đang chờ trong hàng đợi.");
  }
}