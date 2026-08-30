#include "relay.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define RELAY_PUMP1_PIN GPIO_NUM_41
#define TAG "RELAY"

static bool s_relay_state = false;

void relay_init(void) {
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
  ESP_LOGI(TAG, " BẬT BƠM");
  gpio_set_level(RELAY_PUMP1_PIN, 1);
  s_relay_state = true;
}

void relay_turn_off(void) {
  ESP_LOGI(TAG, "TẮT BƠM");
  gpio_set_level(RELAY_PUMP1_PIN, 0);
  s_relay_state = false;
}

void relay_toggle(void) {
  if (s_relay_state) {
    relay_turn_off();
  } else {
    relay_turn_on();
  }
}

bool relay_is_on(void) { return s_relay_state; }