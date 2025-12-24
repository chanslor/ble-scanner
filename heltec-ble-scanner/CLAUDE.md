# Heltec BLE Scanner - CLAUDE.md

This file provides guidance to Claude Code when working with the Heltec BLE Scanner project.

---
## ⚠️ IMPORTANT: ALWAYS USE deploy.sh ⚠️

```bash
./deploy.sh          # Compile and upload
./deploy.sh compile  # Compile only
./deploy.sh upload   # Upload only
./deploy.sh monitor  # Serial monitor (run in separate terminal)
```

---

## Project Overview

ESP32-S3 based BLE scanner that **successfully combines BLE scanning with HTTPS cloud posting**. This solves the fundamental limitation discovered on the regular ESP32.

**Hardware:** Heltec WiFi LoRa 32 V3 (ESP32-S3)
**Display:** Built-in 0.96" OLED (128x64, SSD1306)
**Key Feature:** BLE + HTTPS work simultaneously!

## Why Heltec ESP32-S3 Works (And Regular ESP32 Doesn't)

### The Problem with Regular ESP32

The original BLE scanner on ESP32 (Sunton board) could not do BLE scanning and HTTPS posting together due to **heap memory fragmentation**:

- SSL/TLS requires a **contiguous memory block of 16-20KB** for handshake buffers
- The ESP32 BLE stack fragments heap memory severely
- After BLE runs, the largest contiguous block drops to only **4-9KB**
- HTTPS fails with "connection refused" (actually a memory allocation failure)
- `BLEDevice::deinit()` frees memory but **BLE cannot restart** after deinit

### Why ESP32-S3 Solves This

The ESP32-S3 (used in Heltec V3) has **improved memory architecture**:

| Metric | ESP32 (Sunton) | ESP32-S3 (Heltec) |
|--------|----------------|-------------------|
| Free Heap after BLE init | ~35KB | ~152KB |
| Largest Contiguous Block | ~9KB | ~69KB |
| SSL Requirement | 16-20KB | ✓ Has 69KB! |
| HTTPS POST while BLE active | ❌ Fails | ✓ Works! |
| BLE continues after POST | ❌ Dies | ✓ Continues! |
| Successful POSTs | 0 | 14+ and counting |

### Technical Explanation

1. **More Efficient Memory Management**: ESP32-S3 has improved SRAM architecture that reduces fragmentation when running BLE
2. **Bluetooth 5.0**: The S3's BT5 stack may be more memory-efficient than the older BT4.2 on ESP32
3. **Larger Contiguous Blocks**: Even with BLE running, 69KB contiguous blocks remain available - plenty for SSL handshakes
4. **No Deinit Required**: Since memory is sufficient, we don't need to deinit BLE, avoiding the restart bug

### Key Evidence from Serial Output

```
Starting BLE scan... Heap: 152788, Largest: 69620
  Scan started: true
NEW: GoPro 4008 (E5:F3:6B:E5:16:EF) RSSI: -52
=== HTTPS POST Test ===
  Heap before POST: 151104, Largest: 69620
  Posting 10 devices (1087 bytes)...
  Heap after POST: 107116, Largest: 53236
  SUCCESS! HTTP 200
=== POST Complete ===

Scan complete. Tracking 29 devices. Posts: 14 OK, 0 fail
```

## Hardware Configuration

### Heltec WiFi LoRa 32 V3 Pinout

```
OLED Display (Internal):
  SDA: GPIO 17
  SCL: GPIO 18
  RST: GPIO 21

Vext Power Control: GPIO 36 (LOW = ON)
```

### Board Selection in Arduino CLI

```bash
# Compile
arduino-cli compile --build-path ./build \
  --fqbn esp32:esp32:heltec_wifi_lora_32_V3 .

# Upload
arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:heltec_wifi_lora_32_V3 \
  --input-dir ./build .
```

## Architecture

### Scan + POST Cycle

```
┌─────────────────────────────────────────────────────┐
│                    Main Loop                         │
├─────────────────────────────────────────────────────┤
│ 1. Start BLE scan (5 seconds, async)                │
│                                                     │
│ 2. Collect devices via callbacks                    │
│    → Update device list with MAC, name, RSSI        │
│    → Detect manufacturer (Apple, Samsung, etc.)     │
│                                                     │
│ 3. After scan completes:                            │
│    → Prune stale devices (>2 min not seen)         │
│    → Update OLED display                            │
│    → POST to cloud server (HTTPS!)                  │
│                                                     │
│ 4. Repeat every 15 seconds                          │
└─────────────────────────────────────────────────────┘
```

### Key Difference from ESP32 Version

- **NO `BLEDevice::deinit()`** - Not needed, memory is sufficient
- **NO BLE restart issues** - BLE stays running continuously
- **Reliable HTTPS** - 69KB contiguous block is plenty for SSL

## Files

| File | Description |
|------|-------------|
| `heltec-ble-scanner.ino` | Main sketch |
| `secrets.h` | WiFi credentials, server URL, API key |
| `deploy.sh` | Build and deploy script |
| `CLAUDE.md` | This documentation |

## Configuration (secrets.h)

```cpp
const char* WIFI_SSID = "Your_WiFi";
const char* WIFI_PASSWORD = "Your_Password";
const char* BLE_SERVER_URL = "https://ble-scanner.fly.dev/api/logs";
const char* BLE_API_KEY = "your-api-key";
const char* SCANNER_ID = "heltec-scanner-01";
```

## Server Integration

The scanner POSTs JSON to the Fly.io server:

```json
{
  "scanner_id": "heltec-scanner-01",
  "devices": [
    {
      "mac": "E5:F3:6B:E5:16:EF",
      "name": "GoPro 4008",
      "rssi": -52,
      "device_type": "Unknown",
      "manufacturer": "Unknown"
    }
  ]
}
```

**Server endpoints:**
- `POST /api/logs` - Submit device scans
- `GET /api/logs` - Retrieve logs
- `GET /health` - Health check

## OLED Display Layout

```
┌────────────────────────┐
│ BLE Scanner [29]       │  <- Device count
│ WiFi: 192.168.2.11     │  <- IP address
│ POST: 14 OK, 0 fail    │  <- Success stats
│ Heap: 152K/69K         │  <- Memory status
│ --- Devices ---        │
│ GoPro 4008    -52      │  <- Recent devices
│ LE-Bose Micro -49      │
└────────────────────────┘
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| OLED blank | Check Vext power (GPIO 36 = LOW) |
| Upload fails | Hold PRG button during upload |
| WiFi won't connect | Check 2.4GHz network, verify credentials |
| POST fails | Check API key, verify server is running |
| No devices found | Ensure BLE is enabled on nearby phones |

## Comparison: Heltec vs Sunton

| Feature | Sunton (ESP32) | Heltec (ESP32-S3) |
|---------|----------------|-------------------|
| BLE Scanning | ✓ Works | ✓ Works |
| Display | 480x320 TFT | 128x64 OLED |
| HTTPS POST | ❌ Memory fails | ✓ Works! |
| Cloud Integration | ❌ Local only | ✓ Full cloud |
| SD Card Logging | ✓ Works | Not implemented |
| Touch Interface | ✓ Capacitive | Not available |
| Best Use Case | Standalone display | Cloud-connected |

## Recommended Setup

For a complete BLE monitoring solution:

1. **Heltec Scanner** - Cloud posting, real-time monitoring
2. **Sunton Display** - Local display with SD logging (optional)
3. **Fly.io Server** - Central dashboard at https://ble-scanner.fly.dev

The Heltec handles the critical cloud integration that the ESP32 cannot do.
