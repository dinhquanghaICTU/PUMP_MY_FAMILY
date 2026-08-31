#ifndef __OTA_NODE_H__
#define __OTA_NODE_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  OTA_PACKET_TYPE_START   = 0x01,
  OTA_PACKET_TYPE_DATA    = 0x02,
  OTA_PACKET_TYPE_END     = 0x03,
  OTA_PACKET_TYPE_ACK     = 0x04,
  OTA_PACKET_TYPE_FAIL    = 0x05,
  OTA_PACKET_TYPE_SUCCESS = 0x06
} ota_packet_type_t;

#define OTA_CHUNK_MAX_SIZE 192

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint32_t chunk_index; // Hoặc mã lỗi error_code khi type là FAIL
  uint16_t data_len;
  uint8_t  data[OTA_CHUNK_MAX_SIZE];
} ota_esp_now_packet_t;

esp_err_t ota_node_init(void);

esp_err_t ota_node_start(size_t total_size);

esp_err_t ota_node_write_chunk(uint32_t chunk_index, const uint8_t *data,
                               size_t length);

esp_err_t ota_node_finish(void);

void ota_node_abort(void);

void ota_node_rollback_and_reboot(void);

bool ota_node_is_updating(void);

#endif // __OTA_NODE_H__