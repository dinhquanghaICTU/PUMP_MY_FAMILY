import os
from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    DATABASE_URL: str = os.getenv(
        "DATABASE_URL", 
        "postgresql://pump_admin:PumpFamilyPass2026@postgres_db:5432/pump_family_db"
    )
    JWT_SECRET: str = os.getenv("JWT_SECRET", "pump_family_super_secret_jwt_key_2026")
    JWT_ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_DAYS: int = 30

    MQTT_BROKER_URI: str = os.getenv(
        "MQTT_BROKER_URI", 
        "mqtts://20476a36ce36478d90de6d5676587638.s1.eu.hivemq.cloud:8883"
    )
    MQTT_USERNAME: str = os.getenv("MQTT_USERNAME", "quanghaictu")
    MQTT_PASSWORD: str = os.getenv("MQTT_PASSWORD", "Zdinhquangha1234")

    class Config:
        env_file = ".env"
        extra = "ignore"

settings = Settings()
