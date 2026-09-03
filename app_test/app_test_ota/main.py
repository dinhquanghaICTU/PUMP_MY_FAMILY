#!/usr/bin/env python3
"""
PUMP_MY_FAMILY - SMART WATER PUMP DASHBOARD & OTA HUB
- Giao diện Web trực quan thời gian thực (http://localhost:5050)
- Giám sát mức nước (% và cm), thanh đo mực nước đồ họa sống động
- Điều khiển Bật / Tắt máy bơm, chuyển chế độ Auto / Manual, Khóa trẻ em
- Cấu hình kích thước bồn nước (Chiều cao, Offset cảm biến, Ngưỡng bật/tắt)
- Nạp OTA Dual-Node: Tủ Điện (ESP32-S3) & Bể Nước (ESP32-U qua ESP-NOW)
- Kết nối MQTT HiveMQ Cloud SSL/TLS (Port 8883)
"""

import os
import sys
import json
import time
import socket
import ssl
import hashlib
import threading
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

# ==================== TRẠNG THÁI HỆ THỐNG ====================
APP_STATE = {
    "firmware_path": "",
    "firmware_filename": "",
    "firmware_size": 0,
    "firmware_md5": "",
    "version": "2.0.0",
    "target": "esp32s3_cabinet",
    "mqtt_connected": False,
    "telemetry": {
        "pump": 0,
        "mode": "auto",
        "water_percent": 0.0,
        "distance_cm": 0.0,
        "battery": 0.0,
        "runtime": 0,
        "child_lock": 0,
        "state": "IDLE",
        "last_update": 0
    },
    "logs": []
}

def log_message(msg):
    timestamp = time.strftime("%H:%M:%S")
    entry = f"[{timestamp}] {msg}"
    print(entry)
    APP_STATE["logs"].append(entry)
    if len(APP_STATE["logs"]) > 200:
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

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
CABINET_BIN = os.path.join(PROJECT_ROOT, "my_esp32s3_app/build/main.bin")
TANK_BIN = os.path.join(PROJECT_ROOT, "my_esp32_sensor_node/build/sensor_node.bin")

# ==================== HTTP FILE SERVER PHỤC VỤ FIRMWARE ====================
class FirmwareFileHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        log_message(f"📡 [FILE SERVER] ESP32 yêu cầu tải file: {self.path} từ IP: {self.client_address[0]}")
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

# ==================== MQTT CLIENT ====================
mqtt_client = None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        APP_STATE["mqtt_connected"] = True
        log_message(f"🟢 Đã kết nối HiveMQ Cloud (Port {MQTT_PORT} SSL/TLS)!")
        client.subscribe(TOPIC_STATUS)
        client.subscribe(TOPIC_OTA)
        client.subscribe(TOPIC_COMMAND)
    else:
        APP_STATE["mqtt_connected"] = False
        log_message(f"🔴 Kết nối MQTT thất bại! Code: {rc}")

def on_message(client, userdata, msg):
    payload_str = msg.payload.decode("utf-8", errors="ignore")
    log_message(f"📩 [MQTT NHẬN] Topic: {msg.topic} | {payload_str}")
    
    if msg.topic == TOPIC_STATUS:
        try:
            data = json.loads(payload_str)
            if "pump" in data:
                APP_STATE["telemetry"]["pump"] = data.get("pump", 0)
                APP_STATE["telemetry"]["mode"] = data.get("mode", "auto")
                APP_STATE["telemetry"]["water_percent"] = data.get("water_percent", 0.0)
                APP_STATE["telemetry"]["distance_cm"] = data.get("distance_cm", 0.0)
                APP_STATE["telemetry"]["battery"] = data.get("battery", 0.0)
                APP_STATE["telemetry"]["runtime"] = data.get("runtime", 0)
                APP_STATE["telemetry"]["child_lock"] = data.get("child_lock", 0)
                APP_STATE["telemetry"]["state"] = data.get("state", "IDLE")
                APP_STATE["telemetry"]["last_update"] = int(time.time())
        except Exception:
            pass

