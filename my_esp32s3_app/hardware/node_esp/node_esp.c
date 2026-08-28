#include "node_esp.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "wifi.h"
#include <string.h>

static const char *TAG = "NODE_ESP_NOW";

static SensorData_t g_recv_data = {0};
static uint32_t g_last_packet_id = 0;
static uint32_t g_total_received = 0;
static uint32_t g_total_lost = 0;
static int64_t g_last_recv_time_us = 0;
static bool g_has_data = false;

static void on_esp_now_recv_cb(const esp_now_recv_info_t *recv_info,
                               const uint8_t *incoming_data, int len) {
  if (len != sizeof(SensorData_t)) {
    ESP_LOGW(TAG, "Nhận gói tin sai kích thước: %d bytes (Cần: %d bytes)", len,
             sizeof(SensorData_t));
    return;
  }

  memcpy(&g_recv_data, incoming_data, sizeof(SensorData_t));
  g_total_received++;
  g_last_recv_time_us = esp_timer_get_time();
  g_has_data = true;

  if (g_last_packet_id != 0 && g_recv_data.packet_id > g_last_packet_id + 1) {
    uint32_t lost = g_recv_data.packet_id - g_last_packet_id - 1;
    g_total_lost += lost;
    ESP_LOGW(TAG, "⚠️ [CẢNH BÁO] Vừa rớt mất %lu gói tin!", (unsigned long)lost);
  }
  g_last_packet_id = g_recv_data.packet_id;

  float loss_rate =
      ((float)g_total_lost / (float)(g_total_received + g_total_lost)) * 100.0f;
  int rssi = recv_info->rx_ctrl->rssi;

  ESP_LOGI(TAG,
           "📥 [GÓI #%lu] | Nước: %.2f cm | Pin: %.2fV | RSSI: %d dBm | Rớt: "
           "%.1f%% (Tổng nhận: %lu / Mất: %lu)",
           (unsigned long)g_recv_data.packet_id, g_recv_data.distance_cm,
           g_recv_data.battery_volt, rssi, loss_rate,
           (unsigned long)g_total_received, (unsigned long)g_total_lost);
}

esp_err_t node_esp_init(void) {
  // 1. Đảm bảo Wi-Fi đã khởi tạo và bật
  wifi_init();

  // 2. Khởi tạo ESP-NOW
  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Khởi tạo ESP-NOW thất bại: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_now_register_recv_cb(on_esp_now_recv_cb);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Đăng ký callback nhận thất bại: %s", esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(
      TAG,
      "Khởi tạo ESP-NOW Receiver THÀNH CÔNG! Đang lắng nghe từ con Bể Nước...");
  return ESP_OK;
}

bool node_esp_get_latest_data(SensorData_t *out_data) {
  if (!g_has_data || !out_data)
    return false;
  memcpy(out_data, &g_recv_data, sizeof(SensorData_t));
  return true;
}

uint32_t node_esp_get_seconds_since_last_packet(void) {
  if (g_last_recv_time_us == 0)
    return 999999;
  int64_t diff_us = esp_timer_get_time() - g_last_recv_time_us;
  return (uint32_t)(diff_us / 1000000);
}
