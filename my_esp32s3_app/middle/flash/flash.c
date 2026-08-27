#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NVS_NAMESPACE "wifi_store"
static const char *TAG_NVS = "NVS_WIFI";

esp_err_t nvs_save_wifi_credentials(const char *ssid, const char *password) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK)
    return err;

  err = nvs_set_str(handle, "ssid", ssid);
  if (err == ESP_OK) {
    err = nvs_set_str(handle, "pass", password);
  }

  if (err == ESP_OK) {
    err = nvs_commit(handle);
    ESP_LOGI(TAG_NVS, "Da luu SSID='%s' va PASS vao Flash NVS thanh cong!",
             ssid);
  }

  nvs_close(handle);
  return err;
}

esp_err_t nvs_load_wifi_credentials(char *ssid, size_t max_ssid_len,
                                    char *password, size_t max_pass_len) {
  nvs_handle_t handle;
  esp_err_t err;

  err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    return err;
  }

  size_t ssid_len = max_ssid_len;
  size_t pass_len = max_pass_len;

  err = nvs_get_str(handle, "ssid", ssid, &ssid_len);
  if (err == ESP_OK) {
    err = nvs_get_str(handle, "pass", password, &pass_len);
  }

  nvs_close(handle);
  return err;
}

esp_err_t nvs_erase_wifi_credentials(void) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err == ESP_OK) {
    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG_NVS, "Da xoa trang thong tin Wi-Fi trong Flash!");
  }
  return err;
}
