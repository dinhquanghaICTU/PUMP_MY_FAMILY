# BẢNG TRA CỨU CHÂN PINOUT & QUY TẮC GPIO CHO ESP32-S3 (N8R8 / N16R8)

Tài liệu này tổng hợp chi tiết chức năng, các chân an toàn và **các chân CẤM DÙNG** trên dòng chip **ESP32-S3** (đặc biệt là phiên bản sử dụng Octal Flash / Octal PSRAM như N8R8, N16R8).

---

## 1. ⚠️ CẢNH BÁO QUAN TRỌNG: CÁC CHÂN CẤM DÙNG (Octal Flash / PSRAM)

> [!CAUTION]
> Trên các module ESP32-S3 bản **Octal PSRAM (N8R8, N16R8)**, các chân sau đã được nối ngầm bên trong chip để giao tiếp với bộ nhớ tốc độ cao. **TUYỆT ĐỐI KHÔNG SỬ DỤNG HOẶC NỐI DÂY NGOÀI**, nếu không chip sẽ bị crash / reboot liên tục:
> - **GPIO 26 -> GPIO 32** (Kết nối Octal Flash)
> - **GPIO 33 -> GPIO 37** (Kết nối Octal PSRAM / SPIRAM)

---

## 2. CHÂN STRAPPING PINS (Chân ảnh hưởng khi khởi động Boot)

Các chân này quyết định chế độ khởi động của chip, cần thận trọng khi gắn cảm biến hoặc điện trở kéo ngoài (Pull-up/Pull-down):

| GPIO | Tên Chức Năng | Trạng Thái Mặc Định | Lưu Ý |
| :--- | :--- | :--- | :--- |
| **GPIO 0** | BOOT / Download Mode | Kéo lên (Pull-up) nội | Kéo xuống **GND** khi cấp nguồn để vào chế độ nạp Bootloader. Hoạt động như GPIO thông thường sau khi chip đã boot. |
| **GPIO 45**| VDD_SPI Voltage Select | Kéo xuống (Pull-down) | Không nên kéo ngoài khi boot để tránh cấp sai điện áp Flash. |
| **GPIO 46**| Boot Log / ROM Control | Kéo xuống (Pull-down) | **Không được kéo HIGH khi boot** (nếu kéo lên HIGH sẽ chuyển sang chế độ ROM debug, chip không chạy firmware). |
| **GPIO 3** | JTAG Strapping | Kéo lên (Pull-up) | Có thể dùng làm GPIO sau khi boot. |

---

## 3. CỔNG GIAO TIẾP MẶC ĐỊNH (USB & UART DEBUG)

| GPIO | Chức Năng Mặc Định | Ghi Chú |
| :--- | :--- | :--- |
| **GPIO 19** | **USB D-** (Native USB) | Dùng cho cổng USB OTG / JTAG Hardware Debug. |
| **GPIO 20** | **USB D+** (Native USB) | Dùng cho cổng USB OTG / JTAG Hardware Debug. |
| **GPIO 43** | **UART0 TX** | Chân truyền log UART (nối vào chip nạp COM/UART). |
| **GPIO 44** | **UART0 RX** | Chân nhận lệnh UART (nối vào chip nạp COM/UART). |

---

## 4. BẢNG DANH SÁCH CHÂN AN TOÀN SỬ DỤNG (SAFE GPIOs)

Nhờ bộ chuyển mạch **GPIO Matrix** của ESP32-S3, bạn có thể cấu hình **I2C, SPI, PWM (LEDC), UART, Relay, Cảm biến** vào **BẤT KỲ** chân an toàn nào dưới đây:

### Nhóm 1: GPIO Đa Dụng Tốt Nhất
| GPIO | Hỗ trợ ADC (Đọc Analog) | Hỗ trợ Touch | Khuyên dùng cho |
| :---: | :---: | :---: | :--- |
| **GPIO 1**  | ADC1_CH0 | Touch 1 | Cảm biến Analog / Nút bấm / Relay |
| **GPIO 2**  | ADC1_CH1 | Touch 2 | Cảm biến Analog / Relay / Đèn LED |
| **GPIO 4**  | ADC1_CH3 | Touch 4 | I2C (SDA) / Cảm biến / Nút bấm |
| **GPIO 5**  | ADC1_CH4 | Touch 5 | I2C (SCL) / Cảm biến / Nút bấm |
| **GPIO 6**  | ADC1_CH5 | Touch 6 | SPI (MOSI/SCK) / Output điều khiển |
| **GPIO 7**  | ADC1_CH6 | Touch 7 | SPI (MISO) / Output điều khiển |
| **GPIO 8**  | ADC1_CH7 | Touch 8 | SPI (CS) / Output điều khiển |
| **GPIO 9**  | ADC1_CH8 | Touch 9 | GPIO Input / Output chung |
| **GPIO 10** | ADC1_CH9 | Touch 10| GPIO Input / Output chung |
| **GPIO 11** | ADC2_CH0 | Touch 11| GPIO Input / Output chung |
| **GPIO 12** | ADC2_CH1 | Touch 12| GPIO Input / Output chung |
| **GPIO 13** | ADC2_CH2 | Touch 13| GPIO Input / Output chung |
| **GPIO 14** | ADC2_CH3 | Touch 14| GPIO Input / Output chung |

> 💡 **Lưu ý ADC:** Các chân **ADC1** (GPIO 1 -> 10) hoạt động hoàn hảo ngay cả khi bật Wi-Fi. Các chân **ADC2** (GPIO 11 -> 20) sẽ bị Wi-Fi chia sẻ tài nguyên (hạn chế dùng đọc analog liên tục khi kết nối Wi-Fi).

### Nhóm 2: GPIO Cao Cấp (High GPIOs)
| GPIO | Đặc Điểm | Khuyên dùng |
| :---: | :--- | :--- |
| **GPIO 38** | Không vướng Flash/PSRAM | PWM Bơm / Relay / Động cơ |
| **GPIO 39** | Không vướng Flash/PSRAM | Output / Cảm biến xung lưu lượng |
| **GPIO 40** | Không vướng Flash/PSRAM | Output / Cảm biến độ ẩm đất |
| **GPIO 41** | Không vướng Flash/PSRAM | Output / Giao tiếp ngoài |
| **GPIO 42** | Không vướng Flash/PSRAM | Output / Giao tiếp ngoài |
| **GPIO 47** | Không vướng Flash/PSRAM | Output / Đèn báo |
| **GPIO 48** | Thường nối LED RGB WS2812 trên kit Dev | Đèn LED RGB trạng thái |

---

## 5. GỢI Ý PHÂN BỔ CHÂN CHO DỰ ÁN MÁY BƠM (PUMP_MY_FAMILY)

| Thiết Bị Ngoại Vi | GPIO Đề Xuất | Ghi Chú |
| :--- | :---: | :--- |
| **Relay Điều Khiển Bơm 1** | **GPIO 38** | Kích mức HIGH/LOW (Cách ly Opto) |
| **Relay Điều Khiển Bơm 2 / Van** | **GPIO 39** | Kích mức HIGH/LOW |
| **Cảm Biến Lưu Lượng Nước (Flow Sensor)** | **GPIO 40** | Đọc ngắt xung GPIO (Pulse Interrupt) |
| **Cảm Biến Độ Ẩm Đất / Mực Nước (Analog)**| **GPIO 1 (ADC1)** | Đọc tín hiệu điện áp 0-3.3V ổn định |
| **Nút Bấm Cứu Hộ / Bật Bơm Bằng Tay** | **GPIO 0 hoặc GPIO 4** | Nút bấm chống rung (Debounce) |
| **Giao Tiếp I2C Màn Hình OLED / Cảm biến (SDA)**| **GPIO 4** | Kéo trở 4.7k lên 3.3V |
| **Giao Tiếp I2C Màn Hình OLED / Cảm biến (SCL)**| **GPIO 5** | Kéo trở 4.7k lên 3.3V |
| **LED RGB Báo Trạng Thái Wifi / Lỗi / OTA** | **GPIO 48** | Đèn WS2812 tích hợp sẵn trên kit |
