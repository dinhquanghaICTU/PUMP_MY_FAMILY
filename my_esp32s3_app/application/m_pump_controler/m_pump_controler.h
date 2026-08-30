// #ifndef __M_PUMP_CONTROLER_H__
// #define __M_PUMP_CONTROLER_H__

// #include "esp_err.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/queue.h"
// #include <stdbool.h>
// #include <stdint.h>

// typedef enum { MODE_PUMP_MANUAL = 0, MODE_PUMP_AUTO = 1 } m_mode_pump_t;

// typedef enum {
//   STATE_PUMP_IDLE = 0,
//   STATE_PUMP_RUNNING,
//   STATE_PUMP_ERROR_TIMEOUT,
//   STATE_PUMP_ERROR_DRY_RUN,
//   STATE_PUMP_ERROR_NODE_LOST
// } m_state_pump_t;

// typedef struct {
//   m_mode_pump_t mode;
//   m_state_pump_t state_current;
//   m_state_pump_t state_next;
//   bool is_pump_on;
//   bool child_lock;

//   float tank_height_cm;
//   float sensor_offset_cm;
//   int min_water_percent;
//   int max_water_percent;
//   uint32_t max_runtime_sec;

//   float current_distance_cm;
//   float current_percent;
//   float node_battery_volt;
//   int node_rssi;

//   uint32_t runtime_counter_sec;
//   uint32_t last_node_seen_sec;

//   QueueHandle_t event_queue;
// } m_controler_pump_t;

// esp_err_t m_pump_controler_init(void);
// const m_controler_pump_t *m_pump_controler_get_context(void);
// void m_pump_controler_set_mode(m_mode_pump_t mode);
// void m_pump_controler_set_state(m_state_pump_t state);
// void m_pump_controler_clear_error(void);

// #endif // __M_PUMP_CONTROLER_H__
