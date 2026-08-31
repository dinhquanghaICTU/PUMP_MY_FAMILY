#ifndef __NODE_ESP_H__
#define __NODE_ESP_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((packed)) {
  uint32_t packet_id;
  float distance_cm;
  float battery_volt;
} SensorData_t;

esp_err_t node_esp_init(void);

bool node_esp_get_latest_data(SensorData_t *out_data);

uint32_t node_esp_get_seconds_since_last_packet(void);

// Bắn gói tin thô qua ESP-NOW
esp_err_t node_esp_send_raw(const uint8_t *data, size_t len);

// Chờ ACK từ con Bể Nước cho một chunk_index cụ thể
bool node_esp_wait_ota_ack(uint32_t expected_chunk, uint32_t timeout_ms);

#endif // __NODE_ESP_H__
