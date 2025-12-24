#line 1 "/home/cursor/SECURITY/ble-scanner/PROJECT-SUMMARY.md"
# BLE Security Scanner - Project Summary

## Project Goal

Build a dedicated Bluetooth Low Energy (BLE) security scanner for business premises monitoring. The device continuously scans for Bluetooth devices, displays them in real-time, logs all detections to SD card, and alerts (visually and audibly) when unknown devices are detected.

## Hardware Platform

**Board:** ESP32-3248S035C (Sunton/DIYmall)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-WROOM-32 (dual-core, WiFi + Bluetooth) |
| Display | 3.5" TFT LCD, 480x320 pixels, ST7796 driver |
| Touch | Capacitive touch screen |
| Storage | Built-in SD card slot |
| Audio | SC8002B amplifier + P4 speaker connector |
| Interface | USB-C for power and programming |

## Core Features

### 1. BLE Device Scanning
- Continuous active scanning (5-second cycles)
- Detects all BLE-advertising devices within range (~10-30m)
- Extracts: MAC address, device name, RSSI, manufacturer, service UUIDs
- Device type identification (phone, beacon, wearable, audio, etc.)

### 2. Real-Time Display
- 480x320 pixel landscape TFT display
- Shows 6 devices at once with scrolling
- Color-coded status indicators:
  - 🟢 Green = Known/whitelisted device
  - 🔴 Red = Unknown device (security alert)
  - 🟡 Yellow = New device (< 5 minutes)
- RSSI signal strength bars
- Device count and last scan time in header

### 3. Device Whitelist
- Store trusted devices in SPIFFS
- Persists across reboots
- Add/remove via touch interface (long-press)
- JSON format for easy editing

### 4. SD Card Logging
- All scan results written to CSV files
- Daily log rotation (YYYY-MM-DD.csv)
- Fields: timestamp, MAC, name, RSSI, type, status, manufacturer
- Enables historical analysis and security auditing

### 5. Audio Alerts
- SC8002B amplifier with speaker connector (P4)
- Alternative: Piezo buzzer on GPIO 22
- Alert patterns:
  - Unknown device: 3 urgent beeps
  - New device: 1 short beep
  - Whitelist added: 2 ascending tones

### 6. WiFi Alerts (Optional)
- HTTP POST webhook for unknown devices
- Integrates with external alerting systems
- Serial logging for debugging

## Pin Assignments

### TFT Display (HSPI)
| Function | GPIO |
|----------|------|
| MISO | 12 |
| MOSI | 13 |
| SCLK | 14 |
| CS | 15 |
| DC | 2 |
| RST | -1 (EN) |
| Backlight | 27 |

### SD Card (VSPI)
| Function | GPIO |
|----------|------|
| CS | 5 |
| MOSI | 23 |
| MISO | 19 |
| SCK | 18 |

### Touch Screen
| Function | GPIO |
|----------|------|
| SDA | 33 |
| SCL | 32 |
| INT | 21 |
| RST | 25 |

### Audio
| Function | GPIO |
|----------|------|
| Speaker (SC8002B) | 26 |
| Piezo Buzzer (alt) | 22 |

## Software Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Main Loop                               │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │ BLE Scanner │  │   Device    │  │   Display   │         │
│  │   Module    │→ │   Manager   │→ │  Renderer   │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│         │                │                │                 │
│         ▼                ▼                ▼                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  SD Card    │  │  Whitelist  │  │   Touch     │         │
│  │   Logger    │  │  (SPIFFS)   │  │  Handler    │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│         │                                  │                │
│         ▼                                  ▼                │
│  ┌─────────────┐                   ┌─────────────┐         │
│  │   Audio     │                   │    WiFi     │         │
│  │   Alerts    │                   │   Alerts    │         │
│  └─────────────┘                   └─────────────┘         │
└─────────────────────────────────────────────────────────────┘
```

## Libraries Required

| Library | Purpose |
|---------|---------|
| TFT_eSPI | Display driver (requires User_Setup.h config) |
| ArduinoJson | JSON parsing for whitelist/config |
| BLEDevice | ESP32 BLE scanning (included in core) |
| SD | SD card file operations |
| SPIFFS | Internal flash storage |
| WiFi | Optional webhook alerts |
| HTTPClient | Optional HTTP POST alerts |

## File Structure

```
ble-scanner/
├── ble-scanner.ino           # Main application (~1100 lines)
├── deploy.sh                 # Build/upload/monitor script
├── secrets.h                 # WiFi credentials (gitignored)
├── secrets.h.example         # Credentials template
├── README.md                 # User documentation
├── CLAUDE.md                 # Developer/AI guidance
├── PROJECT-SUMMARY.md        # This file
├── .gitignore                # Git exclusions
├── build/                    # Compiled binaries (gitignored)
└── ESP32-320-480-display-horizontal/  # Reference project
```

## Key Constants

```cpp
// Display
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 30
#define DEVICE_ROW_HEIGHT 46
#define MAX_VISIBLE_DEVICES 6

