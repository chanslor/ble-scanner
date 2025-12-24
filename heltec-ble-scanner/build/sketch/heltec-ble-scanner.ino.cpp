#include <Arduino.h>
#line 1 "/home/cursor/SECURITY/ble-scanner/heltec-ble-scanner/heltec-ble-scanner.ino"
/*
 * Heltec BLE Security Scanner
 *
 * BLE device scanner with HTTPS posting for Heltec WiFi LoRa 32 V3.
 * Tests if ESP32-S3 can handle BLE + HTTPS simultaneously.
 *
 * Hardware: Heltec WiFi LoRa 32 V3 (ESP32-S3)
 * Display: Built-in 0.96" OLED (128x64)
 *
 * Author: BLE Scanner Project
 * Date: December 2024
 */

// ============================================================================
// Board Selection - Set to match your hardware
// ============================================================================

// #define HELTEC_V2  // Uncomment for V2
#define HELTEC_V3     // Default: V3 (ESP32-S3)

// ============================================================================
// Includes
// ============================================================================

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Heltec OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "secrets.h"

// ============================================================================
// Pin Definitions for Heltec V3
// ============================================================================

#ifdef HELTEC_V3
  #define OLED_SDA 17
  #define OLED_SCL 18
  #define OLED_RST 21
  #define VEXT_PIN 36  // Controls OLED power
#else  // V2
  #define OLED_SDA 4
  #define OLED_SCL 15
  #define OLED_RST 16
  #define VEXT_PIN -1  // Not used on V2
#endif

// ============================================================================
// Display Configuration
// ============================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

// ============================================================================
// BLE Scanning Constants
// ============================================================================

#define SCAN_DURATION 5           // Seconds per scan cycle
#define SCAN_INTERVAL 15000       // Milliseconds between scans
#define DEVICE_TIMEOUT 120000     // Milliseconds before device removed (2 min)
#define MAX_TRACKED_DEVICES 30    // Maximum devices to track in memory

// ============================================================================
// Data Structures
// ============================================================================

struct BLEDeviceInfo {
  String mac;
  String name;
  int rssi;
  String deviceType;
  String manufacturer;
  unsigned long lastSeen;
};

// ============================================================================
// Global Variables
// ============================================================================

BLEScan* pBLEScan;
BLEDeviceInfo devices[MAX_TRACKED_DEVICES];
int deviceCount = 0;

unsigned long lastScanTime = 0;
unsigned long lastPostTime = 0;
bool scanInProgress = false;
unsigned long scanStartTime = 0;

bool wifiConnected = false;
int postSuccessCount = 0;
int postFailCount = 0;

// ============================================================================
// Forward Declarations
// ============================================================================

void initDisplay();
void initBLE();
void initWiFi();
void startBLEScan();
void processDevice(BLEAdvertisedDevice& device);
void updateDeviceList(String mac, String name, int rssi, String deviceType, String manufacturer);
void pruneStaleDevices();
void updateDisplay();
void postLogsToServer();
String detectDeviceType(BLEAdvertisedDevice& device);
String detectManufacturer(BLEAdvertisedDevice& device);

// ============================================================================
// BLE Scan Callback Class
// ============================================================================

class BLEScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    processDevice(advertisedDevice);
  }
};

// ============================================================================
// Setup
// ============================================================================

#line 135 "/home/cursor/SECURITY/ble-scanner/heltec-ble-scanner/heltec-ble-scanner.ino"
void setup();
#line 191 "/home/cursor/SECURITY/ble-scanner/heltec-ble-scanner/heltec-ble-scanner.ino"
void loop();
#line 135 "/home/cursor/SECURITY/ble-scanner/heltec-ble-scanner/heltec-ble-scanner.ino"
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("Heltec BLE Security Scanner");
  Serial.println("Testing BLE + HTTPS on ESP32-S3");
  Serial.println("========================================\n");

  // Enable Vext power for OLED (V3)
  #ifdef HELTEC_V3
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);  // LOW = ON
    delay(100);
    Serial.println("Vext power enabled for OLED");
  #endif

  // Initialize display
  initDisplay();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("BLE Scanner");
  display.println("Initializing...");
  display.display();

  // Initialize WiFi
  initWiFi();

  // Initialize BLE
  initBLE();

  // Show ready
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Ready!");
  if (wifiConnected) {
    display.println(WiFi.localIP());
  }
  display.display();
  delay(1000);

  // Force immediate scan
  lastScanTime = millis() - SCAN_INTERVAL;

  Serial.println("\nInitialization complete!");
  Serial.printf("Free heap: %d bytes, Largest block: %d bytes\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  unsigned long currentTime = millis();

  // Start new scan if interval elapsed
  if (!scanInProgress && (currentTime - lastScanTime >= SCAN_INTERVAL)) {
    startBLEScan();
  }

  // Check if scan is complete
  if (scanInProgress && (millis() - scanStartTime >= (SCAN_DURATION * 1000 + 500))) {
    scanInProgress = false;
    lastScanTime = millis();

    // Prune stale devices
    pruneStaleDevices();

    // Update display
    updateDisplay();

    // Post to server
    if (wifiConnected && deviceCount > 0) {
      postLogsToServer();
    }

    Serial.printf("Scan complete. Tracking %d devices. Posts: %d OK, %d fail\n",
                  deviceCount, postSuccessCount, postFailCount);
  }

  delay(10);
}

