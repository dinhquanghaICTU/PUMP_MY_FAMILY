#!/usr/bin/env bash

# ==============================================================================
# ESP32 & ESP32-S3 Build & Flash Automation Script (PUMP_MY_FAMILY)
# ==============================================================================

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CABINET_DIR="$PROJECT_DIR/my_esp32s3_app"
TANK_DIR="$PROJECT_DIR/my_esp32_sensor_node"

# Cổng nạp riêng cho từng node (tự động nhớ)
PORT_CABINET=""
PORT_TANK=""
CURRENT_PORT=""

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
BOLD='\033[1m'
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

get_all_ports() {
    ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
}

auto_assign_ports() {
    local PORTS=($(get_all_ports))
    if [ ${#PORTS[@]} -ge 1 ]; then
        if [ -z "$PORT_CABINET" ]; then
            # Ưu tiên ACM0 cho S3, USB0 cho ESP32
            for p in "${PORTS[@]}"; do
                if [[ "$p" == *"ACM"* ]]; then
                    PORT_CABINET="$p"
                    break
                fi
            done
            [ -z "$PORT_CABINET" ] && PORT_CABINET="${PORTS[0]}"
        fi

        if [ -z "$PORT_TANK" ]; then
            for p in "${PORTS[@]}"; do
                if [[ "$p" == *"USB"* ]]; then
                    PORT_TANK="$p"
                    break
                fi
            done
            [ -z "$PORT_TANK" ] && PORT_TANK="${PORTS[${#PORTS[@]}-1]}"
        fi
    fi
}

update_current_port() {
    if [ "$CURRENT_TARGET" == "esp32s3" ]; then
        CURRENT_PORT="$PORT_CABINET"
    else
        CURRENT_PORT="$PORT_TANK"
    fi
}

select_cabinet() {
    APP_DIR="$CABINET_DIR"
    CURRENT_TARGET="esp32s3"
    DEVICE_NAME="🏠 NODE MASTER TỦ ĐIỆN (ESP32-S3)"
    update_current_port
}

select_tank() {
    APP_DIR="$TANK_DIR"
    CURRENT_TARGET="esp32"
    DEVICE_NAME="🌊 NODE CẢM BIẾN BỂ NƯỚC (ESP32-U)"
    update_current_port
}

change_port_menu() {
    local PORTS=($(get_all_ports))
    echo -e "\n${YELLOW}=== DANH SÁCH CỔNG COM/TTY ĐANG CẮM TRÊN MÁY ===${NC}"
    if [ ${#PORTS[@]} -eq 0 ]; then
        echo -e "${RED}❌ Không tìm thấy cổng USB/ACM nào đang cắm!${NC}"
        read -p "Bấm [Enter] để quay lại..."
        return
    fi

    for i in "${!PORTS[@]}"; do
        echo -e "   [${GREEN}$((i+1))${NC}] ${BOLD}${PORTS[$i]}${NC}"
    done
    echo -e "   [${CYAN}M${NC}] Nhập cổng thủ công bằng tay"
    echo -e "   [0] Quay lại"
    read -p "Chọn cổng cho ${DEVICE_NAME}: " p_choice

    if [[ "$p_choice" =~ ^[0-9]+$ ]] && [ "$p_choice" -ge 1 ] && [ "$p_choice" -le "${#PORTS[@]}" ]; then
        local chosen="${PORTS[$((p_choice-1))]}"
        if [ "$CURRENT_TARGET" == "esp32s3" ]; then
            PORT_CABINET="$chosen"
        else
            PORT_TANK="$chosen"
        fi
        update_current_port
        echo -e "${GREEN}✅ Đã chọn cổng: $chosen cho ${DEVICE_NAME}${NC}"
        sleep 1
    elif [[ "$p_choice" == "m" || "$p_choice" == "M" ]]; then
        read -p "Nhập đường dẫn cổng (ví dụ /dev/ttyUSB1): " manual_port
        if [ -n "$manual_port" ]; then
            if [ "$CURRENT_TARGET" == "esp32s3" ]; then
                PORT_CABINET="$manual_port"
            else
                PORT_TANK="$manual_port"
            fi
            update_current_port
            echo -e "${GREEN}✅ Đã lưu cổng: $manual_port${NC}"
            sleep 1
        fi
    fi
}

get_port_param() {
    update_current_port
    if [ -n "$CURRENT_PORT" ] && [ -e "$CURRENT_PORT" ]; then
        echo -e "${CYAN}[DETECT] Sử dụng cổng: ${GREEN}$CURRENT_PORT${NC}"
        PORT_ARG="-p $CURRENT_PORT"
    else
        echo -e "${YELLOW}[WARN] Cổng '$CURRENT_PORT' không tồn tại, để idf.py tự dò...${NC}"
        PORT_ARG=""
    fi
}

do_all() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${GREEN}==> BUILD + FLASH + MONITOR (${DEVICE_NAME})...${NC}"
    echo -e "${BLUE}====================================================${NC}"
    idf.py $PORT_ARG build flash monitor
}

do_build() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${GREEN}==> BẮT ĐẦU BIÊN DỊCH (${DEVICE_NAME})...${NC}"
    echo -e "${BLUE}====================================================${NC}"
    idf.py build
}

do_flash() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${GREEN}==> ĐANG NẠP CODE VÀO ${DEVICE_NAME} QUA CỔNG ${CURRENT_PORT}...${NC}"
    echo -e "${BLUE}====================================================${NC}"
    idf.py $PORT_ARG flash
}

do_monitor() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    get_port_param
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${GREEN}==> MỞ SERIAL MONITOR: ${CURRENT_PORT} (Bấm Ctrl+] để thoát)...${NC}"
    echo -e "${BLUE}====================================================${NC}"
    idf.py $PORT_ARG monitor
}

