#include "node_esp.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt.h"
#include "ota.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "HARDWARE_NODE_ESP";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF};
static uint8_t s_tank_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_has_tank_mac = false;

static SensorData_t g_recv_data = {0};
static bool g_has_data = false;
static int64_t g_last_recv_time_us = 0;
static uint32_t g_total_received = 0;
static uint32_t g_total_lost = 0;
static uint32_t g_last_packet_id = 0;

static SemaphoreHandle_t s_ack_sem = NULL;
static volatile uint32_t s_last_acked_chunk = 0;

static void on_esp_now_send_cb(const uint8_t *mac_addr,
                               esp_now_send_status_t status) {}

static void on_esp_now_recv_cb(const esp_now_recv_info_t *recv_info,
                               const uint8_t *data, int len) {
  if (!data || len == 0) {
    return;
  }

  // 1. Xử lý các gói tin phản hồi OTA từ con Bể Nước
  if (len == sizeof(ota_esp_now_packet_t)) {
    const ota_esp_now_packet_t *ota_pkt = (const ota_esp_now_packet_t *)data;

    if (ota_pkt->type == OTA_PACKET_TYPE_ACK) {
      s_last_acked_chunk = ota_pkt->chunk_index;
      if (s_ack_sem) {
        xSemaphoreGive(s_ack_sem);
      }
      return;
    } else if (ota_pkt->type == OTA_PACKET_TYPE_FAIL) {
      char err_str[64] = "UNKNOWN";
      if (ota_pkt->data_len > 0) {
        memcpy(err_str, ota_pkt->data, ota_pkt->data_len);
        err_str[ota_pkt->data_len] = '\0';
      }
      ESP_LOGE(TAG, "🚨 [CẢNH BÁO TỪ BỂ NƯỚC] OTA THẤT BẠI! Mã lỗi: %lu (%s)",
               (unsigned long)ota_pkt->chunk_index, err_str);

      // Bắn trạng thái lỗi lên MQTT Cloud để người dùng biết ngay
      char mqtt_buf[256];
      snprintf(mqtt_buf, sizeof(mqtt_buf),
               "{\"event\":\"ota_status\",\"target\":\"node_tank\",\"status\":\"failed\",\"error_code\":%lu,\"message\":\"%s\"}",
               (unsigned long)ota_pkt->chunk_index, err_str);
      app_mqtt_publish("pump/family/status", mqtt_buf, 1, 0);
      return;
    } else if (ota_pkt->type == OTA_PACKET_TYPE_SUCCESS) {
      ESP_LOGI(TAG, "🎉 [BÁO CÁO TỪ BỂ NƯỚC] OTA THÀNH CÔNG 100%%! Bể Nước đang Reboot...");
      char mqtt_buf[256];
      snprintf(mqtt_buf, sizeof(mqtt_buf),
               "{\"event\":\"ota_status\",\"target\":\"node_tank\",\"status\":\"success\",\"message\":\"OTA completed successfully, rebooting\"}");
      app_mqtt_publish("pump/family/status", mqtt_buf, 1, 0);
      return;
    }
  }

  // 2. Xử lý gói tin cảm biến SensorData_t
  if (len == sizeof(SensorData_t)) {
    if (recv_info && recv_info->src_addr) {
      if (!s_has_tank_mac ||
          memcmp(s_tank_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN) != 0) {
        memcpy(s_tank_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
        s_has_tank_mac = true;

        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, s_tank_mac, ESP_NOW_ETH_ALEN);
        peer_info.channel = 0;
        peer_info.ifidx = WIFI_IF_STA;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);

        ESP_LOGI(TAG,
                 "🎯 Ghi nhận MAC Node Bể Nước: %02x:%02x:%02x:%02x:%02x:%02x",
                 s_tank_mac[0], s_tank_mac[1], s_tank_mac[2], s_tank_mac[3],
                 s_tank_mac[4], s_tank_mac[5]);
      }
    }

    memcpy(&g_recv_data, data, sizeof(SensorData_t));
    g_has_data = true;
    g_last_recv_time_us = esp_timer_get_time();
    g_total_received++;

    if (g_last_packet_id != 0 && g_recv_data.packet_id > g_last_packet_id + 1) {
      uint32_t lost = g_recv_data.packet_id - g_last_packet_id - 1;
      g_total_lost += lost;
      ESP_LOGW(TAG, "[CẢNH BÁO] Vừa rớt mất %lu gói tin!", (unsigned long)lost);
    }
    g_last_packet_id = g_recv_data.packet_id;

    float loss_rate =
        ((float)g_total_lost / (float)(g_total_received + g_total_lost)) *
        100.0f;
    int rssi = recv_info->rx_ctrl->rssi;

    ESP_LOGI(TAG,
             "[GÓI #%lu] | Nước: %.2f cm | Pin: %.2fV | RSSI: %d dBm | Rớt: "
             "%.1f%% (Tổng nhận: %lu / Mất: %lu)",
             (unsigned long)g_recv_data.packet_id, g_recv_data.distance_cm,
             g_recv_data.battery_volt, rssi, loss_rate,
             (unsigned long)g_total_received, (unsigned long)g_total_lost);
  }
}

esp_err_t node_esp_init(void) {
  wifi_init();

  if (!s_ack_sem) {
    s_ack_sem = xSemaphoreCreateBinary();
  }

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

  err = esp_now_register_send_cb(on_esp_now_send_cb);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Đăng ký callback gửi thất bại: %s", esp_err_to_name(err));
    return err;
  }

  esp_now_peer_info_t peer_info = {0};
  memcpy(peer_info.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
  peer_info.channel = 0;
  peer_info.ifidx = WIFI_IF_STA;
  peer_info.encrypt = false;

  err = esp_now_add_peer(&peer_info);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    ESP_LOGW(TAG, "Thêm peer ESP-NOW broadcast: %s", esp_err_to_name(err));
  }

  ESP_LOGI(TAG, "Khởi tạo ESP-NOW Receiver & Sender THÀNH CÔNG! Đang lắng nghe "
                "từ con Bể Nước...");
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

esp_err_t node_esp_send_raw(const uint8_t *data, size_t len) {
  if (!data || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  const uint8_t *target_mac = s_has_tank_mac ? s_tank_mac : s_broadcast_mac;
  return esp_now_send(target_mac, data, len);
}

bool node_esp_wait_ota_ack(uint32_t expected_chunk, uint32_t timeout_ms) {
  if (!s_ack_sem) {
    return false;
  }
  int64_t start_time = esp_timer_get_time() / 1000;
  while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
    if (xSemaphoreTake(s_ack_sem, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (s_last_acked_chunk >= expected_chunk) {
        return true;
      }
    }
  }
  return false;
}