// BLE Scanning
#define SCAN_DURATION 5          // Seconds per scan
#define SCAN_INTERVAL 10         // Seconds between scans
#define DEVICE_TIMEOUT 60        // Seconds before removal
#define NEW_DEVICE_THRESHOLD 300 // Seconds to show as "new"
#define MAX_TRACKED_DEVICES 50   // Memory limit

// Colors (RGB565)
#define COLOR_KNOWN   0x07E0     // Green
#define COLOR_UNKNOWN 0xF800     // Red
#define COLOR_NEW     0xFFE0     // Yellow
#define COLOR_BG      0x0000     // Black
#define COLOR_TEXT    0xFFFF     // White
```

## Build Commands

```bash
# Using deploy.sh (recommended)
./deploy.sh          # Compile and upload
./deploy.sh compile  # Compile only
./deploy.sh upload   # Upload only
./deploy.sh monitor  # Serial monitor

# Manual (must use huge_app partition)
arduino-cli compile --build-path ./build \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .
arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app --input-dir ./build .
```

## Build Results

| Metric | Value |
|--------|-------|
| Partition Scheme | `huge_app` (3MB APP / No OTA / 1MB SPIFFS) |
| Binary Size | 1,897,115 bytes (60% of 3MB) |
| RAM Usage | 70,304 bytes (21% of 327KB) |
| Compile Time | ~60 seconds |

## Critical Setup Notes

1. **TFT_eSPI Configuration:** Must **replace entire** `$HOME/Arduino/libraries/TFT_eSPI/User_Setup.h`:
   - Driver: `ST7796_DRIVER` (NOT ILI9341)
   - Pins: MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, BL=27
   - Add: `USE_HSPI_PORT`
   - Speed: `SPI_FREQUENCY 80000000`
   - **White screen = wrong configuration. See README for complete file.**

2. **Partition Scheme:** Must use `huge_app` - default 1.2MB partition is too small

3. **WiFi Credentials:** Copy `secrets.h.example` to `secrets.h` and add credentials

4. **Audio Speaker:** Use 100Ω series resistor with speaker to prevent brownout resets

5. **SD Card:** Format as FAT32, up to 32GB recommended (optional)

## Security Use Cases

- Detect unauthorized phones/devices in secure areas
- Monitor for rogue Bluetooth devices
- Track device presence patterns over time
- Audit Bluetooth activity via SD card logs
- Real-time visual alerts for security personnel

## Limitations

- MAC randomization: Modern phones randomize BLE MACs
- Range: ~10-30m indoors, varies with obstacles
- BLE only: Does not detect Classic Bluetooth
- Radio sharing: Heavy WiFi use may impact BLE scanning

## Implementation Notes

### ESP32 Arduino Core 3.x API Changes
- LEDC: Use `ledcAttach(pin, freq, resolution)` instead of `ledcSetup()` + `ledcAttachPin()`
- Audio: Use `ledcWriteTone(pin, freq)` for tone generation
- BLE: `BLEScan::isScanning()` removed - use timer-based completion check
- SD: Use separate `SPIClass sdSPI(VSPI)` to avoid TFT SPI conflicts

### Touch Support
ESP32-3248S035C uses GT911 capacitive touch (I2C), not XPT2046 (SPI). Touch is currently a placeholder - scanner works fully without it. Add GT911 library for touch support.

---

**Status:** ✅ Complete and Working

**Tested:** December 2024
**ESP32 Core:** 3.3.5
**TFT_eSPI:** 2.5.43
