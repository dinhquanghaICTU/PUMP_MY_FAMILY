from typing import List, Optional
from datetime import datetime
import time
from fastapi import APIRouter, Depends, HTTPException, status
from sqlmodel import Session, select
from app.database import get_session
from app.models import User, Device, PumpLog, PumpControlRequest, TankConfigRequest
from app.auth import get_current_user, verify_can_control_device
from app.mqtt_client import send_pump_command

router = APIRouter(prefix="/api/pumps", tags=["Điều khiển & Lịch sử Bơm"])

@router.post("/{device_id}/control", summary="Gửi lệnh Bật/Tắt/Đổi chế độ Bơm (Kiểm tra quyền)")
def control_pump(
    device_id: str,
    payload: PumpControlRequest,
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    # Kiểm tra xem User có quyền điều khiển (Owner hoặc CAN_CONTROL) không
    device = verify_can_control_device(device_id, current_user, session)

    # Chuẩn hóa lệnh gửi xuống ESP32-S3 theo định dạng firmware yêu cầu
    mqtt_payload = {
        "target_device": device.device_code,
        "sender": current_user.email,
        "timestamp": int(time.time() * 1000)
    }

    act = payload.action.upper()
    if act in ["PUMP_ON", "ON"]:
        mqtt_payload["action"] = "on"
        mqtt_payload["pump"] = 1
        device.is_pump_running = True
    elif act in ["PUMP_OFF", "OFF"]:
        mqtt_payload["action"] = "off"
        mqtt_payload["pump"] = 0
        device.is_pump_running = False
    elif act in ["AUTO_MODE", "AUTO"]:
        mqtt_payload["mode"] = "auto"
        device.is_auto_mode = True
    elif act in ["MANUAL_MODE", "MANUAL"]:
        mqtt_payload["mode"] = "manual"
        device.is_auto_mode = False
    elif act in ["CHILD_LOCK_ON", "LOCK"]:
        mqtt_payload["child_lock"] = 1
        device.is_child_lock = True
    elif act in ["CHILD_LOCK_OFF", "UNLOCK"]:
        mqtt_payload["child_lock"] = 0
        device.is_child_lock = False
    elif act == "TOGGLE":
        mqtt_payload["action"] = "toggle"
        device.is_pump_running = not device.is_pump_running
    else:
        mqtt_payload["action"] = payload.action.lower()

    # Gửi lệnh MQTT qua HiveMQ Cloud xuống ESP32-S3
    send_pump_command(mqtt_payload)

    # Ghi nhận vào nhật ký
    log = PumpLog(
        device_id=device.id,
        action=payload.action,
        triggered_by=current_user.id,
        water_level=device.water_level
    )
    session.add(device)
    session.add(log)
    session.commit()

    return {"message": f"Đã gửi lệnh [{payload.action}] thành công tới máy bơm {device.name}!"}

@router.get("/{device_id}/logs", summary="Xem nhật ký lịch sử hoạt động của máy bơm")
def get_pump_logs(
    device_id: str,
    limit: int = 50,
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    device = session.get(Device, device_id)
    if not device:
        raise HTTPException(status_code=404, detail="Không tìm thấy máy bơm!")

    # Lấy lịch sử giảm dần theo thời gian
    statement = (
        select(PumpLog)
        .where(PumpLog.device_id == device_id)
        .order_by(PumpLog.created_at.desc())
        .limit(limit)
    )
    logs = session.exec(statement).all()

    result = []
    for l in logs:
        user_info = None
        if l.triggered_by:
            u = session.get(User, l.triggered_by)
            if u:
                user_info = {"id": u.id, "full_name": u.full_name, "email": u.email}

        result.append({
            "id": l.id,
            "action": l.action,
            "water_level": l.water_level,
            "created_at": l.created_at,
            "triggered_by": user_info or "Hệ Thống Tự Động (Auto)"
        })

    return {"logs": result}

@router.post("/{device_id}/config", summary="Cấu hình chiều cao téc nước & ngưỡng tự động")
def update_tank_config(
    device_id: str,
    payload: TankConfigRequest,
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    device = verify_can_control_device(device_id, current_user, session)

    device.tank_height_cm = payload.tank_height
    device.sensor_offset_cm = payload.offset
    device.min_water_percent = payload.min_pct
    device.max_water_percent = payload.max_pct
    device.updated_at = datetime.utcnow()

    session.add(device)
    session.commit()
    session.refresh(device)

    # Bắn lệnh MQTT xuống ESP32-S3 theo định dạng sscanf yêu cầu
    config_payload = {
        "tank_height": round(payload.tank_height, 1),
        "offset": round(payload.offset, 1),
        "min_pct": payload.min_pct,
        "max_pct": payload.max_pct
    }
    send_pump_command(config_payload)

    # Ghi nhật ký
    log = PumpLog(
        device_id=device.id,
        action=f"Cấu hình téc: Cao={payload.tank_height}cm, Auto={payload.min_pct}%-{payload.max_pct}%",
        triggered_by=current_user.id,
        water_level=device.water_level
    )
    session.add(log)
    session.commit()

    return {
        "message": "Đã lưu cấu hình téc nước thành công và gửi xuống ESP32!",
        "config": config_payload
    }
