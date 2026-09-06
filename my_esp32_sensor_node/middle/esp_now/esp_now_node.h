#ifndef __ESP_NOW_NODE_H__
#define __ESP_NOW_NODE_H__

#include "esp_err.h"
#include "ota.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint32_t packet_id;
  float distance_cm;
  float battery_volt;
  char fw_version[16];
} SensorData_t;

esp_err_t esp_now_node_init(uint8_t wifi_channel);
esp_err_t esp_now_node_send(const SensorData_t *data);

// Gửi phản hồi trạng thái OTA (Thành công / Thất bại) về cho Master Tủ Điện
esp_err_t esp_now_node_send_ota_response(uint8_t type, uint32_t code, const char *msg);

// Lấy Packet ID duy trì qua Deep Sleep (lưu trong RTC RAM)
uint32_t esp_now_node_get_next_packet_id(void);

// Chờ mở cửa sổ đón tin OTA từ Master Tủ Điện
bool esp_now_node_wait_for_ota_trigger(uint32_t wait_ms);

// Kiểm tra xem đã khóa được kênh và MAC của Master chưa
bool esp_now_node_is_master_locked(void);

// Xóa cờ chờ OTA khi kết thúc hoặc thất bại
void esp_now_node_reset_ota_trigger(void);

#endif // __ESP_NOW_NODE_H__
