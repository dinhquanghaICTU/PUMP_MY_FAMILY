#include "ota.h"
#include "button.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jsmn.h"
#include "m_state_machine.h"
#include "mqtt.h"
#include "node_esp.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "OTA_ENGINE";

static ota_config_t s_current_ota_cfg = {0};
static bool s_is_updating = false;

static char s_fw_ver_buf[32] = {0};
static char s_tank_ver_buf[32] = {0};

const char *ota_get_current_version(void) {
  if (s_fw_ver_buf[0] != '\0') {
    return s_fw_ver_buf;
  }
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = sizeof(s_fw_ver_buf);
    if (nvs_get_str(nvs, "fw_ver", s_fw_ver_buf, &len) == ESP_OK && len > 1) {
      nvs_close(nvs);
      return s_fw_ver_buf;
    }
    nvs_close(nvs);
  }
  const esp_app_desc_t *app_desc = esp_app_get_description();
  if (app_desc && strlen(app_desc->version) > 0) {
    strncpy(s_fw_ver_buf, app_desc->version, sizeof(s_fw_ver_buf) - 1);
  } else {
    strncpy(s_fw_ver_buf, "1.0.0", sizeof(s_fw_ver_buf) - 1);
  }
  return s_fw_ver_buf;
}

const char *ota_get_tank_version(void) {
  if (s_tank_ver_buf[0] != '\0') {
    return s_tank_ver_buf;
  }
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READONLY, &nvs) == ESP_OK) {
    size_t len = sizeof(s_tank_ver_buf);
    if (nvs_get_str(nvs, "tank_ver", s_tank_ver_buf, &len) == ESP_OK &&
        len > 1) {
      nvs_close(nvs);
      return s_tank_ver_buf;
    }
    nvs_close(nvs);
  }
  strncpy(s_tank_ver_buf, "1.0.0", sizeof(s_tank_ver_buf) - 1);
  return s_tank_ver_buf;
}

void ota_set_tank_version(const char *ver) {
  if (!ver || strlen(ver) == 0)
    return;
  if (strncmp(s_tank_ver_buf, ver, sizeof(s_tank_ver_buf)) != 0) {
    strncpy(s_tank_ver_buf, ver, sizeof(s_tank_ver_buf) - 1);
    s_tank_ver_buf[sizeof(s_tank_ver_buf) - 1] = '\0';
    nvs_handle_t nvs;
    if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
      nvs_set_str(nvs, "tank_ver", s_tank_ver_buf);
      nvs_commit(nvs);
      nvs_close(nvs);
    }
    ESP_LOGI(TAG,
             "💾 [CẬP NHẬT VERSION BỂ NƯỚC] Phiên bản Bể Nước hiện tại: [%s]",
             s_tank_ver_buf);
  }
}

esp_err_t ota_parse_json(const char *json_str, ota_config_t *out_cfg) {
  if (!json_str || !out_cfg) {
    return ESP_ERR_INVALID_ARG;
  }
  memset(out_cfg, 0, sizeof(ota_config_t));

  jsmn_parser parser;
  jsmntok_t tokens[32];

  int r = json_parser(json_str, &parser, tokens, 32);
  if (r < 0) {
    ESP_LOGE(TAG, "Lỗi phân tích cú pháp JSON bằng JSMN!");
    return ESP_FAIL;
  }

  if (json_get_str(json_str, tokens, r, "url", out_cfg->url,
                   sizeof(out_cfg->url)) == 0) {
    ESP_LOGE(TAG, "Gói tin OTA thiếu trường 'url'!");
    return ESP_ERR_INVALID_ARG;
  }

  json_get_str(json_str, tokens, r, "version", out_cfg->version,
               sizeof(out_cfg->version));
  json_get_str(json_str, tokens, r, "target", out_cfg->target,
               sizeof(out_cfg->target));
  json_get_str(json_str, tokens, r, "md5", out_cfg->md5, sizeof(out_cfg->md5));
  out_cfg->size = json_get_int(json_str, tokens, r, "size");

  ESP_LOGI(
      TAG,
      "Bóc tách OTA thành công: Version=[%s], Target=[%s], Size=%d, URL=[%s]",
      out_cfg->version, out_cfg->target, out_cfg->size, out_cfg->url);

  return ESP_OK;
}

