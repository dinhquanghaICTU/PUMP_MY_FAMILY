#include "esp_now_node.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "ota.h"
#include "state_machine.h"
#include <string.h>

static const char *TAG = "MIDDLE_ESP_NOW";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF};
static uint8_t s_master_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_has_master_mac = false;

static void on_data_sent(const uint8_t *mac_addr,
                         esp_now_send_status_t status) {
  // Send CB
}

// Callback tiếp nhận gói tin OTA từ Master Tủ Điện
static void on_data_recv(const esp_now_recv_info_t *recv_info,
                         const uint8_t *data, int len) {
  if (len < (int)sizeof(ota_packet_type_t) || !data) {
    return;
  }

  // Tự động ghi nhớ MAC của Master để phản hồi ACK
  if (recv_info && recv_info->src_addr) {
    if (!s_has_master_mac || memcmp(s_master_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN) != 0) {
      memcpy(s_master_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
      s_has_master_mac = true;

      esp_now_peer_info_t peer_info = {0};
      memcpy(peer_info.peer_addr, s_master_mac, ESP_NOW_ETH_ALEN);
      peer_info.channel = 1;
      peer_info.ifidx = WIFI_IF_STA;
      peer_info.encrypt = false;
      esp_now_add_peer(&peer_info);
    }
  }

  const ota_esp_now_packet_t *ota_pkt = (const ota_esp_now_packet_t *)data;

  switch (ota_pkt->type) {

  case OTA_PACKET_TYPE_START: {
    uint32_t total_size = 0;
    memcpy(&total_size, ota_pkt->data, sizeof(uint32_t));

    ESP_LOGW(TAG, "🚀 [OTA BỂ NƯỚC] Bắt đầu phiên nạp! Size: %lu bytes",
             (unsigned long)total_size);

    node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
    esp_err_t start_err = ota_node_start(total_size);
    if (start_err != ESP_OK) {
      esp_now_node_send_ota_response(OTA_PACKET_TYPE_FAIL, start_err, esp_err_to_name(start_err));
      return;
    }

    // Bắn ACK báo Master đã sẵn sàng
    ota_esp_now_packet_t ack_pkt = {
        .type = OTA_PACKET_TYPE_ACK,
        .chunk_index = 0,
        .data_len = 0,
    };
    const uint8_t *target = s_has_master_mac ? s_master_mac : s_broadcast_mac;
    esp_now_send(target, (const uint8_t *)&ack_pkt, sizeof(ota_esp_now_packet_t));
    break;
  }

  case OTA_PACKET_TYPE_DATA: {
    if (ota_node_is_updating()) {
      ota_node_write_chunk(ota_pkt->chunk_index, ota_pkt->data, ota_pkt->data_len);

      // Phản hồi ACK ngay lập tức cho chunk này
      ota_esp_now_packet_t ack_pkt = {
          .type = OTA_PACKET_TYPE_ACK,
          .chunk_index = ota_pkt->chunk_index,
          .data_len = 0,
      };
      const uint8_t *target = s_has_master_mac ? s_master_mac : s_broadcast_mac;
      esp_now_send(target, (const uint8_t *)&ack_pkt, sizeof(ota_esp_now_packet_t));

      if (ota_pkt->chunk_index % 50 == 0) {
        ESP_LOGI(TAG, "📦 [OTA BỂ] Đã nhận và ACK chunk #%lu (%d bytes)",
                 (unsigned long)ota_pkt->chunk_index, ota_pkt->data_len);
      }
    }
    break;
  }

  case OTA_PACKET_TYPE_END: {
    ESP_LOGW(TAG, "🏁 [OTA BỂ NƯỚC] Nhận lệnh KẾT THÚC -> Xác thực Flash & Reboot...");
    ota_node_finish();
    break;
  }

  default:
    break;
  }
}

esp_err_t esp_now_node_init(uint8_t wifi_channel) {
  esp_err_t ret = esp_now_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  esp_now_register_send_cb(on_data_sent);
  esp_now_register_recv_cb(on_data_recv);

  esp_now_peer_info_t peer_info = {0};
  memcpy(peer_info.peer_addr, s_broadcast_mac, ESP_NOW_ETH_ALEN);
  peer_info.channel = 0;
  peer_info.ifidx = WIFI_IF_STA;
  peer_info.encrypt = false;

  ret = esp_now_add_peer(&peer_info);
  if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
    ESP_LOGE(TAG, "Thêm peer ESP-NOW thất bại: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "Khởi tạo ESP-NOW Sender & OTA Receiver thành công (Tự động thích ứng mọi Channel Wi-Fi)");
  return ESP_OK;
}

esp_err_t esp_now_node_send(const SensorData_t *data) {
  if (!data) {
    return ESP_ERR_INVALID_ARG;
  }
  // Tự động quét và phát trên toàn bộ các kênh Wi-Fi (1 -> 13)
  // Giúp Tủ Điện ở bất kỳ Channel Wi-Fi nào cũng nhận được ngay lập tức!
  for (uint8_t ch = 1; ch <= 13; ch++) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    esp_now_send(s_broadcast_mac, (const uint8_t *)data, sizeof(SensorData_t));
  }
  return ESP_OK;
}

esp_err_t esp_now_node_send_ota_response(uint8_t type, uint32_t code, const char *msg) {
  ota_esp_now_packet_t resp_pkt = {
      .type = type,
      .chunk_index = code,
      .data_len = 0,
  };
  if (msg) {
    size_t slen = strlen(msg);
    if (slen > OTA_CHUNK_MAX_SIZE - 1) slen = OTA_CHUNK_MAX_SIZE - 1;
    resp_pkt.data_len = (uint16_t)slen;
    memcpy(resp_pkt.data, msg, slen);
    resp_pkt.data[slen] = '\0';
  }

  const uint8_t *target = s_has_master_mac ? s_master_mac : s_broadcast_mac;
  // Bắn 3 lần lặp lại để chắc chắn Master nhận được
  for (int i = 0; i < 3; i++) {
    esp_now_send(target, (const uint8_t *)&resp_pkt, sizeof(ota_esp_now_packet_t));
    vTaskDelay(pdMS_TO_TICKS(40));
  }
  return ESP_OK;
}
