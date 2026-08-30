#include "led.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "HARDWARE_LED";

static led_strip_handle_t led_strip = NULL;
static led_state_t s_current_led_state = LED_STATE_OFF;

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
  ESP_LOGI(TAG, "Khởi tạo LED WS2812 trên GPIO %d THÀNH CÔNG!", LED_PIN);
}

static void led_set_rgb(uint32_t red, uint32_t green, uint32_t blue) {
  if (led_strip) {
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);
  }
}

void led_set_state(led_state_t state) { s_current_led_state = state; }

void led_task(void *pvParam) {
  ESP_LOGI(TAG, "LED Task đã khởi động!");

  while (1) {
    switch (s_current_led_state) {

    case LED_STATE_OFF:
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      break;

    case LED_STATE_BLE_CONFIG:
      led_set_rgb(0, 0, 50);
      vTaskDelay(pdMS_TO_TICKS(500));
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(500));
      break;

    case LED_STATE_WIFI_CONNECTING:
      led_set_rgb(40, 40, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      break;

    case LED_STATE_WIFI_DISCONNECTED:
      led_set_rgb(50, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(500));
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(500));
      break;

    case LED_STATE_ONLINE_OK:
      led_set_rgb(0, 30, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      break;

    case LED_STATE_PUMP_RUNNING:
      led_set_rgb(0, 40, 40);
      vTaskDelay(pdMS_TO_TICKS(200));
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      break;

    case LED_STATE_OTA_UPDATING:
      led_set_rgb(50, 0, 50);
      vTaskDelay(pdMS_TO_TICKS(100));
      led_set_rgb(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(100));
      break;

    case LED_STATE_ERROR_TRIPPED:
      led_set_rgb(60, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(200));
      break;

    default:
      vTaskDelay(pdMS_TO_TICKS(100));
      break;
    }
  }
}