// 1. Task nạp OTA cho chính con Tủ Điện (Local ESP32-S3)
static void ota_task(void *pvParameter) {
  ESP_LOGI(TAG, "🚀 [OTA LOCAL TỦ ĐIỆN] Bắt đầu tải và nạp từ: %s",
           s_current_ota_cfg.url);

  esp_http_client_config_t http_config = {
      .url = s_current_ota_cfg.url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 20000,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
  };

  esp_https_ota_handle_t https_ota_handle = NULL;
  esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ [OTA LOCAL] Khởi tạo OTA thất bại: %s (0x%x)",
             esp_err_to_name(err), err);
    char err_buf[256];
    snprintf(err_buf, sizeof(err_buf),
             "{\"event\":\"ota_progress\",\"target\":\"esp32s3_cabinet\","
             "\"status\":\"failed\",\"percent\":0,\"error\":\"%s\"}",
             esp_err_to_name(err));
    app_mqtt_publish("pump/family/status", err_buf, 1, 0);
    button_task_start();
    m_state_machine_set_state(STATE_IDLE);
    s_is_updating = false;
    vTaskDelete(NULL);
    return;
  }

  int image_size = esp_https_ota_get_image_size(https_ota_handle);
  if (image_size <= 0) {
    image_size = s_current_ota_cfg.size;
  }

  int last_reported_percent = -1;

  while (1) {
    err = esp_https_ota_perform(https_ota_handle);
    if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      break;
    }
    int read_len = esp_https_ota_get_image_len_read(https_ota_handle);
    int percent = (image_size > 0) ? (read_len * 100 / image_size) : 0;
    if (percent > 100)
      percent = 100;

    // Bắn tiến độ mỗi 5% một lần
    if (percent != last_reported_percent &&
        (percent % 5 == 0 || last_reported_percent == -1)) {
      last_reported_percent = percent;
      ESP_LOGI(TAG, "📡 [OTA S3 PROGRESS] %d%% (%d/%d bytes)", percent,
               read_len, image_size);
      char prog_buf[256];
      snprintf(prog_buf, sizeof(prog_buf),
               "{\"event\":\"ota_progress\",\"target\":\"esp32s3_cabinet\","
               "\"status\":\"in_progress\",\"percent\":%d,\"bytes\":%d,"
               "\"total\":%d}",
               percent, read_len, image_size);
      app_mqtt_publish("pump/family/status", prog_buf, 1, 0);
    }
  }

  if (esp_https_ota_is_complete_data_received(https_ota_handle)) {
    esp_err_t finish_err = esp_https_ota_finish(https_ota_handle);
    if (finish_err == ESP_OK) {
      ESP_LOGI(TAG, "🎉 [OTA LOCAL] CẬP NHẬT THÀNH CÔNG 100%%!");
      if (strlen(s_current_ota_cfg.version) > 0) {
        nvs_handle_t nvs;
        if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
          nvs_set_str(nvs, "fw_pending", s_current_ota_cfg.version);
          nvs_commit(nvs);
          nvs_close(nvs);
          ESP_LOGI(TAG,
                   "💾 Đã ghi nhận phiên bản chờ xác nhận (fw_pending): [%s]",
                   s_current_ota_cfg.version);
        }
      }
      char stat_buf[256];
      snprintf(stat_buf, sizeof(stat_buf),
               "{\"event\":\"ota_progress\",\"target\":\"esp32s3_cabinet\","
               "\"status\":\"rebooting\",\"percent\":100,\"message\":\"Đã ghi "
               "flash 100%%, đang khởi động lại kiểm tra firmware...\"}");
      app_mqtt_publish("pump/family/status", stat_buf, 1, 0);
      vTaskDelay(pdMS_TO_TICKS(500));
      ESP_LOGI(TAG, "Khởi động lại hệ thống sau 2 giây...");
      vTaskDelay(pdMS_TO_TICKS(2000));
      esp_restart();
    } else {
      ESP_LOGE(TAG, "❌ [OTA LOCAL] Finish thất bại: %s (0x%x)",
               esp_err_to_name(finish_err), finish_err);
      char err_buf[256];
      snprintf(err_buf, sizeof(err_buf),
               "{\"event\":\"ota_progress\",\"target\":\"esp32s3_cabinet\","
               "\"status\":\"failed\",\"percent\":%d,\"error\":\"%s\"}",
               last_reported_percent >= 0 ? last_reported_percent : 0,
               esp_err_to_name(finish_err));
      app_mqtt_publish("pump/family/status", err_buf, 1, 0);
    }
  } else {
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "❌ [OTA LOCAL] Nạp thất bại giữa chừng: %s (0x%x)",
             esp_err_to_name(err), err);
    char err_buf[256];
    snprintf(err_buf, sizeof(err_buf),
             "{\"event\":\"ota_progress\",\"target\":\"esp32s3_cabinet\","
             "\"status\":\"failed\",\"percent\":%d,\"error\":\"%s\"}",
             last_reported_percent >= 0 ? last_reported_percent : 0,
             esp_err_to_name(err));
    app_mqtt_publish("pump/family/status", err_buf, 1, 0);
  }

  button_task_start();
  m_state_machine_set_state(STATE_IDLE);
  s_is_updating = false;
  vTaskDelete(NULL);
}