// ============================================================================
// Initialization Functions
// ============================================================================

void initDisplay() {
  #ifdef HELTEC_V3
    Wire.begin(OLED_SDA, OLED_SCL);
  #else
    Wire.begin(OLED_SDA, OLED_SCL);
  #endif

  // Reset OLED
  if (OLED_RST >= 0) {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(20);
    digitalWrite(OLED_RST, HIGH);
    delay(20);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed!");
    return;
  }

  display.clearDisplay();
  display.display();
  Serial.println("OLED display initialized");
}

void initBLE() {
  Serial.printf("initBLE() - Heap: %d, Largest: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  BLEDevice::init("Heltec-Scanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new BLEScanCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("BLE initialized");
  Serial.printf("After BLE init - Heap: %d, Largest: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
}

void initWiFi() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("WiFi not configured");
    wifiConnected = false;
    return;
  }

  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("After WiFi - Heap: %d, Largest: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi connection failed");
  }
}

// ============================================================================
// BLE Scanning
// ============================================================================

void startBLEScan() {
  Serial.printf("Starting BLE scan... Heap: %d, Largest: %d\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  scanStartTime = millis();
  scanInProgress = true;

  pBLEScan->clearResults();
  bool started = pBLEScan->start(SCAN_DURATION, nullptr, false);

  Serial.printf("  Scan started: %s\n", started ? "true" : "false");
}

void processDevice(BLEAdvertisedDevice& device) {
  String mac = device.getAddress().toString().c_str();
  mac.toUpperCase();

  String name = device.haveName() ? device.getName().c_str() : "Unknown";
  int rssi = device.getRSSI();
  String deviceType = detectDeviceType(device);
  String manufacturer = detectManufacturer(device);

  updateDeviceList(mac, name, rssi, deviceType, manufacturer);
}

void updateDeviceList(String mac, String name, int rssi, String deviceType, String manufacturer) {
  unsigned long currentTime = millis();

  // Check if device already exists
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].mac == mac) {
      // Update existing device
      devices[i].rssi = rssi;
      devices[i].lastSeen = currentTime;
      if (name != "Unknown" && devices[i].name == "Unknown") {
        devices[i].name = name;
      }
      return;
    }
  }

  // New device - add to list
  if (deviceCount >= MAX_TRACKED_DEVICES) {
    // Remove oldest device
    int oldestIdx = 0;
    for (int i = 1; i < deviceCount; i++) {
      if (devices[i].lastSeen < devices[oldestIdx].lastSeen) {
        oldestIdx = i;
      }
    }
    for (int i = oldestIdx; i < deviceCount - 1; i++) {
      devices[i] = devices[i + 1];
    }
    deviceCount--;
  }

  // Add new device
  devices[deviceCount].mac = mac;
  devices[deviceCount].name = name;
  devices[deviceCount].rssi = rssi;
  devices[deviceCount].deviceType = deviceType;
  devices[deviceCount].manufacturer = manufacturer;
  devices[deviceCount].lastSeen = currentTime;
  deviceCount++;

  Serial.printf("NEW: %s (%s) RSSI: %d\n", name.c_str(), mac.c_str(), rssi);
}

void pruneStaleDevices() {
  unsigned long currentTime = millis();
  int i = 0;

  while (i < deviceCount) {
    if (currentTime - devices[i].lastSeen > DEVICE_TIMEOUT) {
      for (int j = i; j < deviceCount - 1; j++) {
        devices[j] = devices[j + 1];
      }
      deviceCount--;
    } else {
      i++;
    }
  }
}

// ============================================================================
// Device Type Detection
// ============================================================================

String detectDeviceType(BLEAdvertisedDevice& device) {
  if (device.haveManufacturerData()) {
    String mfgData = device.getManufacturerData();
    if (mfgData.length() >= 2) {
      uint16_t mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
      if (mfgId == 0x004C) return "Apple";
      if (mfgId == 0x0075) return "Samsung";
      if (mfgId == 0x00E0) return "Google";
    }
  }
  return "Unknown";
}

