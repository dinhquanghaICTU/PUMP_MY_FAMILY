#ifndef __OTA_H__
#define __OTA_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char url[256];
  char version[16];
  char target[32];
  char md5[36];
  int size;
} ota_config_t;

// Cấu trúc gói tin OTA bắn qua ESP-NOW cho Node Bể Nước
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
  uint8_t  type;                         // ota_packet_type_t
  uint32_t chunk_index;                  // 0, 1, 2... hoặc error_code
  uint16_t data_len;                     // Độ dài dữ liệu trong chunk (<= 192)
  uint8_t  data[OTA_CHUNK_MAX_SIZE];     // Dữ liệu binary / error message
} ota_esp_now_packet_t;

esp_err_t ota_parse_json(const char *json_str, ota_config_t *out_cfg);

esp_err_t ota_start(const ota_config_t *config);

#endif // __OTA_H__
