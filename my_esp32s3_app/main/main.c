#include "ble.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "mqtt.h"
#include "nvs_flash.h"
#include "wifi.h"

#include "flash.h"
#include "m_state_machine.h"
#include <stdio.h>
#include <string.h>

#define NVS_NAMESPACE "wifi_store"

static const char *TAG = "MY_APP";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Application...");
  ESP_ERROR_CHECK(nvs_flash_init());
  led_init();
  xTaskCreate(m_state_machine_task, "m_state_machine", 4096, NULL, 5, NULL);
  // xTaskCreate(m_state_machine_task, "m_state_machine", 4096, NULL, 5, NULL);

  // while (1) {
  //   turn_blinking_red();
  // }
}
