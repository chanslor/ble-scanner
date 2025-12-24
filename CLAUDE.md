# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---
## ⚠️ IMPORTANT: ALWAYS USE deploy.sh ⚠️

**NEVER run arduino-cli commands directly.** Always use the deploy.sh script:

```bash
./deploy.sh          # Compile and upload (DEFAULT - use this!)
./deploy.sh compile  # Compile only
./deploy.sh upload   # Upload only
./deploy.sh monitor  # Serial monitor (leave to user in separate terminal)
```

The deploy.sh script ensures correct board settings, partition scheme, and build paths.

---

## Project Overview

ESP32-based BLE (Bluetooth Low Energy) security scanner for business premises monitoring. Runs on ESP32-3248S035C development board (Sunton/DIYmall) with 480x320 TFT display. Continuously scans for Bluetooth devices, displays them with signal strength and trust status, and can alert on unknown devices.

**Hardware:** ESP32-3248S035C with ST7796 3.5" TFT display (480x320 pixels, SPI interface), capacitive touch

**Purpose:** Detect unauthorized Bluetooth devices in private business areas for security monitoring

## Development Commands

### Using deploy.sh (REQUIRED)

```bash
./deploy.sh          # Compile and upload (most common)
./deploy.sh compile  # Compile only
./deploy.sh upload   # Upload only
./deploy.sh monitor  # Serial monitor (run in separate terminal)
```

**Note:** The user should run `./deploy.sh monitor` in their own terminal window. Claude should NOT run the monitor command as it blocks the session.

### Manual Arduino CLI (Reference Only - DO NOT USE)

These commands are what deploy.sh runs internally. **Do not run these directly:**

```bash
# Compile with huge_app partition (REQUIRED - default partition too small)
arduino-cli compile --build-path ./build \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .

# Upload to board
arduino-cli upload -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  --input-dir ./build .

# Monitor serial output
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Find connected device port
ls /dev/ttyUSB* /dev/ttyACM*
```

### Build Info

| Metric | Value |
|--------|-------|
| Partition Scheme | `huge_app` (3MB APP / No OTA / 1MB SPIFFS) |
| Binary Size | ~1.9MB (60% of 3MB) |
| RAM Usage | ~70KB (21% of 327KB) |
| Compile Time | ~60 seconds |

**Build Artifacts:** Compiled binaries stored in `build/` directory (gitignored).

## Architecture

### Core Components

1. **BLE Scanner Module**
   - Uses ESP32 BLEDevice library
   - Active scanning with configurable duration/interval
   - Extracts: MAC address, device name, RSSI, manufacturer data, service UUIDs

2. **Device Manager**
   - Tracks detected devices in memory (fixed array, default 50 max)
   - Manages whitelist (trusted devices) stored in SPIFFS
   - Classifies devices: known (green), unknown (red), new (yellow)
   - Prunes stale devices not seen within timeout period

3. **Display Renderer**
   - TFT_eSPI library for ST7796 driver
   - 480x320 landscape orientation
   - Header bar + scrollable device list (6 visible at once)
   - RSSI signal bars, status indicators, device type icons

4. **Touch Handler**
   - Tap: View device details
   - Long press: Add/remove from whitelist
   - Swipe: Scroll device list

5. **SD Card Logger**
   - Writes all scan results to CSV files on built-in SD card
   - Daily log rotation (YYYY-MM-DD.csv format)
   - Automatic directory creation (/ble-logs/)
   - Graceful handling when SD card not present

6. **Audio Alert System**
   - SC8002B amplifier with P4 speaker connector (GPIO 26)
   - Alternative: Piezo buzzer on GPIO 22
   - Different tone patterns for device states
   - PWM-based tone generation using LEDC

7. **WiFi Alert System (Optional)**
   - WiFi connectivity for webhook notifications
   - HTTP POST on unknown device detection
   - Serial logging for all events

### Data Model

```cpp
struct BLEDeviceInfo {
  String mac;              // MAC address (may be randomized)
  String name;             // Advertised name (or "Unknown")
  int rssi;                // Signal strength in dBm
  String deviceType;       // Detected type: phone, beacon, wearable, etc.
  bool isKnown;            // On whitelist
  bool isNew;              // Seen < 5 minutes
  unsigned long firstSeen; // Timestamp of first detection
  unsigned long lastSeen;  // Timestamp of most recent detection
  String manufacturer;     // Manufacturer from data (Apple, Samsung, etc.)
};
```

