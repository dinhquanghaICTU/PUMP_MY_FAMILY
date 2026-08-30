#!/usr/bin/env python3
"""
OTA Firmware Test Tool & MQTT Command Hub - PUMP_MY_FAMILY
- Giao diện Web trực quan (http://localhost:5050)
- Tích hợp sẵn máy chủ HTTP File Server nội bộ để ESP32 tải trực tiếp file .bin
- Ô bắn lệnh tùy biến (Custom Command / Quick Buttons) vào topic `pump/family/command`
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
    "version": "1.0.1",
    "target": "esp32s3_cabinet",
    "mqtt_connected": False,
    "logs": []
}

def log_message(msg):
    timestamp = time.strftime("%H:%M:%S")
    entry = f"[{timestamp}] {msg}"
    print(entry)
    APP_STATE["logs"].append(entry)
    if len(APP_STATE["logs"]) > 100:
        APP_STATE["logs"].pop(0)

def get_local_ip():
    try:
        import subprocess
        ips = subprocess.getoutput("hostname -I").strip().split()
        # 1. Ưu tiên dải mạng Hotspot QuangHa (192.168.12.x)
        for ip in ips:
            if ip.startswith("192.168.12."):
                return ip
        # 2. Ưu tiên các dải LAN nội bộ 192.168.x.x
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
                bytes_sent = 0
                while chunk := f.read(4096):
                    self.wfile.write(chunk)
                    bytes_sent += len(chunk)
            
            log_message(f"✅ [FILE SERVER] Đã truyền xong {bytes_sent}/{file_size} bytes cho ESP32!")
        except Exception as e:
            log_message(f"⚠️ [FILE SERVER] Lỗi truyền file: {e}")

    def log_message(self, format, *args):
        pass

def start_firmware_server():
    server = HTTPServer(("0.0.0.0", FILE_SERVER_PORT), FirmwareFileHandler)
    log_message(f"🚀 Firmware HTTP Server đang chạy tại: http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin")
    server.serve_forever()

# ==================== KẾT NỐI MQTT HIVEMQ ====================
mqtt_client = None

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        APP_STATE["mqtt_connected"] = True
        log_message("🟢 Đã kết nối thành công tới HiveMQ Cloud (Port 8883 SSL/TLS)!")
        client.subscribe(TOPIC_STATUS)
        client.subscribe(TOPIC_OTA)
        client.subscribe(TOPIC_COMMAND)
        log_message(f"📥 Đã lắng nghe các topic: '{TOPIC_STATUS}', '{TOPIC_OTA}', '{TOPIC_COMMAND}'")
    else:
        APP_STATE["mqtt_connected"] = False
        log_message(f"🔴 Kết nối MQTT thất bại! Mã lỗi: {rc}")

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode('utf-8')
        log_message(f"📩 [MQTT NHẬN] Topic: {msg.topic} | Payload: {payload_str}")
    except Exception:
        log_message(f"📩 [MQTT NHẬN] Topic: {msg.topic} | Raw: {msg.payload}")

def start_mqtt():
    global mqtt_client
    try:
        try:
            mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"OTA_Tester_{int(time.time())}")
        except Exception:
            mqtt_client = mqtt.Client(client_id=f"OTA_Tester_{int(time.time())}")

        mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)
        mqtt_client.tls_set(cert_reqs=ssl.CERT_NONE)
        mqtt_client.tls_insecure_set(True)

        mqtt_client.on_connect = on_connect
        mqtt_client.on_message = on_message

        log_message(f"Đang kết nối tới MQTT Broker: {MQTT_BROKER}:{MQTT_PORT}...")
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_forever()
    except Exception as e:
        log_message(f"Lỗi khởi động MQTT: {e}")

# ==================== GIAO DIỆN WEB HTML ====================
HTML_PAGE = """<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Control & OTA Testing Hub</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background: #0f172a; color: #f8fafc; padding: 20px; display: flex; justify-content: center; }
        .container { width: 100%; max-width: 950px; display: flex; flex-direction: column; gap: 20px; }
        .card { background: #1e293b; border-radius: 12px; padding: 24px; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.5); border: 1px solid #334155; }
        h1 { font-size: 22px; color: #38bdf8; margin-bottom: 6px; display: flex; align-items: center; gap: 10px; }
        h2 { font-size: 16px; color: #94a3b8; margin-bottom: 14px; border-bottom: 1px solid #334155; padding-bottom: 6px; }
        .subtitle { color: #64748b; font-size: 13px; margin-bottom: 15px; }
        .badge { display: inline-block; padding: 4px 10px; border-radius: 20px; font-size: 12px; font-weight: bold; }
        .badge-success { background: #065f46; color: #34d399; }
        .badge-danger { background: #881337; color: #fb7185; }
        .badge-info { background: #0369a1; color: #38bdf8; }
        .form-group { margin-bottom: 14px; }
        label { display: block; font-size: 13px; color: #cbd5e1; margin-bottom: 5px; font-weight: 500; }
        input[type="text"], textarea, select {
            width: 100%; padding: 9px 12px; background: #0f172a; border: 1px solid #475569;
            border-radius: 8px; color: #f8fafc; font-size: 13px; outline: none; transition: border-color 0.2s; font-family: inherit;
        }
        input[type="text"]:focus, textarea:focus, select:focus { border-color: #38bdf8; }
        .btn {
            background: #0284c7; color: white; border: none; padding: 10px 18px; border-radius: 8px;
            font-size: 14px; font-weight: 600; cursor: pointer; transition: all 0.2s; display: inline-flex;
            align-items: center; justify-content: center; gap: 6px;
        }
        .btn:hover { background: #0369a1; transform: translateY(-1px); }
        .btn-success { background: #059669; }
        .btn-success:hover { background: #047857; }
        .btn-danger { background: #dc2626; }
        .btn-danger:hover { background: #b91c1c; }
        .btn-warning { background: #d97706; }
        .btn-warning:hover { background: #b45309; }
        .btn-secondary { background: #334155; color: #cbd5e1; }
        .btn-secondary:hover { background: #475569; }
        .quick-actions { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 12px; }
        .quick-btn { padding: 6px 12px; font-size: 12px; border-radius: 6px; background: #334155; color: #f8fafc; border: 1px solid #475569; cursor: pointer; }
        .quick-btn:hover { background: #0284c7; border-color: #38bdf8; }
        .info-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 10px; margin-top: 10px; background: #0f172a; padding: 10px; border-radius: 8px; }
        .info-item { font-size: 12px; }
        .info-label { color: #64748b; }
        .info-val { color: #38bdf8; font-weight: 600; word-break: break-all; }
        .log-box {
            background: #090d16; border: 1px solid #1e293b; border-radius: 8px; padding: 12px;
            height: 240px; overflow-y: auto; font-family: "Fira Code", monospace; font-size: 12px; color: #a5f3fc;
            line-height: 1.5; white-space: pre-wrap;
        }
        .row { display: flex; gap: 12px; }
        .col { flex: 1; }
    </style>
</head>
<body>
    <div class="container">
        <!-- HEADER -->
        <div class="card" style="padding: 16px 24px;">
            <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px;">
                <div>
                    <h1>⚡ PUMP_MY_FAMILY - Test Hub</h1>
                    <p class="subtitle" style="margin-bottom: 0;">Bắn lệnh điều khiển & Kiểm thử cập nhật OTA Firmware qua HiveMQ Cloud</p>
                </div>
                <div style="display: flex; gap: 10px;">
                    <span id="mqtt-status" class="badge badge-danger">MQTT: Đang kết nối...</span>
                    <span class="badge badge-info">HTTP Server: http://__LOCAL_IP__:__HTTP_PORT__</span>
                </div>
            </div>
        </div>

        <!-- KHỐI 1: BẮN LỆNH TEST COMMAND -->
        <div class="card">
            <h2>🎮 Bắn Lệnh Điều Khiển / Test Command (MQTT)</h2>
            
            <div class="quick-actions">
                <span style="font-size: 12px; color: #94a3b8; display: flex; align-items: center; margin-right: 4px;">Nút nhanh:</span>
                <button class="quick-btn" onclick="setCmd('{\\"pump\\":1,\\"action\\":\\"on\\"}')">🟢 Bật Bơm 1</button>
                <button class="quick-btn" onclick="setCmd('{\\"pump\\":0,\\"action\\":\\"off\\"}')">🔴 Tắt Bơm 1</button>
                <button class="quick-btn" onclick="setCmd('{\\"mode\\":\\"auto\\"}')">⚙️ Chế độ Auto</button>
                <button class="quick-btn" onclick="setCmd('{\\"mode\\":\\"manual\\"}')">🖐️ Chế độ Manual</button>
                <button class="quick-btn" onclick="setCmd('{\\"child_lock\\":1}')">🔒 Bật Khóa Trẻ Em</button>
                <button class="quick-btn" onclick="setCmd('{\\"child_lock\\":0}')">🔓 Tắt Khóa Trẻ Em</button>
                <button class="quick-btn" onclick="setCmd('{\\"action\\":\\"get_status\\"}')">📊 Lấy Trạng Thái</button>
            </div>

            <form onsubmit="sendCommand(event)">
                <div class="row">
                    <div class="col" style="flex: 0 0 35%;">
                        <div class="form-group">
                            <label>Topic MQTT:</label>
                            <input type="text" id="cmdTopic" value="pump/family/command">
                        </div>
                    </div>
                    <div class="col">
                        <div class="form-group">
                            <label>Nội dung Payload (JSON hoặc Text):</label>
                            <input type="text" id="cmdPayload" value='{"pump":1,"action":"on"}' placeholder='VD: {"pump":1}'>
                        </div>
                    </div>
                </div>

                <button type="submit" class="btn btn-success" style="width: 100%;">
                    🚀 BẮN LỆNH ĐIỀU KHIỂN QUA MQTT
                </button>
            </form>
        </div>

        <!-- KHỐI 2: OTA FIRMWARE -->
        <div class="card">
            <h2>🚀 Nâng Cấp Firmware Từ Xa (OTA)</h2>
            <form id="otaForm" onsubmit="sendOTA(event)">
                <div class="form-group">
                    <label>📁 Chọn file Firmware (.bin):</label>
                    <input type="file" id="fileInput" accept=".bin" onchange="handleFileSelect(event)">
                </div>

                <div class="info-grid">
                    <div class="info-item"><div class="info-label">Tên File:</div><div id="info-name" class="info-val">Chưa chọn</div></div>
                    <div class="info-item"><div class="info-label">Dung lượng:</div><div id="info-size" class="info-val">0 KB</div></div>
                    <div class="info-item"><div class="info-label">MD5 Checksum:</div><div id="info-md5" class="info-val">Chưa tính</div></div>
                </div>

                <div class="row" style="margin-top: 14px;">
                    <div class="col form-group">
                        <label>🏷️ Version mới:</label>
                        <input type="text" id="versionInput" value="2.0.0">
                    </div>
                    <div class="col form-group">
                        <label>🎯 Thiết bị nhận:</label>
                        <select id="targetInput">
                            <option value="esp32s3_cabinet">Con Tủ Điện (ESP32-S3 Master)</option>
                            <option value="node_tank">Con Bể Nước (ESP32 Node / ESP-NOW)</option>
                            <option value="all">Tất cả (Broadcast)</option>
                        </select>
                    </div>
                </div>

                <div class="form-group">
                    <label>🌐 URL Tải Firmware:</label>
                    <input type="text" id="urlInput" value="http://__LOCAL_IP__:__HTTP_PORT__/firmware.bin">
                </div>

                <button type="submit" class="btn" style="width: 100%;">
                    ⚡ BẮN LỆNH OTA QUA MQTT
                </button>
            </form>
        </div>

        <!-- KHỐI 3: NHẬT KÝ LOG -->
        <div class="card">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                <h2>📋 Nhật Ký Log MQTT & Trạng Thái</h2>
                <button class="btn btn-secondary" onclick="fetchLogs()" style="padding: 4px 12px; font-size: 12px;">Làm mới</button>
            </div>
            <div id="logBox" class="log-box">Đang tải logs...</div>
        </div>
    </div>

    <script>
        let currentFile = null;

        function setCmd(payload) {
            document.getElementById('cmdPayload').value = payload;
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
                if (data.success) {
                    // Flash success effect
                } else {
                    alert("❌ Lỗi: " + data.error);
                }
            });
        }

        function handleFileSelect(event) {
            const file = event.target.files[0];
            if (!file) return;
            currentFile = file;

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
                }
            })
            .catch(err => alert("Lỗi tải file lên: " + err));
        }

        function sendOTA(e) {
            e.preventDefault();
            const version = document.getElementById('versionInput').value;
            const target = document.getElementById('targetInput').value;
            const url = document.getElementById('urlInput').value;

            const payload = {
                action: "ota",
                version: version,
                target: target,
                url: url,
                md5: document.getElementById('info-md5').innerText,
                size: currentFile ? currentFile.size : 0,
                filename: currentFile ? currentFile.name : "firmware.bin"
            };

            fetch('/api/send_ota', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            })
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    alert("✅ ĐÃ GỬI LỆNH OTA QUA MQTT THÀNH CÔNG!");
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
                        statusBadge.innerText = 'MQTT: ĐÃ KẾT NỐI (8883 SSL)';
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

            log_message(f"📁 Đã nhận file firmware: {len(file_data)} bytes | MD5: {md5_hash}")

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

            if not APP_STATE.get("mqtt_connected", False) or mqtt_client is None:
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": "Chưa kết nối tới MQTT Broker!"}).encode("utf-8"))
                return

            payload_json = json.dumps(payload, indent=2)
            mqtt_client.publish(TOPIC_OTA, payload_json, qos=1)
            log_message(f"📤 [BẮN LỆNH OTA] Topic: '{TOPIC_OTA}' \n{payload_json}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"success": True}).encode("utf-8"))

    def log_message(self, format, *args):
        pass

def start_web_ui():
    server = HTTPServer(("0.0.0.0", WEB_UI_PORT), WebUIHandler)
    log_message(f"🌐 Giao diện Web đã mở tại: http://localhost:{WEB_UI_PORT} (hoặc http://{LOCAL_IP}:{WEB_UI_PORT})")
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

    t_file = threading.Thread(target=start_firmware_server, daemon=True)
    t_file.start()

    t_mqtt = threading.Thread(target=start_mqtt, daemon=True)
    t_mqtt.start()

    time.sleep(1)
    try:
        webbrowser.open(f"http://localhost:{WEB_UI_PORT}")
    except Exception:
        pass

    try:
        start_web_ui()
    except KeyboardInterrupt:
        print("\nĐang dừng chương trình...")
