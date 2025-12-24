# Heltec WiFi LoRa 32 Wiring Guide

## Board Information

The code supports both **Heltec WiFi LoRa 32 V2** and **V3** (ESP32-S3).

**Current Configuration**: **V3** (default in code)

To switch versions, edit line 40-41 in `ESP32_HYDRO_STATIC.ino`:
```cpp
// For V2 (older):
#define HELTEC_V2
// #define HELTEC_V3

// For V3 (newer, ESP32-S3) - CURRENT SETTING:
// #define HELTEC_V2
#define HELTEC_V3
```

## Pin Assignments

### Heltec WiFi LoRa 32 V2
- **Built-in OLED**: SDA=GPIO 4, SCL=GPIO 15, RST=GPIO 16 (shared I2C bus)
- **INA219 Sensor**: SDA=GPIO 4, SCL=GPIO 15 (shares bus with OLED)
- **LM393 Moisture Sensor**: AO=GPIO 36 (ADC1_CH0, input only)
- **LoRa Module**: SX1276/SX1278 on separate SPI pins

### Heltec WiFi LoRa 32 V3 (ESP32-S3)
- **Built-in OLED**: SDA=GPIO 17, SCL=GPIO 18, RST=GPIO 21 (internal, not on headers!)
- **INA219 Sensor**: SDA=GPIO 1, SCL=GPIO 2 (separate I2C bus, Header J3 Pins 12/13)
- **LM393 Moisture Sensor**: AO=GPIO 4 (Header J3 Pin 15, ADC1)
- **LoRa Module**: SX1262 on separate SPI pins
- **Important**: GPIO 17/18 are NOT accessible on pin headers - they're internal to the board

## INA219 Current Sensor Module

![INA219 Module](INA219-power-supply.png)

The **INA219** is a high-precision current sensor module that measures the 4-20 mA signal from the hydrostatic sensor.

**Key Features:**
- **I2C Interface**: Communicates with ESP32 via 2 wires (SDA/SCL)
- **High Precision**: ±0.8 mA accuracy for 4-20 mA measurements
- **Bi-directional**: Measures current in both directions
- **Low Power**: Operates on 3.3V from the Heltec board
- **Compact Size**: 25.5mm × 22.3mm module

**Pin Functions:**
- **VIN+ / VIN-**: Current loop connections (19.5V passes through here)
- **VCC**: 3.3V power from Heltec
- **GND**: Ground connection
- **SDA**: I2C data line to ESP32
- **SCL**: I2C clock line to ESP32

**What It Does:**
The INA219 sits in the 4-20 mA current loop and measures the exact current flowing through it. This current value is proportional to water depth, which the ESP32 then converts to inches/feet for display.

**Note:** The INA219 is optional. If not connected, the system will continue to operate with moisture readings only.

## LM393 Soil Moisture Sensor

![LM393 Soil Moisture Sensor](images/soil_moisture_sensor.jpg)

The **LM393** soil moisture sensor module detects moisture levels for rain detection and soil monitoring.

**Key Features:**
- **Analog Output**: Variable voltage proportional to moisture level
- **Digital Output**: Threshold-triggered output (adjustable via potentiometer)
- **Low Power**: Operates on 3.3V from the Heltec board
- **Compact Size**: Small module with separate probe

**Pin Functions:**
- **VCC**: 3.3V power from Heltec
- **GND**: Ground connection
- **AO**: Analog output to ESP32 ADC
- **DO**: Digital output (optional, not used in this project)

**How It Works:**
The probe has two exposed conductors. When inserted into soil or water, moisture between the probes changes the resistance. The LM393 comparator chip converts this resistance change to a voltage that the ESP32 reads via its ADC. Higher moisture = lower voltage = lower ADC reading.

## Wiring Connections

### INA219 Current Sensor → Heltec Board

**For V2 (shared I2C bus):**
```
INA219          Heltec WiFi LoRa 32 V2
------          ----------------------
VCC      --->   3V3
GND      --->   GND
SDA      --->   GPIO 4  (shares with built-in OLED)
SCL      --->   GPIO 15 (shares with built-in OLED)
```

**For V3 (separate I2C bus on Header J3):**
```
INA219          Heltec WiFi LoRa 32 V3 (Header J3 - Left Side)
------          ----------------------------------------------
VCC      --->   3V3 (Pin 2 or 3)
GND      --->   GND (Pin 1)
SDA      --->   GPIO 1  (Pin 12) ← Accessible on header
SCL      --->   GPIO 2  (Pin 13) ← Accessible on header

Note: GPIO 17/18 are used INTERNALLY by the built-in OLED
      and are NOT available on the pin headers!
```

### LM393 Soil Moisture Sensor → Heltec Board

**For V2:**
```
LM393           Heltec WiFi LoRa 32 V2
------          ----------------------
VCC      --->   3V3
GND      --->   GND
AO       --->   GPIO 36 (ADC1_CH0, input only)
DO       --->   (not connected)
```

**For V3 (Header J3 - Left Side):**
```
LM393           Heltec WiFi LoRa 32 V3 (Header J3 - Left Side)
------          ----------------------------------------------
VCC      --->   3V3 (Pin 2 or 3)
GND      --->   GND (Pin 1)
AO       --->   GPIO 4  (Pin 15) ← ADC input
DO       --->   (not connected)
```

### Current Loop (same for all versions)
```
Dell Power Supply → INA219 → Hydrostatic Sensor

Dell White (+19.5V) → INA219 VIN+
INA219 VIN-         → Sensor RED wire (+V)
Sensor BLACK wire   → Dell Black (GND)
All grounds common
```

## Important Notes

