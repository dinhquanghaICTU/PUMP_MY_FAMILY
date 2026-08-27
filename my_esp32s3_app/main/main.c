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
#include <stdio.h>
#include <string.h>

#define NVS_NAMESPACE "wifi_store"

static const char *TAG = "MY_APP";

static void on_mqtt_data_received(const char *topic, int topic_len,
                                  const char *data, int data_len, void *arg) {
  ESP_LOGI(TAG, "Nhan lenh Topic: %.*s | Data: %.*s", topic_len, topic,
           data_len, data);
}

void app_main(void) {

  ESP_ERROR_CHECK(nvs_flash_init());
  led_init();

  char saved_ssid[33] = {0};
  char saved_pass[65] = {0};

  ESP_LOGW("APP", "Chua co Wi-Fi hoac ket noi that bai -> Bat BLE de cau hinh");
  ble_wifi_init("PUMP_DEVICE_CONFIG");
  // ble_wifi_register_callback(on_ble_wifi_config_received);
  // esp_err_t ret = nvs_flash_init();
  // if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
  //     ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
  //   ESP_ERROR_CHECK(nvs_flash_erase());
  //   ret = nvs_flash_init();
  // }
  // ESP_ERROR_CHECK(ret);

  // wifi_init();

  // app_wifi_config_t sta_cfg = {.ssid = WIFI_SSID,
  //                              .password = WIFI_PASS,
  //                              .max_retry = 0,
  //                              .retry_delay_ms = 2000};
  // wifi_connect_sta(&sta_cfg);
  // if (wifi_wait_for_connected(portMAX_DELAY)) {
  //   ESP_LOGI(TAG, "Wi-Fi Connected & Got IP!");

  //   app_mqtt_config_t mqtt_cfg = {.uri = MQTT_BROKER_URI,
  //                                 .username = MQTT_USERNAME,
  //                                 .password = MQTT_PASSWORD,
  //                                 .client_id = "esp32s3_pump_device"};
  //   app_mqtt_init(&mqtt_cfg);
  //   app_mqtt_register_data_cb(on_mqtt_data_received, NULL);
  //   app_mqtt_start();
  //   if (app_mqtt_wait_for_connected(portMAX_DELAY)) {

  //     app_mqtt_subscribe(TOPIC_PUMP_COMMAND, 1);
  //     app_mqtt_publish(TOPIC_PUMP_STATUS,
  //     "{\"pump\":0,\"status\":\"online\"}",
  //                      1, 0);
  //   }
  // }

  while (1) {
    turn_blinking_red();
  }
}
