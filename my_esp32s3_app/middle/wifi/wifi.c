#include "wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "MIDDLE_WIFI";

static EventGroupHandle_t s_wifi_event_group = NULL;
static wifi_state_t s_current_state = WIFI_STATE_IDLE;
static app_wifi_config_t s_sta_config;
static int s_retry_num = 0;
static bool s_is_reconnect_en = true;
static wifi_event_cb_t s_event_callback = NULL;
static void *s_callback_arg = NULL;

static void wifi_set_state(wifi_state_t state) {
  s_current_state = state;
  if (s_event_callback) {
    s_event_callback(state, s_callback_arg);
  }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
      wifi_set_state(WIFI_STATE_CONNECTING);
      esp_wifi_connect();
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
      xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);

      if (!s_is_reconnect_en) {
        wifi_set_state(WIFI_STATE_DISCONNECTED);
        break;
      }

      if (s_sta_config.max_retry <= 0 || s_retry_num < s_sta_config.max_retry) {
        s_retry_num++;
        wifi_set_state(WIFI_STATE_RECONNECTING);
        ESP_LOGW(TAG, "Mất kết nối. Đang thử kết nối lại (lần %d)...",
                 s_retry_num);
        if (s_sta_config.retry_delay_ms > 0) {
          vTaskDelay(pdMS_TO_TICKS(s_sta_config.retry_delay_ms));
        }
        esp_wifi_connect();
      } else {
        wifi_set_state(WIFI_STATE_FAILED);
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGE(TAG, "Kết nối thất bại vượt quá số lần retry.");
      }
      break;

    default:
      break;
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "Đã nhận IP: " IPSTR, IP2STR(&event->ip_info.ip));
      s_retry_num = 0;
      wifi_set_state(WIFI_STATE_CONNECTED);
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
      xEventGroupClearBits(s_wifi_event_group,
                           WIFI_FAIL_BIT | WIFI_DISCONNECTED_BIT);
    }
  }
}

esp_err_t wifi_init(void) {
  if (s_wifi_event_group != NULL) {
    return ESP_OK;
  }

  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    return err;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

  return ESP_OK;
}

esp_err_t wifi_connect_sta(const app_wifi_config_t *config) {
  if (!config)
    return ESP_ERR_INVALID_ARG;

  memcpy(&s_sta_config, config, sizeof(app_wifi_config_t));
  s_retry_num = 0;
  s_is_reconnect_en = true;

  wifi_config_t wifi_cfg = {0};
  strncpy((char *)wifi_cfg.sta.ssid, config->ssid, sizeof(wifi_cfg.sta.ssid));
  strncpy((char *)wifi_cfg.sta.password, config->password,
          sizeof(wifi_cfg.sta.password));
  wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  return esp_wifi_start();
}

esp_err_t wifi_disconnect(void) {
  s_is_reconnect_en = false;
  return esp_wifi_disconnect();
}

void wifi_register_callback(wifi_event_cb_t cb, void *user_data) {
  s_event_callback = cb;
  s_callback_arg = user_data;
}

bool wifi_is_connected(void) {
  return (s_current_state == WIFI_STATE_CONNECTED);
}

bool wifi_wait_for_connected(TickType_t timeout_ticks) {
  if (!s_wifi_event_group)
    return false;
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, timeout_ticks);
  return (bits & WIFI_CONNECTED_BIT) != 0;
}

EventGroupHandle_t wifi_get_event_group(void) { return s_wifi_event_group; }

wifi_state_t wifi_get_state(void) { return s_current_state; }
