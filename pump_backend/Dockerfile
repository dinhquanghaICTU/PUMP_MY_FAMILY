# Sử dụng Python 3.11 slim nhẹ và nhanh
FROM python:3.11-slim

WORKDIR /app

# Cài đặt các gói build cần thiết
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy và cài đặt thư viện Python
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy mã nguồn ứng dụng
COPY app ./app

# Mở cổng 8000 (Swagger UI & API)
EXPOSE 8000

# Chạy server FastAPI
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
