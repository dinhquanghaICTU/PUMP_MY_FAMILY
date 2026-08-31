#include "ble.h"
#include "button.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "m_pump_controler.h"
#include "m_state_machine.h"
#include "mqtt.h"
#include "node_esp.h"
#include "nvs_flash.h"
#include "relay.h"
#include "wifi.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "MY_APP";

void app_main(void) {
  ESP_LOGI(TAG, "Starting Application...");

  esp_ota_mark_app_valid_cancel_rollback();

  ESP_ERROR_CHECK(nvs_flash_init());

  relay_init();
  button_init();
  led_init();
  node_esp_init();
  m_pump_controler_init();

  xTaskCreate(m_state_machine_task, "m_state_machine", 4096, NULL, 5, NULL);
  xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
  xTaskCreate(led_task, "led_task", 2048, NULL, 2, NULL);
}

/* =========================================================================
   ĐOẠN CODE TEST ROLLBACK CŨ (ĐỂ DÀNH THAM KHẢO)
========================================================================= */
/*
void test_rollback_demo(void) {
  ESP_LOGE("ROLLBACK", "==================================================");
  ESP_LOGE("ROLLBACK", "FIRMWARE TEST: ĐANG CHẠY TRÊN PHÂN VÙNG OTA MỚI!");
  ESP_LOGE("ROLLBACK", "CỐ TÌNH GỌI ROLLBACK SAU 3 GIÂY ĐỂ QUAY VỀ BẢN CŨ...");
  ESP_LOGE("ROLLBACK", "==================================================");

  for (int i = 3; i > 0; i--) {
    ESP_LOGW("ROLLBACK", "Kích hoạt Rollback sau: %d giây...", i);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  ESP_LOGE("ROLLBACK", "KÍCH HOẠT ROLLBACK VÀ REBOOT VỀ BẢN CŨ!");
  esp_ota_mark_app_invalid_rollback_and_reboot();
}
*/
