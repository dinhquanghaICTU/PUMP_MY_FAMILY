#include "ota.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "jsmn.h"
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

static void ota_task(void *pvParameter) {

  ESP_LOGI(TAG, "URL: %s", s_current_ota_cfg.url);

  esp_http_client_config_t http_config = {
      .url = s_current_ota_cfg.url,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 20000,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &http_config,
  };

  /*
    check bug in ota this funcion


  */
  esp_err_t ret = esp_https_ota(&ota_config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "CẬP NHẬT OTA THÀNH CÔNG 100!");
    ESP_LOGI(TAG, "Đang khởi động lại hệ thống sau 2 giây...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
  } else {
    ESP_LOGE(TAG, "CẬP NHẬT OTA THẤT BẠI! Mã lỗi: %s (0x%x)",
             esp_err_to_name(ret), ret);
    s_is_updating = false;
  }

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

  BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Không thể tạo ota_task!");
    s_is_updating = false;
    return ESP_FAIL;
  }

  return ESP_OK;
}