def run_mqtt():
    global mqtt_client
    mqtt_client = mqtt.Client(client_id="Python_Dashboard_Hub_" + str(int(time.time())))
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

# ==================== GIAO DIỆN WEB DASHBOARD ====================
HTML_PAGE = """<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PUMP_MY_FAMILY - DASHBOARD & OTA HUB</title>
    <style>
        :root {
            --bg-color: #0b132b;
            --card-bg: #1c2541;
            --card-border: #3a506b;
            --accent: #00f0ff;
            --accent-glow: rgba(0, 240, 255, 0.4);
            --water-color: #00b4d8;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --success: #10b981;
            --danger: #ef4444;
            --warning: #f59e0b;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Segoe UI', system-ui, -apple-system, sans-serif; }
        body { background-color: var(--bg-color); color: var(--text-main); padding: 20px; line-height: 1.5; }
        .container { max-width: 1200px; margin: 0 auto; }
        
        header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 24px; padding-bottom: 14px; border-bottom: 1px solid var(--card-border); }
        h1 { font-size: 24px; color: var(--accent); display: flex; align-items: center; gap: 10px; text-shadow: 0 0 10px var(--accent-glow); }
        
        .badge { padding: 6px 14px; border-radius: 9999px; font-size: 13px; font-weight: bold; display: inline-flex; align-items: center; gap: 6px; }
        .badge-success { background: rgba(16, 185, 129, 0.2); color: #34d399; border: 1px solid #10b981; }
        .badge-danger { background: rgba(239, 68, 68, 0.2); color: #f87171; border: 1px solid #ef4444; }
        
        .grid-main { display: grid; grid-template-columns: 360px 1fr; gap: 20px; margin-bottom: 20px; }
        @media (max-width: 900px) { .grid-main { grid-template-columns: 1fr; } }
        
        .card { background-color: var(--card-bg); border-radius: 16px; padding: 20px; border: 1px solid var(--card-border); box-shadow: 0 8px 16px rgba(0,0,0,0.2); margin-bottom: 20px; }
        .card h2 { font-size: 17px; margin-bottom: 16px; color: var(--text-main); display: flex; align-items: center; gap: 8px; border-left: 4px solid var(--accent); padding-left: 10px; }
        
        /* THANH ĐO BỒN NƯỚC ĐỒ HỌA */
        .tank-container { display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 10px 0; }
        .tank-glass { width: 180px; height: 260px; border: 4px solid #64748b; border-radius: 16px; position: relative; overflow: hidden; background: rgba(15, 23, 42, 0.8); box-shadow: inset 0 0 20px rgba(0,0,0,0.6); }
        .tank-water { position: absolute; bottom: 0; left: 0; right: 0; height: 50%; background: linear-gradient(180deg, #38bdf8 0%, #0284c7 100%); transition: height 0.8s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: 0 0 20px rgba(56, 189, 248, 0.5); }
        .tank-water::after { content: ''; position: absolute; top: -6px; left: 0; right: 0; height: 12px; background: rgba(255,255,255,0.4); border-radius: 50%; }
        .tank-percent-label { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 32px; font-weight: 800; color: #fff; text-shadow: 0 2px 8px rgba(0,0,0,0.8); z-index: 10; }
        .tank-stats { width: 100%; display: flex; justify-content: space-around; margin-top: 16px; font-size: 14px; text-align: center; }
        .stat-box { background: rgba(15, 23, 42, 0.6); padding: 8px 14px; border-radius: 8px; border: 1px solid var(--card-border); flex: 1; margin: 0 4px; }
        .stat-box .val { font-size: 18px; font-weight: bold; color: var(--accent); margin-top: 2px; }
        
        /* BẢNG ĐIỀU KHIỂN & TRẠNG THÁI BƠM */
        .pump-status-card { display: flex; align-items: center; justify-content: space-between; padding: 18px; border-radius: 12px; margin-bottom: 16px; background: #0f172a; border: 1px solid var(--card-border); }
        .pump-indicator { display: flex; align-items: center; gap: 14px; }
        .pump-icon { width: 48px; height: 48px; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 24px; background: #334155; }
        .pump-on { background: linear-gradient(135deg, #10b981, #059669); box-shadow: 0 0 15px rgba(16, 185, 129, 0.6); animation: pulse 1.5s infinite; }
        .pump-off { background: #334155; color: #94a3b8; }
        
        @keyframes pulse {
            0% { transform: scale(1); box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7); }
            70% { transform: scale(1.05); box-shadow: 0 0 0 12px rgba(16, 185, 129, 0); }
            100% { transform: scale(1); }
        }
        
        .control-btns { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; margin-bottom: 16px; }
        button { cursor: pointer; border: none; border-radius: 10px; padding: 12px 16px; font-weight: bold; font-size: 14px; transition: all 0.2s ease; display: inline-flex; align-items: center; justify-content: center; gap: 8px; }
        button:hover { opacity: 0.9; transform: translateY(-2px); }
        button:active { transform: translateY(0); }
        
        .btn-success { background: linear-gradient(135deg, #10b981, #059669); color: white; }
        .btn-danger { background: linear-gradient(135deg, #ef4444, #dc2626); color: white; }
        .btn-primary { background: linear-gradient(135deg, #0ea5e9, #0284c7); color: white; }
        .btn-warning { background: linear-gradient(135deg, #f59e0b, #d97706); color: white; }
        .btn-purple { background: linear-gradient(135deg, #8b5cf6, #6d28d9); color: white; }
        .btn-dark { background: #334155; color: #f8fafc; border: 1px solid var(--card-border); }
        
        .row { display: flex; gap: 12px; margin-bottom: 12px; flex-wrap: wrap; }
        .col { flex: 1; min-width: 140px; }
        input, select { width: 100%; padding: 10px 14px; border-radius: 8px; border: 1px solid var(--card-border); background: #0f172a; color: white; font-size: 14px; }
        label { display: block; font-size: 12px; color: var(--text-muted); margin-bottom: 4px; }
        
        .log-box { background-color: #020617; border-radius: 10px; padding: 14px; font-family: 'Consolas', monospace; font-size: 12px; height: 180px; overflow-y: auto; color: #a5f3fc; border: 1px solid var(--card-border); }
        .log-box div { margin-bottom: 4px; line-height: 1.4; word-break: break-all; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>⚡ PUMP_MY_FAMILY - TRUNG TÂM ĐIỀU KHIỂN & TEST</h1>
            <div id="mqtt-status" class="badge badge-danger">🔴 MQTT: Đang kết nối...</div>
        </header>

        <div class="grid-main">
            <!-- CỘT 1: THANH ĐO MỰC NƯỚC THỜI GIAN THỰC -->
            <div class="card">
                <h2>🌊 Mực Nước Bể Nước</h2>
                <div class="tank-container">
                    <div class="tank-glass">
                        <div id="tank-water" class="tank-water" style="height: 0%;"></div>
                        <div id="tank-pct" class="tank-percent-label">-- %</div>
                    </div>
                    <div class="tank-stats">
                        <div class="stat-box">
                            <div style="color:var(--text-muted);">Khoảng cách</div>
                            <div id="tank-dist" class="val">-- cm</div>
                        </div>
                        <div class="stat-box">
                            <div style="color:var(--text-muted);">Pin Node</div>
                            <div id="tank-batt" class="val">-- V</div>
                        </div>
                    </div>
                </div>
            </div>

            <!-- CỘT 2: BẢNG ĐIỀU KHIỂN MÁY BƠM THÔNG MINH -->
            <div>
                <div class="card">
                    <h2>🎛️ Bảng Điều Khiển Máy Bơm</h2>
                    
                    <div class="pump-status-card">
                        <div class="pump-indicator">
                            <div id="pump-icon" class="pump-icon pump-off">⚙️</div>
                            <div>
                                <div style="font-size: 18px; font-weight: bold;" id="pump-status-text">MÁY BƠM: TẮT</div>
                                <div style="font-size: 13px; color: var(--text-muted);" id="pump-state-detail">Trạng thái: IDLE | Thời gian chạy: 0s</div>
                            </div>
                        </div>
                        <div>
                            <span id="mode-badge" class="badge" style="background:#0369a1; color:#e0f2fe;">CHẾ ĐỘ: TỰ ĐỘNG</span>
                        </div>
                    </div>

                    <div class="control-btns">
                        <button class="btn-success" onclick="sendCommand({'action': 'on'})">⚡ BẬT BƠM</button>
                        <button class="btn-danger" onclick="sendCommand({'action': 'off'})">🛑 TẮT BƠM</button>
                        <button class="btn-primary" onclick="sendCommand({'mode': 'auto'})">🤖 CHẾ ĐỘ AUTO</button>
                        <button class="btn-dark" onclick="sendCommand({'mode': 'manual'})">✋ CHẾ ĐỘ MANUAL</button>
                        <button class="btn-warning" onclick="toggleChildLock()">🔒 KHÓA TRẺ EM</button>
                        <button class="btn-purple" onclick="sendCommand({'action': 'clear_error'})">🔄 XÓA LỖI</button>
                    </div>

                    <!-- CÀI ĐẶT THAM SỐ BỂ -->
                    <div style="margin-top: 16px; padding-top: 14px; border-top: 1px dashed var(--card-border);">
                        <div style="font-size: 14px; font-weight: bold; margin-bottom: 10px; color: var(--accent);">⚙️ Cấu Hình Thông Số Bể (Cm & %):</div>
                        <div class="row">
                            <div class="col">
                                <label>Chiều cao bể (cm):</label>
                                <input type="number" id="cfg-tank-height" value="120">
                            </div>
                            <div class="col">
                                <label>Offset nắp/vùng mù (cm):</label>
                                <input type="number" id="cfg-offset" value="25">
                            </div>
                            <div class="col">
                                <label>Bật khi cạn dưới (%):</label>
                                <input type="number" id="cfg-min-pct" value="25">
                            </div>
                            <div class="col">
                                <label>Tắt khi đầy trên (%):</label>
                                <input type="number" id="cfg-max-pct" value="95">
                            </div>
                        </div>
                        <button class="btn-dark" style="width: 100%;" onclick="saveTankConfig()">💾 Lưu Cấu Hình Tham Số Bể Lên ESP32</button>
                    </div>
                </div>
            </div>
        </div>

        <!-- KHU VỰC OTA FIRMWARE DUAL-NODE -->
        <div class="card">
            <h2>🚀 Nạp Firmware Từ Xa (OTA Dual-Node)</h2>
            <div style="margin-bottom: 12px; display: flex; gap: 10px; flex-wrap: wrap;">
                <button class="btn-primary" onclick="selectPreset('cabinet')">🏠 1-CHẠM: Chọn Firmware Tủ Điện (main.bin)</button>
                <button class="btn-purple" onclick="selectPreset('tank')">🌊 1-CHẠM: Chọn Firmware Bể Nước (sensor_node.bin)</button>
            </div>
            
            <div class="row">
                <div class="col">
                    <label>Mục tiêu nạp (Target):</label>
                    <select id="ota-target">
                        <option value="esp32s3_cabinet">Tủ Điện Master (ESP32-S3)</option>
                        <option value="esp32_tank">Bể Nước (ESP32-U qua ESP-NOW)</option>
                    </select>
                </div>
                <div class="col">
                    <label>Phiên bản (Version):</label>
                    <input type="text" id="ota-version" value="2.0.0">
                </div>
            </div>
            
            <div id="ota-file-info" style="background:#0f172a; padding:10px 14px; border-radius:8px; margin-bottom:12px; font-size:13px; border:1px solid var(--card-border);">
                Chưa chọn file firmware...
            </div>

            <button class="btn-warning" style="width:100%; font-size:15px; padding:14px;" onclick="startOTA()">⚡ BẮN LỆNH OTA QUA MQTT NGAY BÂY GIỜ</button>
        </div>

        <!-- NHẬT KÝ HOẠT ĐỘNG MQTT -->
        <div class="card">
            <h2>📋 Nhật Ký Log MQTT Trực Tuyến</h2>
            <div id="log-box" class="log-box"></div>
        </div>
    </div>

    <script>
        let currentChildLock = 0;

        function updateUI() {
            fetch('/api/status')
                .then(r => r.json())
                .then(data => {
                    // Cập nhật trạng thái MQTT
                    const mqttBadge = document.getElementById('mqtt-status');
                    if (data.mqtt_connected) {
                        mqttBadge.className = 'badge badge-success';
                        mqttBadge.innerText = '🟢 MQTT: Đã kết nối HiveMQ Cloud';
                    } else {
                        mqttBadge.className = 'badge badge-danger';
                        mqttBadge.innerText = '🔴 MQTT: Mất kết nối!';
                    }

                    // Cập nhật Telemetry
                    const t = data.telemetry || {};
                    const pct = Math.max(0, Math.min(100, t.water_percent || 0));
                    
                    document.getElementById('tank-water').style.height = pct + '%';
                    document.getElementById('tank-pct').innerText = (t.water_percent >= 0 ? t.water_percent.toFixed(1) + '%' : 'MẤT SÓNG');
                    document.getElementById('tank-dist').innerText = (t.distance_cm > 0 ? t.distance_cm.toFixed(1) + ' cm' : '-- cm');
                    document.getElementById('tank-batt').innerText = (t.battery > 0 ? t.battery.toFixed(2) + ' V' : '-- V');

                    // Cập nhật máy bơm
                    const isPumpOn = (t.pump === 1);
                    const pumpIcon = document.getElementById('pump-icon');
                    const pumpText = document.getElementById('pump-status-text');
                    const pumpDetail = document.getElementById('pump-state-detail');

                    if (isPumpOn) {
                        pumpIcon.className = 'pump-icon pump-on';
                        pumpIcon.innerText = '💧';
                        pumpText.innerText = 'MÁY BƠM: ĐANG BẬT';
                        pumpText.style.color = '#34d399';
                    } else {
                        pumpIcon.className = 'pump-icon pump-off';
                        pumpIcon.innerText = '⚙️';
                        pumpText.innerText = 'MÁY BƠM: TẮT';
                        pumpText.style.color = '#f8fafc';
                    }

                    pumpDetail.innerText = `Trạng thái: ${t.state || 'IDLE'} | Thời gian chạy: ${t.runtime || 0}s`;

                    // Chế độ
                    const modeBadge = document.getElementById('mode-badge');
                    if (t.mode === 'auto') {
                        modeBadge.innerText = 'CHẾ ĐỘ: TỰ ĐỘNG (AUTO)';
                        modeBadge.style.background = '#0369a1';
                    } else {
                        modeBadge.innerText = 'CHẾ ĐỘ: THỦ CÔNG (MANUAL)';
                        modeBadge.style.background = '#475569';
                    }

                    currentChildLock = t.child_lock || 0;

                    // Log Box
                    const logBox = document.getElementById('log-box');
                    logBox.innerHTML = (data.logs || []).map(l => `<div>${l}</div>`).join('');
                    logBox.scrollTop = logBox.scrollHeight;
                })
                .catch(err => console.error(err));
        }

        function sendCommand(payload) {
            fetch('/api/send_raw', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({topic: 'pump/family/command', payload: JSON.stringify(payload)})
            });
        }

        function toggleChildLock() {
            const next = currentChildLock ? 0 : 1;
            sendCommand({'child_lock': next});
        }

        function saveTankConfig() {
            const cfg = {
                tank_height: parseFloat(document.getElementById('cfg-tank-height').value),
                offset: parseFloat(document.getElementById('cfg-offset').value),
                min_pct: parseInt(document.getElementById('cfg-min-pct').value),
                max_pct: parseInt(document.getElementById('cfg-max-pct').value)
            };
            sendCommand({'config': cfg});
            alert('Đã gửi thông số cấu hình bể lên ESP32!');
        }

        function selectPreset(preset) {
            fetch(`/api/preset?type=${preset}`)
                .then(r => r.json())
                .then(d => {
                    if (d.success) {
                        document.getElementById('ota-target').value = d.target;
                        document.getElementById('ota-file-info').innerHTML = `
                            <strong>📁 File đã chọn:</strong> ${d.filename} (${(d.size/1024).toFixed(1)} KB)<br>
                            <strong>🔑 MD5:</strong> ${d.md5}<br>
                            <strong>🎯 Mục tiêu:</strong> ${d.target === 'esp32s3_cabinet' ? 'Tủ Điện (ESP32-S3)' : 'Bể Nước (ESP32-U)'}
                        `;
                    } else {
                        alert(d.error);
                    }
                });
        }

        function startOTA() {
            const target = document.getElementById('ota-target').value;
            const version = document.getElementById('ota-version').value;
            fetch('/api/send_ota', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({target: target, version: version})
            }).then(r => r.json()).then(d => {
                if (!d.success) alert(d.error);
            });
        }

        setInterval(updateUI, 1000);
        updateUI();
    </script>
</body>
</html>
"""