static void ota_tank_esp_now_task(void *pvParameter) {
  ESP_LOGI(TAG, "🚀 [OTA GATEWAY PER-CHUNK ACK] Bắt đầu nạp cho Bể Nước: %s",
           s_current_ota_cfg.url);
  m_state_machine_set_state(STATE_OTA_NODE_TANK);

  esp_http_client_config_t http_config = {
      .url = s_current_ota_cfg.url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 25000,
      .keep_alive_enable = true,
  };

  esp_http_client_handle_t client = esp_http_client_init(&http_config);
  if (!client) {
    ESP_LOGE(TAG, "Không thể khởi tạo HTTP Client cho OTA Bể Nước!");
    button_task_start();
    m_state_machine_set_state(STATE_IDLE);
    s_is_updating = false;
    vTaskDelete(NULL);
    return;
  }

  esp_err_t err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Mở kết nối HTTP tải OTA Bể Nước thất bại: %s",
             esp_err_to_name(err));
    esp_http_client_cleanup(client);
    button_task_start();
    m_state_machine_set_state(STATE_IDLE);
    s_is_updating = false;
    vTaskDelete(NULL);
    return;
  }

  int content_length = esp_http_client_fetch_headers(client);
  if (content_length <= 0) {
    content_length = s_current_ota_cfg.size;
  }
  ESP_LOGI(TAG, "Kích thước file firmware Bể Nước: %d bytes", content_length);

  ota_esp_now_packet_t pkt_start = {
      .type = OTA_PACKET_TYPE_START,
      .chunk_index = 0,
      .data_len = sizeof(uint32_t),
  };
  uint32_t total_sz = (uint32_t)content_length;
  memcpy(pkt_start.data, &total_sz, sizeof(uint32_t));

  // 1. Handshake với Node Bể Nước - Chờ Node ACK sẵn sàng nhận OTA
  bool tank_ready = false;
  ESP_LOGI(TAG,
           "⏳ Đang gửi lệnh khởi động OTA và chờ phản hồi từ Node Bể Nước...");

  for (int retry = 1; retry <= 25; retry++) {
    node_esp_reset_ota_ack();
    node_esp_send_raw((const uint8_t *)&pkt_start,
                      sizeof(ota_esp_now_packet_t));

    if (node_esp_wait_ota_ack(0, 500)) {
      tank_ready = true;
      ESP_LOGI(TAG,
               "✅ [HANDSHAKE THÀNH CÔNG] Node Bể Nước đã phản hồi ACK SẴN "
               "SÀNG nhận OTA (Lần %d)!",
               retry);
      break;
    }
    ESP_LOGW(TAG, "Đang gọi Node Bể Nước (Lần %d/25)...", retry);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  if (!tank_ready) {
    ESP_LOGE(TAG, "❌ [LỖI OTA] Không nhận được phản hồi từ Node Bể Nước sau "
                  "25 lần thử! Hủy tiến trình.");
    char err_buf[256];
    snprintf(err_buf, sizeof(err_buf),
             "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\","
             "\"status\":\"failed\",\"percent\":0,\"error\":\"TANK_NODE_"
             "UNRESPONSIVE\","
             "\"message\":\"Không thể kết nối với Node Bể Nước (Timeout chờ "
             "ACK)! Vui lòng kiểm tra nguồn và khoảng cách con bể.\"}");
    app_mqtt_publish("pump/family/status", err_buf, 1, 0);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    button_task_start();
    m_state_machine_set_state(STATE_IDLE);
    s_is_updating = false;
    vTaskDelete(NULL);
    return;
  }

  char init_buf[256];
  snprintf(init_buf, sizeof(init_buf),
           "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\","
           "\"status\":\"in_progress\",\"percent\":0,\"bytes\":0,\"total\":%d,"
           "\"message\":\"Node Bể Nước đã sẵn sàng, đang truyền firmware...\"}",
           content_length);
  app_mqtt_publish("pump/family/status", init_buf, 1, 0);

  vTaskDelay(pdMS_TO_TICKS(400));

  uint32_t chunk_idx = 0;
  size_t total_sent = 0;
  ota_esp_now_packet_t pkt_data;
  pkt_data.type = OTA_PACKET_TYPE_DATA;
  uint32_t total_retries = 0;
  int consecutive_failed_chunks = 0;

  while (1) {
    int read_bytes =
        esp_http_client_read(client, (char *)pkt_data.data, OTA_CHUNK_MAX_SIZE);
    if (read_bytes <= 0) {
      break; // Hết file
    }

    pkt_data.chunk_index = chunk_idx;
    pkt_data.data_len = (uint16_t)read_bytes;

    bool chunk_acked = false;
    for (int attempt = 0; attempt < 15; attempt++) {
      node_esp_reset_ota_ack();
      node_esp_send_raw((const uint8_t *)&pkt_data,
                        sizeof(ota_esp_now_packet_t));

      if (node_esp_wait_ota_ack(chunk_idx, 35)) {
        chunk_acked = true;
        break; // Đã nhận ACK thành công!
      } else {
        total_retries++;
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }

    if (!chunk_acked) {
      consecutive_failed_chunks++;
      ESP_LOGE(TAG,
               "❌ Không nhận được ACK chunk #%lu sau 15 lần thử (Liên tiếp "
               "mất: %d)",
               (unsigned long)chunk_idx, consecutive_failed_chunks);
      if (consecutive_failed_chunks >= 3) {
        ESP_LOGE(TAG,
                 "❌ [LỖI OTA] Mất kết nối hoàn toàn với Node Bể Nước tại "
                 "chunk #%lu! Hủy tiến trình.",
                 (unsigned long)chunk_idx);
        char fail_buf[256];
        snprintf(
            fail_buf, sizeof(fail_buf),
            "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\","
            "\"status\":\"failed\",\"percent\":%d,\"error\":\"CHUNK_TIMEOUT\","
            "\"message\":\"Mất kết nối với Node Bể Nước giữa chừng!\"}",
            (content_length > 0) ? (int)((total_sent * 100) / content_length)
                                 : 0);
        app_mqtt_publish("pump/family/status", fail_buf, 1, 0);

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        button_task_start();
        m_state_machine_set_state(STATE_IDLE);
        s_is_updating = false;
        vTaskDelete(NULL);
        return;
      }
    } else {
      consecutive_failed_chunks = 0;
    }

    total_sent += read_bytes;
    chunk_idx++;

    // Nhường CPU cho Wi-Fi stack & MQTT task duy trì kết nối
    vTaskDelay(pdMS_TO_TICKS(3));

    if (chunk_idx % 25 == 0) {
      int progress =
          (content_length > 0) ? (int)((total_sent * 100) / content_length) : 0;
      if (progress > 100)
        progress = 100;
      ESP_LOGI(TAG,
               "[OTA PER-CHUNK ACK] Tiến độ: %d%% (%d/%d bytes - Chunk #%lu "
               "| Retries: %lu)",
               progress, (int)total_sent, content_length,
               (unsigned long)chunk_idx, (unsigned long)total_retries);
      char prog_buf[256];
      snprintf(
          prog_buf, sizeof(prog_buf),
          "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\",\"status\":"
          "\"in_progress\",\"percent\":%d,\"bytes\":%d,\"total\":%d}",
          progress, (int)total_sent, content_length);
      app_mqtt_publish("pump/family/status", prog_buf, 1, 0);
    }
  }

  // Chờ 300ms
  vTaskDelay(pdMS_TO_TICKS(300));

  // 3. Bắn gói END sang Node Bể Nước để kết thúc và Reboot
  ota_esp_now_packet_t pkt_end = {
      .type = OTA_PACKET_TYPE_END,
      .chunk_index = chunk_idx,
      .data_len = 0,
  };
  node_esp_reset_tank_ota_status();
  for (int retry = 0; retry < 3; retry++) {
    node_esp_send_raw((const uint8_t *)&pkt_end, sizeof(ota_esp_now_packet_t));
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG,
           "📦 Đã gửi toàn bộ firmware sang Node Bể Nước (%d bytes). Đang chờ "
           "Bể Nước xác thực Flash...",
           (int)total_sent);

  char tank_err[64] = "TIMEOUT";
  tank_ota_response_t tank_res =
      node_esp_wait_tank_ota_finish(15000, tank_err, sizeof(tank_err));

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (tank_res == TANK_OTA_RESP_SUCCESS) {
    const char *final_tank_ver = (strlen(s_current_ota_cfg.version) > 0)
                                     ? s_current_ota_cfg.version
                                     : "1.0.0";
    ota_set_tank_version(final_tank_ver);
    ESP_LOGI(
        TAG,
        "🎉 [XÁC NHẬN TỪ BỂ NƯỚC] Flash hợp lệ! Cập nhật phiên bản mới: [%s]",
        final_tank_ver);
    char stat_buf[256];
    snprintf(stat_buf, sizeof(stat_buf),
             "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\",\"status\":"
             "\"success\",\"percent\":100,\"version\":\"%s\",\"message\":\"Cập "
             "nhật Bể Nước thành công 100%%! Node Bể Nước đang khởi động với "
             "firmware v%s\"}",
             final_tank_ver, final_tank_ver);
    app_mqtt_publish("pump/family/status", stat_buf, 1, 0);
  } else if (tank_res == TANK_OTA_RESP_FAIL) {
    ESP_LOGE(TAG,
             "❌ [BỂ NƯỚC TỪ CHỐI FIRMWARE] Lỗi: %s (Có thể sai chip ESP32 "
             "hoặc file lỗi)",
             tank_err);

    char fail_buf[256];
    snprintf(
        fail_buf, sizeof(fail_buf),
        "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\",\"status\":"
        "\"failed\",\"percent\":0,\"error\":\"VALIDATE_FAILED\",\"message\":"
        "\"Bể Nước từ chối firmware: %s (Sai chip ESP32 hoặc file hỏng)\"}",
        tank_err);
    app_mqtt_publish("pump/family/status", fail_buf, 1, 0);
  } else {
    ESP_LOGW(TAG, "⚠️ Không nhận được phản hồi kết thúc từ Bể Nước sau 15 giây");
    char fail_buf[256];
    snprintf(fail_buf, sizeof(fail_buf),
             "{\"event\":\"ota_progress\",\"target\":\"esp32_tank\",\"status\":"
             "\"failed\",\"percent\":100,\"error\":\"RESP_TIMEOUT\","
             "\"message\":\"Hết thời gian chờ Bể Nước xác nhận Flash\"}");
    app_mqtt_publish("pump/family/status", fail_buf, 1, 0);
  }

  button_task_start();
  m_state_machine_set_state(STATE_IDLE);
  s_is_updating = false;

  vTaskDelete(NULL);
}

esp_err_t ota_start(const ota_config_t *config) {
  if (!config || strlen(config->url) == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  if (s_is_updating) {
    ESP_LOGW(TAG,
             "Hệ thống đang trong quá trình OTA, bỏ qua yêu cầu trùng lặp!");
    return ESP_ERR_INVALID_STATE;
  }

  s_is_updating = true;
  memcpy(&s_current_ota_cfg, config, sizeof(ota_config_t));

  if (strcmp(config->target, "esp32_tank") == 0 ||
      strcmp(config->target, "sensor_node") == 0 ||
      strstr(config->target, "tank") != NULL) {
    ESP_LOGI(TAG, "Phát hiện mục tiêu OTA là [NODE BỂ NƯỚC] -> Khởi động OTA "
                  "Gateway Per-Chunk ACK...");
    BaseType_t ret = xTaskCreate(ota_tank_esp_now_task, "ota_tank_task", 8192,
                                 NULL, 5, NULL);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Không thể tạo ota_tank_task!");
      s_is_updating = false;
      return ESP_FAIL;
    }
  } else {
    ESP_LOGI(
        TAG,
        "Phát hiện mục tiêu OTA là [NODE TỦ ĐIỆN] -> Khởi động OTA Local...");
    m_state_machine_set_state(STATE_OTA);
    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Không thể tạo ota_task!");
      s_is_updating = false;
      return ESP_FAIL;
    }
  }

  return ESP_OK;
}

bool ota_is_updating(void) { return s_is_updating; }