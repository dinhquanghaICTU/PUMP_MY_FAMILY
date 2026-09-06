#!/bin/bash
set -e

echo "=========================================="
echo "🚀 1. KÉO CODE MỚI VỀ..."
echo "=========================================="
git pull origin main || true

echo "=========================================="
echo "🔨 2. BUILD LẠI CÁC CONTAINER (BACKEND + FRONTEND)..."
echo "=========================================="
docker compose build

echo "=========================================="
echo "🔄 3. KHỞI ĐỘNG LẠI DỊCH VỤ..."
echo "=========================================="
docker compose up -d

echo "=========================================="
echo "✅ DEPLOY HOÀN TẤT! ĐANG XEM LOG FASTAPI:"
echo "=========================================="
docker compose logs -f --tail=30 backend_api
