# SƠ ĐỒ PINOUT & ĐẤU NỐI PHẦN CỨNG ESP32-S3 (ASCII DIAGRAM)

Dự án: **PUMP_MY_FAMILY - Hệ Thống Điều Khiển Máy Bơm Thông Minh**  
Phần cứng: **ESP32-S3-DevKitC-1 (N8R8 / N16R8 - Octal Flash & PSRAM)**

---

## 1. SƠ ĐỒ CHÂN BOARD ESP32-S3 DEVKIT (ASCII PINOUT)

```text
                        ┌────────────────────────┐
                        │      ESP32-S3 PCB      │
                        │      ANTENNA WIFI      │
                        │   ┌────────────────┐   │
                        │   │  ESP32-S3-WROOM│   │
                        │   │  (N8R8/N16R8)  │   │
                        │   └────────────────┘   │
                        │                        │
       [Nguồn 3.3V] ─── │ 3V3                GND │ ─── [GND Chung]
       [Nguồn 3.3V] ─── │ 3V3               GPIO43│ ─── [UART0 TX - Debug Log]
      [Chân Reset]  ─── │ RST               GPIO44│ ─── [UART0 RX - Debug Log]
 [I2C SDA - Màn hình] ─── │ GPIO4              GPIO1 │ ─── [ADC1_CH0 - Cảm Biến Mực Nước/Độ Ẩm]
 [I2C SCL - Màn hình] ─── │ GPIO5              GPIO2 │ ─── [ADC1_CH1 - Dự phòng]
   [Dự phòng SPI]   ─── │ GPIO6             GPIO42│ ─── [JSN-SR04T - Chân TRIG (Phát Xung)]
   [Dự phòng SPI]   ─── │ GPIO7             GPIO41│ ─── [JSN-SR04T - Chân ECHO (Thu Xung)]
   [Dự phòng SPI]   ─── │ GPIO15            GPIO40│ ─── [FLOW SENSOR - Cảm Biến Lưu Lượng]
   [Digital IO]     ─── │ GPIO16            GPIO39│ ─── [RELAY 2 - Van Nước / Bơm Phụ]
   [Digital IO]     ─── │ GPIO17            GPIO38│ ─── [RELAY 1 - Máy Bơm Chính]
   [Digital IO]     ─── │ GPIO18           *GPIO37│ ─── [⛔ CẤM DÙNG - PSRAM]
   [Dự phòng SPI]   ─── │ GPIO8            *GPIO36│ ─── [⛔ CẤM DÙNG - PSRAM]
[JTAG Strapping]    ─── │ GPIO3            *GPIO35│ ─── [⛔ CẤM DÙNG - PSRAM]
[Strapping - Kéo GND]── │ GPIO46            GPIO0 │ ─── [BOOT / Nút Bấm Tay Bật Bơm]
   [Digital IO]     ─── │ GPIO9             GPIO45│ ─── [Strapping VDD_SPI]
   [Digital IO]     ─── │ GPIO10            GPIO48│ ─── [LED RGB WS2812 ONBOARD]
   [Digital IO]     ─── │ GPIO11            GPIO47│ ─── [Digital IO / Buzzer Báo Động]
   [Digital IO]     ─── │ GPIO12            GPIO21│ ─── [Digital IO]
   [Digital IO]     ─── │ GPIO13            GPIO20│ ─── [USB Native D+]
   [Digital IO]     ─── │ GPIO14            GPIO19│ ─── [USB Native D-]
   [Nguồn vào 5V]   ─── │ 5V                 GND │ ─── [GND Chung]
   [GND Chung]      ─── │ GND                GND │ ─── [GND Chung]
                        │   ┌────┐      ┌────┐   │
                        │   │USB │      │UART│   │
                        └───┴────┴──────┴────┴───┘
```

> **Ghi chú ký hiệu:**  
> - ⛔ `*GPIO33 -> *GPIO37` & `*GPIO26 -> *GPIO32`: Chân kết nối Flash/PSRAM nội, **TUYỆT ĐỐI KHÔNG DÙNG**.

---

## 2. SƠ ĐỒ ĐẤU NỐI TOÀN BỘ THIẾT BỊ NGOẠI VI (WIRING DIAGRAM)