### Key Constants

```cpp
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 30
#define DEVICE_ROW_HEIGHT 46
#define MAX_VISIBLE_DEVICES 6
#define MAX_TRACKED_DEVICES 50

#define SCAN_DURATION 5        // Seconds per scan cycle
#define SCAN_INTERVAL 10       // Seconds between scans
#define DEVICE_TIMEOUT 60      // Seconds before device removed
#define NEW_DEVICE_THRESHOLD 300  // Seconds to show as "new"
```

### Color Scheme

```cpp
#define COLOR_KNOWN    0x07E0   // Green - trusted device
#define COLOR_UNKNOWN  0xF800   // Red - alert, unknown device
#define COLOR_NEW      0xFFE0   // Yellow - recently discovered
#define COLOR_FADING   0x7BEF   // Gray - signal lost
#define COLOR_HEADER   0x001F   // Blue - header background
#define COLOR_BG       0x0000   // Black - main background
#define COLOR_TEXT     0xFFFF   // White - default text
```

## Execution Flow

### Startup Sequence

1. Initialize TFT display (480x320 landscape)
2. Display splash screen "BLE Scanner - Initializing..."
3. Initialize SPIFFS, load whitelist from `/whitelist.json`
4. Initialize BLE with scan callbacks
5. (Optional) Connect to WiFi if credentials configured
6. Begin first BLE scan
7. Transition to main display

### Main Loop

```
┌─────────────────────────────────────────────┐
│                 Main Loop                    │
├─────────────────────────────────────────────┤
│ 1. Check if scan interval elapsed           │
│    → Start new BLE scan                     │
│                                             │
│ 2. Process scan results (callback-driven)   │
│    → Add/update devices in tracking array   │
│    → Classify: known/unknown/new            │
│    → Trigger alert if unknown detected      │
│                                             │
│ 3. Prune stale devices (timeout expired)    │
│                                             │
│ 4. Handle touch input                       │
│    → Scroll, select, whitelist actions      │
│                                             │
│ 5. Update display                           │
│    → Redraw changed elements only           │
│    → Update header with device count/time   │
│                                             │
│ 6. Send queued alerts (if WiFi enabled)     │
└─────────────────────────────────────────────┘
```

### BLE Scan Callback

The `onResult` callback fires for each discovered device during a scan:

```cpp
void onResult(BLEAdvertisedDevice device) {
  // Extract device info
  // Check against whitelist
  // Update or add to tracking array
  // Queue alert if unknown
}
```

## Display Layout

```
┌─────────────────────────────────────────────────────┐
│ BLE SCANNER    [N devices]     Last: Xs ago  (30px)│
├─────────────────────────────────────────────────────┤
│ ● Device Name      RSSI   [bars]   Type     (46px) │
│ ● Device Name      RSSI   [bars]   Type     (46px) │
│ ● Device Name      RSSI   [bars]   Type     (46px) │
│ ● Device Name      RSSI   [bars]   Type     (46px) │
│ ● Device Name      RSSI   [bars]   Type     (46px) │
│ ● Device Name      RSSI   [bars]   Type     (46px) │
└─────────────────────────────────────────────────────┘
```

**Row Layout (46px height):**
- X=5: Status dot (color indicates trust status)
- X=25: Device name (truncated to 14 chars)
- X=200: RSSI value (e.g., "-65dBm")
- X=280: Signal bars (10 segments)
- X=400: Device type label

## SPIFFS Storage

### Whitelist File: `/whitelist.json`

```json
{
  "devices": [
    {
      "mac": "AA:BB:CC:DD:EE:FF",
      "name": "Friendly Name",
      "type": "phone",
      "added": "2024-01-15T10:30:00Z"
    }
  ]
}
```

### Configuration File: `/config.json` (optional)

```json
{
  "scan_duration": 5,
  "scan_interval": 10,
  "device_timeout": 60,
  "wifi_enabled": true,
  "alert_webhook": "https://...",
  "scanner_id": "office-01"
}
```

## SD Card Storage

### Hardware Pins (ESP32-3248S035C)

```cpp
#define SD_CS    5     // SD Card chip select
#define SD_MOSI  23    // SD Card MOSI
#define SD_MISO  19    // SD Card MISO
#define SD_SCK   18    // SD Card clock
```

