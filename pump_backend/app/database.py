from sqlmodel import SQLModel, create_engine, Session
from app.config import settings

engine = create_engine(settings.DATABASE_URL, echo=False)

from sqlalchemy import text

def init_db():
    # Tự động tạo bảng nếu chưa có (không làm mất dữ liệu cũ)
    SQLModel.metadata.create_all(engine)
    # Tự động bổ sung các cột mới nếu bảng đã tồn tại từ trước (Auto Migration)
    with engine.connect() as conn:
        conn.execute(text("""
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS tank_height_cm DOUBLE PRECISION DEFAULT 120.0;
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS sensor_offset_cm DOUBLE PRECISION DEFAULT 10.0;
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS min_water_percent INTEGER DEFAULT 20;
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS max_water_percent INTEGER DEFAULT 90;
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS firmware_version VARCHAR(50) DEFAULT '1.0.0';
            ALTER TABLE devices ADD COLUMN IF NOT EXISTS tank_firmware_version VARCHAR(50) DEFAULT '1.0.0';
        """))
        conn.commit()

def get_session():
    with Session(engine) as session:
        yield session
