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
} SensorData_t;

esp_err_t esp_now_node_init(uint8_t wifi_channel);
esp_err_t esp_now_node_send(const SensorData_t *data);

// Gửi phản hồi trạng thái OTA (Thành công / Thất bại) về cho Master Tủ Điện
esp_err_t esp_now_node_send_ota_response(uint8_t type, uint32_t code, const char *msg);

#endif // __ESP_NOW_NODE_H__
