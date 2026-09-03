#include "m_state_machine.h"
#include "ble.h"
#include "button.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "m_pump_controler.h"
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

  khi có call back từ mqtt về nó sẽ nhày vào đây để chuânr bị parer json

*/
static void on_mqtt_message_received(const char *topic, int topic_len,
                                     const char *data, int data_len,
                                     void *arg) {

  ESP_LOGI(TAG, "==> Nhan MQTT Topic: %.*s | Payload: %.*s", topic_len, topic,
           data_len, data);

  /*
   nếu gặp command từa ota xuống nó sẽ nhảy vào đây để handle


  */
  if (strncmp(topic, TOPIC_PUMP_OTA, topic_len) == 0) {
    char payload_str[512] = {0};
    if (data_len < (int)sizeof(payload_str)) {
      memcpy(payload_str, data, data_len);
    } else {
      memcpy(payload_str, data, sizeof(payload_str) - 1);
    }

    ota_config_t ota_cfg;

    /*
      nó sẽ nhatr vào đây để paser chũôi json này tách ra url firmware


[07:20:16] 📤 [BẮN LỆNH OTA] Topic: 'pump/family/ota'
{
  "action": "ota",
  "version": "1.0.0",
  "target": "esp32s3_cabinet",
  "url": "http://172.172.8.170:8080/firmware.bin",
  "md5": "d1dab7350981a9d59c4c639195750990",
  "size": 1219344,
  "filename": "main.bin"
}
    */
    if (ota_parse_json(payload_str, &ota_cfg) == ESP_OK) {
      ESP_LOGI(TAG, "Nhận lệnh OTA hợp lệ! Bắt đầu nâng cấp firmware...");
      m_state_machine_set_state(STATE_OTA);
      ota_start(&ota_cfg);
    }
  }

  /*
    Xử lý các lệnh điều khiển máy bơm từ MQTT: pump/family/command
  */
  if (strncmp(topic, TOPIC_PUMP_COMMAND, topic_len) == 0) {
    char cmd_str[256] = {0};
    if (data_len < (int)sizeof(cmd_str)) {
      memcpy(cmd_str, data, data_len);
    } else {
      memcpy(cmd_str, data, sizeof(cmd_str) - 1);
    }

    ESP_LOGI(TAG, "🎮 [LỆNH ĐIỀU KHIỂN MQTT] Payload: %s", cmd_str);

    // 1. Lệnh Bật / Tắt bơm
    if (strstr(cmd_str, "\"action\":\"on\"") || strstr(cmd_str, "\"pump\":1") || strstr(cmd_str, "\"pump\":\"on\"")) {
      m_pump_controler_set_pump(true);
    } else if (strstr(cmd_str, "\"action\":\"off\"") || strstr(cmd_str, "\"pump\":0") || strstr(cmd_str, "\"pump\":\"off\"")) {
      m_pump_controler_set_pump(false);
    } else if (strstr(cmd_str, "\"action\":\"toggle\"")) {
      m_pump_controler_toggle_pump();
    }

    // 2. Chuyển chế độ Auto / Manual
    if (strstr(cmd_str, "\"mode\":\"auto\"") || strstr(cmd_str, "\"mode\":1")) {
      m_pump_controler_set_mode(MODE_PUMP_AUTO);
    } else if (strstr(cmd_str, "\"mode\":\"manual\"") || strstr(cmd_str, "\"mode\":0")) {
      m_pump_controler_set_mode(MODE_PUMP_MANUAL);
    }

    // 3. Khóa trẻ em
    if (strstr(cmd_str, "\"child_lock\":1") || strstr(cmd_str, "\"child_lock\":true")) {
      m_pump_controler_set_child_lock(true);
    } else if (strstr(cmd_str, "\"child_lock\":0") || strstr(cmd_str, "\"child_lock\":false")) {
      m_pump_controler_set_child_lock(false);
    }

    // 4. Cài đặt ngưỡng và kích thước bể (Config)
    if (strstr(cmd_str, "tank_height") || strstr(cmd_str, "min_pct") || strstr(cmd_str, "offset")) {
      float tank_h = 0.0f, offset_cm = 0.0f;
      int min_pct = -1, max_pct = -1;
      
      char *p;
      if ((p = strstr(cmd_str, "\"tank_height\":"))) sscanf(p + 14, "%f", &tank_h);
      if ((p = strstr(cmd_str, "\"offset\":"))) sscanf(p + 9, "%f", &offset_cm);
      if ((p = strstr(cmd_str, "\"min_pct\":"))) sscanf(p + 10, "%d", &min_pct);
      if ((p = strstr(cmd_str, "\"max_pct\":"))) sscanf(p + 10, "%d", &max_pct);

      const m_controler_pump_t *ctx = m_pump_controler_get_context();
      if (tank_h <= 0.0f) tank_h = ctx->tank_height_cm;
      if (offset_cm <= 0.0f) offset_cm = ctx->sensor_offset_cm;
      if (min_pct < 0) min_pct = ctx->min_water_percent;
      if (max_pct < 0) max_pct = ctx->max_water_percent;

      m_pump_controler_set_config(tank_h, offset_cm, min_pct, max_pct, 2700);
    }

    // 5. Xóa cảnh báo lỗi
    if (strstr(cmd_str, "\"action\":\"clear_error\"")) {
      m_pump_controler_clear_error();
    }
  }
}

