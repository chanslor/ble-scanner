#!/bin/bash
set -e

# Heltec BLE Scanner - Build and Deploy Script
# Usage:
#   ./deploy.sh          - Compile and upload
#   ./deploy.sh compile  - Compile only
#   ./deploy.sh upload   - Upload only
#   ./deploy.sh monitor  - Open serial monitor

cd /home/cursor/SECURITY/ble-scanner/heltec-ble-scanner

# Configuration - Heltec WiFi LoRa 32 V3 (ESP32-S3)
BOARD="esp32:esp32:heltec_wifi_lora_32_V3"
PORT="/dev/ttyUSB0"
BAUD="115200"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

find_port() {
    if [ -e "/dev/ttyUSB0" ]; then
        PORT="/dev/ttyUSB0"
    elif [ -e "/dev/ttyUSB1" ]; then
        PORT="/dev/ttyUSB1"
    elif [ -e "/dev/ttyACM0" ]; then
        PORT="/dev/ttyACM0"
    else
        echo -e "${RED}Error: No USB serial device found${NC}"
        exit 1
    fi
    echo -e "${GREEN}Using port: ${PORT}${NC}"
}

compile() {
    echo -e "${YELLOW}=== Compiling Heltec BLE Scanner ===${NC}"
    echo "Board: ${BOARD}"
    echo ""

    arduino-cli compile \
        --build-path ./build \
        --fqbn ${BOARD} \
        .

    echo ""
    echo -e "${GREEN}=== Compilation Successful ===${NC}"
}

upload() {
    find_port

    echo -e "${YELLOW}=== Uploading to Heltec ===${NC}"
    echo "Port: ${PORT}"
    echo ""

    arduino-cli upload \
        -p ${PORT} \
        --fqbn ${BOARD} \
        --input-dir ./build \
        .

    echo ""
    echo -e "${GREEN}=== Upload Successful ===${NC}"
}

monitor() {
    find_port

    echo -e "${YELLOW}=== Opening Serial Monitor ===${NC}"
    echo "Port: ${PORT}"
    echo "Baud: ${BAUD}"
    echo "Press Ctrl+C to exit"
    echo ""

    arduino-cli monitor \
        -p ${PORT} \
        -c baudrate=${BAUD}
}

case "${1}" in
    compile)
        compile
        ;;
    upload)
        upload
        ;;
    monitor)
        monitor
        ;;
    "")
        compile
        upload
        echo ""
        echo -e "${GREEN}=== Deploy Complete ===${NC}"
        echo "Run './deploy.sh monitor' to view serial output"
        ;;
    *)
        echo "Heltec BLE Scanner - Build & Deploy Script"
        echo ""
        echo "Usage:"
        echo "  ./deploy.sh          - Compile and upload"
        echo "  ./deploy.sh compile  - Compile only"
        echo "  ./deploy.sh upload   - Upload only"
        echo "  ./deploy.sh monitor  - Open serial monitor"
        ;;
esac
