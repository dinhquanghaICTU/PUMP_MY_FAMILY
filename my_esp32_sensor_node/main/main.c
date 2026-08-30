#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "config.h"

static const char *TAG = "SENSOR_NODE_MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Starting Sensor Node (ESP32-U)...");

    ESP_ERROR_CHECK(nvs_flash_init());
}
