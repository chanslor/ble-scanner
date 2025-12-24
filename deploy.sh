#!/bin/bash
set -e

# BLE Security Scanner - Build and Deploy Script
# Usage:
#   ./deploy.sh          - Compile and upload
#   ./deploy.sh compile  - Compile only
#   ./deploy.sh upload   - Upload only (must compile first)
#   ./deploy.sh monitor  - Open serial monitor

cd /home/cursor/SECURITY/ble-scanner

# Configuration
# Using huge_app partition (3MB APP / No OTA / 1MB SPIFFS) for BLE + WiFi + Display
BOARD="esp32:esp32:esp32:PartitionScheme=huge_app"
PORT="/dev/ttyUSB0"
BAUD="115200"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to find the correct port
find_port() {
    if [ -e "/dev/ttyUSB0" ]; then
        PORT="/dev/ttyUSB0"
    elif [ -e "/dev/ttyUSB1" ]; then
        PORT="/dev/ttyUSB1"
    elif [ -e "/dev/ttyACM0" ]; then
        PORT="/dev/ttyACM0"
    elif [ -e "/dev/ttyACM1" ]; then
        PORT="/dev/ttyACM1"
    else
        echo -e "${RED}Error: No USB serial device found${NC}"
        echo "Available devices:"
        ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "  None found"
        exit 1
    fi
    echo -e "${GREEN}Using port: ${PORT}${NC}"
}

# Compile function
compile() {
    echo -e "${YELLOW}=== Compiling BLE Scanner ===${NC}"
    echo "Board: ${BOARD}"
    echo ""

    arduino-cli compile \
        --build-path ./build \
        --fqbn ${BOARD} \
        .

    echo ""
    echo -e "${GREEN}=== Compilation Successful ===${NC}"
    echo ""

    # Show binary size
    if [ -f "./build/ble-scanner.ino.bin" ]; then
        SIZE=$(ls -lh ./build/ble-scanner.ino.bin | awk '{print $5}')
        echo "Binary size: ${SIZE}"
    fi
}

# Upload function
upload() {
    find_port

    echo -e "${YELLOW}=== Uploading to ESP32 ===${NC}"
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

# Monitor function
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

# Main script
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
        # Default: compile and upload
        compile
        upload
        echo ""
        echo -e "${GREEN}=== Deploy Complete ===${NC}"
        echo "Run './deploy.sh monitor' to view serial output"
        ;;
    *)
        echo "BLE Security Scanner - Build & Deploy Script"
        echo ""
        echo "Usage:"
        echo "  ./deploy.sh          - Compile and upload"
        echo "  ./deploy.sh compile  - Compile only"
        echo "  ./deploy.sh upload   - Upload only"
        echo "  ./deploy.sh monitor  - Open serial monitor"
        ;;
esac