# ==================== WEB API SERVER ====================
class WebUIHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path in ["/", "/index.html"]:
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode("utf-8"))

        elif parsed.path == "/api/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(APP_STATE).encode("utf-8"))

        elif parsed.path == "/api/preset":
            params = parse_qs(parsed.query)
            preset_type = params.get("type", [""])[0]

            if preset_type == "cabinet":
                file_path = CABINET_BIN
                target_name = "esp32s3_cabinet"
            elif preset_type == "tank":
                file_path = TANK_BIN
                target_name = "esp32_tank"
            else:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(json.dumps({"success": False, "error": "Sai preset"}).encode("utf-8"))
                return

            if not os.path.exists(file_path):
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({
                    "success": False,
                    "error": f"Chưa tìm thấy file build tại: {file_path}. Vui lòng build dự án trước!"
                }).encode("utf-8"))
                return

            with open(file_path, "rb") as f:
                content = f.read()

            md5_hash = hashlib.md5(content).hexdigest()
            APP_STATE["firmware_path"] = file_path
            APP_STATE["firmware_filename"] = os.path.basename(file_path)
            APP_STATE["firmware_size"] = len(content)
            APP_STATE["firmware_md5"] = md5_hash
            APP_STATE["target"] = target_name

            log_message(f"🎯 [1-CHẠM PRESET] Đã chọn: {os.path.basename(file_path)} ({len(content)} bytes) | MD5: {md5_hash}")

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({
                "success": True,
                "filename": os.path.basename(file_path),
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
        if parsed.path == "/api/send_raw":
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

            mqtt_client.publish(TOPIC_OTA, json.dumps(ota_msg), qos=1)
            log_message(f"📤 [BẮN LỆNH OTA] Topic: '{TOPIC_OTA}'\\n{json.dumps(ota_msg, indent=2)}")

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
    print("  🚀 PUMP_MY_FAMILY - SMART DASHBOARD & OTA TEST TOOL")
    print("=" * 60)
    print(f"  👉 IP Mạng Nội Bộ (Local IP) : {LOCAL_IP}")
    print(f"  👉 Web Dashboard             : http://localhost:{WEB_UI_PORT}")
    print(f"  👉 HTTP Firmware Server      : http://{LOCAL_IP}:{FILE_SERVER_PORT}/firmware.bin")
    print(f"  👉 MQTT Broker (HiveMQ)      : {MQTT_BROKER}:{MQTT_PORT}")
    print("=" * 60)

    t_file = threading.Thread(target=run_file_server, daemon=True)
    t_file.start()

    t_mqtt = threading.Thread(target=run_mqtt, daemon=True)
    t_mqtt.start()

    t_web = threading.Thread(target=run_web_server, daemon=True)
    t_web.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nĐang dừng ứng dụng...")
