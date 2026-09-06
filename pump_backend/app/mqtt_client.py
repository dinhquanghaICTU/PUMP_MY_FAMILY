import json
import ssl
import time
from datetime import datetime
from urllib.parse import urlparse
import paho.mqtt.client as mqtt
from sqlmodel import Session, select
from app.config import settings
from app.database import engine
from app.models import Device

current_ota_progress = {
    "target": "esp32s3_cabinet",
    "status": "idle",
    "percent": 0,
    "bytes": 0,
    "total": 0,
    "version": None,
    "error": None,
    "message": None,
    "timestamp": 0
}

parsed_url = urlparse(settings.MQTT_BROKER_URI)
broker_host = parsed_url.hostname or "localhost"
broker_port = parsed_url.port or 8883
is_tls = parsed_url.scheme in ["mqtts", "ssl"]

# Sử dụng API mới của Paho MQTT v2
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="fastapi_backend_server")

if settings.MQTT_USERNAME:
    mqtt_client.username_pw_set(settings.MQTT_USERNAME, settings.MQTT_PASSWORD)

if is_tls:
    mqtt_client.tls_set(cert_reqs=ssl.CERT_REQUIRED)

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("[FastAPI] Đã kết nối thành công tới HiveMQ Cloud!")
        # Lắng nghe trạng thái từ ESP32-S3
        client.subscribe("pump/+/status", qos=1)
        print("[FastAPI] Đang lắng nghe topic: pump/+/status")
    else:
        print(f"[FastAPI] Kết nối HiveMQ thất bại, mã lỗi: {rc}")

