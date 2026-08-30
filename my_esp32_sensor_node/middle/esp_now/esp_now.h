#ifndef __ESP_NOW_MIDDLE_H__
#define __ESP_NOW_MIDDLE_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

// Định dạng gói tin truyền qua ESP-NOW (khớp 100% với Node Master Tủ Điện)
typedef struct __attribute__((packed)) {
  uint32_t packet_id;
  float distance_cm;
  float battery_volt;
} SensorData_t;

// Khởi tạo ESP-NOW với Channel Wi-Fi và thêm Peer Broadcast
esp_err_t esp_now_node_init(uint8_t wifi_channel);

// Hàm bắn gói tin cảm biến đi
esp_err_t esp_now_node_send(const SensorData_t *data);

#endif // __ESP_NOW_MIDDLE_H__