**Note:** SD card uses a separate SPI bus from the TFT display.

### Log File Structure

```
SD Card Root/
└── ble-logs/
    ├── 2024-01-15.csv
    ├── 2024-01-16.csv
    └── ...
```

### CSV Format

```csv
timestamp,mac,name,rssi,device_type,status,manufacturer
2024-01-15T14:30:05,AA:BB:CC:DD:EE:FF,iPhone-John,-45,phone,known,Apple
```

### SD Card Functions

```cpp
bool initSDCard();              // Initialize SD card, create /ble-logs/ dir
bool isSDCardPresent();         // Check if SD card is mounted
void logDeviceToSD(BLEDeviceInfo& device);  // Append device to daily CSV
String getLogFilename();        // Returns current date filename
void rotateLogsIfNeeded();      // Delete old logs if card is full
```

### Error Handling

- If SD card not present: continue operation, display "No SD" indicator
- If write fails: retry once, then skip and continue scanning
- If card full: delete oldest log files (configurable retention period)

## Audio System

### Hardware Options

**Option 1: P4 Speaker Connector (SC8002B Amplifier)**
```cpp
#define AUDIO_PIN 26        // GPIO 26 controls SC8002B amplifier
#define AUDIO_CHANNEL 0     // LEDC channel for PWM
```

**Option 2: Piezo Buzzer**
```cpp
#define BUZZER_PIN 22       // GPIO 22 available on P3 header
#define BUZZER_CHANNEL 1    // LEDC channel for PWM
```

### Tone Generation

Uses ESP32 LEDC (LED Control) peripheral for PWM tone generation:

```cpp
void playTone(int frequency, int duration_ms) {
  ledcSetup(AUDIO_CHANNEL, frequency, 8);  // 8-bit resolution
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  ledcWrite(AUDIO_CHANNEL, 128);           // 50% duty cycle
  delay(duration_ms);
  ledcWrite(AUDIO_CHANNEL, 0);             // Silence
}
```

### Alert Patterns

```cpp
void alertUnknownDevice() {
  // 3 short urgent beeps at 2kHz
  for (int i = 0; i < 3; i++) {
    playTone(2000, 100);
    delay(100);
  }
}

void alertNewDevice() {
  // Single short beep at 1kHz
  playTone(1000, 150);
}

void alertWhitelistAdded() {
  // Two ascending tones
  playTone(800, 150);
  delay(50);
  playTone(1200, 200);
}
```

### Important Notes

- **100Ω Resistor Required:** When using speaker via P4, a 100Ω series resistor prevents brownout resets
- **Volume Control:** Adjust PWM duty cycle (0-255) to control volume
- **Non-blocking:** Consider using timer interrupts for tones to avoid blocking BLE scans

## Key Implementation Details

### Device Type Detection

Identify device type from manufacturer data and service UUIDs:

```cpp
String detectDeviceType(BLEAdvertisedDevice& device) {
  // Check manufacturer ID
  // 0x004C = Apple (iPhone, AirPods, Watch, etc.)
  // 0x0075 = Samsung
  // 0x00E0 = Google

  // Check service UUIDs
  // 0x180D = Heart Rate → Wearable
  // 0x180F = Battery → Generic BLE
  // 0x1812 = HID → Keyboard/Mouse
  // 0xFE9F = Google Nearby → Phone

  // Check name patterns
  // "AirPods", "Galaxy Buds" → Audio
  // "Tile", "AirTag" → Tracker
  // "Beacon", "iBeacon" → Beacon

  return "Unknown";
}
```

### RSSI to Signal Bars

```cpp
int rssiToBars(int rssi) {
  // 10-segment bar display
  if (rssi >= -50) return 10;      // Excellent
  if (rssi >= -60) return 8;       // Very Good
  if (rssi >= -70) return 6;       // Good
  if (rssi >= -80) return 4;       // Fair
  if (rssi >= -90) return 2;       // Weak
  return 1;                         // Very Weak
}
```

### MAC Address Randomization Handling

Modern devices randomize BLE MAC addresses. Strategies:
- Track by name + manufacturer when available
- Note that same device may appear as multiple entries
- Display warning icon for randomized MACs (first byte LSB = 1)

## BLE + WiFi/HTTPS Coexistence - CRITICAL

ESP32 shares a single 2.4GHz radio for WiFi and Bluetooth. **There is also a critical heap memory fragmentation issue when using BLE with HTTPS.**