def on_message(client, userdata, msg):
    try:
        topic = msg.topic
        payload_str = msg.payload.decode("utf-8")
        print(f"[FastAPI] Nhận MQTT [{topic}]: {payload_str}")

        data = json.loads(payload_str)
        # Giả sử ESP32 gửi { "water_level": 80, "pump_running": true, ... }
        with Session(engine) as session:
            statement = select(Device).where(Device.device_code == "PUMP_FAMILY_01")
            device = session.exec(statement).first()
            if device:
                # 1. Bắt tín hiệu ngắt kết nối (LWT hoặc OFFLINE)
                if data.get("is_online") is False or data.get("state") == "OFFLINE":
                    device.is_online = False
                    device.is_tank_online = False
                    device.updated_at = datetime.utcnow()
                    session.add(device)
                    session.commit()
                    print(f"🔴 [MQTT LWT/OFFLINE] Thiết bị {device.device_code} đã OFFLINE!")
                    return

                device.is_online = True
                device.updated_at = datetime.utcnow()

                if "tank_online" in data:
                    device.is_tank_online = bool(data["tank_online"] == 1)
                elif "water_percent" in data and float(data["water_percent"]) >= 0:
                    device.is_tank_online = True

                if "water_percent" in data:
                    # Mức nước từ cảm biến (nếu -1 là chưa có dữ liệu từ node)
                    pct = float(data["water_percent"])
                    if pct >= 0:
                        device.water_level = max(0, min(100, int(pct)))
                    else:
                        device.is_tank_online = False
                if "pump" in data:
                    device.is_pump_running = bool(data["pump"] == 1)
                if "mode" in data:
                    device.is_auto_mode = (data["mode"] == "auto")
                if "child_lock" in data:
                    device.is_child_lock = bool(data["child_lock"] == 1)
                if "battery" in data and data["battery"] is not None:
                    device.battery_voltage = round(float(data["battery"]), 2)
                if "distance_cm" in data and data["distance_cm"] is not None:
                    device.distance_cm = round(float(data["distance_cm"]), 1)
                if "runtime" in data and data["runtime"] is not None:
                    device.pump_runtime = int(data["runtime"])
                # CHỈ cập nhật phiên bản từ gói tin telemetry thông thường, KHÔNG lấy từ gói tin ota_progress
                if not data.get("event"):
                    if "version" in data and data["version"]:
                        new_s3_ver = str(data["version"]).strip()
                        old_s3_ver = str(device.firmware_version or "").strip()
                        device.firmware_version = new_s3_ver
                        
                        # Nếu đang OTA tủ điện và version thay đổi hoặc khớp target version -> Đẩy OTA thành công 100%!
                        is_target_match = False
                        if current_ota_progress.get("version"):
                            exp_ver = str(current_ota_progress["version"]).lstrip("v").strip()
                            got_ver = new_s3_ver.lstrip("v").strip()
                            if exp_ver == got_ver:
                                is_target_match = True
                        is_version_changed = bool(old_s3_ver and new_s3_ver != old_s3_ver)

                        if current_ota_progress.get("target") == "esp32s3_cabinet" and (is_target_match or is_version_changed):
                            current_ota_progress["status"] = "success"
                            current_ota_progress["percent"] = 100
                            current_ota_progress["message"] = f"Tủ Điện đã kích hoạt firmware v{new_s3_ver} thành công!"
                            print(f"🎉 [OTA AUTO SUCCESS] S3 Telemetry xác nhận version v{new_s3_ver} (Cũ: {old_s3_ver})!")

                    if "tank_version" in data and data["tank_version"]:
                        new_tank_ver = str(data["tank_version"]).strip()
                        old_tank_ver = str(device.tank_firmware_version or "").strip()
                        device.tank_firmware_version = new_tank_ver
                        
                        # Nếu đang OTA bể nước và version thay đổi hoặc khớp target version -> Đẩy OTA thành công 100%!
                        is_target_match = False
                        if current_ota_progress.get("version"):
                            exp_ver = str(current_ota_progress["version"]).lstrip("v").strip()
                            got_ver = new_tank_ver.lstrip("v").strip()
                            if exp_ver == got_ver:
                                is_target_match = True
                        is_version_changed = bool(old_tank_ver and new_tank_ver != old_tank_ver)

                        if current_ota_progress.get("target") in ("esp32_tank", "node_tank") and (is_target_match or is_version_changed):
                            current_ota_progress["status"] = "success"
                            current_ota_progress["percent"] = 100
                            current_ota_progress["message"] = f"Node Bể Nước đã kích hoạt firmware v{new_tank_ver} thành công!"
                            print(f"🎉 [OTA AUTO SUCCESS] Node Bể Telemetry xác nhận version v{new_tank_ver} (Cũ: {old_tank_ver})!")
                session.add(device)
                session.commit()

        # Bắt sự kiện tiến độ OTA từ ESP32
        if data.get("event") in ("ota_progress", "ota_status"):
            target = data.get("target", "esp32s3_cabinet")
            if target == "node_tank":
                target = "esp32_tank"
            current_ota_progress["target"] = target
            current_ota_progress["status"] = data.get("status", "in_progress")
            current_ota_progress["percent"] = data.get("percent", 0)
            current_ota_progress["bytes"] = data.get("bytes", 0)
            current_ota_progress["total"] = data.get("total", 0)
            current_ota_progress["version"] = data.get("version")
            current_ota_progress["water_percent"] = data.get("water_percent")
            current_ota_progress["target_percent"] = data.get("target_percent")
            current_ota_progress["error"] = data.get("error")
            current_ota_progress["message"] = data.get("message")
            current_ota_progress["timestamp"] = int(time.time())
            print(f"📡 [OTA PROGRESS] {current_ota_progress['target']} -> {current_ota_progress['percent']}% ({current_ota_progress['status']})")
    except Exception as e:
        print(f"[FastAPI] Lỗi xử lý tin MQTT: {e}")

mqtt_client.on_connect = on_connect
mqtt_client.on_message = on_message

def start_mqtt():
    try:
        print(f"[FastAPI] Đang kết nối tới HiveMQ Cloud: {broker_host}:{broker_port}...")
        mqtt_client.connect(broker_host, broker_port, 60)
        mqtt_client.loop_start()
    except Exception as e:
        print(f"[FastAPI] Lỗi khởi động MQTT Client: {e}")

def send_pump_command(command: dict):
    topic = "pump/family/command"
    # ESP32 dùng hàm C strstr tìm chuỗi kiểu '"action":"off"' (KHÔNG CÓ DẤU CÁCH sau dấu 2 chấm)
    payload = json.dumps(command, separators=(',', ':'))
    res = mqtt_client.publish(topic, payload, qos=1)
    if res.rc == mqtt.MQTT_ERR_SUCCESS:
        print(f" [FastAPI] Đã gửi lệnh xuống ESP32 [{topic}]: {payload}")
    else:
        print(f" [FastAPI] Gửi lệnh MQTT thất bại!")
