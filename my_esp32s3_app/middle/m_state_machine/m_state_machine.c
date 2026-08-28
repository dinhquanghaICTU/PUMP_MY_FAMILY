#include "m_state_machine.h"
#include "ble.h"
#include "config.h"
#include "esp_log.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt.h"
#include "ota.h"
#include "wifi.h"
#include <string.h>

static const char *TAG = "STATE_MACHINE";

/*

    set env public
*/
char saved_ssid[MAX_SSID_LEN] = {0};
char saved_pass[MAX_PASS_LEN] = {0};

m_state_machine_t g_state_machine = {.state_current = STATE_WIFI_CONFIG,
                                     .state_next = STATE_WIFI_CONFIG};

void m_state_machine_init(void) {
  g_state_machine.state_current = STATE_WIFI_CONFIG;
  g_state_machine.state_next = STATE_WIFI_CONFIG;
  g_state_machine.ble_config_wifi = false;
  g_state_machine.retry_count = 0;
}

void m_state_machine_set_state(state_t state) {
  g_state_machine.state_next = state;
}

/*
    funcion handle callback received message



*/
static void on_mqtt_message_received(const char *topic, int topic_len,
                                     const char *data, int data_len,
                                     void *arg) {

  ESP_LOGI(TAG, "==> Nhan MQTT Topic: %.*s | Payload: %.*s", topic_len, topic,
           data_len, data);

  if (strncmp(topic, TOPIC_PUMP_OTA, topic_len) == 0) {
    char payload_str[512] = {0};
    if (data_len < (int)sizeof(payload_str)) {
      memcpy(payload_str, data, data_len);
    } else {
      memcpy(payload_str, data, sizeof(payload_str) - 1);
    }

    ota_config_t ota_cfg;
    if (ota_parse_json(payload_str, &ota_cfg) == ESP_OK) {
      ESP_LOGI(TAG, "Nhận lệnh OTA hợp lệ! Bắt đầu nâng cấp firmware...");
      m_state_machine_set_state(STATE_OTA);
      ota_start(&ota_cfg);
    }
  }
}

bool get_ssid_password(char *ssid_out, char *pass_out) {
  if (!ssid_out || !pass_out)
    return false;
  memset(ssid_out, 0, MAX_SSID_LEN);
  memset(pass_out, 0, MAX_PASS_LEN);
  esp_err_t err =
      nvs_load_wifi_credentials(ssid_out, MAX_SSID_LEN, pass_out, MAX_PASS_LEN);
  if (err == ESP_OK && strlen(ssid_out) > 0 && strlen(pass_out) > 0) {
    ESP_LOGI(TAG, "Da tim thay Wi-Fi trong Flash NVS: SSID = [%s], passs %s ",
             ssid_out, pass_out);
    return true;
  }
  ESP_LOGW(TAG, "Chua co Wi-Fi duoc luu trong Flash NVS!");
  return false;
}

