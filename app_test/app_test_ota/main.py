#!/usr/bin/env bash
#!/usr/bin/env python3
"""
OTA Firmware Test Tool & MQTT Command Hub - PUMP_MY_FAMILY
- Giao diện Web trực quan (http://localhost:5050)
- Tích hợp sẵn máy chủ HTTP File Server nội bộ để ESP32 tải trực tiếp file .bin
- Ô bắn lệnh tùy biến (Custom Command / Quick Buttons) vào topic `pump/family/command`
- Hỗ trợ OTA Dual-Node: Nạp cho Tủ Điện (ESP32-S3) & Bể Nước (ESP32-U qua ESP-NOW)
- Kết nối bảo mật MQTT HiveMQ Cloud SSL/TLS (Port 8883)
- Hiển thị log phản hồi trạng thái từ ESP32 theo thời gian thực
"""

import os
import sys
import json
import time
import socket
import ssl
import hashlib
import threading
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs, urlparse

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Vui lòng cài đặt paho-mqtt: pip install paho-mqtt")
    sys.exit(1)

# ==================== CẤU HÌNH MQTT HIVEMQ ====================
MQTT_BROKER = "20476a36ce36478d90de6d5676587638.s1.eu.hivemq.cloud"
MQTT_PORT = 8883
MQTT_USER = "quanghaictu"
MQTT_PASS = "Zdinhquangha1234"
TOPIC_OTA = "pump/family/ota"
TOPIC_COMMAND = "pump/family/command"
TOPIC_STATUS = "pump/family/status"

# ==================== BIẾN TRẠNG THÁI TOÀN CỤC ====================
APP_STATE = {
    "firmware_path": "",
    "firmware_filename": "",
    "firmware_size": 0,
    "firmware_md5": "",
    "version": "2.0.0",
    "target": "esp32s3_cabinet",
    "mqtt_connected": False,
    "logs": []
}

def log_message(msg):
    timestamp = time.strftime("%H:%M:%S")
    entry = f"[{timestamp}] {msg}"
    print(entry)
    APP_STATE["logs"].append(entry)
    if len(APP_STATE["logs"]) > 150:
        APP_STATE["logs"].pop(0)

def get_local_ip():
    try:
        import subprocess
        ips = subprocess.getoutput("hostname -I").strip().split()
        for ip in ips:
            if ip.startswith("192.168.12."):
                return ip
        for ip in ips:
            if ip.startswith("192.168."):
                return ip
        if ips:
            return ips[0]
    except Exception:
        pass
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

LOCAL_IP = get_local_ip()
FILE_SERVER_PORT = 8080
WEB_UI_PORT = 5050

# Đường dẫn file build sẵn
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
CABINET_BIN = os.path.join(PROJECT_ROOT, "my_esp32s3_app/build/main.bin")
TANK_BIN = os.path.join(PROJECT_ROOT, "my_esp32_sensor_node/build/sensor_node.bin")

# ==================== MÁY CHỦ HTTP PHỤC VỤ FILE FIRMWARE ====================
class FirmwareFileHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        log_message(f"📡 [FILE SERVER] ESP32 yêu cầu tải: {self.path} từ IP: {self.client_address[0]}")
        
        file_path = APP_STATE.get("firmware_path", "")
        if not file_path or not os.path.isfile(file_path):
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"File not found")
            log_message("❌ [FILE SERVER] Không tìm thấy file firmware!")
            return

        try:
            file_size = os.path.getsize(file_path)
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(file_size))
            self.send_header("Content-Disposition", f'attachment; filename="{os.path.basename(file_path)}"')
            self.end_headers()

            with open(file_path, "rb") as f:
                sent = 0
                while chunk := f.read(4096):
                    self.wfile.write(chunk)
                    sent += len(chunk)
            log_message(f"✅ [FILE SERVER] Đã truyền xong {sent}/{file_size} bytes cho ESP32!")
        except Exception as e:
            log_message(f"❌ [FILE SERVER ERROR] Lỗi khi gửi file: {e}")

    def log_message(self, format, *args):
        pass

