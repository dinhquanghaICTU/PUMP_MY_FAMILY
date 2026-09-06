import bcrypt
from datetime import datetime, timedelta
from typing import Optional
from jose import JWTError, jwt
from fastapi import Depends, HTTPException, status
from fastapi.security import OAuth2PasswordBearer, HTTPBearer, HTTPAuthorizationCredentials
from sqlmodel import Session, select
from app.config import settings
from app.database import get_session
from app.models import User, Device, DeviceShare, SharePermission

security = HTTPBearer()

def hash_password(password: str) -> str:
    # bcrypt giới hạn tối đa 72 bytes
    pwd_bytes = password.encode("utf-8")[:72]
    salt = bcrypt.gensalt()
    return bcrypt.hashpw(pwd_bytes, salt).decode("utf-8")

def verify_password(plain_password: str, hashed_password: str) -> bool:
    pwd_bytes = plain_password.encode("utf-8")[:72]
    return bcrypt.checkpw(pwd_bytes, hashed_password.encode("utf-8"))

def create_access_token(data: dict, expires_delta: Optional[timedelta] = None) -> str:
    to_encode = data.copy()
    if expires_delta:
        expire = datetime.utcnow() + expires_delta
    else:
        expire = datetime.utcnow() + timedelta(days=settings.ACCESS_TOKEN_EXPIRE_DAYS)
    to_encode.update({"exp": expire})
    encoded_jwt = jwt.encode(to_encode, settings.JWT_SECRET, algorithm=settings.JWT_ALGORITHM)
    return encoded_jwt

def get_current_user(
    credentials: HTTPAuthorizationCredentials = Depends(security),
    session: Session = Depends(get_session)
) -> User:
    token = credentials.credentials
    credentials_exception = HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Token không hợp lệ hoặc đã hết hạn!",
        headers={"WWW-Authenticate": "Bearer"},
    )
    try:
        payload = jwt.decode(token, settings.JWT_SECRET, algorithms=[settings.JWT_ALGORITHM])
        user_id: str = payload.get("id")
        if user_id is None:
            raise credentials_exception
    except JWTError:
        raise credentials_exception

    user = session.get(User, user_id)
    if user is None:
        raise credentials_exception
    return user

def verify_can_control_device(device_id: str, user: User, session: Session) -> Device:
    device = session.get(Device, device_id)
    if not device:
        raise HTTPException(status_code=404, detail="Không tìm thấy thiết bị này!")

    # 1. Nếu là chủ sở hữu (Owner) -> Toàn quyền!
    if device.owner_id == user.id:
        return device

    # 2. Nếu được chia sẻ quyền CAN_CONTROL -> Hợp lệ
    statement = select(DeviceShare).where(
        DeviceShare.device_id == device_id,
        DeviceShare.user_id == user.id,
        DeviceShare.permission == SharePermission.CAN_CONTROL
    )
    share = session.exec(statement).first()
    if share:
        return device

    raise HTTPException(
        status_code=status.HTTP_403_FORBIDDEN,
        detail="Từ chối! Bạn chỉ có quyền XEM hoặc chưa được chia sẻ máy bơm này."
    )
