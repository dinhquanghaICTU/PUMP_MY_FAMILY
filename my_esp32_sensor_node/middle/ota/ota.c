#include "ota.h"
#include "esp_app_format.h"
#include "esp_log.h"
#include "esp_now_node.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state_machine.h"

static const char *TAG = "MIDDLE_OTA_NODE";

static const esp_partition_t *s_update_partition = NULL;
static esp_ota_handle_t s_ota_handle = 0;
static bool s_is_updating = false;
static size_t s_total_bytes_written = 0;
static uint32_t s_last_chunk_index = 0;
static uint32_t s_expected_chunk_index = 0;

esp_err_t ota_node_init(void) {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (!running) {
    ESP_LOGW(TAG, "Không tìm thấy running partition!");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Đang chạy trên phân vùng: %s (Offset: 0x%08lx, Size: 0x%08lx)",
           running->label, (unsigned long)running->address,
           (unsigned long)running->size);

  if (running->type == ESP_PARTITION_TYPE_APP &&
      running->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN &&
      running->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX) {
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
      if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "Phát hiện Firmware mới cần xác minh -> Đánh dấu HỢP LỆ & Hủy Rollback!");
        esp_ota_mark_app_valid_cancel_rollback();
      }
    }
  }

  return ESP_OK;
}

esp_err_t ota_node_start(size_t total_size) {
  if (s_is_updating && s_ota_handle != 0) {
    esp_ota_abort(s_ota_handle);
    s_ota_handle = 0;
  }

  s_total_bytes_written = 0;
  s_last_chunk_index = 0;
  s_expected_chunk_index = 0;

  s_update_partition = esp_ota_get_next_update_partition(NULL);
  if (!s_update_partition) {
    ESP_LOGE(TAG, "Không tìm thấy phân vùng OTA hợp lệ!");
    node_state_machine_set_state(NODE_STATE_IDLE);
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGI(TAG, "Mở phiên OTA trên phân vùng: %s (Dung lượng: %d bytes)...",
           s_update_partition->label, (int)total_size);

  esp_err_t err = esp_ota_begin(s_update_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin thất bại: %s", esp_err_to_name(err));
    s_is_updating = false;
    node_state_machine_set_state(NODE_STATE_IDLE);
    return err;
  }

  s_is_updating = true;
  ESP_LOGI(TAG, "Khởi tạo OTA Handle thành công, sẵn sàng nhận dữ liệu!");
  return ESP_OK;
}

esp_err_t ota_node_write_chunk(uint32_t chunk_index, const uint8_t *data, size_t length) {
  if (!s_is_updating || s_ota_handle == 0 || !data || length == 0) {
    return ESP_ERR_INVALID_STATE;
  }

  // 1. Chống ghi trùng: Nếu là chunk cũ Master gửi lại do mất ACK -> Bỏ qua không ghi vào Flash
  if (chunk_index < s_expected_chunk_index) {
    return ESP_OK;
  }

  // 2. Ghi chunk mới vào Flash
  esp_err_t err = esp_ota_write(s_ota_handle, data, length);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Lỗi ghi OTA chunk #%lu: %s", (unsigned long)chunk_index, esp_err_to_name(err));
    return err;
  }

  s_total_bytes_written += length;
  s_last_chunk_index = chunk_index;
  s_expected_chunk_index = chunk_index + 1;
  return ESP_OK;
}

esp_err_t ota_node_finish(void) {
  if (!s_is_updating || s_ota_handle == 0) {
    node_state_machine_set_state(NODE_STATE_IDLE);
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Đang kết thúc ghi OTA... Đã nhận tổng cộng: %d bytes (Chunk cuối: #%lu)",
           (int)s_total_bytes_written, (unsigned long)s_last_chunk_index);

  esp_err_t err = esp_ota_end(s_ota_handle);
  s_ota_handle = 0;
  s_is_updating = false;

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "❌ esp_ota_end thất bại (Mã lỗi: %s) -> Báo lỗi cho Master và phục hồi Firmware cũ!", esp_err_to_name(err));
    // Bắn gói tin FAIL về cho Master Tủ Điện
    esp_now_node_send_ota_response(OTA_PACKET_TYPE_FAIL, (uint32_t)err, esp_err_to_name(err));
    node_state_machine_set_state(NODE_STATE_IDLE);
    return err;
  }

  err = esp_ota_set_boot_partition(s_update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition thất bại: %s", esp_err_to_name(err));
    esp_now_node_send_ota_response(OTA_PACKET_TYPE_FAIL, (uint32_t)err, "SET_BOOT_FAILED");
    node_state_machine_set_state(NODE_STATE_IDLE);
    return err;
  }

  // Báo thành công cho Master trước khi reboot
  esp_now_node_send_ota_response(OTA_PACKET_TYPE_SUCCESS, 0, "OTA_SUCCESS");

  ESP_LOGI(TAG, "🎉 NẠP OTA QUA ESP-NOW THÀNH CÔNG 100%%!");
  ESP_LOGI(TAG, "Khởi động lại chip sau 2 giây để kích hoạt Firmware mới...");
  vTaskDelay(pdMS_TO_TICKS(2000));
  esp_restart();

  return ESP_OK;
}

void ota_node_abort(void) {
  if (s_is_updating && s_ota_handle != 0) {
    esp_ota_abort(s_ota_handle);
    s_ota_handle = 0;
    s_is_updating = false;
    ESP_LOGW(TAG, "Đã hủy phiên OTA và quay về đo đạc!");
    node_state_machine_set_state(NODE_STATE_IDLE);
  }
}

void ota_node_rollback_and_reboot(void) {
  ESP_LOGE(TAG, "KÍCH HOẠT ROLLBACK VÀ REBOOT!");
  esp_ota_mark_app_invalid_rollback_and_reboot();
}

bool ota_node_is_updating(void) { return s_is_updating; }
