#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "MIDDLE_WIFI";
static bool s_is_wifi_initialized = false;

esp_err_t wifi_init_sta(uint8_t channel) {
  if (s_is_wifi_initialized) {
    return ESP_OK;
  }

  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Cố định Channel Wi-Fi an toàn cho ESP-NOW
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  s_is_wifi_initialized = true;
  ESP_LOGI(TAG, "Khởi tạo Wi-Fi STA cho ESP-NOW THÀNH CÔNG (Channel %d)",
           channel);
  return ESP_OK;
}

esp_err_t wifi_set_channel(uint8_t channel) {
  if (!s_is_wifi_initialized) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_wifi_set_promiscuous(true);
  esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  return err;
}