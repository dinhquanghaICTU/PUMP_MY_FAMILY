#include "esp_now_node.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "ota.h"
#include "state_machine.h"
#include <string.h>

static const char *TAG = "MIDDLE_ESP_NOW";
static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                    0xFF, 0xFF, 0xFF};
static uint8_t s_master_mac[ESP_NOW_ETH_ALEN] = {0};
static bool s_has_master_mac = false;
static bool s_master_locked = false;

// Dữ liệu duy trì trong RTC Fast Memory - không bị mất khi Deep Sleep
RTC_DATA_ATTR static uint8_t s_rtc_master_mac[ESP_NOW_ETH_ALEN] = {0};
RTC_DATA_ATTR static bool s_rtc_has_master_mac = false;
RTC_DATA_ATTR static bool s_rtc_master_locked = false;
RTC_DATA_ATTR static uint8_t s_rtc_channel = 1;
RTC_DATA_ATTR static uint32_t s_rtc_packet_id = 0;
RTC_DATA_ATTR static int s_rtc_consecutive_tx_fails = 0;

static uint8_t s_listen_channel = 1;
static volatile bool s_ota_trigger_received = false;

static void on_data_sent(const uint8_t *mac_addr,
                         esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    s_rtc_consecutive_tx_fails = 0;
  } else {
    s_rtc_consecutive_tx_fails++;
    if (s_rtc_consecutive_tx_fails >= 5) {
      if (s_master_locked) {
        ESP_LOGW(TAG, "Mất liên lạc với Master (5 lần TX fail), mở lại quét 13 kênh để tìm lại Master...");
        s_master_locked = false;
        s_rtc_master_locked = false;
      }
    }
  }
}

static volatile bool s_ota_finishing = false;

static void ota_finish_worker_task(void *param) {
  ota_node_finish();
  s_ota_finishing = false;
  vTaskDelete(NULL);
}

// Callback tiếp nhận gói tin từ Master Tủ Điện
static void on_data_recv(const esp_now_recv_info_t *recv_info,
                         const uint8_t *data, int len) {
  if (len < (int)sizeof(ota_packet_type_t) || !data) {
    return;
  }

  // Tự động học MAC và Channel phát sóng thực tế của Master Tủ Điện
  if (recv_info) {
    if (recv_info->rx_ctrl && recv_info->rx_ctrl->channel >= 1 && recv_info->rx_ctrl->channel <= 13) {
      s_listen_channel = recv_info->rx_ctrl->channel;
    }

    if (recv_info->src_addr) {
      if (!s_has_master_mac || memcmp(s_master_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN) != 0) {
        memcpy(s_master_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
        s_has_master_mac = true;
        memcpy(s_rtc_master_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
        s_rtc_has_master_mac = true;
        s_rtc_channel = s_listen_channel;

        if (esp_now_is_peer_exist(s_master_mac)) {
          esp_now_del_peer(s_master_mac);
        }
        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, s_master_mac, ESP_NOW_ETH_ALEN);
        peer_info.channel = 0; // 0 = dùng kênh hiện tại của interface
        peer_info.ifidx = WIFI_IF_STA;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);

        ESP_LOGI(TAG, "🎯 Khóa MAC Master Tủ Điện: %02x:%02x:%02x:%02x:%02x:%02x (Kênh %d)",
                 s_master_mac[0], s_master_mac[1], s_master_mac[2],
                 s_master_mac[3], s_master_mac[4], s_master_mac[5], s_listen_channel);
      }
      s_master_locked = true;
      s_rtc_master_locked = true;
      s_rtc_consecutive_tx_fails = 0;
    }
  }

  const ota_esp_now_packet_t *ota_pkt = (const ota_esp_now_packet_t *)data;

  switch (ota_pkt->type) {

  case OTA_PACKET_TYPE_ACK: {
    // Nhận ACK từ Master: Đã nhận dữ liệu thành công -> Giữ vững khóa kênh
    s_master_locked = true;
    s_rtc_master_locked = true;
    s_rtc_consecutive_tx_fails = 0;
    break;
  }

  case OTA_PACKET_TYPE_START: {
    uint32_t total_size = 0;
    memcpy(&total_size, ota_pkt->data, sizeof(uint32_t));

    s_ota_trigger_received = true;
    s_ota_finishing = false;
    ESP_LOGW(TAG, "🚀 [OTA BỂ NƯỚC] Bắt đầu phiên nạp! Size: %lu bytes (Kênh: %d)",
             (unsigned long)total_size, s_listen_channel);

    // Dành toàn quyền 100% cho OTA: Tắt toàn bộ tiết kiệm điện, khóa cứng radio vào kênh Master
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_listen_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    node_state_machine_set_state(NODE_STATE_OTA_UPDATING);
    esp_err_t start_err = ota_node_start(total_size);
    if (start_err != ESP_OK) {
      esp_now_node_send_ota_response(OTA_PACKET_TYPE_FAIL, start_err, esp_err_to_name(start_err));
      return;
    }

    // Bắn ACK báo Master đã sẵn sàng (phát 3 lần để tránh rớt gói)
    ota_esp_now_packet_t ack_pkt = {
        .type = OTA_PACKET_TYPE_ACK,
        .chunk_index = 0,
        .data_len = 0,
    };
    const uint8_t *target = s_has_master_mac ? s_master_mac : s_broadcast_mac;
    for (int i = 0; i < 3; i++) {
      esp_now_send(target, (const uint8_t *)&ack_pkt, sizeof(ota_esp_now_packet_t));
      vTaskDelay(pdMS_TO_TICKS(15));
    }
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
    ESP_LOGW(TAG, "🏁 [OTA BỂ NƯỚC] Nhận lệnh KẾT THÚC -> Tạo Task xác thực Flash & Reboot...");
    if (ota_node_is_updating() && !s_ota_finishing) {
      s_ota_finishing = true;
      BaseType_t ret = xTaskCreate(ota_finish_worker_task, "ota_fin_tsk", 4096, NULL, 5, NULL);
      if (ret != pdPASS) {
        ESP_LOGE(TAG, "Không thể tạo ota_fin_tsk! Fallback gọi trực tiếp...");
        ota_node_finish();
        s_ota_finishing = false;
      }
    }
    break;
  }

  default:
    break;
  }
}

