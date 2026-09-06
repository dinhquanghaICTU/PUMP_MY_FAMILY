#include "AJ-SR04M.h"
#include "config.h"
#include "esp_log.h"
#include "esp_now_node.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "nvs_flash.h"
#include "ota.h"
#include "state_machine.h"
#include "wifi.h"
#include "esp_sleep.h"
#include <stdio.h>

static const char *TAG = "SENSOR_NODE_MAIN";

void app_main(void) {
  /*
      init flash và lưu thông tin cấu hình
  */
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  // init ota
  ota_node_init();
  // init led
  led_init(STATUS_LED_PIN);
  // init sensor aj_sr04m
  aj_sr04m_init(TRIG_PIN, ECHO_PIN);
  // khởi tạo wifi và esp-now
  wifi_init_sta(ESP_NOW_WIFI_CHANNEL);
  esp_now_node_init(ESP_NOW_WIFI_CHANNEL);
  // khởi tạo state machine
  node_state_machine_init();

  // Chỉ chớp LED 3 lần khi khởi động nguội (Cold Boot), bỏ qua khi thức dậy từ Deep Sleep để tiết kiệm pin tối đa
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    led_blink(3, 100);
  }

  // khởi tạo các task của node
  xTaskCreate(node_state_machine_task, "node_fsm_task", 4096, NULL, 5, NULL);
}