```text
+---------------------------------------------------------------------------------------+
|                                    HỆ THỐNG MÁY BƠM                                   |
+---------------------------------------------------------------------------------------+

                           +--------------------+
                           |    ESP32-S3 BOARD  |
                           +--------------------+
                                     │
           ┌─────────────────────────┼─────────────────────────┐
           │                         │                         │
      [GPIO 38]                 [GPIO 39]                 [GPIO 40]
           │                         │                         │ (Xung Ngắt)
           ▼                         ▼                         ▲
    ┌─────────────┐           ┌─────────────┐           ┌─────────────┐
    │ MODULE      │           │ MODULE      │           │ CẢM BIẾN    │
    │ RELAY 1     │           │ RELAY 2     │           │ LƯU LƯỢNG   │
    │ (Cách ly    │           │ (Van Nước / │           │ (YF-S201)   │
    │  Opto)      │           │  Bơm Phụ)   │           │             │
    └──────┬──────┘           └──────┬──────┘           └──────┬──────┘
           │                         │                         │
           ▼                         ▼                         │ (Chia áp 5V->3.3V)
     [MÁY BƠM 1]               [VAN / BƠM 2]                   │
     (220V AC / 12V)           (220V AC / 12V)                 │
                                                               │
───────────────────────────────────────────────────────────────┼─────────────────────────
                                                               │
           ┌─────────────────────────┬─────────────────────────┘
           │                         │
        [GPIO 1]                  [GPIO 21]
       (ADC1_CH0)                     │
            │                         ▼
            ▼                  ┌─────────────┐
     ┌─────────────┐           │ NÚT BẤM TAY │
     │ CẢM BIẾN    │           │ MANUAL /    │ ─── (Nhấn nối GND)
     │ ĐỘ ẨM ĐẤT / │           │ EMERGENCY   │
     │ MỰC NƯỚC    │           └─────────────┘
     │ (0 - 3.1V)  │
     └─────────────┘
                                 [GPIO 48] (Nội trên Board)
                                     │
                                     ▼
                              ┌─────────────┐
                              │ LED WS2812  │ ── [Báo trạng thái Wi-Fi, MQTT, BLE, Lỗi]
                              └─────────────┘

       [GPIO 4] (SDA) ────────┐
       [GPIO 5] (SCL) ───┐    │
                         ▼    ▼
                   ┌─────────────┐
                   │ MÀN HÌNH    │
                   │ OLED I2C    │ ── (Địa chỉ 0x3C, hiển thị IP, lưu lượng, trạng thái)
                   │ (SSD1306)   │
                   └─────────────┘
```

---

## 3. BẢNG TÓM TẮT PINOUT & VAI TRÒ CHỨC NĂNG

| STT | Chân GPIO | Tên Thiết Bị / Chức Năng | Chế Độ | Mức Logic / Tín Hiệu | Mô Tả & Lưu Ý Kỹ Thuật |
|:---:|:---:|:---|:---:|:---:|:---|
| **1** | **GPIO 38** | **Relay Bơm Chính** | Output | 3.3V Logic | Kích mở Relay đóng điện máy bơm chính (Khuyên dùng mạch có Opto) |
| **2** | **GPIO 39** | **Relay Bơm Phụ / Van Nước** | Output | 3.3V Logic | Kích mở van xả hoặc bơm tăng áp |
| **3** | **GPIO 40** | **Cảm Biến Lưu Lượng (Flow)** | Input (Interrupt)| 3.3V Pulse | Đếm số xung nước chảy qua tua-bin (Cần hạ áp nếu cảm biến ra 5V) |
| **4** | **GPIO 1** | **Cảm Biến Nước / Độ Ẩm** | ADC Input (ADC1)| 0.0V - 3.1V | Đọc điện áp cảm biến tuyến tính (ADC1 an toàn 100% khi bật Wi-Fi) |
| **5** | **GPIO 21** | **Nút Bấm Cứu Hộ / Bơm Tay**| Input (Pull-up) | Active LOW | Nhấn để bật tắt bơm thủ công, giữ 3s bật/tắt Khóa trẻ em |
| **6** | **GPIO 48** | **LED RGB WS2812 Onboard** | RMT Output | 3.3V / 5V | Đèn báo: **Xanh lá** (WiFi OK), **Xanh dương** (BLE), **Đỏ** (Lỗi/Mất mạng) |
| **7** | **GPIO 4** | **I2C SDA** | I2C Data | 3.3V | Đường truyền dữ liệu màn hình OLED / Cảm biến môi trường |
| **8** | **GPIO 5** | **I2C SCL** | I2C Clock | 3.3V | Xung nhịp đồng hồ giao tiếp I2C |
| **9** | **GPIO 43** | **UART0 TX** | UART TX | 3.3V | Truyền log Debug ra máy tính (Baudrate 115200) |
| **10**| **GPIO 44** | **UART0 RX** | UART RX | 3.3V | Nhận lệnh điều khiển qua UART |
| **11**| **GPIO 42** | **JSN-SR04T (TRIG)** | Output | 3.3V | Chân phát xung kích hoạt đo khoảng cách siêu âm (10µs) |
| **12**| **GPIO 41** | **JSN-SR04T (ECHO)** | Input | 3.3V | Chân nhận độ rộng xung phản xạ siêu âm |

---

## 4. ⚠️ BẢNG CÁC CHÂN CẤM & CHÂN ĐẶC BIỆT

```text
+---------------------------------------------------------------------------------------+
| ⛔ CÁC CHÂN TUYỆT ĐỐI KHÔNG SỬ DỤNG (NỐI FLASH / PSRAM N8R8, N16R8)                   |
+---------------------------------------------------------------------------------------+
|  • GPIO 26, 27, 28, 29, 30, 31, 32  ===> Nối Octal Flash nội                          |
|  • GPIO 33, 34, 35, 36, 37          ===> Nối Octal PSRAM (SPIRAM) nội                 |
|  ==> Dùng các chân này sẽ gây CRASH hoặc REBOOT liên tục!                             |
+---------------------------------------------------------------------------------------+

+---------------------------------------------------------------------------------------+
| ⚠️ CHÂN STRAPPING BOOTLOADER                                                          |
+---------------------------------------------------------------------------------------+
|  • GPIO 0  : Mặc định Pull-up. Nối xuống GND khi boot để vào Download Mode.           |
|  • GPIO 46 : Mặc định Pull-down. CẤM KÉO LÊN HIGH KHI BOOT (gây lỗi ROM).             |
|  • GPIO 45 : Chọn áp VDD_SPI, để mặc định Pull-down.                                  |
+---------------------------------------------------------------------------------------+
```