String detectManufacturer(BLEAdvertisedDevice& device) {
  if (device.haveManufacturerData()) {
    String mfgData = device.getManufacturerData();
    if (mfgData.length() >= 2) {
      uint16_t mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);
      switch (mfgId) {
        case 0x004C: return "Apple";
        case 0x0075: return "Samsung";
        case 0x00E0: return "Google";
        case 0x0006: return "Microsoft";
        default: break;
      }
    }
  }
  return "Unknown";
}

// ============================================================================
// Display Update
// ============================================================================

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setCursor(0, 0);
  display.printf("BLE Scanner [%d]", deviceCount);

  // WiFi status
  display.setCursor(0, 10);
  if (wifiConnected) {
    display.printf("WiFi: %s", WiFi.localIP().toString().c_str());
  } else {
    display.print("WiFi: Disconnected");
  }

  // POST stats
  display.setCursor(0, 20);
  display.printf("POST: %d OK, %d fail", postSuccessCount, postFailCount);

  // Memory
  display.setCursor(0, 30);
  display.printf("Heap: %dK/%dK", ESP.getFreeHeap()/1024, ESP.getMaxAllocHeap()/1024);

  // Show first few devices
  display.setCursor(0, 42);
  display.println("--- Devices ---");

  int showCount = min(deviceCount, 2);
  for (int i = 0; i < showCount; i++) {
    display.setCursor(0, 52 + (i * 10));
    String dispName = devices[i].name.substring(0, 12);
    display.printf("%s %d", dispName.c_str(), devices[i].rssi);
  }

  display.display();
}

// ============================================================================
// Server POST - The key test!
// ============================================================================

void postLogsToServer() {
  if (strlen(BLE_SERVER_URL) == 0) return;
  if (strlen(BLE_API_KEY) == 0 || strcmp(BLE_API_KEY, "CHANGE_ME_AFTER_DEPLOY") == 0) {
    Serial.println("WARNING: API key not configured");
    return;
  }

  Serial.println("=== HTTPS POST Test ===");
  Serial.printf("  Heap before POST: %d, Largest: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Build JSON payload - send most recently seen devices
  int devicesToSend = min(deviceCount, 10);

  // Create array of indices sorted by lastSeen (most recent first)
  int sortedIndices[MAX_TRACKED_DEVICES];
  for (int i = 0; i < deviceCount; i++) {
    sortedIndices[i] = i;
  }
  // Simple bubble sort by lastSeen descending
  for (int i = 0; i < deviceCount - 1; i++) {
    for (int j = 0; j < deviceCount - i - 1; j++) {
      if (devices[sortedIndices[j]].lastSeen < devices[sortedIndices[j+1]].lastSeen) {
        int temp = sortedIndices[j];
        sortedIndices[j] = sortedIndices[j+1];
        sortedIndices[j+1] = temp;
      }
    }
  }

  StaticJsonDocument<2048> doc;
  doc["scanner_id"] = SCANNER_ID;

  JsonArray devicesArray = doc.createNestedArray("devices");
  for (int i = 0; i < devicesToSend; i++) {
    int idx = sortedIndices[i];  // Use sorted index
    JsonObject devObj = devicesArray.createNestedObject();
    devObj["mac"] = devices[idx].mac;
    devObj["name"] = devices[idx].name;
    devObj["rssi"] = devices[idx].rssi;
    devObj["device_type"] = devices[idx].deviceType;
    devObj["manufacturer"] = devices[idx].manufacturer;
  }

  char payload[2048];
  serializeJson(doc, payload, sizeof(payload));

  Serial.printf("  Posting %d devices (%d bytes)...\n", devicesToSend, strlen(payload));

  // Attempt HTTPS POST (the key test!)
  HTTPClient http;

  if (!http.begin(BLE_SERVER_URL)) {
    Serial.println("  http.begin() failed!");
    postFailCount++;
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + BLE_API_KEY);
  http.setTimeout(15000);

  int httpCode = http.POST(payload);

  Serial.printf("  Heap after POST: %d, Largest: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  if (httpCode == 200) {
    Serial.printf("  SUCCESS! HTTP %d\n", httpCode);
    postSuccessCount++;
  } else if (httpCode > 0) {
    Serial.printf("  HTTP Error: %d\n", httpCode);
    postFailCount++;
  } else {
    Serial.printf("  Connection Error: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
    postFailCount++;
  }

  http.end();
  Serial.println("=== POST Complete ===\n");
}

