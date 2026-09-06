#include "AJ-SR04M.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
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

  // Tạm dừng bộ điều phối FreeRTOS để tránh context switch sang Wi-Fi task làm sai lệch thời gian đo xung
  vTaskSuspendAll();

  gpio_set_level((gpio_num_t)s_trig_pin, 0);
  esp_rom_delay_us(4);
  gpio_set_level((gpio_num_t)s_trig_pin, 1);
  esp_rom_delay_us(15);
  gpio_set_level((gpio_num_t)s_trig_pin, 0);

  int64_t start_wait = esp_timer_get_time();
  while (gpio_get_level((gpio_num_t)s_echo_pin) == 0) {
    if ((esp_timer_get_time() - start_wait) > 15000) {
      xTaskResumeAll();
      return ESP_ERR_TIMEOUT;
    }
  }

  int64_t echo_start = esp_timer_get_time();
  while (gpio_get_level((gpio_num_t)s_echo_pin) == 1) {
    // 17500us tương đương ~300cm (téc nước gia đình tối đa 2.5m - 3m)
    if ((esp_timer_get_time() - echo_start) > 17500) {
      xTaskResumeAll();
      return ESP_ERR_TIMEOUT;
    }
  }
  int64_t echo_end = esp_timer_get_time();

  xTaskResumeAll();

  int64_t pulse_time_us = echo_end - echo_start;
  float dist = (float)pulse_time_us / 58.0f;

  // Dải đo hợp lý cho téc nước gia đình (18cm -> 280cm)
  if (dist < 18.0f || dist > 280.0f) {
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

static float s_ema_dist = -1.0f;
static float s_last_valid_dist = -1.0f;
static int s_consecutive_fail_count = 0;
static int s_glitch_confirm_count = 0;

esp_err_t aj_sr04m_read_filtered_distance(float *out_distance_cm,
                                          int samples_count) {
  if (!out_distance_cm || samples_count <= 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thu thập 7 mẫu để loại bỏ triệt để xung ngoại lai và dội thành téc
  int total_samples = samples_count < 7 ? 7 : samples_count;

  float *samples = (float *)malloc(sizeof(float) * total_samples);
  if (!samples) {
    return ESP_ERR_NO_MEM;
  }

  int valid_count = 0;
  for (int i = 0; i < total_samples; i++) {
    float temp_dist = 0.0f;
    if (aj_sr04m_read_distance(&temp_dist) == ESP_OK) {
      samples[valid_count++] = temp_dist;
    }
    // Nghỉ 25ms để triệt tiêu tiếng vang dội thành téc (acoustic reverberation)
    vTaskDelay(pdMS_TO_TICKS(25));
  }

  if (valid_count >= 3) {
    s_consecutive_fail_count = 0;
    // 1. Sắp xếp các mẫu theo thứ tự tăng dần
    qsort(samples, valid_count, sizeof(float), compare_float);

    // 2. Trimmed Mean: Loại bỏ ngoại lai min và max
    float sum = 0.0f;
    int count = 0;
    int start_idx = (valid_count >= 6) ? 2 : ((valid_count >= 4) ? 1 : 0);
    int end_idx = (valid_count >= 6) ? (valid_count - 2) : ((valid_count >= 4) ? (valid_count - 1) : valid_count);

    for (int i = start_idx; i < end_idx; i++) {
      sum += samples[i];
      count++;
    }
    float raw_dist = (count > 0) ? (sum / (float)count) : samples[valid_count / 2];
    free(samples);

    // 3. Chống nhảy số ảo (Glitch Rejector):
    // Mặt nước téc gia đình không thể thay đổi đột biến quá 25cm trong vài giây
    if (s_ema_dist > 0.0f) {
      float jump = fabsf(raw_dist - s_ema_dist);
      if (jump > 25.0f) {
        s_glitch_confirm_count++;
        // Nếu chỉ là xung nhiễu thoáng qua (dưới 3 lần): Giữ nguyên số đo ổn định
        if (s_glitch_confirm_count < 3) {
          *out_distance_cm = s_ema_dist;
          return ESP_OK;
        }
      } else {
        s_glitch_confirm_count = 0;
      }
    }

    // 4. Exponential Moving Average (EMA) - Làm mượt mặt nước chống gợn sóng
    if (s_ema_dist < 0.0f) {
      s_ema_dist = raw_dist;
    } else {
      s_ema_dist = 0.35f * raw_dist + 0.65f * s_ema_dist;
    }

    s_last_valid_dist = s_ema_dist;
    *out_distance_cm = s_ema_dist;
    return ESP_OK;
  }

  free(samples);
  s_consecutive_fail_count++;

  // Nếu bị hụt echo tạm thời (< 5 chu kỳ liên tiếp): Giữ nguyên giá trị cũ để không bị giật -1.0cm
  if (s_last_valid_dist > 0.0f && s_consecutive_fail_count <= 5) {
    *out_distance_cm = s_last_valid_dist;
    return ESP_OK;
  }

  return ESP_FAIL;
}
