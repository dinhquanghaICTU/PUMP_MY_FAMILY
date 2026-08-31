#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  NODE_STATE_INIT = 0,
  NODE_STATE_IDLE,
  NODE_STATE_MEASURE,
  NODE_STATE_SEND_DATA,
  NODE_STATE_OTA_UPDATING,
  NODE_STATE_ERROR
} node_state_t;

typedef struct {
  node_state_t state_current;
  node_state_t state_next;

  uint32_t packet_counter;
  float current_distance_cm;
  float current_battery_volt;

  int64_t last_measure_time;
  uint32_t measure_interval_ms;

  bool is_sensor_ok;
  bool is_ota_running;
} node_state_machine_t;

void node_state_machine_init(void);
void node_state_machine_set_state(node_state_t state);
void node_state_machine_task(void *pvParam);

#endif // __STATE_MACHINE_H__