def run_file_server():
    server = HTTPServer(("0.0.0.0", FILE_SERVER_PORT), FirmwareFileHandler)
    server.serve_forever()

# ==================== MQTT CLIENT HANDLERS ====================
mqtt_client = None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        APP_STATE["mqtt_connected"] = True
        log_message(f"🟢 Đã kết nối thành công tới HiveMQ Cloud (Port {MQTT_PORT} SSL/TLS)!")
        client.subscribe(TOPIC_STATUS)
        client.subscribe(TOPIC_OTA)
        client.subscribe(TOPIC_COMMAND)
        log_message(f"📥 Đã lắng nghe các topic: '{TOPIC_STATUS}', '{TOPIC_OTA}', '{TOPIC_COMMAND}'")
    else:
        APP_STATE["mqtt_connected"] = False
        log_message(f"🔴 Kết nối MQTT thất bại! Return Code: {rc}")

def on_message(client, userdata, msg):
    payload_str = msg.payload.decode("utf-8", errors="ignore")
    log_message(f"📩 [MQTT NHẬN] Topic: {msg.topic} | Payload: {payload_str}")

def run_mqtt():
    global mqtt_client
    mqtt_client = mqtt.Client(client_id="Python_Test_Hub_" + str(int(time.time())))
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)
    mqtt_client.tls_set(cert_reqs=ssl.CERT_NONE)
    mqtt_client.tls_insecure_set(True)

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    log_message(f"Đang kết nối tới MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}...")
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        log_message(f"❌ Không thể kết nối MQTT: {e}")

