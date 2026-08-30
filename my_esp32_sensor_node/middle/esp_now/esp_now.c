#include "esp_now.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include <string.h>

static const char *TAG = "MIDDLE_ESP_NOW";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF};

static void on_data_sent(const uint8_t *mac_addr,
                         esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    ESP_LOGD(TAG, "Gửi ESP-NOW thành công!");
  } else {
    ESP_LOGW(TAG, "Gửi ESP-NOW thất bại!");
  }
}

esp_err_t esp_now_node_init(uint8_t wifi_channel) {
  esp_err_t ret = esp_now_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  esp_now_register_send_cb(on_data_sent);

  esp_now_peer_info_t peer_info = {0};
  memcpy(peer_info.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
  peer_info.channel = wifi_channel;
  peer_info.ifidx = WIFI_IF_STA;
  peer_info.encrypt = false;

  ret = esp_now_add_peer(&peer_info);
  if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
    ESP_LOGE(TAG, "Thêm peer ESP-NOW thất bại: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "Khởi tạo ESP-NOW Sender thành công (Channel %d)",
           wifi_channel);
  return ESP_OK;
}

esp_err_t esp_now_node_send(const SensorData_t *data) {
  if (!data) {
    return ESP_ERR_INVALID_ARG;
  }
  return esp_now_send(s_broadcast_mac, (const uint8_t *)data,
                      sizeof(SensorData_t));
}