do_set_target() {
    ensure_idf_env
    cd "$APP_DIR" || exit 1
    echo -e "${BLUE}====================================================${NC}"
    echo -e "${GREEN}==> ĐẶT TARGET: $CURRENT_TARGET cho ${DEVICE_NAME}...${NC}"
    echo -e "${BLUE}====================================================${NC}"
    idf.py set-target $CURRENT_TARGET
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

show_menu() {
    auto_assign_ports
    update_current_port

    while true; do
        clear
        auto_assign_ports
        update_current_port

        local PORTS=($(get_all_ports))

        echo -e "${CYAN}================================================================${NC}"
        echo -e "${GREEN}${BOLD}       PUMP_MY_FAMILY - DUAL-NODE EMBEDDED TOOLKIT (ESP-IDF)     ${NC}"
        echo -e "${CYAN}================================================================${NC}"
        echo -e " 🎯 ${BOLD}THIẾT BỊ ĐANG CHỌN:${NC} ${YELLOW}${BOLD}${DEVICE_NAME}${NC}"
        echo -e " 🔌 ${BOLD}CỔNG NẠP HIỆN TẠI:${NC}  ${GREEN}${BOLD}${CURRENT_PORT:-Chưa chọn}${NC}"
        echo -e " 📁 Đường dẫn source:   ${MAGENTA}${APP_DIR}${NC}"
        echo -e "${CYAN}----------------------------------------------------------------${NC}"
        echo -e " 📡 ${BOLD}CÁC CỔNG USB ĐANG ONLINE TRÊN MÁY:${NC}"
        if [ ${#PORTS[@]} -eq 0 ]; then
            echo -e "    ${RED}❌ Không có cổng USB nào đang cắm!${NC}"
        else
            for p in "${PORTS[@]}"; do
                local tag=""
                [ "$p" == "$PORT_CABINET" ] && tag="${CYAN}[Tủ Điện S3]${NC}"
                [ "$p" == "$PORT_TANK" ] && tag="${GREEN}[Bể Nước ESP32]${NC}"
                [ "$p" == "$CURRENT_PORT" ] && tag="$tag ${YELLOW}<= ĐANG CHỌN${NC}"
                echo -e "    • ${BOLD}$p${NC} $tag"
            done
        fi
        echo -e "${CYAN}----------------------------------------------------------------${NC}"
        echo -e " ${YELLOW}👉 CHỌN THIẾT BỊ HOẶC ĐỔI CỔNG:${NC}"
        echo -e "   [A] 🏠 Chuyển sang: Node Master Tủ Điện (ESP32-S3)"
        echo -e "   [B] 🌊 Chuyển sang: Node Cảm Biến Bể Nước (ESP32-U)"
        echo -e "   [P] 🔌 Đổi cổng nạp COM/TTY cho thiết bị đang chọn"
        echo -e "${CYAN}----------------------------------------------------------------${NC}"
        echo -e " ${YELLOW}👉 THAO TÁC NẠP & MONITOR:${NC}"
        echo -e "   ${GREEN}[1] Chạy Tất Cả: Build + Flash + Monitor (Nhanh nhất)${NC}"
        echo -e "   ${CYAN}[2] Build dự án (idf.py build)${NC}"
        echo -e "   ${YELLOW}[3] Flash nạp firmware (idf.py flash)${NC}"
        echo -e "   ${MAGENTA}[4] Mở Serial Monitor xem log (idf.py monitor)${NC}"
        echo -e "   [5] Đặt lại Target Chip (idf.py set-target)"
        echo -e "   [6] Mở cấu hình phần cứng (idf.py menuconfig)"
        echo -e "   [7] Dọn dẹp thư mục build (Clean)"
        echo -e "   [0] Thoát"
        echo -e "${CYAN}================================================================${NC}"
        read -p "Nhập lựa chọn của bạn [A, B, P, 1-7, 0]: " choice

        case $choice in
            a|A) select_cabinet;;
            b|B) select_tank;;
            p|P) change_port_menu;;
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

show_menu
