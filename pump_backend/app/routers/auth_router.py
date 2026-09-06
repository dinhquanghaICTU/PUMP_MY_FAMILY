from fastapi import APIRouter, Depends, HTTPException, status
from sqlmodel import Session, select
from app.database import get_session
from app.models import User, UserRegisterRequest, UserLoginRequest, UserResponse, TokenResponse
from app.auth import hash_password, verify_password, create_access_token, get_current_user

router = APIRouter(prefix="/api/auth", tags=["Xác thực & Tài khoản"])

@router.post("/register", response_model=UserResponse, status_code=status.HTTP_201_CREATED, summary="Đăng ký tài khoản mới")
def register(payload: UserRegisterRequest, session: Session = Depends(get_session)):
    statement = select(User).where(User.email == payload.email)
    existing_user = session.exec(statement).first()
    if existing_user:
        raise HTTPException(status_code=400, detail="Email này đã được đăng ký!")

    new_user = User(
        email=payload.email,
        password=hash_password(payload.password),
        full_name=payload.full_name
    )
    session.add(new_user)
    session.commit()
    session.refresh(new_user)

    return UserResponse(
        id=new_user.id,
        email=new_user.email,
        full_name=new_user.full_name,
        role=new_user.role
    )

@router.post("/login", response_model=TokenResponse, summary="Đăng nhập cấp JWT Token")
def login(payload: UserLoginRequest, session: Session = Depends(get_session)):
    statement = select(User).where(User.email == payload.email)
    user = session.exec(statement).first()
    if not user or not verify_password(payload.password, user.password):
        raise HTTPException(status_code=400, detail="Email hoặc mật khẩu không chính xác!")

    token = create_access_token(data={"id": user.id, "email": user.email, "role": user.role})
    
    return TokenResponse(
        access_token=token,
        token_type="bearer",
        user=UserResponse(
            id=user.id,
            email=user.email,
            full_name=user.full_name,
            role=user.role
        )
    )

@router.get("/me", response_model=UserResponse, summary="Lấy thông tin tài khoản hiện tại")
def get_profile(current_user: User = Depends(get_current_user)):
    return UserResponse(
        id=current_user.id,
        email=current_user.email,
        full_name=current_user.full_name,
        role=current_user.role
    )
