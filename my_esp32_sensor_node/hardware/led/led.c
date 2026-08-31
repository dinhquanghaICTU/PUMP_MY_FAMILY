#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "HW_LED";
static int s_led_pin = -1;
static bool s_led_state = false;

esp_err_t led_init(int pin) {
  s_led_pin = pin;
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << s_led_pin),
      .mode = GPIO_MODE_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  esp_err_t ret = gpio_config(&io_conf);
  led_off();
  ESP_LOGI(TAG, "LED trạng thái đã khởi tạo trên GPIO %d", pin);
  return ret;
}

void led_on(void) {
  if (s_led_pin >= 0) {
    gpio_set_level((gpio_num_t)s_led_pin, 1);
    s_led_state = true;
  }
}

void led_off(void) {
  if (s_led_pin >= 0) {
    gpio_set_level((gpio_num_t)s_led_pin, 0);
    s_led_state = false;
  }
}

void led_toggle(void) {
  if (s_led_state) {
    led_off();
  } else {
    led_on();
  }
}

void led_blink(int count, int delay_ms) {
  for (int i = 0; i < count; i++) {
    led_on();
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    led_off();
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}
