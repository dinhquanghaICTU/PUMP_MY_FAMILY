#!/usr/bin/env bash

# ==============================================================================
# ESP32-S3 Build & Flash Automation Script
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -d "$PROJECT_DIR/my_esp32s3_app" ]; then
    APP_DIR="$PROJECT_DIR/my_esp32s3_app"
else
    APP_DIR="$PROJECT_DIR"
fi

cd "$APP_DIR" || exit 1

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 1. Tự động nạp môi trường ESP-IDF nếu chưa có idf.py
ensure_idf_env() {
    if ! command -v idf.py &> /dev/null; then
        echo -e "${YELLOW}[INFO] Đang nạp môi trường ESP-IDF...${NC}"
        if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
            . "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1
        else
            echo -e "${RED}[ERROR] Không tìm thấy ~/esp/esp-idf/export.sh! Hãy kiểm tra lại đường dẫn SDK.${NC}"
            exit 1
        fi
    fi
}

# 2. Tự động phát hiện cổng COM (USB/UART)
detect_port() {
    local PORTS
    PORTS=($(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null))
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo ""
    elif [ ${#PORTS[@]} -eq 1 ]; then
        echo "${PORTS[0]}"
    else
        # Nhiều cổng: ưu tiên cổng ACM0 hoặc USB0
        echo "${PORTS[0]}"
    fi
}

PORT_ARG=""
get_port_param() {
    DETECTED_PORT=$(detect_port)
    if [ -n "$DETECTED_PORT" ]; then
        echo -e "${CYAN}[DETECT] Tìm thấy cổng ESP32: $DETECTED_PORT${NC}"
        PORT_ARG="-p $DETECTED_PORT"
    else
        echo -e "${YELLOW}[WARN] Chưa cắm cổng USB/COM hoặc không nhận diện được (sẽ để idf.py tự dò)...${NC}"
        PORT_ARG=""
    fi
}

# Functions
do_build() {
    ensure_idf_env
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> BẮT ĐẦU BIÊN DỊCH (BUILD)...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py build
}

do_flash() {
    ensure_idf_env
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> ĐANG NẠP FIRMWARE (FLASH)...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG flash
}

do_monitor() {
    ensure_idf_env
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> MỞ SERIAL MONITOR (Bấm Ctrl+] để thoát)...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG monitor
}

do_all() {
    ensure_idf_env
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> BUILD + FLASH + MONITOR...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG build flash monitor
}

do_menuconfig() {
    ensure_idf_env
    idf.py menuconfig
}

do_clean() {
    ensure_idf_env
    echo -e "${YELLOW}==> Đang dọn dẹp thư mục build...${NC}"
    idf.py clean
    echo -e "${GREEN}Hoàn tất dọn dẹp!${NC}"
}

do_fullclean() {
    ensure_idf_env
    echo -e "${RED}==> Đang xóa sạch toàn bộ thư mục build (Fullclean)...${NC}"
    idf.py fullclean
    echo -e "${GREEN}Hoàn tất! Đã giải phóng dung lượng ổ cứng.${NC}"
}

show_menu() {
    while true; do
        clear
        echo -e "${CYAN}====================================================${NC}"
        echo -e "${GREEN}       ESP32-S3 BUILD & FLASH TOOLKIT (ESP-IDF)     ${NC}"
        echo -e "${CYAN}====================================================${NC}"
        echo -e " Thư mục làm việc: ${YELLOW}$APP_DIR${NC}"
        DETECTED=$(detect_port)
        if [ -n "$DETECTED" ]; then
            echo -e " Cổng thiết bị:    ${GREEN}$DETECTED${NC}"
        else
            echo -e " Cổng thiết bị:    ${RED}Chưa kết nối${NC}"
        fi
        echo -e "${CYAN}----------------------------------------------------${NC}"
        echo -e " [1] Build dự án (idf.py build)"
        echo -e " [2] Flash nạp code vào chip (idf.py flash)"
        echo -e " [3] Xem Serial Log Monitor (idf.py monitor)"
        echo -e " ${GREEN}[4] Chạy Tất Cả: Build + Flash + Monitor${NC}"
        echo -e " [5] Mở Menu Cấu Hình Phần Cứng (idf.py menuconfig)"
        echo -e " [6] Dọn dẹp build nhẹ (Clean)"
        echo -e " [7] Xóa sạch thư mục build để tiết kiệm dung lượng (Full Clean)"
        echo -e " [0] Thoát"
        echo -e "${CYAN}====================================================${NC}"
        read -p "Nhập lựa chọn của bạn [0-7]: " choice

        case $choice in
            1) do_build; read -p "Bấm [Enter] để tiếp tục...";;
            2) do_flash; read -p "Bấm [Enter] để tiếp tục...";;
            3) do_monitor; read -p "Bấm [Enter] để tiếp tục...";;
            4) do_all; read -p "Bấm [Enter] để tiếp tục...";;
            5) do_menuconfig;;
            6) do_clean; read -p "Bấm [Enter] để tiếp tục...";;
            7) do_fullclean; read -p "Bấm [Enter] để tiếp tục...";;
            0) echo -e "${GREEN}Tạm biệt!${NC}"; exit 0;;
            *) echo -e "${RED}Lựa chọn không hợp lệ!${NC}"; sleep 1;;
        esac
    done
}

# Kiểm tra nếu có truyền tham số dòng lệnh
if [ $# -gt 0 ]; then
    case "$1" in
        build)       do_build;;
        flash)       do_flash;;
        monitor)     do_monitor;;
        all)         do_all;;
        menuconfig)  do_menuconfig;;
        clean)       do_clean;;
        fullclean)   do_fullclean;;
        *)           echo "Cách dùng: $0 [build|flash|monitor|all|menuconfig|clean|fullclean]";;
    esac
else
    show_menu
fi
