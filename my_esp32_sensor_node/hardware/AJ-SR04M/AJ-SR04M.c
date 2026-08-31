#include "AJ-SR04M.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

static const char *TAG = "HW_AJ_SR04M";
static int s_trig_pin = -1;
static int s_echo_pin = -1;

esp_err_t aj_sr04m_init(int trig_pin, int echo_pin) {
  s_trig_pin = trig_pin;
  s_echo_pin = echo_pin;

  gpio_config_t trig_conf = {
      .pin_bit_mask = (1ULL << s_trig_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&trig_conf);
  gpio_set_level((gpio_num_t)s_trig_pin, 0);

  gpio_config_t echo_conf = {
      .pin_bit_mask = (1ULL << s_echo_pin),
      .mode = GPIO_MODE_INPUT,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&echo_conf);

  ESP_LOGI(TAG, "Khởi tạo AJ-SR04M thành công (TRIG: GPIO %d, ECHO: GPIO %d)",
           trig_pin, echo_pin);
  return ESP_OK;
}

esp_err_t aj_sr04m_read_distance(float *out_distance_cm) {
  if (!out_distance_cm || s_trig_pin < 0 || s_echo_pin < 0) {
    return ESP_ERR_INVALID_ARG;
  }

  gpio_set_level((gpio_num_t)s_trig_pin, 0);
  esp_rom_delay_us(4);
  gpio_set_level((gpio_num_t)s_trig_pin, 1);
  esp_rom_delay_us(15);
  gpio_set_level((gpio_num_t)s_trig_pin, 0);

  int64_t start_wait = esp_timer_get_time();
  while (gpio_get_level((gpio_num_t)s_echo_pin) == 0) {
    if ((esp_timer_get_time() - start_wait) > 30000) {
      return ESP_ERR_TIMEOUT;
    }
  }

  int64_t echo_start = esp_timer_get_time();
  while (gpio_get_level((gpio_num_t)s_echo_pin) == 1) {
    if ((esp_timer_get_time() - echo_start) > 35000) {
      return ESP_ERR_TIMEOUT;
    }
  }
  int64_t echo_end = esp_timer_get_time();

  int64_t pulse_time_us = echo_end - echo_start;

  float dist = (float)pulse_time_us / 58.0f;

  if (dist < 15.0f || dist > 500.0f) {
    return ESP_ERR_INVALID_RESPONSE;
  }

  *out_distance_cm = dist;
  return ESP_OK;
}

static int compare_float(const void *a, const void *b) {
  float fa = *(const float *)a;
  float fb = *(const float *)b;
  return (fa > fb) - (fa < fb);
}

esp_err_t aj_sr04m_read_filtered_distance(float *out_distance_cm,
                                          int samples_count) {
  if (!out_distance_cm || samples_count <= 0) {
    return ESP_ERR_INVALID_ARG;
  }

  float *samples = (float *)malloc(sizeof(float) * samples_count);
  if (!samples) {
    return ESP_ERR_NO_MEM;
  }

  int valid_count = 0;
  for (int i = 0; i < samples_count; i++) {
    float temp_dist = 0.0f;
    if (aj_sr04m_read_distance(&temp_dist) == ESP_OK) {
      samples[valid_count++] = temp_dist;
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }

  if (valid_count == 0) {
    free(samples);
    return ESP_FAIL;
  }

  qsort(samples, valid_count, sizeof(float), compare_float);
  *out_distance_cm = samples[valid_count / 2];

  free(samples);
  return ESP_OK;
}