esp_err_t esp_now_node_init(uint8_t wifi_channel) {
  s_ota_trigger_received = false;

  // Khôi phục Master MAC và Kênh Wi-Fi từ RTC RAM nếu thức dậy từ Deep Sleep
  if (s_rtc_master_locked && s_rtc_has_master_mac && s_rtc_channel >= 1 && s_rtc_channel <= 13) {
    memcpy(s_master_mac, s_rtc_master_mac, ESP_NOW_ETH_ALEN);
    s_has_master_mac = true;
    s_master_locked = true;
    s_listen_channel = s_rtc_channel;
    ESP_LOGI(TAG, "⚡ [RTC RESTORE] Khôi phục Master MAC: %02x:%02x:%02x:%02x:%02x:%02x (Kênh %d)",
             s_master_mac[0], s_master_mac[1], s_master_mac[2],
             s_master_mac[3], s_master_mac[4], s_master_mac[5], s_listen_channel);
  } else if (wifi_channel >= 1 && wifi_channel <= 13) {
    s_listen_channel = wifi_channel;
  }

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

  // Nếu đã khôi phục Master từ RTC RAM, thêm peer Master ngay lập tức để gửi Unicast không cần quét
  if (s_has_master_mac) {
    esp_now_peer_info_t master_peer = {0};
    memcpy(master_peer.peer_addr, s_master_mac, ESP_NOW_ETH_ALEN);
    master_peer.channel = 0;
    master_peer.ifidx = WIFI_IF_STA;
    master_peer.encrypt = false;
    esp_now_add_peer(&master_peer);
  }

  ESP_LOGI(TAG, "Khởi tạo ESP-NOW Sender & OTA Receiver thành công (Kênh mặc định %d)", s_listen_channel);
  return ESP_OK;
}

esp_err_t esp_now_node_send(const SensorData_t *data) {
  if (!data) {
    return ESP_ERR_INVALID_ARG;
  }
  // Nếu đang cập nhật OTA thì tuyệt đối không quét kênh làm gián đoạn việc nhận firmware!
  if (ota_node_is_updating()) {
    return ESP_OK;
  }

  // 1. Chế độ Siêu Tiết Kiệm Pin: Nếu đã nhận diện và khóa được Master trên kênh cụ thể
  // Chỉ phát 1 gói tin Unicast duy nhất thẳng tới Master -> Tiết kiệm 92% năng lượng RF
  if (s_master_locked && s_has_master_mac) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(s_listen_channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    return esp_now_send(s_master_mac, (const uint8_t *)data, sizeof(SensorData_t));
  }

  // 2. Chế độ Tìm Kiếm / Khởi tạo: Quét và phát trên toàn bộ các kênh Wi-Fi (1 -> 13)
  // Giúp Tủ Điện ở bất kỳ Channel Wi-Fi nào cũng nhận được ngay lập tức!
  for (uint8_t ch = 1; ch <= 13; ch++) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    esp_now_send(s_broadcast_mac, (const uint8_t *)data, sizeof(SensorData_t));
  }

  // QUAN TRỌNG: Trả lại kênh Wi-Fi về s_listen_channel để luôn nhận được gói tin từ Master!
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_listen_channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

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

uint32_t esp_now_node_get_next_packet_id(void) {
  s_rtc_packet_id++;
  return s_rtc_packet_id;
}

bool esp_now_node_is_master_locked(void) {
  return (s_master_locked && s_has_master_mac);
}

bool esp_now_node_wait_for_ota_trigger(uint32_t wait_ms) {
  if (s_ota_trigger_received || ota_node_is_updating()) {
    return true;
  }
  int64_t start_ms = esp_timer_get_time() / 1000;
  while ((esp_timer_get_time() / 1000 - start_ms) < wait_ms) {
    if (s_ota_trigger_received || ota_node_is_updating()) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return (s_ota_trigger_received || ota_node_is_updating());
}
