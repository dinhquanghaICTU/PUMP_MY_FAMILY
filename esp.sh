#!/usr/bin/env bash

# ==============================================================================
# ESP32 & ESP32-S3 Build & Flash Automation Script (PUMP_MY_FAMILY)
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CABINET_DIR="$PROJECT_DIR/my_esp32s3_app"
TANK_DIR="$PROJECT_DIR/my_esp32_sensor_node"

# Mặc định bắt đầu với Node Tủ Điện
APP_DIR="$CABINET_DIR"
CURRENT_TARGET="esp32s3"
DEVICE_NAME="🏠 NODE MASTER TỦ ĐIỆN (ESP32-S3)"

# Color codes
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

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

detect_port() {
    local PORTS
    PORTS=($(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null))
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo ""
    else
        echo "${PORTS[0]}"
    fi
}

get_port_param() {
    DETECTED_PORT=$(detect_port)
    if [ -n "$DETECTED_PORT" ]; then
        echo -e "${CYAN}[DETECT] Cổng nạp: $DETECTED_PORT${NC}"
        PORT_ARG="-p $DETECTED_PORT"
    else
        echo -e "${YELLOW}[WARN] Chưa cắm cổng USB hoặc không nhận diện được (để idf.py tự dò)...${NC}"
        PORT_ARG=""
    fi
}

select_cabinet() {
    APP_DIR="$CABINET_DIR"
    CURRENT_TARGET="esp32s3"
    DEVICE_NAME="🏠 NODE MASTER TỦ ĐIỆN (ESP32-S3)"
}

select_tank() {
    APP_DIR="$TANK_DIR"
    CURRENT_TARGET="esp32"
    DEVICE_NAME="🌊 NODE CẢM BIẾN BỂ NƯỚC (ESP32-U)"
}

do_set_target() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> ĐẶT TARGET: $CURRENT_TARGET cho ${DEVICE_NAME}...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py set-target $CURRENT_TARGET
}

do_build() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> BẮT ĐẦU BIÊN DỊCH (${DEVICE_NAME})...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py build
}

do_flash() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> ĐANG NẠP CODE VÀO ${DEVICE_NAME}...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG flash
}

do_monitor() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> MỞ SERIAL MONITOR (Bấm Ctrl+] để thoát)...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG monitor
}

do_all() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}=======================================${NC}"
    echo -e "${GREEN}==> BUILD + FLASH + MONITOR (${DEVICE_NAME})...${NC}"
    echo -e "${BLUE}=======================================${NC}"
    idf.py $PORT_ARG build flash monitor
}

do_menuconfig() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    idf.py menuconfig
}

do_clean() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${YELLOW}==> Đang dọn dẹp thư mục build...${NC}"
    idf.py clean
    echo -e "${GREEN}Hoàn tất dọn dẹp!${NC}"
}

do_fullclean() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${RED}==> Đang xóa sạch toàn bộ thư mục build (Fullclean)...${NC}"
    idf.py fullclean
    echo -e "${GREEN}Hoàn tất! Đã giải phóng dung lượng ổ cứng.${NC}"
}

show_menu() {
    while true; do
        clear
        echo -e "${CYAN}================================================================${NC}"
        echo -e "${GREEN}       PUMP_MY_FAMILY - EMBEDDED TOOLKIT (ESP-IDF)              ${NC}"
        echo -e "${CYAN}================================================================${NC}"
        echo -e " 🎯 THIẾT BỊ ĐANG CHỌN: ${YELLOW}${DEVICE_NAME}${NC}"
        echo -e " 📁 Đường dẫn source:   ${MAGENTA}${APP_DIR}${NC}"
        DETECTED=$(detect_port)
        if [ -n "$DETECTED" ]; then
            echo -e " 🔌 Cổng USB nhận diện: ${GREEN}$DETECTED${NC}"
        else
            echo -e " 🔌 Cổng USB nhận diện: ${RED}Chưa cắm USB${NC}"
        fi
        echo -e "${CYAN}----------------------------------------------------------------${NC}"
        echo -e " ${YELLOW}👉 CHỌN LOẠI THIẾT BỊ ĐỂ LÀM VIỆC:${NC}"
        echo -e "   [A] 🏠 Chuyển sang: Node Master Tủ Điện (ESP32-S3)"
        echo -e "   [B] 🌊 Chuyển sang: Node Cảm Biến Bể Nước (ESP32-U)"
        echo -e "${CYAN}----------------------------------------------------------------${NC}"
        echo -e " ${YELLOW}👉 THAO TÁC NẠP & BUILD:${NC}"
        echo -e "   ${GREEN}[1] Chạy Tất Cả: Build + Flash + Monitor (Nhanh nhất)${NC}"
        echo -e "   [2] Build dự án (idf.py build)"
        echo -e "   [3] Flash nạp firmware (idf.py flash)"
        echo -e "   [4] Mở Serial Monitor xem log (idf.py monitor)"
        echo -e "   [5] Đặt lại Target Chip (idf.py set-target)"
        echo -e "   [6] Mở cấu hình phần cứng (idf.py menuconfig)"
        echo -e "   [7] Dọn dẹp build (Clean / Full Clean)"
        echo -e "   [0] Thoát"
        echo -e "${CYAN}================================================================${NC}"
        read -p "Nhập lựa chọn của bạn [A, B, 1-7, 0]: " choice

        case $choice in
            a|A) select_cabinet;;
            b|B) select_tank;;
            1) do_all; read -p "Bấm [Enter] để tiếp tục...";;
            2) do_build; read -p "Bấm [Enter] để tiếp tục...";;
            3) do_flash; read -p "Bấm [Enter] để tiếp tục...";;
            4) do_monitor; read -p "Bấm [Enter] để tiếp tục...";;
            5) do_set_target; read -p "Bấm [Enter] để tiếp tục...";;
            6) do_menuconfig;;
            7) do_clean; read -p "Bấm [Enter] để tiếp tục...";;
            0) echo -e "${GREEN}Tạm biệt!${NC}"; exit 0;;
            *) echo -e "${RED}Lựa chọn không hợp lệ!${NC}"; sleep 1;;
        esac
    done
}

if [ $# -gt 0 ]; then
    case "$1" in
        cabinet)     select_cabinet; do_all;;
        tank)        select_tank; do_all;;
        build)       do_build;;
        flash)       do_flash;;
        monitor)     do_monitor;;
        all)         do_all;;
        *)           echo "Cách dùng: $0 [cabinet|tank|build|flash|monitor|all]";;
    esac
else
    show_menu
fi