### The Problem: BLE Fragments Heap, Breaking HTTPS

**Discovery Date:** December 2024

**Symptoms:**
- HTTPS POST works perfectly BEFORE BLE initialization
- HTTPS POST fails with "connection refused" (-1) AFTER BLE starts
- Plain TCP connections work fine after BLE starts
- Error persists even when BLE scan is stopped
- Error persists even with `btStop()` called

**Root Cause:**
The ESP32 BLE stack allocates memory in a way that severely fragments the heap. SSL/TLS requires a **contiguous memory block of 16-20KB** for the handshake buffers. After BLE runs:

| Condition | Free Heap | Largest Contiguous Block |
|-----------|-----------|-------------------------|
| Before BLE init | ~82KB | ~73KB ✓ |
| After BLE running | ~30KB | ~4-8KB ✗ |
| After `pBLEScan->stop()` | ~30KB | ~8KB ✗ |
| After `btStop()` | ~40KB | ~15KB ✗ |
| After `BLEDevice::deinit(true)` | ~162KB | ~45KB ✓ |

The SSL library cannot allocate its buffers with only 4-15KB contiguous, even though total free heap appears sufficient.

### Failed Approaches

We tried many approaches that did NOT work:

1. **Simple `http.begin(url)`** - Caused `abort()` crash
2. **`WiFiClientSecure` with `setInsecure()`** - "connection refused"
3. **`NetworkClientSecure`** - Same issue
4. **`pBLEScan->stop()`** - Didn't free enough memory
5. **`btStop()` / `btStart()`** - Partially freed memory but BT restart failed with error -7
6. **Pre-allocating SSL client before BLE** - SSL buffers allocated lazily during handshake, not at object creation
7. **Reducing SSL buffer sizes** - `setBufferSizes()` not available in ESP32 Arduino Core 3.x
8. **Using HTTP instead of HTTPS** - Fly.io redirects (301) to HTTPS, can't disable

### The Solution: Disable Cloud POST, Use Local Web Server

**After extensive testing, BLE deinit/reinit does NOT work reliably.** While `BLEDevice::deinit(true)` frees enough memory for HTTPS POST, the BLE stack cannot be restarted - `pBLEScan->start()` returns `false` after reinit.

**Current implementation:**
- **Cloud POST is DISABLED** - HTTPS and BLE are incompatible on ESP32
- **Local web server works** - Access device data at `http://<IP>/status`
- **SD card logging works** - View/download logs at `http://<IP>/logs`

### Alternative Approaches (Not Implemented)

If cloud posting is needed, consider:

1. **Separate gateway device** - Raspberry Pi pulls from ESP32's local web server, posts to cloud
2. **MQTT instead of HTTPS** - Lower memory overhead, may work with BLE
3. **Periodic reboot** - POST on boot before BLE init, then scan until next scheduled reboot
4. **Different hosting** - Platform accepting plain HTTP (not Fly.io)

### Debugging Tools Used

When debugging this issue, these heap diagnostics were essential:

```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Largest block: %d bytes\n", ESP.getMaxAllocHeap());
```

The key insight was that **total free heap is misleading** - what matters is the **largest contiguous block**.

### Local Web Server Endpoints

- `http://<IP>/` - Dashboard with links
- `http://<IP>/status` - JSON with current devices and stats
- `http://<IP>/logs` - List SD card log files (JSON)
- `http://<IP>/download?file=FILENAME` - Download specific log file (CSV)

### Key Takeaways

1. **BLE severely fragments ESP32 heap** - This is a fundamental limitation
2. **SSL needs large contiguous blocks** - ~16-20KB minimum
3. **Total free heap is misleading** - Check `ESP.getMaxAllocHeap()`
4. **`BLEDevice::deinit(true)` frees memory but BREAKS BLE** - Cannot restart after deinit
5. **BLE + HTTPS are fundamentally incompatible on ESP32** - Use local web server instead
6. **Error messages can be misleading** - "connection refused" was actually a memory allocation failure

## Library Dependencies

