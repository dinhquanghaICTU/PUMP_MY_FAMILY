import asyncio
import os
from contextlib import asynccontextmanager
from datetime import datetime
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from sqlmodel import Session, select
from app.database import init_db, engine
from app.models import User, Role, Device
from app.mqtt_client import start_mqtt
from app.routers import auth_router, device_router, pump_router, admin_router

FIRMWARE_DIR = os.path.join(os.getcwd(), "firmware")
os.makedirs(FIRMWARE_DIR, exist_ok=True)

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Khởi chạy Database (tự tạo bảng)
    print("Đang khởi tạo Database PostgreSQL...")
    init_db()

    # Kiểm tra và cấp quyền ADMIN cho tài khoản đầu tiên
    try:
        with Session(engine) as session:
            admin_user = session.exec(select(User).where(User.role == Role.ADMIN)).first()
            if not admin_user:
                first_user = session.exec(select(User).order_by(User.created_at.asc())).first()
                if first_user:
                    first_user.role = Role.ADMIN
                    session.add(first_user)
                    session.commit()
                    print(f"👑 [Admin] Đã tự động cấp quyền ADMIN cho: {first_user.email}")
    except Exception as e:
        print(f"Lỗi kiểm tra quyền Admin: {e}")
    
    # Khởi chạy MQTT kết nối HiveMQ Cloud
    print("Đang kết nối HiveMQ Cloud...")
    start_mqtt()

    # Khởi chạy Heartbeat Monitor phát hiện thiết bị mất nguồn / mất mạng trong 6 giây
    async def device_heartbeat_worker():
        while True:
            try:
                await asyncio.sleep(2)
                now = datetime.utcnow()
                with Session(engine) as session:
                    devices = session.exec(select(Device).where(Device.is_online == True)).all()
                    changed = False
                    for dev in devices:
                        if dev.updated_at and (now - dev.updated_at).total_seconds() > 6.0:
                            dev.is_online = False
                            dev.is_tank_online = False
                            session.add(dev)
                            changed = True
                            print(f"🔴 [HEARTBEAT TIMEOUT] {dev.device_code} không gửi tín hiệu quá 6s -> Chuyển sang OFFLINE!")
                    if changed:
                        session.commit()
            except asyncio.CancelledError:
                break
            except Exception as ex:
                print(f"Lỗi kiểm tra Heartbeat thiết bị: {ex}")

    heartbeat_task = asyncio.create_task(device_heartbeat_worker())
    
    yield
    heartbeat_task.cancel()
    print("Đang tắt ứng dụng...")

app = FastAPI(
    title="PUMP_MY_FAMILY IoT API",
    description="Hệ thống quản lý máy bơm thông minh gia đình, phân quyền & điều khiển qua HiveMQ Cloud",
    version="1.0.0",
    lifespan=lifespan
)

# CORS Middleware (Cho phép Mobile App gọi vào)
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Phục vụ thư mục firmware cho ESP32 tải OTA trực tiếp
app.mount("/firmware", StaticFiles(directory=FIRMWARE_DIR), name="firmware")

# Đăng ký các Router
app.include_router(auth_router.router)
app.include_router(device_router.router)
app.include_router(pump_router.router)
app.include_router(admin_router.router)

@app.get("/", summary="Kiểm tra trạng thái server")
def root():
    return {
        "status": "ONLINE",
        "system": "PUMP_MY_FAMILY Backend (FastAPI + Python)",
        "docs_url": "/docs",
        "time": datetime.utcnow().isoformat()
    }
