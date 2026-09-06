from typing import Optional, List
from datetime import datetime
from enum import Enum
import uuid
from sqlmodel import SQLModel, Field, Relationship
from pydantic import BaseModel, EmailStr

# ----------------- ENUMS -----------------
class Role(str, Enum):
    ADMIN = "ADMIN"
    USER = "USER"

class SharePermission(str, Enum):
    VIEW_ONLY = "VIEW_ONLY"       # Chỉ xem mực nước
    CAN_CONTROL = "CAN_CONTROL"   # Bật/tắt bơm và đổi chế độ

# ----------------- DATABASE MODELS -----------------

class User(SQLModel, table=True):
    __tablename__ = "users"

    id: str = Field(default_factory=lambda: str(uuid.uuid4()), primary_key=True)
    email: str = Field(unique=True, index=True)
    password: str
    full_name: str
    role: Role = Field(default=Role.USER)
    created_at: datetime = Field(default_factory=datetime.utcnow)

    # Quan hệ
    owned_devices: List["Device"] = Relationship(back_populates="owner")
    shares: List["DeviceShare"] = Relationship(back_populates="user")
    pump_logs: List["PumpLog"] = Relationship(back_populates="user")


class Device(SQLModel, table=True):
    __tablename__ = "devices"

    id: str = Field(default_factory=lambda: str(uuid.uuid4()), primary_key=True)
    device_code: str = Field(unique=True, index=True) # vd: "PUMP_FAMILY_01"
    name: str                                        # vd: "Máy Bơm Tầng 1"
    owner_id: str = Field(foreign_key="users.id")

    is_online: bool = Field(default=False)
    is_pump_running: bool = Field(default=False)
    is_auto_mode: bool = Field(default=True)
    is_child_lock: bool = Field(default=False)
    water_level: int = Field(default=0)              # 0 - 100%
    tank_height_cm: float = Field(default=120.0)     # Chiều cao téc nước (cm)
    sensor_offset_cm: float = Field(default=10.0)    # Khoảng cách cảm biến tới đỉnh nước (cm)
    min_water_percent: int = Field(default=20)       # Ngưỡng tự động bơm (%)
    max_water_percent: int = Field(default=90)       # Ngưỡng tự động ngắt (%)
    firmware_version: str = Field(default="1.0.0")   # Phiên bản firmware Node Tủ Điện (S3)
    tank_firmware_version: str = Field(default="1.0.0") # Phiên bản firmware Node Bể Nước (ESP32)
    battery_voltage: Optional[float] = Field(default=None) # Điện áp pin Node Bể (V)
    distance_cm: Optional[float] = Field(default=None)     # Khoảng cách mặt nước từ cảm biến (cm)
    pump_runtime: Optional[int] = Field(default=0)         # Thời gian bơm hiện tại (giây)
    created_at: datetime = Field(default_factory=datetime.utcnow)
    updated_at: datetime = Field(default_factory=datetime.utcnow)

    # Quan hệ
    owner: Optional[User] = Relationship(back_populates="owned_devices")
    shares: List["DeviceShare"] = Relationship(back_populates="device")
    logs: List["PumpLog"] = Relationship(back_populates="device")


class DeviceShare(SQLModel, table=True):
    __tablename__ = "device_shares"

    id: str = Field(default_factory=lambda: str(uuid.uuid4()), primary_key=True)
    device_id: str = Field(foreign_key="devices.id")
    user_id: str = Field(foreign_key="users.id")
    permission: SharePermission = Field(default=SharePermission.VIEW_ONLY)
    created_at: datetime = Field(default_factory=datetime.utcnow)

    device: Optional[Device] = Relationship(back_populates="shares")
    user: Optional[User] = Relationship(back_populates="shares")


class PumpLog(SQLModel, table=True):
    __tablename__ = "pump_logs"

    id: str = Field(default_factory=lambda: str(uuid.uuid4()), primary_key=True)
    device_id: str = Field(foreign_key="devices.id")
    action: str                                      # "AUTO_ON", "MANUAL_ON", "STOP"
    water_level: Optional[int] = None
    triggered_by: Optional[str] = Field(default=None, foreign_key="users.id")
    created_at: datetime = Field(default_factory=datetime.utcnow)

    device: Optional[Device] = Relationship(back_populates="logs")
    user: Optional[User] = Relationship(back_populates="pump_logs")


# ----------------- SCHEMAS FOR API (REQUEST/RESPONSE) -----------------

class UserRegisterRequest(BaseModel):
    email: EmailStr
    password: str
    full_name: str

class UserLoginRequest(BaseModel):
    email: EmailStr
    password: str

class UserResponse(BaseModel):
    id: str
    email: str
    full_name: str
    role: Role

class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    user: UserResponse

class DeviceCreateRequest(BaseModel):
    device_code: str
    name: str

class DeviceShareRequest(BaseModel):
    target_email: EmailStr
    permission: SharePermission = SharePermission.VIEW_ONLY

class PumpControlRequest(BaseModel):
    action: str  # "PUMP_ON", "PUMP_OFF", "AUTO_MODE", "MANUAL_MODE", "CHILD_LOCK_ON", "CHILD_LOCK_OFF"

# Schema Cấu Hình Bể Nước
class TankConfigRequest(BaseModel):
    tank_height: float = 120.0
    offset: float = 10.0
    min_pct: int = 20
    max_pct: int = 90

# Schemas Quản Trị User (Admin)
class AdminUserCreateRequest(BaseModel):
    email: EmailStr
    password: str
    full_name: str
    role: Role = Role.USER

class AdminUserUpdateRequest(BaseModel):
    full_name: Optional[str] = None
    role: Optional[Role] = None
    password: Optional[str] = None  # Nếu có giá trị thì đổi mật khẩu

class AdminUserResponse(BaseModel):
    id: str
    email: str
    full_name: str
    role: Role
    owned_devices_count: int = 0
    created_at: datetime

# Schema Bắn Lệnh OTA
class OtaTriggerRequest(BaseModel):
    target: str          # "esp32s3_cabinet" hoặc "esp32_tank"
    version: str         # vd: "1.0.1"
    url: str             # Đường dẫn http://.../firmware/...
    filename: str        # vd: "cabinet_v1.0.1.bin"
    size: int            # Kích thước file (bytes)
    md5: Optional[str] = ""
