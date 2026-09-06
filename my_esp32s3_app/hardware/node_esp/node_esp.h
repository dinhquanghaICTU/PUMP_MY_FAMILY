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
  char fw_version[16];
} SensorData_t;

esp_err_t node_esp_init(void);

bool node_esp_get_latest_data(SensorData_t *out_data);

uint32_t node_esp_get_seconds_since_last_packet(void);

// Bắn gói tin thô qua ESP-NOW
esp_err_t node_esp_send_raw(const uint8_t *data, size_t len);

// Reset cờ ACK OTA trước khi gửi lệnh mới
void node_esp_reset_ota_ack(void);

// Chờ ACK từ con Bể Nước cho một chunk_index cụ thể
bool node_esp_wait_ota_ack(uint32_t expected_chunk, uint32_t timeout_ms);

typedef enum {
  TANK_OTA_RESP_NONE = 0,
  TANK_OTA_RESP_SUCCESS,
  TANK_OTA_RESP_FAIL
} tank_ota_response_t;

// Reset trạng thái kết thúc OTA của Bể Nước
void node_esp_reset_tank_ota_status(void);

// Chờ xác nhận thành công hoặc báo lỗi từ Bể Nước sau khi nạp xong
tank_ota_response_t node_esp_wait_tank_ota_finish(uint32_t timeout_ms, char *out_err_msg, size_t max_len);

#endif // __NODE_ESP_H__