# ==================== GIAO DIỆN WEB HTML ====================
HTML_PAGE = """<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PUMP_MY_FAMILY - TEST HUB & OTA CONTROLLER</title>
    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --accent: #38bdf8;
            --accent-hover: #0284c7;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --success: #22c55e;
            --danger: #ef4444;
            --warning: #f59e0b;
            --border: #334155;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); padding: 20px; line-height: 1.5; }
        .container { max-width: 1100px; margin: 0 auto; }
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; padding-bottom: 12px; border-bottom: 1px solid var(--border); }
        h1 { font-size: 22px; color: var(--accent); display: flex; align-items: center; gap: 8px; }
        .badge { padding: 4px 10px; border-radius: 9999px; font-size: 12px; font-weight: bold; }
        .badge-success { background: #064e3b; color: #34d399; }
        .badge-danger { background: #7f1d1d; color: #f87171; }
        
        .card { background-color: var(--card-bg); border-radius: 12px; padding: 20px; margin-bottom: 20px; border: 1px solid var(--border); box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }
        .card h2 { font-size: 16px; margin-bottom: 16px; color: var(--text-main); display: flex; align-items: center; gap: 8px; border-left: 4px solid var(--accent); padding-left: 8px; }
        
        .row { display: flex; gap: 16px; margin-bottom: 14px; flex-wrap: wrap; }
        .col { flex: 1; min-width: 240px; }
        
        label { display: block; font-size: 13px; color: var(--text-muted); margin-bottom: 6px; }
        input[type="text"], select { width: 100%; padding: 10px 14px; background: #0f172a; border: 1px solid var(--border); border-radius: 8px; color: var(--text-main); font-size: 14px; }
        input[type="text"]:focus, select:focus { outline: none; border-color: var(--accent); }
        input[type="file"] { width: 100%; padding: 8px; background: #0f172a; border: 1px dashed var(--accent); border-radius: 8px; color: var(--text-muted); cursor: pointer; }
        
        .btn { padding: 10px 18px; border: none; border-radius: 8px; font-size: 14px; font-weight: bold; cursor: pointer; transition: 0.2s; display: inline-flex; align-items: center; justify-content: center; gap: 6px; }
        .btn-primary { background: var(--accent); color: #0f172a; }
        .btn-primary:hover { background: var(--accent-hover); }
        .btn-success { background: var(--success); color: #fff; }
        .btn-warning { background: var(--warning); color: #0f172a; }
        .btn-danger { background: var(--danger); color: #fff; }
        .btn-secondary { background: #475569; color: #fff; }
        
        .quick-actions { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 14px; }
        .quick-btn { background: #334155; color: var(--text-main); border: 1px solid var(--border); border-radius: 6px; padding: 6px 12px; font-size: 13px; cursor: pointer; transition: 0.2s; }
        .quick-btn:hover { background: var(--accent); color: #0f172a; }

        .preset-bin-btn { background: #1e3a8a; border: 1px solid #3b82f6; color: #93c5fd; border-radius: 8px; padding: 10px 14px; font-size: 13px; cursor: pointer; font-weight: bold; flex: 1; }
        .preset-bin-btn:hover { background: #2563eb; color: #ffffff; }

        .preset-tank-btn { background: #064e3b; border: 1px solid #10b981; color: #6ee7b7; border-radius: 8px; padding: 10px 14px; font-size: 13px; cursor: pointer; font-weight: bold; flex: 1; }
        .preset-tank-btn:hover { background: #059669; color: #ffffff; }

        .info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 10px; background: #0f172a; padding: 12px; border-radius: 8px; margin-top: 10px; border: 1px solid var(--border); }
        .info-item { font-size: 13px; }
        .info-label { color: var(--text-muted); }
        .info-val { font-weight: bold; color: var(--accent); word-break: break-all; }
        
        .log-box { background: #090d16; border: 1px solid var(--border); border-radius: 8px; padding: 12px; height: 280px; overflow-y: auto; font-family: 'Courier New', Courier, monospace; font-size: 12px; line-height: 1.6; color: #38bdf8; white-space: pre-wrap; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🚰 PUMP_MY_FAMILY - TEST HUB & OTA GATEWAY</h1>
            <div id="mqtt-status" class="badge badge-danger">Đang kết nối MQTT...</div>
        </header>

        <!-- KHỐI 1: OTA FIRMWARE -->
        <div class="card">
            <h2>🚀 Nâng Cấp Firmware Từ Xa (OTA)</h2>

            <div style="display: flex; gap: 10px; margin-bottom: 14px;">
                <button type="button" class="preset-bin-btn" onclick="selectPreset('cabinet')">
                    🏠 1-CHẠM: Chọn Firmware Tủ Điện (my_esp32s3_app/build/main.bin)
                </button>
                <button type="button" class="preset-tank-btn" onclick="selectPreset('tank')">
                    🌊 1-CHẠM: Chọn Firmware Bể Nước (my_esp32_sensor_node/build/sensor_node.bin)
                </button>
            </div>

            <form id="otaForm" onsubmit="sendOTA(event)">
                <div class="form-group" style="margin-bottom: 12px;">
                    <label>📁 Hoặc tải lên file .bin tùy chọn từ máy:</label>
                    <input type="file" id="fileInput" accept=".bin" onchange="handleFileSelect(event)">
                </div>

                <div class="info-grid">
                    <div class="info-item"><div class="info-label">Tên File:</div><div id="info-name" class="info-val">Chưa chọn</div></div>
                    <div class="info-item"><div class="info-label">Dung lượng:</div><div id="info-size" class="info-val">0 KB</div></div>
                    <div class="info-item"><div class="info-label">MD5 Checksum:</div><div id="info-md5" class="info-val">Chưa tính</div></div>
                </div>

                <div class="row" style="margin-top: 14px;">
                    <div class="col">
                        <label>🎯 Thiết bị nhận:</label>
                        <select id="targetInput">
                            <option value="esp32s3_cabinet">🏠 Con Tủ Điện (ESP32-S3 Master - Nạp Trực Tiếp)</option>
                            <option value="esp32_tank">🌊 Con Bể Nước (ESP32-U Node - Bắn qua ESP-NOW)</option>
                        </select>
                    </div>
                    <div class="col">
                        <label>🏷️ Version mới:</label>
                        <input type="text" id="versionInput" value="2.0.0">
                    </div>
                </div>

                <div class="form-group" style="margin-bottom: 16px;">
                    <label>🌐 URL Tải Firmware:</label>
                    <input type="text" id="urlInput" value="http://__LOCAL_IP__:__HTTP_PORT__/firmware.bin">
                </div>

                <button type="submit" class="btn btn-primary" style="width: 100%; font-size: 15px; padding: 12px;">
                    ⚡ BẮN LỆNH OTA QUA MQTT NGAY BÂY GIỜ
                </button>
            </form>
        </div>

        <!-- KHỐI 2: ĐIỀU KHIỂN COMMAND -->
        <div class="card">
            <h2>🎮 Điều Khiển Nhanh Bằng Lệnh MQTT (`pump/family/command`)</h2>
            <div class="quick-actions">
                <button class="quick-btn" onclick="setCmd('{\\"pump\\":1,\\"action\\":\\"on\\"}')">⚡ Bật Bơm</button>
                <button class="quick-btn" onclick="setCmd('{\\"pump\\":1,\\"action\\":\\"off\\"}')">🛑 Tắt Bơm</button>
                <button class="quick-btn" onclick="setCmd('{\\"mode\\":\\"auto\\"}')">🤖 Chế độ Auto</button>
                <button class="quick-btn" onclick="setCmd('{\\"mode\\":\\"manual\\"}')">🖐️ Chế độ Manual</button>
                <button class="quick-btn" onclick="setCmd('{\\"child_lock\\":1}')">🔒 Bật Khóa Trẻ Em</button>
                <button class="quick-btn" onclick="setCmd('{\\"child_lock\\":0}')">🔓 Tắt Khóa Trẻ Em</button>
                <button class="quick-btn" onclick="setCmd('{\\"action\\":\\"get_status\\"}')">📊 Lấy Trạng Thái</button>
            </div>

            <form onsubmit="sendCommand(event)">
                <div class="row">
                    <div class="col" style="flex: 0 0 35%;">
                        <label>Topic MQTT:</label>
                        <input type="text" id="cmdTopic" value="pump/family/command">
                    </div>
                    <div class="col">
                        <label>Nội dung Payload (JSON):</label>
                        <input type="text" id="cmdPayload" value='{"pump":1,"action":"on"}'>
                    </div>
                </div>
                <button type="submit" class="btn btn-success" style="width: 100%;">
                    🚀 BẮN LỆNH ĐIỀU KHIỂN
                </button>
            </form>
        </div>

        <!-- KHỐI 3: NHẬT KÝ LOG -->
        <div class="card">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                <h2>📋 Nhật Ký Log MQTT & Trạng Thái Theo Thời Gian Thực</h2>
                <button class="btn btn-secondary" onclick="fetchLogs()" style="padding: 4px 12px; font-size: 12px;">Làm mới</button>
            </div>
            <div id="logBox" class="log-box">Đang tải logs...</div>
        </div>
    </div>

    <script>
        function setCmd(payload) {
            document.getElementById('cmdPayload').value = payload;
        }

        function selectPreset(type) {
            fetch('/api/select_preset?type=' + type)
                .then(res => res.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('info-name').innerText = data.filename;
                        document.getElementById('info-size').innerText = (data.size / 1024).toFixed(2) + ' KB (' + data.size + ' bytes)';
                        document.getElementById('info-md5').innerText = data.md5;
                        document.getElementById('targetInput').value = data.target;
                        document.getElementById('urlInput').value = data.url;
                        alert("✅ Đã chọn firmware: " + data.filename + " cho mục tiêu: " + data.target);
                    } else {
                        alert("❌ " + data.error);
                    }
                });
        }

        function sendCommand(e) {
            e.preventDefault();
            const topic = document.getElementById('cmdTopic').value;
            const payload = document.getElementById('cmdPayload').value;

            fetch('/api/send_raw', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ topic: topic, payload: payload })
            })
            .then(res => res.json())
            .then(data => {
                if (!data.success) {
                    alert("❌ Lỗi: " + data.error);
                }
            });
        }

        function handleFileSelect(event) {
            const file = event.target.files[0];
            if (!file) return;

            document.getElementById('info-name').innerText = file.name;
            document.getElementById('info-size').innerText = (file.size / 1024).toFixed(2) + ' KB (' + file.size + ' bytes)';

            const formData = new FormData();
            formData.append("firmware", file);

            fetch('/api/upload', {
                method: 'POST',
                body: formData
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    document.getElementById('info-md5').innerText = data.md5;
                    document.getElementById('urlInput').value = data.url;
                } else {
                    alert("Lỗi tải file lên máy chủ!");
                }
            });
        }

        function sendOTA(e) {
            e.preventDefault();
            const version = document.getElementById('versionInput').value;
            const target = document.getElementById('targetInput').value;
            const url = document.getElementById('urlInput').value;

            fetch('/api/send_ota', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    version: version,
                    target: target,
                    url: url
                })
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    alert("🚀 Đã bắn lệnh OTA qua MQTT thành công! Hãy theo dõi log phía dưới.");
                } else {
                    alert("❌ Lỗi: " + data.error);
                }
            });
        }

        function fetchLogs() {
            fetch('/api/status')
                .then(res => res.json())
                .then(data => {
                    const statusBadge = document.getElementById('mqtt-status');
                    if (data.mqtt_connected) {
                        statusBadge.className = 'badge badge-success';
                        statusBadge.innerText = 'MQTT: Đang trực tuyến (HiveMQ)';
                    } else {
                        statusBadge.className = 'badge badge-danger';
                        statusBadge.innerText = 'MQTT: Mất kết nối!';
                    }
                    const logBox = document.getElementById('logBox');
                    logBox.innerText = data.logs.join('\\n');
                    logBox.scrollTop = logBox.scrollHeight;
                });
        }

        setInterval(fetchLogs, 1200);
        fetchLogs();
    </script>
</body>
</html>
"""

class WebUIHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/" or parsed.path == "/index.html":
            html = HTML_PAGE.replace("__LOCAL_IP__", LOCAL_IP).replace("__HTTP_PORT__", str(FILE_SERVER_PORT))
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(html.encode("utf-8"))
        elif parsed.path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(APP_STATE).encode("utf-8"))
        elif parsed.path == "/api/select_preset":
            qs = parse_qs(parsed.query)
            preset_type = qs.get("type", ["cabinet"])[0]

            if preset_type == "tank":
                target_path = TANK_BIN
                target_name = "esp32_tank"
                filename = "sensor_node.bin"
            else:
                target_path = CABINET_BIN
                target_name = "esp32s3_cabinet"
                filename = "main.bin"

            if not os.path.isfile(target_path):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": f"Chưa tìm thấy file {target_path}! Hãy build dự án trước."}).encode("utf-8"))
                return

            with open(target_path, "rb") as f:
                content = f.read()

            upload_dir = os.path.dirname(os.path.abspath(__file__))
            save_path = os.path.join(upload_dir, "firmware.bin")
            with open(save_path, "wb") as f:
                f.write(content)

            md5_hash = hashlib.md5(content).hexdigest()
            APP_STATE["firmware_path"] = save_path
            APP_STATE["firmware_filename"] = filename
            APP_STATE["firmware_size"] = len(content)
            APP_STATE["firmware_md5"] = md5_hash
            APP_STATE["target"] = target_name

            log_message(f"📁 [1-CHẠM PRESET] Đã nạp file {filename} ({len(content)} bytes) | MD5: {md5_hash} | Target: {target_name}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "success": True,
                "filename": filename,
                "size": len(content),
                "md5": md5_hash,
                "target": target_name,
                "url": f"http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin"
            }).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == "/api/upload":
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length)
            
            upload_dir = os.path.dirname(os.path.abspath(__file__))
            save_path = os.path.join(upload_dir, "firmware.bin")
            
            boundary = self.headers.get("Content-Type").split("boundary=")[-1].encode()
            parts = body.split(b"--" + boundary)
            file_data = b""
            for part in parts:
                if b'filename="' in part:
                    sub_parts = part.split(b"\r\n\r\n", 1)
                    if len(sub_parts) == 2:
                        file_data = sub_parts[1].rsplit(b"\r\n", 1)[0]
                        break

            if not file_data:
                file_data = body

            with open(save_path, "wb") as f:
                f.write(file_data)

            md5_hash = hashlib.md5(file_data).hexdigest()
            APP_STATE["firmware_path"] = save_path
            APP_STATE["firmware_filename"] = "firmware.bin"
            APP_STATE["firmware_size"] = len(file_data)
            APP_STATE["firmware_md5"] = md5_hash

            log_message(f"📁 Đã nhận file firmware tải lên: {len(file_data)} bytes | MD5: {md5_hash}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "success": True,
                "md5": md5_hash,
                "size": len(file_data),
                "url": f"http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin"
            }).encode("utf-8"))

        elif parsed.path == "/api/send_raw":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length)
            payload = json.loads(post_data.decode("utf-8"))
            topic = payload.get("topic", TOPIC_COMMAND)
            cmd_data = payload.get("payload", "")

            if not APP_STATE.get("mqtt_connected", False) or mqtt_client is None:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": "Chưa kết nối tới MQTT Broker!"}).encode("utf-8"))
                return

            mqtt_client.publish(topic, cmd_data, qos=1)
            log_message(f"📤 [BẮN COMMAND] Topic: '{topic}' | Data: {cmd_data}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"success": True}).encode("utf-8"))

        elif parsed.path == "/api/send_ota":
            content_length = int(self.headers.get("Content-Length", 0))
            post_data = self.rfile.read(content_length)
            payload = json.loads(post_data.decode("utf-8"))

            if not APP_STATE.get("firmware_md5"):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": "Vui lòng chọn file Firmware trước khi nạp!"}).encode("utf-8"))
                return

            if not APP_STATE.get("mqtt_connected", False) or mqtt_client is None:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": "MQTT chưa kết nối!"}).encode("utf-8"))
                return

            ota_msg = {
                "action": "ota",
                "version": payload.get("version", "2.0.0"),
                "target": payload.get("target", "esp32s3_cabinet"),
                "url": payload.get("url", f"http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin"),
                "md5": APP_STATE["firmware_md5"],
                "size": APP_STATE["firmware_size"],
                "filename": APP_STATE["firmware_filename"]
            }

            ota_payload_str = json.dumps(ota_msg, indent=2)
            mqtt_client.publish(TOPIC_OTA, ota_payload_str, qos=1)
            log_message(f"📤 [BẮN LỆNH OTA] Topic: '{TOPIC_OTA}' \n{ota_payload_str}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"success": True}).encode("utf-8"))

    def log_message(self, format, *args):
        pass

def run_web_server():
    server = HTTPServer(("0.0.0.0", WEB_UI_PORT), WebUIHandler)
    server.serve_forever()

if __name__ == "__main__":
    print("=" * 60)
    print("      PUMP_MY_FAMILY - CONTROL & OTA TEST HUB")
    print("=" * 60)
    print(f"• Địa chỉ IP mạng LAN: {LOCAL_IP}")
    print(f"• Giao diện Web UI   : http://localhost:{WEB_UI_PORT}")
    print(f"• File Server OTA     : http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin")
    print(f"• MQTT Broker         : {MQTT_BROKER}:{MQTT_PORT} (SSL/TLS)")
    print("=" * 60)

    t_file = threading.Thread(target=run_file_server, daemon=True)
    t_file.start()
    log_message(f"🚀 Firmware HTTP Server đang chạy tại: http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin")

    t_mqtt = threading.Thread(target=run_mqtt, daemon=True)
    t_mqtt.start()

    t_web = threading.Thread(target=run_web_server, daemon=True)
    t_web.start()
    log_message(f"🌐 Giao diện Web đã mở tại: http://localhost:{WEB_UI_PORT} (hoặc http://{LOCAL_IP}:{WEB_UI_PORT})")

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nĐang dừng chương trình...")
        sys.exit(0)
