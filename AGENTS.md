# PROJECT GUIDELINES: ESP32-S3 Embedded Firmware (PUMP_MY_FAMILY)

Tài liệu này cung cấp toàn bộ hướng dẫn, tiêu chuẩn lập trình, quy tắc phần cứng và quy trình build/flash cho trợ lý AI (và lập trình viên) khi phát triển mã nguồn trong dự án này.

---

## 1. Thông Tin Dự Án & Phần Cứng

- **Chip:** ESP32-S3 (Xtensa Dual-Core 32-bit LX7, up to 240 MHz).
- **Phân bản bộ nhớ:** Hỗ trợ Octal Flash / Octal PSRAM (N8R8 / N16R8 - 8MB/16MB Flash, 8MB PSRAM).
- **Framework / SDK:** Official **ESP-IDF v5.3+** (C / C++).
- **Hệ điều hành RTOS:** FreeRTOS (ESP-IDF flavor).

---

## 2. Quy Trình Môi Trường & Dòng Lệnh (CLI Workflow)

Mỗi khi thực thi lệnh liên quan đến ESP-IDF qua terminal:

```bash
# 1. Kích hoạt môi trường ESP-IDF
get_idf
# Hoặc: . $HOME/esp/esp-idf/export.sh

# 2. Điều hướng vào thư mục dự án
cd ~/OUT_SRC/PUMP_MY_FAMILY/my_esp32s3_app

# 3. Đặt target cho chip ESP32-S3 (khi khởi tạo hoặc chuyển target)
idf.py set-target esp32s3

# 4. Cấu hình phần cứng (Menuconfig)
idf.py menuconfig

# 5. Build dự án
idf.py build

# 6. Nạp firmware & Mở log (Serial Monitor)
idf.py flash monitor
# Hoặc chỉ định cổng cụ thể (Linux):
# idf.py -p /dev/ttyUSB0 flash monitor
# idf.py -p /dev/ttyACM0 flash monitor
```

---

## 3. Cấu Trúc Dự Án (Project Structure)

Dự án tuân theo cấu trúc chuẩn của ESP-IDF:

```text
PUMP_MY_FAMILY/
├── AGENTS.md                  # Hướng dẫn quy tắc cho AI & Dev
├── GEMINI.md                  # Hướng dẫn quy tắc tương thích
├── my_esp32s3_app/             # Ứng dụng chính
│   ├── CMakeLists.txt         # Root CMake project
│   ├── sdkconfig              # Cấu hình phần cứng tạo bởi menuconfig
│   ├── main/                  # Thư mục chứa mã nguồn chính
│   │   ├── CMakeLists.txt     # Khai báo SRCS và INCLUDE_DIRS
│   │   ├── main.c             # Entry point (app_main)
│   │   └── ...                # Các module .c / .h
│   └── components/            # (Tùy chọn) Các driver/module độc lập tái sử dụng
```

---

## 4. Quy Tắc & Tiêu Chuẩn Viết Code (Coding Standards)

### 4.1. Khởi tạo và Hàm chính
- Entry point luôn là:
  ```c
  #include <stdio.h>
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "esp_log.h"
  #include "esp_err.h"

  static const char *TAG = "APP_MAIN";

  void app_main(void) {
      ESP_LOGI(TAG, "Starting application...");
  }
  ```

### 4.2. Logging
- **KHÔNG** dùng `printf()` tùy tiện cho mục đích debug/log hệ thống.
- **BẮT BUỘC** dùng thư viện `esp_log.h` với các mức:
  - `ESP_LOGE(TAG, ...)` : Lỗi nghiêm trọng.
  - `ESP_LOGW(TAG, ...)` : Cảnh báo.
  - `ESP_LOGI(TAG, ...)` : Thông tin tiến trình.
  - `ESP_LOGD(TAG, ...)` / `ESP_LOGV(TAG, ...)` : Debug chi tiết.

### 4.3. Quản lý Bộ Nhớ & Tối Ưu Hóa (ESP32-S3 PSRAM)
- Phân bổ bộ nhớ thông thường (Internal SRAM - tốc độ cao, dùng cho DMA/Interrupts):
  ```c
  void *buf = malloc(size);
  ```
- Phân bổ bộ nhớ lớn (External Octal PSRAM / SPIRAM - dùng cho buffer hình ảnh, âm thanh, buffer mạng lớn):
  ```c
  #include "esp_heap_caps.h"
  void *large_buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  ```
- **Luôn kiểm tra `NULL`** sau khi cấp phát và giải phóng (`free()`) khi không sử dụng để tránh memory leak.
- Đảm bảo stack size cho các FreeRTOS Task được tính toán hợp lý (thường tối thiểu 2048 - 4096 bytes tùy độ phức tạp).

### 4.4. Xử Lý Lỗi (Error Handling)
- Các hàm của ESP-IDF trả về kiểu `esp_err_t`.
- Luôn kiểm tra mã lỗi trả về:
  ```c
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ```

---

## 5. Quy Tắc Dành Cho AI Khi Làm Việc Trong Workspace Này

1. **Tuân thủ CMake:** Khi tạo mới file `.c` hoặc thư mục con trong `main/` hoặc `components/`, AI phải tự động kiểm tra và cập nhật `CMakeLists.txt` tương ứng.
2. **Không tự ý sửa file build cache:** Không chỉnh sửa thủ công các file trong thư mục `build/`.
3. **Cấu hình phần cứng:** Nếu code cần tính năng mới (Wi-Fi, BLE, PSRAM, FreeRTOS config, NVS...), nhắc lập trình viên hoặc hướng dẫn cấu hình qua `sdkconfig` / `menuconfig`.
4. **An toàn bộ nhớ:** Chú ý giới hạn kích thước Stack của FreeRTOS task và tránh block CPU core 0/1 không nhường thời gian (luôn có `vTaskDelay` trong vòng lặp vô tận của Task).
