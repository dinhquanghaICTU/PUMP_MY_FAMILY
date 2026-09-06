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

  ESP_ERROR_CHECK(nvs_flash_init());

  // Xác nhận phân vùng OTA đang chạy
  const esp_partition_t *running_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
  bool is_new_ota_verified = false;

  if (running_part && esp_ota_get_state_partition(running_part, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      ESP_LOGI(TAG, "🚀 [OTA FIRST BOOT] Ứng dụng mới boot lần đầu, xác nhận hợp lệ...");
      esp_ota_mark_app_valid_cancel_rollback();
      is_new_ota_verified = true;
    }
  }

  // Chuyển fw_pending thành fw_ver chính thức NẾU app mới boot hợp lệ
  // Nếu đã bị Rollback về bản cũ (chạy phân vùng factory hoặc ota cũ): Báo lỗi và xóa fw_pending!
  nvs_handle_t nvs;
  if (nvs_open("system_cfg", NVS_READWRITE, &nvs) == ESP_OK) {
    char pending[32] = {0};
    size_t len = sizeof(pending);
    if (nvs_get_str(nvs, "fw_pending", pending, &len) == ESP_OK && len > 1) {
      if (is_new_ota_verified) {
        nvs_set_str(nvs, "fw_ver", pending);
        ESP_LOGI(TAG, "🎉 Đã xác nhận phiên bản firmware mới hợp lệ: [%s]", pending);
      } else {
        ESP_LOGE(TAG, "🚨 [ROLLBACK PHÁT HIỆN] Bản firmware [%s] bị crash hoặc lỗi khởi động, đã tự động Rollback về an toàn!", pending);
        nvs_set_str(nvs, "fw_rollback", pending);
      }
      nvs_erase_key(nvs, "fw_pending");
      nvs_commit(nvs);
    }
    nvs_close(nvs);
  }

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