bool get_ssid_password(char *ssid_out, char *pass_out) {
  if (!ssid_out || !pass_out)
    return false;
  memset(ssid_out, 0, MAX_SSID_LEN);
  memset(pass_out, 0, MAX_PASS_LEN);
  static bool s_logged_empty = false;
  esp_err_t err =
      nvs_load_wifi_credentials(ssid_out, MAX_SSID_LEN, pass_out, MAX_PASS_LEN);
  if (err == ESP_OK && strlen(ssid_out) > 0 && strlen(pass_out) > 0) {
    ESP_LOGI(TAG, "Da tim thay Wi-Fi trong Flash NVS: SSID = [%s], passs %s ",
             ssid_out, pass_out);
    s_logged_empty = false;
    return true;
  }
  if (!s_logged_empty) {
    ESP_LOGW(TAG, "Chua co Wi-Fi duoc luu trong Flash NVS!");
    s_logged_empty = true;
  }

  return false;
}

void m_state_machine_task(void *arg) {
  while (1) {
    g_state_machine.state_current = g_state_machine.state_next;
    switch (g_state_machine.state_current) {
    case STATE_WIFI_CONFIG:
      led_set_state(LED_STATE_BLE_CONFIG);
      /*
        nếu chưa có ssid vs pass sẵn sẽ nhảy qua swwich
        STATE_WIFI_CONNECT
      */
      if (get_ssid_password(saved_ssid, saved_pass)) {
        m_state_machine_set_state(STATE_WIFI_CONNECT);
        break;
      }
      /*
        nếu chưa có ssid vs pass sẽ nhảy vào hàm nay gọi ble để config wwifi

      */
      if (!g_state_machine.ble_config_wifi) {
        ble_wifi_init("PUMP_DEVICE_CONFIG");
        g_state_machine.ble_config_wifi =
            true; // set cờ wifi bật lên true để lần sau vào loop không init lại
        break;
      }
      /*
        nếu có wifi sẵn rồi thì sẽ nhảy vào switch
        STATE_WIFI_CONNECT
      */
      else if (connect_wifi) {
        m_state_machine_set_state(STATE_WIFI_CONNECT);
        break;
      }
      break;
    case STATE_WIFI_CONNECT: {
      /*
        nếu lúc đầu lấy ssid vs pass từ ble nó sẽ nhảy vào đây để tắt ble đi
      */
      if (g_state_machine.ble_config_wifi) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ble_wifi_deinit();
        ble_wifi_get_credentials(saved_ssid, saved_pass);
        g_state_machine.ble_config_wifi = false;
      }
      /*
        chỗ này nó lấy ssid vs pass wifi ở bên dưới flash
      */
      if (strlen(saved_ssid) == 0) {
        get_ssid_password(saved_ssid, saved_pass);
      }
      /*

        tạo phiên kết nối wwifi
      */
      ESP_LOGI(TAG, "Ket noi Wi-Fi voi SSID: [%s]", saved_ssid);
      wifi_init();

      app_wifi_config_t sta_cfg = {.max_retry = 3, .retry_delay_ms = 2000};
      strncpy(sta_cfg.ssid, saved_ssid, sizeof(sta_cfg.ssid) - 1);
      strncpy(sta_cfg.password, saved_pass, sizeof(sta_cfg.password) - 1);
      ESP_LOGE(TAG, "check debug ssid: [%s] , pass [%s]", sta_cfg.ssid,
               sta_cfg.password);
      wifi_connect_sta(&sta_cfg);
      /*

        đoạn này nó chờ xem kết nối có thành công không
      */
      if (wifi_wait_for_connected(pdMS_TO_TICKS(15000))) {

        ESP_LOGI(TAG,
                 "connect wifi successfully  and save ssid and pass to flash");
        /*

         nếu thành công nó lưu ssid vs pass vào flash để mục đích lần sau
         connect lại
         */
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

        /*
          nếu kết nối wwifi không thành công nó sẽ nhảy vào đây tăng retry count
          nếu qúa 3 lần nó sẽ nhảy ra swich STATE_WIFI_CONFIG để vào lại

        */

        ESP_LOGE(TAG, "Wi-Fi Connect Failed!");
        g_state_machine.retry_count++;
        m_state_machine_set_state(STATE_WIFI_CONNECT_FAILSE);
      }
      break;
    }
    /*
      ở case này nõ sẽ lấy ra ip để chuẩn bị lên mqtt


    */
    case STATE_WIFI_GOT_IP: {
      ESP_LOGI(TAG, "wifi ok");
      led_set_state(LED_STATE_ONLINE_OK);
      m_state_machine_set_state(STATE_MQTT_CONNECTING);
      break;
    }

    /*

      khi có ip address nó sẽ vào đây để config mqtt như là url broker ,
      username and passs

    */
    case STATE_MQTT_CONNECTING: {
      ESP_LOGI(TAG, "start connect to mqtt ");

      app_mqtt_config_t mqtt_cfg = {.uri = MQTT_BROKER_URI,
                                    .username = MQTT_USERNAME,
                                    .password = MQTT_PASSWORD,
                                    .client_id = "esp32s3_pump_family"};

      app_mqtt_init(&mqtt_cfg);
      app_mqtt_register_data_cb(on_mqtt_message_received, NULL);
      // khởi tạo và bắt đầu chạy mqtt
      app_mqtt_start();

      // chỗ này nó sẽ đợi connect thành công nhảy qua case STATE_MQTT_CONNECTED
      if (app_mqtt_wait_for_connected(pdMS_TO_TICKS(10000))) {
        ESP_LOGI(TAG, "MQTT Connected thanh cong!");
        m_state_machine_set_state(STATE_MQTT_CONNECTED);
      } else {
        ESP_LOGE(TAG, "Ket noi MQTT That bai hoac Timeout!");

        if (!wifi_is_connected()) {
          ESP_LOGE(TAG, "wifi disconnect!");
          m_state_machine_set_state(STATE_WIFI_DISCONNECT);
        } else {
          ESP_LOGE(TAG, "wifi connected but mqtt disconnect!");
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
      static int s_mqtt_disc_count = 0;
      static int64_t s_last_check_time = 0;
      int64_t now = esp_timer_get_time() / 1000; // ms

      // Kiểm tra trạng thái mạng mỗi 2 giây
      if (now - s_last_check_time >= 2000) {
        s_last_check_time = now;

        if (!wifi_is_connected()) {
          ESP_LOGW(TAG,
                   " Wi-Fi bị ngắt -> Tự động quay về STATE_WIFI_CONNECT!");
          s_mqtt_disc_count = 0;
          m_state_machine_set_state(STATE_WIFI_CONNECT);
        } else if (!app_mqtt_is_connected()) {
          s_mqtt_disc_count++;
          ESP_LOGW(TAG, " MQTT đang mất kết nối (lần %d/5)...",
                   s_mqtt_disc_count);
          if (s_mqtt_disc_count >= 5) {
            ESP_LOGE(TAG,
                     " MQTT mất kết nối 5 lần -> Tự động khởi động lại Wi-Fi!");
            s_mqtt_disc_count = 0;
            m_state_machine_set_state(STATE_WIFI_CONNECT);
          }
        } else {
          s_mqtt_disc_count = 0;
        }
      }
      break;
    }

    case STATE_WIFI_DISCONNECT: {
      ESP_LOGW(TAG, "Đã ngắt Wi-Fi -> Kết nối lại!");
      vTaskDelay(pdMS_TO_TICKS(1000));
      m_state_machine_set_state(STATE_WIFI_CONNECT);
      break;
    }

    case STATE_OTA: {
      // Đang trong tiến trình nạp OTA, tạm dừng các tác vụ khác
      button_task_stop();
      break;
    }

    case STATE_WIFI_CONNECT_FAILSE:
      led_set_state(LED_STATE_WIFI_DISCONNECTED);
      ESP_LOGW(TAG, "Kết nối Wi-Fi thất bại -> Chờ 2 giây để thử lại...");
      vTaskDelay(pdMS_TO_TICKS(2000));
      m_state_machine_set_state(STATE_WIFI_CONFIG);
      break;

    default:
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

extern bool connect_wifi;
void m_state_machine_reset_wifi(void) {
  ESP_LOGW(TAG, "🚨 Xóa toàn bộ cấu hình Wi-Fi -> Chuyển sang BLE Config!");
  nvs_erase_wifi_credentials();
  wifi_disconnect();
  m_pump_controler_set_mode(MODE_PUMP_MANUAL);
  m_pump_controler_set_child_lock(false);
  memset(saved_ssid, 0, sizeof(saved_ssid));
  memset(saved_pass, 0, sizeof(saved_pass));
  connect_wifi = false;
  g_state_machine.ble_config_wifi = false;
  m_state_machine_set_state(STATE_WIFI_CONFIG);
}
