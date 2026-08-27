#include "led.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static led_strip_handle_t led_strip = NULL;

void led_init(void) {
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_PIN,
      .max_leds = 1,
      .led_model = LED_MODEL_WS2812,
      .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
      .flags =
          {
              .invert_out = false,
          },
  };

  led_strip_rmt_config_t rmt_config = {
      .clk_src = RMT_CLK_SRC_DEFAULT,
      .resolution_hz = 10 * 1000 * 1000,
      .flags =
          {
              .with_dma = false,
          },
  };

  ESP_ERROR_CHECK(
      led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  led_strip_clear(led_strip);
}

void turn_on_led(void) {
  if (led_strip) {
    led_strip_set_pixel(led_strip, 0, 0, 50, 0);
    led_strip_refresh(led_strip);
  }
}

void turn_off_led(void) {
  if (led_strip) {
    led_strip_clear(led_strip);
    led_strip_refresh(led_strip);
  }
}

void turn_blinking_red(void) {
  turn_on_led();
  vTaskDelay(pdMS_TO_TICKS(500));
  turn_off_led();
  vTaskDelay(pdMS_TO_TICKS(500));
}
