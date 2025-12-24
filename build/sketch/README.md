#line 1 "/home/cursor/SECURITY/ble-scanner/README.md"
# ESP32 BLE Security Scanner

Bluetooth Low Energy (BLE) device scanner for business security monitoring on ESP32-3248S035C development board with 480x320 TFT display.

![Hardware](https://img.shields.io/badge/Hardware-ESP32--3248S035C-blue)
![Display](https://img.shields.io/badge/Display-ST7796%20480x320-green)
![BLE](https://img.shields.io/badge/Bluetooth-BLE%205.0-blue)
![License](https://img.shields.io/badge/License-MIT-yellow)

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Software Installation](#software-installation)
- [Critical TFT_eSPI Configuration](#critical-tft_espi-configuration)
- [WiFi Configuration](#wifi-configuration)
- [Building and Uploading](#building-and-uploading)
- [Usage](#usage)
- [Display Layout](#display-layout)
- [Device Management](#device-management)
- [Alerting](#alerting)
- [Audio Alerts](#audio-alerts)
- [Troubleshooting](#troubleshooting)
- [Project Structure](#project-structure)
- [Security Considerations](#security-considerations)

## Overview

This project transforms an ESP32-3248S035C development board into a dedicated Bluetooth security monitor for your business premises. It continuously scans for BLE devices, displays them on a 3.5" TFT screen, and can alert you when unknown devices are detected in your private area.

**Use Cases:**
- Detect unauthorized Bluetooth devices in secure areas
- Monitor for rogue devices or potential security threats
- Track device presence patterns over time
- Identify unknown phones, beacons, or IoT devices

## Features

- **Continuous BLE Scanning:** Detects all nearby Bluetooth Low Energy devices
- **Real-time Display:** Shows detected devices with signal strength (RSSI) on 480x320 TFT
- **Device Classification:**
  - 🟢 **Known/Trusted:** Devices on your whitelist
  - 🔴 **Unknown/Alert:** Unrecognized devices requiring attention
  - 🟡 **New:** Recently discovered devices (configurable timeout)
- **Signal Strength Visualization:** RSSI bars showing proximity
- **Device Details:** MAC address, device name (if broadcast), device type
- **Persistence:** Remembers known devices across reboots (SPIFFS storage)
- **SD Card Logging:** Writes all scan results to built-in SD card for historical analysis
- **WiFi Alerting:** Optional webhook/API notifications for unknown devices
- **Touch Interface:** Add devices to whitelist directly from display
- **Auto-refresh:** Configurable scan intervals
- **Elapsed Time Tracking:** Shows how long each device has been visible
- **Audio Alerts:** Audible beep/tone for unknown device detection (via speaker connector or piezo buzzer)

## Hardware Requirements

- **Board:** ESP32-3248S035C (Sunton/DIYmall)
  - MCU: ESP32-WROOM-32 (WiFi + Bluetooth)
  - Display: 3.5" TFT LCD, 480x320 pixels
  - Driver: ST7796
  - Interface: SPI
  - Touch: Capacitive touch (used for device management)
  - Bluetooth: BLE 4.2 / 5.0 compatible

- **USB Cable:** USB-C for programming and power
- **Power:** 5V via USB or external power supply (for permanent installation)

**Optional:**
- Enclosure for wall mounting
- External 5V power supply for 24/7 operation
- MicroSD card for extended logging (FAT32 formatted, up to 32GB recommended)
- Small speaker (4Ω or 8Ω) for audio alerts via P4 connector
- Piezo buzzer for simple beep alerts via GPIO 22

## Software Installation

### 1. Install Arduino CLI

```bash
# Create local bin directory
mkdir -p $HOME/local/bin

# Download and install Arduino CLI (Linux x64)
cd $HOME/local/bin
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Add to PATH (add to ~/.bashrc or ~/.bash_profile)
export PATH="$HOME/local/bin:$PATH"

# Verify installation
arduino-cli version
```

### 2. Configure Arduino CLI for ESP32

```bash
# Initialize Arduino CLI config
arduino-cli config init

# Add ESP32 board manager URL
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Update core index
arduino-cli core update-index

# Install ESP32 platform
arduino-cli core install esp32:esp32
```

### 3. Install Required Libraries

```bash
# Update library index
arduino-cli lib update-index

# Install TFT_eSPI (display driver)
arduino-cli lib install TFT_eSPI

# Install ArduinoJson (for config/whitelist storage)
arduino-cli lib install ArduinoJson

# Verify libraries
arduino-cli lib list
```

**Note:** The ESP32 BLE library is included with the ESP32 Arduino core - no separate installation needed.

## Critical TFT_eSPI Configuration

⚠️ **IMPORTANT:** The TFT_eSPI library requires hardware-specific configuration. This is the most common cause of display issues (white/black screen).

**Replace the entire contents of:** `$HOME/Arduino/libraries/TFT_eSPI/User_Setup.h`

### Complete User_Setup.h for ESP32-3248S035C

```cpp
//                            USER DEFINED SETTINGS
//   Configuration for ESP32-3248S035C (Sunton/DIYmall) with ST7796 480x320 TFT

#define USER_SETUP_INFO "ESP32-3248S035C"

// Section 1: Driver
// ST7796 driver for ESP32-3248S035C 480x320 display
//#define ILI9341_DRIVER    // <-- Make sure this is COMMENTED OUT
#define ST7796_DRIVER       // <-- This must be ENABLED

// Section 2: Pin Configuration
// ESP32-3248S035C uses HSPI for the TFT display
#define TFT_MISO 12   // SPI MISO
#define TFT_MOSI 13   // SPI MOSI
#define TFT_SCLK 14   // SPI Clock
#define TFT_CS   15   // Chip select control pin
#define TFT_DC    2   // Data Command control pin
#define TFT_RST  -1   // Reset pin (connected to EN, use -1)

// Backlight control
#define TFT_BL   27            // LED back-light control pin
#define TFT_BACKLIGHT_ON HIGH  // Level to turn ON back-light

// Section 3: Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// Section 4: SPI Settings
#define SPI_FREQUENCY       80000000   // 80MHz for fast rendering
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Use HSPI port (required for ESP32-3248S035C)
#define USE_HSPI_PORT
```

### Key Configuration Points

| Setting | Value | Why |
|---------|-------|-----|
| Driver | `ST7796_DRIVER` | ESP32-3248S035C uses ST7796, NOT ILI9341 |
| SPI Port | `USE_HSPI_PORT` | TFT is on HSPI, SD card on VSPI |
| Reset Pin | `-1` | Connected to EN (board reset) |
| SPI Speed | 80MHz | Maximum for ST7796 |

### Pin Reference

| Function | GPIO | Notes |
|----------|------|-------|
| TFT_MISO | 12 | SPI Master In |
| TFT_MOSI | 13 | SPI Master Out |
| TFT_SCLK | 14 | SPI Clock |
| TFT_CS | 15 | Chip Select |
| TFT_DC | 2 | Data/Command |
| TFT_RST | -1 | Reset (use EN) |
| TFT_BL | 27 | Backlight PWM |
| TOUCH_CS | 33 | Touch Chip Select |
| AUDIO_OUT | 26 | Speaker amplifier (SC8002B) |

## Audio Alerts

The ESP32-3248S035C has a built-in **SC8002B audio amplifier** with a speaker connector (P4). You can also use a simple piezo buzzer on GPIO 22.

### Option 1: Speaker via P4 Connector (Recommended)

The board has a 2-pin JST connector (P4) for an external speaker:

| Component | Details |
|-----------|---------|
| Amplifier | SC8002B Class-D (8.5x gain) |
| Control Pin | GPIO 26 |
| Connector | P4 (2-pin JST labeled "SPEAK") |
| Speaker | 4Ω or 8Ω, 1-3W |

**Wiring:**
```
P4 Connector:
  Pin 1 (VO1) ──┬── Speaker +
                │
              [100Ω]  ← REQUIRED resistor
                │
  Pin 2 (VO2) ──┴── Speaker -
```

⚠️ **Important:** You MUST use a 100Ω series resistor with the speaker to prevent overcurrent that causes the 3.3V supply to drop and reset the ESP32.

### Option 2: Piezo Buzzer on GPIO 22

For simple beep alerts, connect a piezo buzzer to the available GPIO 22 pin:

| Pin | Connection |
|-----|------------|
| GPIO 22 | Buzzer + (via P3 header) |
| GND | Buzzer - |

No resistor needed for passive piezo buzzers.

### Alert Sound Patterns

| Event | Sound Pattern |
|-------|---------------|
| Unknown device detected | 3 short beeps (urgent) |
| New device detected | 1 short beep |
| Device added to whitelist | 2 ascending tones |
| Scan complete | 1 soft click (optional) |

## WiFi Configuration

Create `secrets.h` from template:

```bash
cp secrets.h.example secrets.h
```

Edit `secrets.h`:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi Credentials (for alerts - optional)
const char* WIFI_SSID = "Your_WiFi_SSID";
const char* WIFI_PASSWORD = "Your_WiFi_Password";

// Alert Webhook (optional)
const char* ALERT_WEBHOOK_URL = "";  // Leave empty to disable

#endif
```

⚠️ **Security:** `secrets.h` is gitignored - never commit credentials.

## Building and Uploading

### Using deploy.sh (Recommended)

```bash
cd /home/cursor/SECURITY/ble-scanner

# Compile and upload
./deploy.sh

# Compile only
./deploy.sh compile

# Upload only (after compiling)
./deploy.sh upload

# Monitor serial output
./deploy.sh monitor
```

### Manual Commands

```bash
# Compile with huge_app partition (required for BLE + WiFi + Display)
arduino-cli compile --build-path ./build \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .

# Find device port
ls /dev/ttyUSB* /dev/ttyACM*

# Upload
arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  --input-dir ./build .

# Monitor serial output
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

### Build Notes

| Item | Details |
|------|---------|
| Partition Scheme | `huge_app` (3MB APP / No OTA / 1MB SPIFFS) |
| Binary Size | ~1.9MB (60% of available space) |
| RAM Usage | ~70KB (21% of available) |

## Usage

### First Boot

1. Power on the device
2. Display shows "BLE Scanner - Initializing..."
3. BLE scanning begins automatically
4. Detected devices appear on screen

### Normal Operation

- **Scan Cycle:** Continuous scanning with configurable interval (default: 5 seconds active scan)
- **Display Updates:** Real-time as devices are detected
- **Device Timeout:** Devices not seen for 60 seconds fade/remove from display

### Touch Controls

- **Tap device:** View detailed information
- **Long press device:** Add to/remove from whitelist
- **Tap header:** Force immediate rescan
- **Swipe up/down:** Scroll through device list (if > 6 devices)

## Display Layout

**Screen:** 480x320 pixels (landscape)

```
┌─────────────────────────────────────────────────────┐
│ BLE SCANNER    [6 devices]     Scan: 5s ago  (30px)│ ← Header
├─────────────────────────────────────────────────────┤
│ 🟢 iPhone-John       -45dBm  ████████░░  Phone     │ ← Device 1
├─────────────────────────────────────────────────────┤
│ 🟢 Office-Beacon     -62dBm  █████░░░░░  Beacon    │ ← Device 2
├─────────────────────────────────────────────────────┤
│ 🔴 Unknown           -78dBm  ███░░░░░░░  ????      │ ← Device 3 (ALERT)
├─────────────────────────────────────────────────────┤
│ 🟡 NEW: Galaxy-A52   -55dBm  ██████░░░░  Phone     │ ← Device 4 (NEW)
├─────────────────────────────────────────────────────┤
│ 🟢 Fitbit-HR         -70dBm  ████░░░░░░  Wearable  │ ← Device 5
├─────────────────────────────────────────────────────┤
│ 🟢 AirPods-Pro       -48dBm  ████████░░  Audio     │ ← Device 6
└─────────────────────────────────────────────────────┘
```

### Color Coding

| Status | Color | Meaning |
|--------|-------|---------|
| 🟢 | Green | Known/whitelisted device |
| 🔴 | Red | Unknown device - potential security concern |
| 🟡 | Yellow | New device (seen < 5 minutes) |
| ⚪ | Gray | Device signal lost (fading out) |

### Signal Strength (RSSI)

| Range | Bars | Proximity |
|-------|------|-----------|
| > -50 dBm | ████████░░ | Very close (< 1m) |
| -50 to -70 | █████░░░░░ | Near (1-5m) |
| -70 to -85 | ███░░░░░░░ | Medium (5-10m) |
| < -85 dBm | █░░░░░░░░░ | Far (> 10m) |

## SD Card Logging

The ESP32-3248S035C has a built-in SD card slot. All scan results are logged to the SD card for historical analysis and security auditing.

### SD Card Setup

1. Format a MicroSD card as FAT32 (up to 32GB recommended)
2. Insert into the SD card slot on the ESP32-3248S035C board
3. The scanner will automatically detect and begin logging

### Log File Structure

```
/ble-logs/
├── 2024-01-15.csv      # Daily log files
├── 2024-01-16.csv
├── 2024-01-17.csv
└── ...
```

### Log File Format (CSV)

```csv
timestamp,mac,name,rssi,device_type,status,manufacturer
2024-01-15T14:30:05,AA:BB:CC:DD:EE:FF,iPhone-John,-45,phone,known,Apple
2024-01-15T14:30:05,XX:XX:XX:XX:XX:XX,Unknown,-78,unknown,alert,Unknown
2024-01-15T14:30:06,11:22:33:44:55:66,Galaxy-A52,-55,phone,new,Samsung
```

### Log Fields

| Field | Description |
|-------|-------------|
| timestamp | ISO 8601 format detection time |
| mac | Bluetooth MAC address |
| name | Device advertised name (or "Unknown") |
| rssi | Signal strength in dBm |
| device_type | Detected type: phone, beacon, wearable, audio, etc. |
| status | known, unknown, or new |
| manufacturer | Detected manufacturer from BLE data |

### Storage Estimates

| Scan Interval | Devices/Scan | Daily Size | 32GB Duration |
|---------------|--------------|------------|---------------|
| 10 seconds | 10 | ~50 MB | ~640 days |
| 10 seconds | 50 | ~250 MB | ~128 days |
| 30 seconds | 10 | ~17 MB | ~1900 days |

### SD Card Status

- **Green LED blink:** Successful write
- **No SD card:** Logging disabled, display shows "No SD" indicator
- **SD card full:** Oldest logs automatically deleted (configurable)

### Retrieving Logs

#### Option 1: Download via WiFi (Recommended)

The scanner runs a web server when connected to WiFi, allowing you to download logs without removing the SD card.

**Using the download script:**

```bash
# Auto-discover scanner and download all logs
./download-logs.sh

# Specify scanner IP address
./download-logs.sh 192.168.1.100

# Download to specific directory
./download-logs.sh 192.168.1.100 ./my-logs
```

**Manual download:**

1. Find the scanner's IP address (shown in serial output or on display)
2. Open browser: `http://<scanner-ip>/`
3. Use the web interface to browse and download logs

**API Endpoints:**

| Endpoint | Description |
|----------|-------------|
| `/` | Web interface with status and links |
| `/logs` | JSON list of available log files |
| `/download?file=FILENAME` | Download a specific log file |
| `/status` | JSON with current scanner status and detected devices |

**Example using curl:**

```bash
# List available logs
curl http://192.168.1.100/logs

# Download a specific log file
curl -O http://192.168.1.100/download?file=2024-12-23.csv

# Get current status
curl http://192.168.1.100/status
```

#### Option 2: Remove SD Card

1. Power off the scanner
2. Remove the SD card
3. Insert into computer and copy `/ble-logs/` folder
4. Import CSV files into spreadsheet or analysis tool

## Device Management

### Whitelist

Known/trusted devices are stored in SPIFFS and persist across reboots.

**Adding devices:**
1. Long-press device on display → "Add to Whitelist"
2. Or edit `whitelist.json` directly

**Whitelist format (`/whitelist.json` in SPIFFS):**
```json
{
  "devices": [
    {
      "mac": "AA:BB:CC:DD:EE:FF",
      "name": "John's iPhone",
      "type": "phone",
      "added": "2024-01-15T10:30:00Z"
    },
    {
      "mac": "11:22:33:44:55:66",
      "name": "Office Beacon",
      "type": "beacon",
      "added": "2024-01-10T08:00:00Z"
    }
  ]
}
```

### Device Types

The scanner attempts to identify device types from:
- Manufacturer data (Apple, Samsung, Google, etc.)
- Service UUIDs (Heart rate = wearable, Audio = headphones, etc.)
- Device name patterns

## Alerting

### WiFi Webhook (Optional)

When enabled, sends HTTP POST for unknown devices:

```json
{
  "event": "unknown_device",
  "mac": "XX:XX:XX:XX:XX:XX",
  "name": "Device Name",
  "rssi": -65,
  "timestamp": "2024-01-15T14:30:00Z",
  "scanner_id": "office-scanner-01"
}
```

Configure in `secrets.h`:
```cpp
const char* ALERT_WEBHOOK_URL = "https://your-server.com/ble-alert";
```

### Serial Logging

All events logged to serial (115200 baud):
```
[14:30:05] SCAN: Found 8 devices
[14:30:05] KNOWN: AA:BB:CC:DD:EE:FF (iPhone-John) -45dBm
[14:30:05] ALERT: XX:XX:XX:XX:XX:XX (Unknown) -78dBm
[14:30:05] NEW: 11:22:33:44:55:66 (Galaxy-A52) -55dBm
```

## Troubleshooting

### Display White Screen

**Most common cause:** Wrong TFT_eSPI configuration.

1. **Replace entire `User_Setup.h`** with the configuration in [Critical TFT_eSPI Configuration](#critical-tft_espi-configuration)
2. Ensure these are set correctly:
   - `ST7796_DRIVER` enabled (NOT `ILI9341_DRIVER`)
   - `USE_HSPI_PORT` defined
   - Pin numbers: MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, BL=27
3. **Recompile after any `User_Setup.h` changes** - the library caches settings

### Display Black Screen

1. Check `TFT_BL 27` and `TFT_BACKLIGHT_ON HIGH` are defined
2. Verify USB power supply is adequate (500mA minimum)
3. Try reducing `SPI_FREQUENCY` to 40000000 if display is unstable

### Sketch Too Big Error

The BLE + WiFi + Display libraries require more than the default 1.2MB partition:

```bash
# Use huge_app partition (3MB)
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .
```

Or use `./deploy.sh` which sets this automatically.

### No Devices Found

1. BLE scanning starts ~5 seconds after boot - wait for first scan
2. Check serial monitor for BLE initialization errors
3. WiFi connection happens first; BLE scans after WiFi connects
4. If WiFi is slow to connect, BLE scanning is delayed

### WiFi Connection Issues

1. ESP32 only supports 2.4GHz networks (not 5GHz)
2. Verify credentials in `secrets.h`
3. WiFi is optional - scanner works without it (no webhooks/NTP)
4. Check serial monitor for connection status codes

### Touch Not Working

The ESP32-3248S035C uses a **GT911 capacitive touch controller** (I2C), NOT the resistive XPT2046. Touch is currently a placeholder - the scanner works fully without it. To add touch support:

1. Install a GT911 library
2. Implement the `getTouchPoint()` function in the code

### SD Card Not Detected

1. Format card as FAT32 (not exFAT)
2. Use cards up to 32GB
3. SD card is optional - scanner works without it
4. Check serial monitor for SD initialization messages

### Memory Issues

If scanner becomes unstable with many devices:
- Reduce `MAX_TRACKED_DEVICES` (default: 50)
- Increase `SCAN_INTERVAL` to reduce processing load
- Check heap memory in serial monitor

## Project Structure

```
ble-scanner/
├── ble-scanner.ino           # Main application (~1200 lines)
├── deploy.sh                 # Build/upload/monitor script
├── download-logs.sh          # Download logs from scanner via WiFi
├── secrets.h                 # WiFi credentials (gitignored)
├── secrets.h.example         # Credentials template
├── README.md                 # This file
├── CLAUDE.md                 # AI assistant guidance
├── PROJECT-SUMMARY.md        # Project overview
├── .gitignore                # Git exclusions
├── build/                    # Compiled binaries (gitignored)
├── logs/                     # Downloaded log files (gitignored)
└── ESP32-320-480-display-horizontal/  # Reference project
```

## Security Considerations

### What This Scanner Detects

- **BLE Advertisements:** Public broadcasts from devices
- **Device Names:** If the device broadcasts one
- **MAC Addresses:** Hardware identifiers (may be randomized)
- **Signal Strength:** Approximate distance estimation

### Limitations

- **MAC Randomization:** Modern phones randomize BLE MAC addresses for privacy. The same phone may appear as different devices over time.
- **Hidden Devices:** Devices not actively advertising won't be detected
- **Range:** ESP32 BLE range is typically 10-30 meters indoors
- **Classic Bluetooth:** This scanner focuses on BLE; classic Bluetooth inquiry is possible but more power-intensive

### Privacy Note

This tool is intended for monitoring your own private business premises. Ensure compliance with local privacy laws regarding wireless monitoring. The scanner only receives publicly broadcast BLE advertisements - it does not intercept communications.

## Development Notes

### BLE + WiFi Coexistence

ESP32 shares a single radio for WiFi and Bluetooth. To minimize conflicts:
- BLE scanning pauses during WiFi transmissions
- WiFi alerts are batched and sent periodically
- Consider disabling WiFi if not needed for alerts

### Memory Management

- Device list uses fixed array (default: 50 devices max)
- Oldest/weakest signal devices pruned when full
- Whitelist stored in SPIFFS (persists across reboots)
- JSON parsing uses 4KB buffer

### Power Consumption

For 24/7 operation:
- Display backlight is the largest power draw
- Consider dimming/sleep modes for overnight operation
- Typical consumption: ~150mA active, ~80mA with display dimmed

## License

MIT License - See LICENSE file

## Acknowledgments

- TFT_eSPI library by Bodmer
- ESP32 Arduino Core by Espressif
- ArduinoJson by Benoit Blanchon

---

**Last Updated:** December 2024
**Target Hardware:** ESP32-3248S035C (Sunton/DIYmall)
**ESP32 Core:** 3.x
