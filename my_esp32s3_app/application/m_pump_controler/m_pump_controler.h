#ifndef __M_PUMP_CONTROLER_H__
#define __M_PUMP_CONTROLER_H__

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  MODE_PUMP_MANUAL = 0,
  MODE_PUMP_AUTO   = 1
} m_mode_pump_t;

typedef enum {
  STATE_PUMP_IDLE = 0,
  STATE_PUMP_RUNNING,
  STATE_PUMP_ERROR_TIMEOUT,
  STATE_PUMP_ERROR_DRY_RUN,
  STATE_PUMP_ERROR_NODE_LOST
} m_state_pump_t;

typedef struct {
  m_mode_pump_t mode;
  m_state_pump_t state_current;
  m_state_pump_t state_next;
  bool is_pump_on;
  bool child_lock;

  // Cấu hình kích thước bồn nước (Mặc định: Bồn 120cm, Cảm biến đặt cách mặt nước đầy 25cm)
  float tank_height_cm;        // Chiều sâu hữu dụng của bể (cm)
  float sensor_offset_cm;      // Khoảng cách từ cảm biến tới mức nước đầy (cm - tránh vùng mù)
  int min_water_percent;       // Mức nước cạn tự động bật bơm (mặc định: 25%)
  int max_water_percent;       // Mức nước đầy tự động tắt bơm (mặc định: 95%)
  uint32_t max_runtime_sec;    // Thời gian bơm tối đa tự ngắt chống tràn/cháy bơm (mặc định: 45 phút = 2700s)

  // Dữ liệu đo đạc thời gian thực
  float current_distance_cm;
  float current_percent;
  float node_battery_volt;
  int node_rssi;

  uint32_t runtime_counter_sec;
  uint32_t last_node_seen_sec;
} m_controler_pump_t;

esp_err_t m_pump_controler_init(void);

const m_controler_pump_t *m_pump_controler_get_context(void);

void m_pump_controler_set_mode(m_mode_pump_t mode);

void m_pump_controler_set_pump(bool turn_on);

void m_pump_controler_toggle_pump(void);

void m_pump_controler_set_child_lock(bool enable);

void m_pump_controler_set_config(float tank_height_cm, float sensor_offset_cm,
                                 int min_pct, int max_pct, uint32_t max_runtime_sec);

void m_pump_controler_clear_error(void);

float m_pump_controler_calculate_percent(float distance_cm);

#endif // __M_PUMP_CONTROLER_H__
