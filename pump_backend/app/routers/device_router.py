from typing import List, Dict, Any
from fastapi import APIRouter, Depends, HTTPException, status
from sqlmodel import Session, select
from app.database import get_session
from app.models import User, Device, DeviceShare, DeviceCreateRequest, DeviceShareRequest
from app.auth import get_current_user

router = APIRouter(prefix="/api/devices", tags=["Quản lý & Chia sẻ Máy Bơm"])

@router.post("/", response_model=Device, status_code=status.HTTP_201_CREATED, summary="Thêm máy bơm mới (Bạn là Chủ sở hữu)")
def create_device(
    payload: DeviceCreateRequest, 
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    # Kiểm tra mã thiết bị đã có chưa
    statement = select(Device).where(Device.device_code == payload.device_code)
    existing_device = session.exec(statement).first()
    if existing_device:
        raise HTTPException(status_code=400, detail="Mã máy bơm (device_code) này đã tồn tại!")

    device = Device(
        device_code=payload.device_code,
        name=payload.name,
        owner_id=current_user.id
    )
    session.add(device)
    session.commit()
    session.refresh(device)
    return device

@router.get("/", summary="Xem danh sách máy bơm (Của mình + Được chia sẻ)")
def list_devices(
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    # 1. Máy do mình làm chủ (Owner)
    owned_devices = session.exec(
        select(Device).where(Device.owner_id == current_user.id)
    ).all()

    # 2. Máy do người khác share cho mình
    shares = session.exec(
        select(DeviceShare).where(DeviceShare.user_id == current_user.id)
    ).all()

    shared_list = []
    for s in shares:
        d = session.get(Device, s.device_id)
        if d:
            shared_list.append({
                "id": d.id,
                "device_code": d.device_code,
                "name": d.name,
                "is_online": d.is_online,
                "is_pump_running": d.is_pump_running,
                "is_auto_mode": d.is_auto_mode,
                "is_child_lock": d.is_child_lock,
                "water_level": d.water_level,
                "tank_height_cm": d.tank_height_cm,
                "sensor_offset_cm": d.sensor_offset_cm,
                "min_water_percent": d.min_water_percent,
                "max_water_percent": d.max_water_percent,
                "firmware_version": d.firmware_version or "1.0.0",
                "tank_firmware_version": d.tank_firmware_version or "1.0.0",
                "permission": s.permission,
                "role": "MEMBER"
            })

    return {
        "owned_devices": [
            {
                "id": d.id,
                "device_code": d.device_code,
                "name": d.name,
                "is_online": d.is_online,
                "is_pump_running": d.is_pump_running,
                "is_auto_mode": d.is_auto_mode,
                "is_child_lock": d.is_child_lock,
                "water_level": d.water_level,
                "tank_height_cm": d.tank_height_cm,
                "sensor_offset_cm": d.sensor_offset_cm,
                "min_water_percent": d.min_water_percent,
                "max_water_percent": d.max_water_percent,
                "firmware_version": d.firmware_version or "1.0.0",
                "tank_firmware_version": d.tank_firmware_version or "1.0.0",
                "role": "OWNER"
            }
            for d in owned_devices
        ],
        "shared_devices": shared_list
    }

@router.post("/{device_id}/share", summary="Chủ nhà chia sẻ máy bơm cho người khác qua Email")
def share_device(
    device_id: str,
    payload: DeviceShareRequest,
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    device = session.get(Device, device_id)
    if not device or device.owner_id != current_user.id:
        raise HTTPException(status_code=403, detail="Chỉ Chủ sở hữu mới có quyền chia sẻ máy bơm này!")

    # Tìm user được chia sẻ
    target_user = session.exec(select(User).where(User.email == payload.target_email)).first()
    if not target_user:
        raise HTTPException(status_code=404, detail="Không tìm thấy tài khoản người dùng có email này!")

    if target_user.id == current_user.id:
        raise HTTPException(status_code=400, detail="Bạn không thể tự chia sẻ máy bơm cho chính mình!")

    # Kiểm tra xem đã share chưa
    existing_share = session.exec(
        select(DeviceShare).where(
            DeviceShare.device_id == device_id,
            DeviceShare.user_id == target_user.id
        )
    ).first()

    if existing_share:
        existing_share.permission = payload.permission
        session.add(existing_share)
    else:
        share = DeviceShare(
            device_id=device_id,
            user_id=target_user.id,
            permission=payload.permission
        )
        session.add(share)

    session.commit()
    return {"message": f"Đã chia sẻ quyền [{payload.permission}] cho {payload.target_email} thành công!"}

@router.delete("/{device_id}/share/{target_user_id}", summary="Chủ nhà thu hồi quyền chia sẻ")
def revoke_share(
    device_id: str,
    target_user_id: str,
    current_user: User = Depends(get_current_user),
    session: Session = Depends(get_session)
):
    device = session.get(Device, device_id)
    if not device or device.owner_id != current_user.id:
        raise HTTPException(status_code=403, detail="Chỉ Chủ sở hữu mới có quyền thu hồi chia sẻ!")

    share = session.exec(
        select(DeviceShare).where(
            DeviceShare.device_id == device_id,
            DeviceShare.user_id == target_user_id
        )
    ).first()

    if not share:
        raise HTTPException(status_code=404, detail="Không tìm thấy bản ghi chia sẻ này!")

    session.delete(share)
    session.commit()
    return {"message": "Đã thu hồi quyền truy cập thành công!"}
