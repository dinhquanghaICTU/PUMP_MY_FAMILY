#include "relay.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define RELAY_PUMP1_PIN GPIO_NUM_41
#define TAG "RELAY"

void relay_init() {
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << RELAY_PUMP1_PIN),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&io_conf);
  relay_turn_off();
}

void relay_turn_on(void) {
  ESP_LOGE(TAG, "turn_on pump");
  gpio_set_level(RELAY_PUMP1_PIN, 1);
}

void relay_turn_off(void) { gpio_set_level(RELAY_PUMP1_PIN, 0); }