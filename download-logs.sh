#!/bin/bash
#
# BLE Scanner Log Downloader
# Downloads log files from the BLE Scanner's SD card via WiFi
#
# Usage:
#   ./download-logs.sh                    # Auto-discover scanner, download all logs
#   ./download-logs.sh 192.168.1.100      # Use specific IP address
#   ./download-logs.sh 192.168.1.100 logs # Download to specific directory
#

set -e

# Configuration
DEFAULT_OUTPUT_DIR="./logs"
SCANNER_PORT=80
TIMEOUT=10

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Parse arguments
SCANNER_IP="${1:-}"
OUTPUT_DIR="${2:-$DEFAULT_OUTPUT_DIR}"

echo -e "${CYAN}=== BLE Scanner Log Downloader ===${NC}"
echo ""

# Try to discover scanner if IP not provided
if [ -z "$SCANNER_IP" ]; then
    echo -e "${YELLOW}No IP address provided. Attempting to discover scanner...${NC}"

    # Try common local network ranges
    for subnet in "192.168.1" "192.168.0" "10.0.0" "172.16.0"; do
        echo -e "  Scanning ${subnet}.x ..."
        for i in $(seq 1 254); do
            ip="${subnet}.${i}"
            # Quick check if host is up and has our endpoint
            if curl -s --connect-timeout 1 "http://${ip}/status" 2>/dev/null | grep -q "scanner_id"; then
                SCANNER_IP="$ip"
                echo -e "${GREEN}  Found scanner at ${SCANNER_IP}${NC}"
                break 2
            fi
        done
    done

    if [ -z "$SCANNER_IP" ]; then
        echo -e "${RED}Error: Could not auto-discover scanner.${NC}"
        echo "Please provide the scanner IP address as an argument:"
        echo "  $0 <scanner-ip> [output-directory]"
        echo ""
        echo "Check the scanner's serial output or display for its IP address."
        exit 1
    fi
fi

SCANNER_URL="http://${SCANNER_IP}:${SCANNER_PORT}"

# Verify connection to scanner
echo -e "Connecting to scanner at ${CYAN}${SCANNER_URL}${NC}..."
if ! curl -s --connect-timeout "$TIMEOUT" "${SCANNER_URL}/status" > /dev/null 2>&1; then
    echo -e "${RED}Error: Cannot connect to scanner at ${SCANNER_IP}${NC}"
    echo "Make sure:"
    echo "  1. The scanner is powered on"
    echo "  2. The scanner is connected to WiFi"
    echo "  3. Your computer is on the same network"
    exit 1
fi

# Get scanner status
echo -e "${GREEN}Connected!${NC}"
echo ""

STATUS=$(curl -s --connect-timeout "$TIMEOUT" "${SCANNER_URL}/status")
SCANNER_ID=$(echo "$STATUS" | grep -o '"scanner_id":"[^"]*"' | cut -d'"' -f4)
DEVICE_COUNT=$(echo "$STATUS" | grep -o '"device_count":[0-9]*' | cut -d':' -f2)
SD_CARD=$(echo "$STATUS" | grep -o '"sd_card":[a-z]*' | cut -d':' -f2)

echo -e "Scanner ID:     ${CYAN}${SCANNER_ID}${NC}"
echo -e "Devices tracked: ${CYAN}${DEVICE_COUNT}${NC}"
echo -e "SD Card:        ${CYAN}${SD_CARD}${NC}"
echo ""

# Check if SD card is present
if [ "$SD_CARD" != "true" ]; then
    echo -e "${RED}Error: SD card not present in scanner${NC}"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"
echo -e "Downloading logs to: ${CYAN}${OUTPUT_DIR}/${NC}"
echo ""

# Get list of log files
echo "Fetching file list..."
FILES_JSON=$(curl -s --connect-timeout "$TIMEOUT" "${SCANNER_URL}/logs")

# Check for errors
if echo "$FILES_JSON" | grep -q '"error"'; then
    ERROR=$(echo "$FILES_JSON" | grep -o '"error":"[^"]*"' | cut -d'"' -f4)
    echo -e "${RED}Error: ${ERROR}${NC}"
    exit 1
fi

# Parse file list (simple extraction without jq dependency)
FILES=$(echo "$FILES_JSON" | grep -o '"name":"[^"]*"' | cut -d'"' -f4)

if [ -z "$FILES" ]; then
    echo -e "${YELLOW}No log files found on SD card.${NC}"
    exit 0
fi

# Count files
FILE_COUNT=$(echo "$FILES" | wc -l)
echo -e "Found ${CYAN}${FILE_COUNT}${NC} log file(s)"
echo ""

# Download each file
DOWNLOADED=0
for filename in $FILES; do
    echo -n "  Downloading ${filename}... "

    OUTPUT_PATH="${OUTPUT_DIR}/${filename}"

    if curl -s --connect-timeout "$TIMEOUT" \
        "${SCANNER_URL}/download?file=${filename}" \
        -o "$OUTPUT_PATH"; then

        # Get file size
        SIZE=$(stat -f%z "$OUTPUT_PATH" 2>/dev/null || stat -c%s "$OUTPUT_PATH" 2>/dev/null || echo "?")
        echo -e "${GREEN}OK${NC} (${SIZE} bytes)"
        ((DOWNLOADED++))
    else
        echo -e "${RED}FAILED${NC}"
    fi
done

echo ""
echo -e "${GREEN}Downloaded ${DOWNLOADED}/${FILE_COUNT} files to ${OUTPUT_DIR}/${NC}"
echo ""

# Show summary of downloaded files
echo "Downloaded files:"
ls -la "$OUTPUT_DIR"/*.csv 2>/dev/null || echo "  (no CSV files found)"
