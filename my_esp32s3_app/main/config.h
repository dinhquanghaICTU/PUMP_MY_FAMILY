#ifndef __CONFIG_H__
#define __CONFIG_H__

#define WIFI_SSID "QuangHa"
#define WIFI_PASS "66668888"

#define MQTT_BROKER_URI                                                        \
  "mqtts://20476a36ce36478d90de6d5676587638.s1.eu.hivemq.cloud:8883"
#define MQTT_USERNAME "quanghaictu"
#define MQTT_PASSWORD "Zdinhquangha1234"

#define TOPIC_PUMP_COMMAND "pump/family/command"
#define TOPIC_PUMP_STATUS "pump/family/status"
#define TOPIC_PUMP_OTA "pump/family/ota"

/* ==================== HARDWARE PIN CONFIGURATION ==================== */
#define BUTTON_PIN          21  // Nút bấm cứu hộ / Bật tắt bơm tay / Giữ 3s khóa trẻ em
#define RELAY_PUMP1_PIN     38  // Relay Bơm 1 (Bơm chính)
#define RELAY_PUMP2_PIN     39  // Relay Bơm 2 (Van / Bơm phụ)
#define FLOW_SENSOR_PIN     40  // Cảm biến lưu lượng dòng chảy (Xung ngắt)
#define LED_WS2812_PIN      48  // LED RGB báo trạng thái

#endif /* __CONFIG_H__ */