void m_state_machine_task(void *arg) {
  while (1) {
    g_state_machine.state_current = g_state_machine.state_next;
    switch (g_state_machine.state_current) {
    case STATE_WIFI_CONFIG:

      if (get_ssid_password(saved_ssid, saved_pass)) {
        m_state_machine_set_state(STATE_WIFI_CONNECT);
        break;
      }
      if (!g_state_machine.ble_config_wifi) {
        ble_wifi_init("PUMP_DEVICE_CONFIG");
        g_state_machine.ble_config_wifi = true;
        break;
      }

      else if (connect_wifi) {
        m_state_machine_set_state(STATE_WIFI_CONNECT);
        break;
      }
      break;
    case STATE_WIFI_CONNECT: {
      if (g_state_machine.ble_config_wifi) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ble_wifi_deinit();
        ble_wifi_get_credentials(saved_ssid, saved_pass);
        g_state_machine.ble_config_wifi = false;
      }
      if (strlen(saved_ssid) == 0) {
        get_ssid_password(saved_ssid, saved_pass);
      }

      ESP_LOGI(TAG, "Ket noi Wi-Fi voi SSID: [%s]", saved_ssid);
      wifi_init();

      app_wifi_config_t sta_cfg = {.max_retry = 3, .retry_delay_ms = 2000};
      strncpy(sta_cfg.ssid, saved_ssid, sizeof(sta_cfg.ssid) - 1);
      strncpy(sta_cfg.password, saved_pass, sizeof(sta_cfg.password) - 1);
      ESP_LOGE(TAG, "check debug ssid: [%s] , pass [%s]", sta_cfg.ssid,
               sta_cfg.password);
      wifi_connect_sta(&sta_cfg);

      if (wifi_wait_for_connected(pdMS_TO_TICKS(15000))) {

        ESP_LOGI(TAG,
                 "connect wifi successfully  and save ssid and pass to flash");
        if (strlen(saved_ssid) > 0) {
          esp_err_t err = nvs_save_wifi_credentials(saved_ssid, saved_pass);
          if (err == ESP_OK) {
            ESP_LOGI(TAG, "Luu SSID va PASS vao Flash NVS thanh cong!");
          } else {
            ESP_LOGE(TAG, "Luu Flash NVS that bai: %d", err);
          }
        }
        g_state_machine.retry_count = 0;
        m_state_machine_set_state(STATE_WIFI_GOT_IP);
      } else {
        ESP_LOGE(TAG, "Wi-Fi Connect Failed!");
        g_state_machine.retry_count++;
        m_state_machine_set_state(STATE_WIFI_CONNECT_FAILSE);
      }
      break;
    }

    case STATE_WIFI_GOT_IP: {
      ESP_LOGI(TAG, "wifi ok");
      m_state_machine_set_state(STATE_MQTT_CONNECTING);
      break;
    }
    case STATE_MQTT_CONNECTING: {
      ESP_LOGI(TAG, "start connect to mqtt ");

      app_mqtt_config_t mqtt_cfg = {.uri = MQTT_BROKER_URI,
                                    .username = MQTT_USERNAME,
                                    .password = MQTT_PASSWORD,
                                    .client_id = "esp32s3_pump_family"};

      app_mqtt_init(&mqtt_cfg);
      app_mqtt_register_data_cb(on_mqtt_message_received, NULL);

      app_mqtt_start();

      if (app_mqtt_wait_for_connected(pdMS_TO_TICKS(10000))) {
        ESP_LOGI(TAG, "MQTT Connected thanh cong!");
        m_state_machine_set_state(STATE_MQTT_CONNECTED);
      } else {
        ESP_LOGE(TAG, "Ket noi MQTT That bai hoac Timeout!");

        if (!wifi_is_connected()) {
          m_state_machine_set_state(STATE_WIFI_DISCONNECT);
        } else {
          vTaskDelay(pdMS_TO_TICKS(2000));
          m_state_machine_set_state(STATE_MQTT_CONNECTING);
        }
      }
      break;
    }
    case STATE_MQTT_CONNECTED: {
      ESP_LOGI(TAG, "MQTT connected successfully!");

      app_mqtt_subscribe(TOPIC_PUMP_COMMAND, 1);
      app_mqtt_subscribe(TOPIC_PUMP_OTA, 1);
      m_state_machine_set_state(STATE_IDLE);
      break;
    }

    case STATE_IDLE: {
      // Trạng thái nghỉ - Hệ thống chạy bình thường
      break;
    }

    case STATE_OTA: {
      // Đang trong tiến trình nạp OTA, tạm dừng các tác vụ khác
      break;
    }

    case STATE_WIFI_CONNECT_FAILSE:
      if (g_state_machine.retry_count < MAX_RETRY_COUNT) {
        m_state_machine_set_state(STATE_WIFI_CONFIG);
        ESP_LOGI(TAG, "retry connect wifi count: %d",
                 g_state_machine.retry_count);
      }
      break;

    default:
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