1. **No External OLED Needed**: The Heltec board has a built-in 0.96" OLED display
2. **INA219 is Optional**: If not connected, system continues in moisture-only mode
3. **I2C Bus Configuration**:
   - **V2**: Single I2C bus shared between OLED and INA219
   - **V3**: Two separate I2C buses (OLED on GPIO 17/18, INA219 on GPIO 1/2)
4. **I2C Addresses**:
   - INA219: 0x40
   - Built-in OLED: 0x3C
   - No conflicts on either version!
5. **V3 Critical Note**: GPIO 17/18 are internal-only pins. Use GPIO 1/2 for INA219 and GPIO 4 for moisture sensor.
6. **Moisture Sensor ADC**:
   - V2: Uses GPIO 36 (ADC1_CH0)
   - V3: Uses GPIO 4 (ADC1)

## Upload Instructions

### Connect Heltec Board
1. Connect Heltec to computer via USB-C cable
2. The board should appear as `/dev/ttyUSB0` or similar

### Compile and Upload

**For V2:**
```bash
arduino-cli compile --build-path ./build --fqbn esp32:esp32:heltec_wifi_lora_32_V2 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:heltec_wifi_lora_32_V2 --input-dir ./build .
```

**For V3 (current default):**
```bash
arduino-cli compile --build-path ./build --fqbn esp32:esp32:heltec_wifi_lora_32_V3 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:heltec_wifi_lora_32_V3 --input-dir ./build .
```

### Monitor Output
```bash
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

## Expected Output

### Serial Monitor (V3 Example - With INA219)
```
Heltec WiFi LoRa 32 - Hydrostatic Water Level Sensor
====================================================
Board: Heltec WiFi LoRa 32 V3
I2C: OLED on GPIO17/18, INA219 on GPIO1/2
INA219 initialized successfully
Built-in OLED display initialized successfully
Soil moisture sensor on GPIO4

Tank depth range: 0 - 100 cm
Current range: 4 - 20 mA

Starting measurements...
====================================================

--- Sensor Reading ---
Current: 7.46 mA
Depth: 8.5 in (21.6%)
Moisture: 45.2% (raw: 2850)
```

### Serial Monitor (V3 Example - Without INA219)
```
Heltec WiFi LoRa 32 - Hydrostatic Water Level Sensor
====================================================
Board: Heltec WiFi LoRa 32 V3
I2C: OLED on GPIO17/18, INA219 on GPIO1/2
Failed to find INA219 chip - water level disabled
Check wiring to GPIO1 (SDA) and GPIO2 (SCL)
Built-in OLED display initialized successfully
Soil moisture sensor on GPIO4

Starting measurements...
====================================================

--- Sensor Reading ---
Water level: N/A (INA219 not connected)
Moisture: 45.2% (raw: 2850)
```

### Built-in OLED Display
The built-in display will show:
- Tank percentage and Moisture percentage (header)
- Current in mA (if INA219 connected)
- Depth in inches (or feet + inches if > 12")
- Moisture percentage

If INA219 is not connected, display shows moisture-only mode with large moisture reading.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| OLED not working | Check you selected correct version (V2/V3 in code) |
| INA219 not found on V2 | Verify wiring to GPIO 4 (SDA) and GPIO 15 (SCL) - system continues in moisture-only mode |
| INA219 not found on V3 | Verify wiring to GPIO 1 (SDA) and GPIO 2 (SCL) - NOT GPIO 17/18! System continues in moisture-only mode |
| Moisture always 0% | V3: Check AO connected to GPIO 4. V2: Check AO connected to GPIO 36 |
| Moisture always 100% | Probe may be shorted or in water - check connections |
| Upload fails | Press and hold PRG button during upload |
| Board not detected | Install CP2102 USB-to-serial driver if needed |
| "Wrong chip" error on V3 | Use heltec_wifi_lora_32_V3 FQBN (ESP32-S3, not ESP32) |
| OLED blank but serial works | Enable Vext power: GPIO 36 = LOW (see below) |

## LoRa Wireless Network (Implemented!)

The Heltec board's built-in LoRa module is now used for a three-unit wireless network:

- **V3**: SX1262 LoRa chip @ 915 MHz (US ISM band)

**Network Architecture:**
```
[River Unit] --LoRa--> [Ridge Relay] --LoRa--> [Home Unit]
```

- **River Unit** (`river_unit/`) - Sensors + LoRa transmitter
- **Ridge Relay** (`ridge_relay/`) - Battery-powered repeater with deep sleep
- **Home Unit** (`home_unit/`) - LoRa receiver + OLED display

See **[LORA_SETUP.md](LORA_SETUP.md)** for complete setup instructions.

## Vext Power for OLED

**Important:** Some Heltec V3 boards require enabling Vext power (GPIO 36) for the OLED to work:

```cpp
pinMode(36, OUTPUT);
digitalWrite(36, LOW);  // LOW = ON
delay(100);
```

This is already included in the LoRa sketches. If your OLED is blank but serial works, this is likely the issue.

## Board Resources

**Heltec WiFi LoRa 32 V3 (ESP32-S3):**
- **MCU**: ESP32-S3 dual-core @ 240MHz
- **Flash Memory**: 4MB (8MB on some variants)
- **RAM**: 327KB SRAM
- **Current Flash Usage**: 331,979 bytes (9% of 4MB)
- **Current RAM Usage**: 21,888 bytes (6% of RAM)
- **Plenty of room** for LoRa, WiFi, and additional features!

**Heltec WiFi LoRa 32 V2:**
- **MCU**: ESP32 dual-core @ 240MHz
- **Flash Memory**: 4MB
- **RAM**: 320KB SRAM
