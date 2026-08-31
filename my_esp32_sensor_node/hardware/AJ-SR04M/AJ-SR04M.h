#ifndef __AJ_SR04M_H__
#define __AJ_SR04M_H__

#include "esp_err.h"
#include <stdint.h>

esp_err_t aj_sr04m_init(int trig_pin, int echo_pin);

esp_err_t aj_sr04m_read_distance(float *out_distance_cm);

esp_err_t aj_sr04m_read_filtered_distance(float *out_distance_cm,
                                          int samples_count);

#endif // __AJ_SR04M_H__
