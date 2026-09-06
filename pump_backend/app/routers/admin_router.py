import os
import hashlib
import json
import time
from typing import List
from fastapi import APIRouter, Depends, HTTPException, status, UploadFile, File
from sqlmodel import Session, select, func
from app.database import get_session
from app.models import User, Device, DeviceShare, Role, AdminUserCreateRequest, AdminUserUpdateRequest, AdminUserResponse, OtaTriggerRequest
from app.auth import get_current_user, hash_password
from app.mqtt_client import mqtt_client

router = APIRouter(prefix="/api/admin", tags=["Quản Trị Hệ Thống (Admin)"])

FIRMWARE_DIR = os.path.join(os.getcwd(), "firmware")
os.makedirs(FIRMWARE_DIR, exist_ok=True)


def verify_admin_role(current_user: User = Depends(get_current_user)) -> User:
    if current_user.role != Role.ADMIN:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="Bạn không có quyền quản trị viên (Admin)!"
        )
    return current_user


# ==================== 1. QUẢN LÝ NGƯỜI DÙNG (USER CRUD) ====================

@router.get("/users", response_model=List[AdminUserResponse], summary="Lấy danh sách người dùng")
def get_all_users(
    admin: User = Depends(verify_admin_role),
    session: Session = Depends(get_session)
):
    users = session.exec(select(User).order_by(User.created_at.desc())).all()
    result = []
    for u in users:
        dev_count = len(u.owned_devices) if u.owned_devices else 0
        result.append(AdminUserResponse(
            id=u.id,
            email=u.email,
            full_name=u.full_name,
            role=u.role,
            owned_devices_count=dev_count,
            created_at=u.created_at
        ))
    return result


@router.post("/users", response_model=AdminUserResponse, status_code=status.HTTP_201_CREATED, summary="Thêm người dùng mới")
def create_user_by_admin(
    payload: AdminUserCreateRequest,
    admin: User = Depends(verify_admin_role),
    session: Session = Depends(get_session)
):
    existing = session.exec(select(User).where(User.email == payload.email)).first()
    if existing:
        raise HTTPException(status_code=400, detail="Email này đã tồn tại trong hệ thống!")

    new_user = User(
        email=payload.email,
        full_name=payload.full_name,
        password=hash_password(payload.password),
        role=payload.role
    )
    session.add(new_user)
    session.commit()
    session.refresh(new_user)

    return AdminUserResponse(
        id=new_user.id,
        email=new_user.email,
        full_name=new_user.full_name,
        role=new_user.role,
        owned_devices_count=0,
        created_at=new_user.created_at
    )


@router.put("/users/{user_id}", response_model=AdminUserResponse, summary="Chỉnh sửa người dùng")
def update_user_by_admin(
    user_id: str,
    payload: AdminUserUpdateRequest,
    admin: User = Depends(verify_admin_role),
    session: Session = Depends(get_session)
):
    target_user = session.get(User, user_id)
    if not target_user:
        raise HTTPException(status_code=404, detail="Không tìm thấy người dùng này!")

    if payload.full_name is not None:
        target_user.full_name = payload.full_name
    if payload.role is not None:
        target_user.role = payload.role
    if payload.password and len(payload.password.strip()) > 0:
        target_user.password = hash_password(payload.password.strip())

    session.add(target_user)
    session.commit()
    session.refresh(target_user)

    dev_count = len(target_user.owned_devices) if target_user.owned_devices else 0
    return AdminUserResponse(
        id=target_user.id,
        email=target_user.email,
        full_name=target_user.full_name,
        role=target_user.role,
        owned_devices_count=dev_count,
        created_at=target_user.created_at
    )


@router.delete("/users/{user_id}", summary="Xóa người dùng khỏi hệ thống")
def delete_user_by_admin(
    user_id: str,
    admin: User = Depends(verify_admin_role),
    session: Session = Depends(get_session)
):
    if admin.id == user_id:
        raise HTTPException(status_code=400, detail="Bạn không thể tự xóa tài khoản của chính mình!")

    target_user = session.get(User, user_id)
    if not target_user:
        raise HTTPException(status_code=404, detail="Không tìm thấy người dùng này!")

    # Xóa các liên kết chia sẻ của user này
    shares = session.exec(select(DeviceShare).where(DeviceShare.user_id == user_id)).all()
    for s in shares:
        session.delete(s)

    session.delete(target_user)
    session.commit()
    return {"message": f"Đã xóa tài khoản {target_user.email} thành công!"}