- `BLEDevice.h` - ESP32 BLE library (included in ESP32 core)
- `BLEUtils.h` - BLE utilities
- `BLEScan.h` - BLE scanning
- `BLEAdvertisedDevice.h` - Advertised device handling
- `TFT_eSPI.h` - Display driver (requires User_Setup.h config)
- `ArduinoJson.h` - JSON parsing for whitelist/config
- `SPIFFS.h` - File storage for whitelist persistence
- `SD.h` - SD card access for logging
- `SPI.h` - SPI bus for SD card (separate from TFT SPI)
- `WiFi.h` - Optional WiFi connectivity
- `HTTPClient.h` - Optional webhook alerts

## Important Constraints

1. **TFT_eSPI Configuration:** Must replace entire `User_Setup.h` with ESP32-3248S035C configuration:
   - Driver: `ST7796_DRIVER` (NOT ILI9341)
   - Pins: MISO=12, MOSI=13, SCLK=14, CS=15, DC=2, RST=-1, BL=27
   - SPI: `USE_HSPI_PORT` required
   - Speed: 80MHz
   - **White screen = wrong driver or pins. Recompile after changes.**

2. **Partition Scheme:** Must use `huge_app` partition (3MB). Default 1.2MB is too small for BLE+WiFi+Display.

3. **Memory Limits:**
   - Max 50 tracked devices (configurable)
   - 4KB JSON buffer for whitelist
   - Prune aggressively to prevent heap fragmentation

3. **Radio Sharing:** BLE and WiFi share antenna. Heavy WiFi usage impacts BLE scan reliability. Prefer BLE-only operation when possible.

4. **MAC Randomization:** Cannot reliably track devices with randomized MACs by address alone. Use name/manufacturer as secondary identifiers.

5. **Scan Duration vs. Responsiveness:** Longer scans find more devices but reduce UI responsiveness. Default 5-second scan balances both.

6. **SD Card SPI Bus:** The SD card uses a separate SPI bus (VSPI) from the TFT display (HSPI). Code uses `SPIClass sdSPI(VSPI)` to avoid conflicts.

7. **ESP32 Arduino Core 3.x API Changes:**
   - Use `ledcAttach(pin, freq, resolution)` instead of `ledcSetup()` + `ledcAttachPin()`
   - Use `ledcWriteTone(pin, freq)` for audio generation
   - `BLEScan::isScanning()` removed - use timer-based scan completion check

## Key Functions Reference

Main source file: `ble-scanner.ino`

- `setup()` - Initialize display, SPIFFS, BLE, optional WiFi
- `loop()` - Main execution loop (scan timing, display updates, touch handling)
- `startBLEScan()` - Initiate a new BLE scan cycle
- `onScanResult()` - Callback for each discovered device
- `updateDeviceList()` - Add/update device in tracking array
- `isDeviceKnown()` - Check MAC against whitelist
- `classifyDevice()` - Determine type from manufacturer/services
- `pruneStaleDevices()` - Remove devices not seen recently
- `drawDisplay()` - Full screen render
- `drawDeviceRow()` - Render single device row
- `handleTouch()` - Process touch events
- `addToWhitelist()` - Add device MAC to persistent whitelist
- `sendAlert()` - HTTP POST to webhook (if configured)
- `loadWhitelist()` - Read whitelist from SPIFFS
- `saveWhitelist()` - Write whitelist to SPIFFS
- `initSDCard()` - Initialize SD card and create log directory
- `logDeviceToSD()` - Write device detection to daily CSV log
- `getLogFilename()` - Generate date-based log filename
- `initAudio()` - Initialize LEDC PWM for audio output
- `playTone()` - Generate tone at specified frequency/duration
- `alertUnknownDevice()` - Play urgent alert pattern
- `alertNewDevice()` - Play new device notification

## Testing Checklist

- [ ] Display initializes correctly (no black screen)
- [ ] BLE scan finds nearby devices
- [ ] Known devices show green status
- [ ] Unknown devices show red status
- [ ] New devices show yellow status briefly
- [ ] RSSI bars reflect signal strength
- [ ] Touch scrolling works (if > 6 devices)
- [ ] Long-press adds device to whitelist
- [ ] Whitelist persists after reboot
- [ ] WiFi alerts send successfully (if configured)
- [ ] Serial logging shows all events
- [ ] Device timeout removes stale entries
- [ ] SD card detected and mounted
- [ ] CSV log files created with correct format
- [ ] Daily log rotation works correctly
- [ ] Scanner operates normally without SD card inserted
- [ ] Audio alert plays for unknown devices
- [ ] Different tones for different events
- [ ] Audio works with speaker (P4) or piezo buzzer (GPIO 22)
