#ifndef __NODE_ESP_H__
#define __NODE_ESP_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint32_t packet_id;
  float distance_cm;
  float battery_volt;
} SensorData_t;

esp_err_t node_esp_init(void);

bool node_esp_get_latest_data(SensorData_t *out_data);

uint32_t node_esp_get_seconds_since_last_packet(void);

#endif