# ==================== 2. QUẢN LÝ OTA (FIRMWARE & TRIGGER) ====================

@router.post("/ota/upload", summary="Tải lên file firmware (.bin)")
async def upload_firmware(
    file: UploadFile = File(...),
    admin: User = Depends(verify_admin_role)
):
    if not file.filename.endswith(".bin"):
        raise HTTPException(status_code=400, detail="Chỉ chấp nhận file định dạng firmware (.bin)!")

    # Đặt tên file an toàn
    safe_filename = os.path.basename(file.filename)
    file_path = os.path.join(FIRMWARE_DIR, safe_filename)

    content = await file.read()
    file_size = len(content)

    if file_size == 0:
        raise HTTPException(status_code=400, detail="File firmware rỗng (0 bytes)!")

    # Tính MD5 checksum
    md5_hash = hashlib.md5(content).hexdigest()

    with open(file_path, "wb") as f:
        f.write(content)

    return {
        "filename": safe_filename,
        "size": file_size,
        "md5": md5_hash,
        "download_url": f"/firmware/{safe_filename}",
        "uploaded_at": int(time.time())
    }


@router.get("/ota/firmwares", summary="Danh sách firmware có sẵn trên server")
def list_firmwares(admin: User = Depends(verify_admin_role)):
    files = []
    if os.path.exists(FIRMWARE_DIR):
        for f in os.listdir(FIRMWARE_DIR):
            if f.endswith(".bin"):
                fp = os.path.join(FIRMWARE_DIR, f)
                stat = os.stat(fp)
                files.append({
                    "filename": f,
                    "size": stat.st_size,
                    "download_url": f"/firmware/{f}",
                    "modified_at": int(stat.st_mtime)
                })
    files.sort(key=lambda x: x["modified_at"], reverse=True)
    return files


@router.post("/ota/trigger", summary="Bắn lệnh OTA qua MQTT tới ESP32")
def trigger_ota(
    payload: OtaTriggerRequest,
    admin: User = Depends(verify_admin_role)
):
    topic = "pump/family/ota"
    ota_data = {
        "action": "ota",
        "version": payload.version,
        "target": payload.target,       # "esp32s3_cabinet" hoặc "esp32_tank"
        "url": payload.url,
        "md5": payload.md5 or "",
        "size": payload.size,
        "filename": payload.filename
    }

    # Bắn MQTT không khoảng trắng để an toàn với mọi parser C
    payload_str = json.dumps(ota_data, separators=(',', ':'))
    res = mqtt_client.publish(topic, payload_str, qos=1)

    if res.rc == 0:
        from app.mqtt_client import current_ota_progress
        current_ota_progress["target"] = payload.target
        current_ota_progress["status"] = "in_progress"
        current_ota_progress["percent"] = 0
        current_ota_progress["bytes"] = 0
        current_ota_progress["total"] = payload.size
        current_ota_progress["version"] = payload.version
        current_ota_progress["error"] = None
        current_ota_progress["message"] = "Đang kích hoạt nạp OTA..."
        print(f"🚀 [Admin OTA] Đã bắn lệnh OTA [{topic}]: {payload_str}")
        return {
            "status": "success",
            "message": f"Đã gửi lệnh OTA thành công tới mục tiêu: {payload.target}!",
            "ota_payload": ota_data
        }
    else:
        raise HTTPException(status_code=500, detail="Không thể gửi lệnh MQTT tới HiveMQ!")

@router.get("/ota/progress", summary="Xem tiến độ nạp OTA theo thời gian thực (%)")
def get_ota_progress(admin: User = Depends(verify_admin_role)):
    from app.mqtt_client import current_ota_progress
    return current_ota_progress

