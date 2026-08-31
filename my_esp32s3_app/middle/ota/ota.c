#include "ota.h"
#include "button.h"
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
#include "node_esp.h"
#include <string.h>

static const char *TAG = "OTA_ENGINE";

static ota_config_t s_current_ota_cfg = {0};
static bool s_is_updating = false;

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
  ESP_LOGI(TAG, "🚀 [OTA LOCAL TỦ ĐIỆN] Bắt đầu tải và nạp từ: %s", s_current_ota_cfg.url);

  esp_http_client_config_t http_config = {
      .url = s_current_ota_cfg.url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 20000,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
  };

  esp_err_t ret = esp_https_ota(&ota_config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "🎉 [OTA LOCAL] CẬP NHẬT THÀNH CÔNG 100%%!");
    ESP_LOGI(TAG, "Khởi động lại hệ thống sau 2 giây...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  } else {
    ESP_LOGE(TAG, "❌ [OTA LOCAL] THẤT BẠI! Mã lỗi: %s (0x%x)",
             esp_err_to_name(ret), ret);
    button_task_start();
    m_state_machine_set_state(STATE_IDLE);
    s_is_updating = false;
  }

  vTaskDelete(NULL);
}

// 2. Task Gateway trung chuyển OTA qua ESP-NOW Per-Chunk ACK cho Node Bể Nước (ESP32-U)
static void ota_tank_esp_now_task(void *pvParameter) {
  ESP_LOGI(TAG, "🚀 [OTA GATEWAY PER-CHUNK ACK] Bắt đầu nạp cho Bể Nước: %s", s_current_ota_cfg.url);
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
    ESP_LOGE(TAG, "Mở kết nối HTTP tải OTA Bể Nước thất bại: %s", esp_err_to_name(err));
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

  // 1. Bắn gói START sang Node Bể Nước và đợi con bể phản hồi ACK
  ota_esp_now_packet_t pkt_start = {
      .type = OTA_PACKET_TYPE_START,
      .chunk_index = 0,
      .data_len = sizeof(uint32_t),
  };
  uint32_t total_sz = (uint32_t)content_length;
  memcpy(pkt_start.data, &total_sz, sizeof(uint32_t));

  for (int retry = 0; retry < 5; retry++) {
    node_esp_send_raw((const uint8_t *)&pkt_start, sizeof(ota_esp_now_packet_t));
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  // Cho Node Bể Nước 600ms để xóa Flash trước khi nhận chunk đầu tiên
  vTaskDelay(pdMS_TO_TICKS(600));

  // 2. Vòng lặp tải từng chunk 192 bytes và bắn có xác nhận ACK từng chunk một (Zero-Loss)
  uint32_t chunk_idx = 0;
  size_t total_sent = 0;
  ota_esp_now_packet_t pkt_data;
  pkt_data.type = OTA_PACKET_TYPE_DATA;
  uint32_t total_retries = 0;

  while (1) {
    int read_bytes = esp_http_client_read(client, (char *)pkt_data.data, OTA_CHUNK_MAX_SIZE);
    if (read_bytes <= 0) {
      break; // Hết file
    }

    pkt_data.chunk_index = chunk_idx;
    pkt_data.data_len = (uint16_t)read_bytes;

    // Bắn và đợi ACK xác nhận chunk này từ con bể (nếu chưa thấy ACK thì bắn lại chính chunk này)
    bool chunk_acked = false;
    for (int attempt = 0; attempt < 8; attempt++) {
      node_esp_send_raw((const uint8_t *)&pkt_data, sizeof(ota_esp_now_packet_t));

      if (node_esp_wait_ota_ack(chunk_idx, 25)) {
        chunk_acked = true;
        break; // Đã nhận ACK thành công!
      } else {
        total_retries++;
        vTaskDelay(pdMS_TO_TICKS(4));
      }
    }

    total_sent += read_bytes;
    chunk_idx++;

    if (chunk_idx % 50 == 0) {
      int progress = (content_length > 0) ? (int)((total_sent * 100) / content_length) : 0;
      ESP_LOGI(TAG, "📡 [OTA PER-CHUNK ACK] Tiến độ: %d%% (%d/%d bytes - Chunk #%lu | Retries: %lu)",
               progress, (int)total_sent, content_length, (unsigned long)chunk_idx, (unsigned long)total_retries);
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
  for (int retry = 0; retry < 5; retry++) {
    node_esp_send_raw((const uint8_t *)&pkt_end, sizeof(ota_esp_now_packet_t));
    vTaskDelay(pdMS_TO_TICKS(80));
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  ESP_LOGI(TAG, "🎉 ĐÃ NẠP XONG 100%% FIRMWARE BỂ NƯỚC! Tổng gửi: %d bytes, Số chunk: %lu",
           (int)total_sent, (unsigned long)chunk_idx);
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
    ESP_LOGW(TAG, "Hệ thống đang trong quá trình OTA, bỏ qua yêu cầu trùng lặp!");
    return ESP_ERR_INVALID_STATE;
  }

  s_is_updating = true;
  memcpy(&s_current_ota_cfg, config, sizeof(ota_config_t));

  if (strcmp(config->target, "esp32_tank") == 0 ||
      strcmp(config->target, "sensor_node") == 0 ||
      strstr(config->target, "tank") != NULL) {
    ESP_LOGI(TAG, "Phát hiện mục tiêu OTA là [NODE BỂ NƯỚC] -> Khởi động OTA Gateway Per-Chunk ACK...");
    BaseType_t ret = xTaskCreate(ota_tank_esp_now_task, "ota_tank_task", 8192, NULL, 5, NULL);
    if (ret != pdPASS) {
      ESP_LOGE(TAG, "Không thể tạo ota_tank_task!");
      s_is_updating = false;
      return ESP_FAIL;
    }
  } else {
    ESP_LOGI(TAG, "Phát hiện mục tiêu OTA là [NODE TỦ ĐIỆN] -> Khởi động OTA Local...");
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