/*
 * BLE Security Scanner
 *
 * Bluetooth Low Energy device scanner for business security monitoring.
 * Runs on ESP32-3248S035C development board with 480x320 TFT display.
 *
 * Features:
 * - Continuous BLE scanning with device detection
 * - Real-time display with color-coded device status
 * - SD card logging of all detected devices
 * - Audio alerts for unknown devices
 * - Touch interface for whitelist management
 * - Optional WiFi webhook alerts
 *
 * Hardware: ESP32-3248S035C (Sunton/DIYmall)
 * Display: ST7796 480x320 TFT
 *
 * Author: Security Scanner Project
 * Date: December 2024
 */

// ============================================================================
// Includes
// ============================================================================

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <time.h>

#include "secrets.h"

// SD Card SPI instance (separate from TFT)
SPIClass sdSPI(VSPI);

// ============================================================================
// Pin Definitions
// ============================================================================

// SD Card (VSPI)
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

// Audio
#define AUDIO_PIN 26          // SC8002B amplifier on P4 connector
#define BUZZER_PIN 22         // Alternative: piezo buzzer on P3 header
#define AUDIO_CHANNEL 0       // LEDC channel for audio

// Touch (I2C) - handled by TFT_eSPI
#define TOUCH_SDA 33
#define TOUCH_SCL 32

// ============================================================================
// Display Constants
// ============================================================================

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define HEADER_HEIGHT 30
#define DEVICE_ROW_HEIGHT 46
#define MAX_VISIBLE_DEVICES 6
#define ROW_START_Y (HEADER_HEIGHT + 5)

// ============================================================================
// BLE Scanning Constants
// ============================================================================

#define SCAN_DURATION 5           // Seconds per scan cycle
#define SCAN_INTERVAL 10000       // Milliseconds between scans
#define DEVICE_TIMEOUT 60000      // Milliseconds before device removed
#define NEW_DEVICE_THRESHOLD 300000  // Milliseconds to show as "new" (5 min)
#define MAX_TRACKED_DEVICES 50    // Maximum devices to track in memory

// ============================================================================
// Color Definitions (RGB565)
// ============================================================================

#define COLOR_BG        0x0000    // Black background
#define COLOR_HEADER_BG 0x000F    // Dark blue header
#define COLOR_TEXT      0xFFFF    // White text
#define COLOR_KNOWN     0x07E0    // Green - trusted device
#define COLOR_UNKNOWN   0xF800    // Red - alert, unknown device
#define COLOR_NEW       0xFFE0    // Yellow - recently discovered
#define COLOR_FADING    0x7BEF    // Gray - signal lost
#define COLOR_RSSI_HIGH 0x07E0    // Green - strong signal
#define COLOR_RSSI_MED  0xFFE0    // Yellow - medium signal
#define COLOR_RSSI_LOW  0xF800    // Red - weak signal
#define COLOR_DIVIDER   0x3186    // Dark gray divider line

// ============================================================================
// Data Structures
// ============================================================================

struct BLEDeviceInfo {
  String mac;                     // MAC address
  String name;                    // Advertised name (or "Unknown")
  int rssi;                       // Signal strength in dBm
  String deviceType;              // Detected type: phone, beacon, etc.
  String manufacturer;            // Manufacturer from data
  bool isKnown;                   // On whitelist
  bool isNew;                     // Seen < 5 minutes
  unsigned long firstSeen;        // Timestamp of first detection
  unsigned long lastSeen;         // Timestamp of most recent detection
  bool alertSent;                 // Already alerted for this device
};

struct WhitelistEntry {
  String mac;
  String name;
  String type;
};

// ============================================================================
// Global Variables
// ============================================================================

// Display
TFT_eSPI tft = TFT_eSPI();

// BLE
BLEScan* pBLEScan;
bool scanInProgress = false;

// Device tracking
BLEDeviceInfo devices[MAX_TRACKED_DEVICES];
int deviceCount = 0;
int scrollOffset = 0;

// Whitelist
WhitelistEntry whitelist[50];
int whitelistCount = 0;

// Timing
unsigned long lastScanTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long scanStartTime = 0;

// SD Card
bool sdCardPresent = false;
String currentLogDate = "";

// WiFi
bool wifiConnected = false;

// Web Server
WebServer server(80);

// Audio
bool audioEnabled = true;
bool useSpeaker = true;  // true = P4 speaker, false = GPIO 22 piezo

// ============================================================================
// Forward Declarations
// ============================================================================

void initDisplay();
void initBLE();
void initSDCard();
void initAudio();
void initWiFi();
void testHttpsConnection();
void loadWhitelist();
void saveWhitelist();
void startBLEScan();
void processDevice(BLEAdvertisedDevice& device);
void updateDeviceList(String mac, String name, int rssi, String deviceType, String manufacturer);
bool isDeviceKnown(String mac);
String detectDeviceType(BLEAdvertisedDevice& device);
String detectManufacturer(BLEAdvertisedDevice& device);
void pruneStaleDevices();
void drawDisplay();
void drawHeader();
void drawDeviceRow(int index, int yPos);
void drawRSSIBars(int x, int y, int rssi);
void updateElapsedTime();
void handleTouch();
void addToWhitelist(int deviceIndex);
void removeFromWhitelist(int deviceIndex);
void logDeviceToSD(BLEDeviceInfo& device);
String getLogFilename();
void playTone(int frequency, int duration);
void alertUnknownDevice();
void alertNewDevice();
void alertWhitelistAdded();
void sendWebhookAlert(BLEDeviceInfo& device);
void postLogsToServer();
int rssiToBars(int rssi);
String formatElapsedTime(unsigned long ms);
void initWebServer();
void handleRoot();
void handleListLogs();
void handleDownloadLog();
void handleStatus();

// ============================================================================
// BLE Scan Callback Class
// ============================================================================

class BLEScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    processDevice(advertisedDevice);
  }
};

// Global callback instance (must be after class definition)
BLEScanCallbacks* pScanCallbacks = nullptr;

// ============================================================================
// Setup
// ============================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== BLE Security Scanner ===");
  Serial.println("Initializing...");

  // Initialize display first for visual feedback
  initDisplay();

  // Show splash screen
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("BLE Security Scanner", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30);
  tft.setTextSize(1);
  tft.drawString("Initializing...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);

  // Initialize components
  tft.drawString("Loading whitelist...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30);
  if (SPIFFS.begin(true)) {
    loadWhitelist();
    Serial.printf("Whitelist loaded: %d devices\n", whitelistCount);
  } else {
    Serial.println("SPIFFS mount failed");
  }

  tft.drawString("Initializing SD card...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 50);
  initSDCard();

  tft.drawString("Initializing audio...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 70);
  initAudio();

  tft.drawString("Initializing WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 90);
  initWiFi();

  if (wifiConnected) {
    tft.drawString("Starting web server...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 110);
    initWebServer();
  }

  tft.drawString("Initializing BLE...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 130);
  initBLE();

  // Play startup sound
  if (audioEnabled) {
    playTone(1000, 100);
    delay(50);
    playTone(1500, 100);
  }

  delay(500);

  // Draw main display
  tft.fillScreen(COLOR_BG);
  drawDisplay();

  // Start first scan
  lastScanTime = millis() - SCAN_INTERVAL;  // Force immediate scan

  Serial.println("Initialization complete!");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
  unsigned long currentTime = millis();

  // Start new scan if interval elapsed and not currently scanning
  if (!scanInProgress && (currentTime - lastScanTime >= SCAN_INTERVAL)) {
    startBLEScan();
  }

  // Check if scan is complete (scan duration has elapsed)
  if (scanInProgress && (millis() - scanStartTime >= (SCAN_DURATION * 1000 + 500))) {
    scanInProgress = false;
    lastScanTime = millis();

    // Prune devices not seen recently
    pruneStaleDevices();

    // Server POST disabled - BLE + HTTPS have incompatible memory requirements
    // Use local web server at http://<IP>/status to view devices
    // Or pull SD card logs from http://<IP>/logs

    // Update display
    drawDisplay();

    Serial.printf("Scan complete. Tracking %d devices\n", deviceCount);
  }

  // Update elapsed time display every second
  if (currentTime - lastDisplayUpdate >= 1000) {
    updateElapsedTime();
    lastDisplayUpdate = currentTime;
  }

  // Handle touch input
  handleTouch();

  // Handle web server requests - call frequently for responsiveness
  if (wifiConnected) {
    server.handleClient();
  }

  // Periodic WiFi debug (every 30 seconds)
  static unsigned long lastWifiDebug = 0;
  if (millis() - lastWifiDebug >= 30000) {
    lastWifiDebug = millis();
    Serial.println("=== WiFi Status Check ===");
    Serial.printf("  wifiConnected: %s\n", wifiConnected ? "true" : "false");
    Serial.printf("  WiFi.status(): %d\n", WiFi.status());
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress ip = WiFi.localIP();
      Serial.printf("  IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
      Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
    }
  }

  // Yield to other tasks
  yield();

  // Minimal delay
  delay(1);
}

// ============================================================================
// Initialization Functions
// ============================================================================

void initDisplay() {
  tft.init();
  tft.setRotation(1);  // Landscape (480x320)
  tft.fillScreen(COLOR_BG);

  // Note: Backlight is controlled by TFT_eSPI via TFT_BL pin (GPIO 27)
  // defined in User_Setup.h. If display is white, check User_Setup.h:
  // - Must have ST7796_DRIVER enabled (not ILI9341)
  // - TFT_BL must be 27
  // - TFT_BACKLIGHT_ON must be HIGH

  Serial.println("Display initialized");
}

void initBLE() {
  Serial.printf("initBLE() - Free heap: %d, Largest block: %d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  BLEDevice::init("BLE-Scanner");
  pBLEScan = BLEDevice::getScan();
  pScanCallbacks = new BLEScanCallbacks();

  pBLEScan->setAdvertisedDeviceCallbacks(pScanCallbacks);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  Serial.println("BLE initialized");
}

void initSDCard() {
  // Initialize separate SPI bus for SD card (VSPI) to avoid conflict with TFT (HSPI)
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (SD.begin(SD_CS, sdSPI)) {
    sdCardPresent = true;

    // Create logs directory if it doesn't exist
    if (!SD.exists("/ble-logs")) {
      SD.mkdir("/ble-logs");
    }

    Serial.println("SD card initialized");
    Serial.printf("SD Card Size: %lluMB\n", SD.cardSize() / (1024 * 1024));
  } else {
    sdCardPresent = false;
    Serial.println("SD card not found - logging disabled");
  }
}

void initAudio() {
  // Setup LEDC for tone generation (ESP32 Arduino Core 3.x API)
  int audioPin = useSpeaker ? AUDIO_PIN : BUZZER_PIN;
  ledcAttach(audioPin, 2000, 8);  // pin, frequency, resolution
  ledcWrite(audioPin, 0);  // Start silent

  Serial.printf("Audio initialized on GPIO %d\n", audioPin);
}

void initWiFi() {
  // Check if WiFi credentials are configured
  if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "Your_WiFi_SSID") == 0) {
    Serial.println("WiFi not configured - alerts disabled");
    wifiConnected = false;
    return;
  }

  Serial.println("=== WiFi Debug ===");
  Serial.printf("SSID: %s\n", WIFI_SSID);

  // Match working project's WiFi initialization sequence
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Clear any previous connection state
  delay(100);
  Serial.println("Mode set to STA, disconnected previous");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("WiFi.begin() called");

  // Wait for connection with timeout
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.printf(".");
    if (attempts % 10 == 9) {
      Serial.printf(" [status=%d]\n", WiFi.status());
    }
    attempts++;
  }

  Serial.printf("\nFinal WiFi.status() = %d\n", WiFi.status());
  // Status codes: 0=IDLE, 1=NO_SSID_AVAIL, 2=SCAN_COMPLETED, 3=CONNECTED, 4=CONNECT_FAILED, 5=CONNECTION_LOST, 6=DISCONNECTED

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;

    // Get IPv4 address
    IPAddress ip = WiFi.localIP();
    IPAddress gateway = WiFi.gatewayIP();
    IPAddress subnet = WiFi.subnetMask();

    Serial.println("=== WiFi Connected ===");
    Serial.printf("  IP Address:  %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    Serial.printf("  Gateway:     %d.%d.%d.%d\n", gateway[0], gateway[1], gateway[2], gateway[3]);
    Serial.printf("  Subnet Mask: %d.%d.%d.%d\n", subnet[0], subnet[1], subnet[2], subnet[3]);
    Serial.printf("  RSSI:        %d dBm\n", WiFi.RSSI());
    Serial.printf("  MAC:         %s\n", WiFi.macAddress().c_str());
    Serial.printf("  DNS:         %s\n", WiFi.dnsIP().toString().c_str());

    // Test network connectivity with simple TCP connection
    Serial.println("=== Network Connectivity Test ===");

    // Test gateway reachability via TCP connect to port 80 (or just ARP)
    Serial.printf("  Testing gateway %s...\n", gateway.toString().c_str());
    WiFiClient testClient;
    testClient.setTimeout(3000);
    unsigned long pingStart = millis();
    if (testClient.connect(gateway, 80)) {
      unsigned long pingTime = millis() - pingStart;
      Serial.printf("  Gateway reachable! Response time: %lu ms\n", pingTime);
      testClient.stop();
    } else {
      // Gateway might not have port 80 open, try DNS resolution as connectivity test
      Serial.println("  Gateway port 80 closed (normal for most routers)");
    }

    // Test external connectivity by resolving and connecting to a known host
    Serial.println("  Testing external connectivity (google.com)...");
    pingStart = millis();
    if (testClient.connect("google.com", 80)) {
      unsigned long pingTime = millis() - pingStart;
      Serial.printf("  External connectivity OK! Response time: %lu ms\n", pingTime);
      testClient.stop();
    } else {
      Serial.println("  External connectivity FAILED!");
    }

    // Configure time via NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    // Test HTTPS connectivity with known working endpoint
    Serial.println("Testing HTTPS connectivity...");
    testHttpsConnection();
  } else {
    wifiConnected = false;
    Serial.println("WiFi connection FAILED");
    Serial.printf("Status code: %d\n", WiFi.status());
  }
}

// Test HTTPS connectivity - compare working API vs our server
void testHttpsConnection() {
  // Test 1: Known working API (river levels)
  Serial.println("Test 1: Connecting to docker-blue-sound-1751.fly.dev...");
  HTTPClient http1;
  if (http1.begin("https://docker-blue-sound-1751.fly.dev/api/river-levels")) {
    http1.setTimeout(10000);
    int code1 = http1.GET();
    if (code1 > 0) {
      Serial.printf("  SUCCESS: HTTP %d\n", code1);
    } else {
      Serial.printf("  FAILED: %s\n", http1.errorToString(code1).c_str());
    }
    http1.end();
  } else {
    Serial.println("  FAILED: http.begin() returned false");
  }

  // Test 2: Our BLE scanner server (GET)
  Serial.println("Test 2: GET to ble-scanner.fly.dev/health...");
  HTTPClient http2;
  if (http2.begin("https://ble-scanner.fly.dev/health")) {
    http2.setTimeout(10000);
    int code2 = http2.GET();
    if (code2 > 0) {
      Serial.printf("  SUCCESS: HTTP %d\n", code2);
    } else {
      Serial.printf("  FAILED: %s\n", http2.errorToString(code2).c_str());
    }
    http2.end();
  } else {
    Serial.println("  FAILED: http.begin() returned false");
  }

  // Test 3: POST to our BLE scanner server (test POST before BLE starts)
  Serial.println("Test 3: POST to ble-scanner.fly.dev/api/logs...");
  Serial.printf("  Free heap: %d bytes, Largest block: %d bytes\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  HTTPClient http3;
  if (http3.begin("https://ble-scanner.fly.dev/api/logs")) {
    http3.setTimeout(10000);
    http3.addHeader("Content-Type", "application/json");
    http3.addHeader("Authorization", String("Bearer ") + BLE_API_KEY);

    // Simple test payload
    String testPayload = "{\"scanner_id\":\"test\",\"devices\":[]}";
    int code3 = http3.POST(testPayload);
    if (code3 > 0) {
      Serial.printf("  SUCCESS: HTTP %d\n", code3);
      Serial.printf("  Response: %s\n", http3.getString().c_str());
    } else {
      Serial.printf("  FAILED: %s\n", http3.errorToString(code3).c_str());
    }
    http3.end();
  } else {
    Serial.println("  FAILED: http.begin() returned false");
  }

  Serial.println("HTTPS connectivity test complete.");
}

// ============================================================================
// Whitelist Management
// ============================================================================

void loadWhitelist() {
  whitelistCount = 0;

  if (!SPIFFS.exists("/whitelist.json")) {
    Serial.println("No whitelist file found");
    return;
  }

  File file = SPIFFS.open("/whitelist.json", "r");
  if (!file) {
    Serial.println("Failed to open whitelist file");
    return;
  }

  StaticJsonDocument<4096> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.printf("Whitelist parse error: %s\n", error.c_str());
    return;
  }

  JsonArray devicesArray = doc["devices"];
  for (JsonObject device : devicesArray) {
    if (whitelistCount >= 50) break;

    whitelist[whitelistCount].mac = device["mac"].as<String>();
    whitelist[whitelistCount].name = device["name"].as<String>();
    whitelist[whitelistCount].type = device["type"].as<String>();
    whitelistCount++;
  }

  Serial.printf("Loaded %d whitelist entries\n", whitelistCount);
}

void saveWhitelist() {
  StaticJsonDocument<4096> doc;
  JsonArray devicesArray = doc.createNestedArray("devices");

  for (int i = 0; i < whitelistCount; i++) {
    JsonObject device = devicesArray.createNestedObject();
    device["mac"] = whitelist[i].mac;
    device["name"] = whitelist[i].name;
    device["type"] = whitelist[i].type;
  }

  File file = SPIFFS.open("/whitelist.json", "w");
  if (!file) {
    Serial.println("Failed to open whitelist for writing");
    return;
  }

  serializeJson(doc, file);
  file.close();

  Serial.printf("Saved %d whitelist entries\n", whitelistCount);
}

bool isDeviceKnown(String mac) {
  mac.toUpperCase();
  for (int i = 0; i < whitelistCount; i++) {
    String wlMac = whitelist[i].mac;
    wlMac.toUpperCase();
    if (mac == wlMac) {
      return true;
    }
  }
  return false;
}

void addToWhitelist(int deviceIndex) {
  if (deviceIndex < 0 || deviceIndex >= deviceCount) return;
  if (whitelistCount >= 50) {
    Serial.println("Whitelist full!");
    return;
  }

  BLEDeviceInfo& dev = devices[deviceIndex];

  // Check if already whitelisted
  if (isDeviceKnown(dev.mac)) {
    Serial.println("Device already whitelisted");
    return;
  }

  whitelist[whitelistCount].mac = dev.mac;
  whitelist[whitelistCount].name = dev.name;
  whitelist[whitelistCount].type = dev.deviceType;
  whitelistCount++;

  dev.isKnown = true;

  saveWhitelist();
  alertWhitelistAdded();
  drawDisplay();

  Serial.printf("Added to whitelist: %s (%s)\n", dev.name.c_str(), dev.mac.c_str());
}

void removeFromWhitelist(int deviceIndex) {
  if (deviceIndex < 0 || deviceIndex >= deviceCount) return;

  BLEDeviceInfo& dev = devices[deviceIndex];
  String macToRemove = dev.mac;
  macToRemove.toUpperCase();

  // Find and remove from whitelist
  for (int i = 0; i < whitelistCount; i++) {
    String wlMac = whitelist[i].mac;
    wlMac.toUpperCase();
    if (macToRemove == wlMac) {
      // Shift remaining entries
      for (int j = i; j < whitelistCount - 1; j++) {
        whitelist[j] = whitelist[j + 1];
      }
      whitelistCount--;
      dev.isKnown = false;
      saveWhitelist();
      drawDisplay();
      Serial.printf("Removed from whitelist: %s\n", dev.mac.c_str());
      return;
    }
  }
}

// ============================================================================
// BLE Scanning
// ============================================================================

void startBLEScan() {
  Serial.printf("Starting BLE scan... heap=%d, largest=%d\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  scanStartTime = millis();
  scanInProgress = true;

  // Clear previous scan results
  pBLEScan->clearResults();

  // Start async scan (non-blocking)
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
        devices[i].name = name;  // Update name if we got a better one
      }
      if (deviceType != "Unknown" && devices[i].deviceType == "Unknown") {
        devices[i].deviceType = deviceType;
      }
      if (manufacturer != "Unknown" && devices[i].manufacturer == "Unknown") {
        devices[i].manufacturer = manufacturer;
      }

      // Check if still "new"
      devices[i].isNew = (currentTime - devices[i].firstSeen) < NEW_DEVICE_THRESHOLD;

      return;
    }
  }

  // New device - add to list
  if (deviceCount >= MAX_TRACKED_DEVICES) {
    // Remove oldest/weakest device
    int removeIndex = 0;
    int weakestRSSI = 0;
    for (int i = 0; i < deviceCount; i++) {
      if (devices[i].rssi < weakestRSSI) {
        weakestRSSI = devices[i].rssi;
        removeIndex = i;
      }
    }
    // Shift remaining devices
    for (int i = removeIndex; i < deviceCount - 1; i++) {
      devices[i] = devices[i + 1];
    }
    deviceCount--;
  }

  // Add new device
  BLEDeviceInfo newDevice;
  newDevice.mac = mac;
  newDevice.name = name;
  newDevice.rssi = rssi;
  newDevice.deviceType = deviceType;
  newDevice.manufacturer = manufacturer;
  newDevice.isKnown = isDeviceKnown(mac);
  newDevice.isNew = true;
  newDevice.firstSeen = currentTime;
  newDevice.lastSeen = currentTime;
  newDevice.alertSent = false;

  devices[deviceCount] = newDevice;
  deviceCount++;

  // Log to SD card
  logDeviceToSD(newDevice);

  // Alert for unknown devices
  if (!newDevice.isKnown && !newDevice.alertSent) {
    Serial.printf("ALERT: Unknown device %s (%s) RSSI: %d\n",
                  name.c_str(), mac.c_str(), rssi);
    alertUnknownDevice();
    sendWebhookAlert(newDevice);
    devices[deviceCount - 1].alertSent = true;
  } else if (newDevice.isNew) {
    Serial.printf("NEW: %s (%s) RSSI: %d\n", name.c_str(), mac.c_str(), rssi);
    alertNewDevice();
  }
}

void pruneStaleDevices() {
  unsigned long currentTime = millis();
  int i = 0;

  while (i < deviceCount) {
    unsigned long age = currentTime - devices[i].lastSeen;
    if (age > DEVICE_TIMEOUT) {

      // Shift remaining devices
      for (int j = i; j < deviceCount - 1; j++) {
        devices[j] = devices[j + 1];
      }
      deviceCount--;
      // Don't increment i - check the device that shifted into this position
    } else {
      i++;
    }
  }
}

// ============================================================================
// Device Type Detection
// ============================================================================

String detectDeviceType(BLEAdvertisedDevice& device) {
  // Check manufacturer data
  if (device.haveManufacturerData()) {
    String mfgData = device.getManufacturerData();
    if (mfgData.length() >= 2) {
      uint16_t mfgId = (uint8_t)mfgData[0] | ((uint8_t)mfgData[1] << 8);

      // Apple devices
      if (mfgId == 0x004C) {
        if (mfgData.length() > 2) {
          uint8_t type = (uint8_t)mfgData[2];
          if (type == 0x02) return "iBeacon";
          if (type == 0x05) return "AirDrop";
          if (type == 0x07) return "AirPods";
          if (type == 0x09) return "AirPlay";
          if (type == 0x10) return "AirTag";
        }
        return "Apple";
      }

      // Samsung
      if (mfgId == 0x0075) return "Samsung";

      // Google
      if (mfgId == 0x00E0) return "Google";

      // Microsoft
      if (mfgId == 0x0006) return "Microsoft";

      // Tile trackers
      if (mfgId == 0x0477) return "Tile";
    }
  }

  // Check service UUIDs
  if (device.haveServiceUUID()) {
    BLEUUID uuid = device.getServiceUUID();
    String uuidStr = uuid.toString().c_str();

    // Standard Bluetooth services
    if (uuidStr.indexOf("180d") >= 0) return "Wearable";      // Heart Rate
    if (uuidStr.indexOf("180f") >= 0) return "BLE Device";    // Battery
    if (uuidStr.indexOf("1812") >= 0) return "HID";           // Human Interface Device
    if (uuidStr.indexOf("1803") >= 0) return "Beacon";        // Link Loss
    if (uuidStr.indexOf("fe9f") >= 0) return "Phone";         // Google Nearby
  }

  // Check device name patterns
  String name = device.haveName() ? String(device.getName().c_str()) : "";
  name.toLowerCase();

  if (name.indexOf("airpods") >= 0) return "Audio";
  if (name.indexOf("galaxy buds") >= 0) return "Audio";
  if (name.indexOf("beats") >= 0) return "Audio";
  if (name.indexOf("jbl") >= 0) return "Audio";
  if (name.indexOf("bose") >= 0) return "Audio";
  if (name.indexOf("fitbit") >= 0) return "Wearable";
  if (name.indexOf("watch") >= 0) return "Wearable";
  if (name.indexOf("band") >= 0) return "Wearable";
  if (name.indexOf("beacon") >= 0) return "Beacon";
  if (name.indexOf("tile") >= 0) return "Tracker";
  if (name.indexOf("airtag") >= 0) return "Tracker";
  if (name.indexOf("iphone") >= 0) return "Phone";
  if (name.indexOf("galaxy") >= 0) return "Phone";
  if (name.indexOf("pixel") >= 0) return "Phone";

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
        case 0x0477: return "Tile";
        case 0x0087: return "Garmin";
        case 0x0157: return "Huawei";
        case 0x0310: return "Xiaomi";
        case 0x0059: return "Nordic";
        case 0x004F: return "Sony";
        default: break;
      }
    }
  }
  return "Unknown";
}

// ============================================================================
// Display Functions
// ============================================================================

void drawDisplay() {
  tft.fillScreen(COLOR_BG);
  drawHeader();

  // Draw visible devices
  int visibleCount = min(deviceCount - scrollOffset, MAX_VISIBLE_DEVICES);
  for (int i = 0; i < visibleCount; i++) {
    int deviceIndex = i + scrollOffset;
    int yPos = ROW_START_Y + (i * DEVICE_ROW_HEIGHT);
    drawDeviceRow(deviceIndex, yPos);

    // Draw divider line
    if (i < visibleCount - 1) {
      tft.drawLine(0, yPos + DEVICE_ROW_HEIGHT - 1, SCREEN_WIDTH, yPos + DEVICE_ROW_HEIGHT - 1, COLOR_DIVIDER);
    }
  }

  // Draw scroll indicators if needed
  if (scrollOffset > 0) {
    tft.fillTriangle(SCREEN_WIDTH - 20, ROW_START_Y + 5,
                     SCREEN_WIDTH - 10, ROW_START_Y + 5,
                     SCREEN_WIDTH - 15, ROW_START_Y, COLOR_TEXT);
  }
  if (scrollOffset + MAX_VISIBLE_DEVICES < deviceCount) {
    int bottomY = ROW_START_Y + (MAX_VISIBLE_DEVICES * DEVICE_ROW_HEIGHT) - 10;
    tft.fillTriangle(SCREEN_WIDTH - 20, bottomY,
                     SCREEN_WIDTH - 10, bottomY,
                     SCREEN_WIDTH - 15, bottomY + 5, COLOR_TEXT);
  }

  // Show "No SD" indicator if SD card not present
  if (!sdCardPresent) {
    tft.setTextColor(COLOR_UNKNOWN);
    tft.setTextSize(1);
    tft.setTextDatum(BR_DATUM);
    tft.drawString("No SD", SCREEN_WIDTH - 5, SCREEN_HEIGHT - 5);
  }
}

void drawHeader() {
  // Header background
  tft.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER_BG);

  // Title (smaller font)
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("BLE SCANNER", 5, HEADER_HEIGHT / 2);

  // IP address (if connected)
  if (wifiConnected) {
    tft.setTextColor(COLOR_KNOWN);
    tft.drawString(WiFi.localIP().toString(), 80, HEADER_HEIGHT / 2);
  }

  // Device count
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  char countStr[20];
  sprintf(countStr, "[%d]", deviceCount);
  tft.drawString(countStr, SCREEN_WIDTH / 2 + 40, HEADER_HEIGHT / 2);

  // Elapsed time since last scan
  tft.setTextDatum(MR_DATUM);
  String elapsed = formatElapsedTime(millis() - lastScanTime);
  tft.drawString(elapsed, SCREEN_WIDTH - 5, HEADER_HEIGHT / 2);
}

void drawDeviceRow(int index, int yPos) {
  if (index < 0 || index >= deviceCount) return;

  BLEDeviceInfo& dev = devices[index];

  // Determine status color
  uint16_t statusColor;
  if (dev.isKnown) {
    statusColor = COLOR_KNOWN;
  } else if (dev.isNew) {
    statusColor = COLOR_NEW;
  } else {
    statusColor = COLOR_UNKNOWN;
  }

  // Status dot
  tft.fillCircle(15, yPos + DEVICE_ROW_HEIGHT / 2, 8, statusColor);

  // Device name (truncated to 14 chars)
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setTextDatum(ML_DATUM);
  String displayName = dev.name.substring(0, 14);
  if (dev.name.length() > 14) displayName += "..";
  tft.drawString(displayName, 30, yPos + 15);

  // Device type and manufacturer
  tft.setTextSize(1);
  tft.setTextColor(COLOR_FADING);
  String subInfo = dev.deviceType;
  if (dev.manufacturer != "Unknown") {
    subInfo += " | " + dev.manufacturer;
  }
  tft.drawString(subInfo.substring(0, 25), 30, yPos + 35);

  // RSSI value
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setTextDatum(MR_DATUM);
  char rssiStr[10];
  sprintf(rssiStr, "%ddBm", dev.rssi);
  tft.drawString(rssiStr, 280, yPos + DEVICE_ROW_HEIGHT / 2);

  // RSSI bars
  drawRSSIBars(290, yPos + 12, dev.rssi);

  // MAC address (last 8 chars)
  tft.setTextColor(COLOR_FADING);
  tft.setTextSize(1);
  tft.setTextDatum(MR_DATUM);
  String shortMac = dev.mac.substring(dev.mac.length() - 8);
  tft.drawString(shortMac, SCREEN_WIDTH - 10, yPos + DEVICE_ROW_HEIGHT / 2);
}

void drawRSSIBars(int x, int y, int rssi) {
  int bars = rssiToBars(rssi);
  int barWidth = 6;
  int barSpacing = 2;
  int maxHeight = 20;

  for (int i = 0; i < 10; i++) {
    int barHeight = (i + 1) * 2;
    int barX = x + (i * (barWidth + barSpacing));
    int barY = y + (maxHeight - barHeight);

    uint16_t barColor;
    if (i < bars) {
      if (bars >= 7) barColor = COLOR_RSSI_HIGH;
      else if (bars >= 4) barColor = COLOR_RSSI_MED;
      else barColor = COLOR_RSSI_LOW;
    } else {
      barColor = COLOR_DIVIDER;
    }

    tft.fillRect(barX, barY, barWidth, barHeight, barColor);
  }
}

void updateElapsedTime() {
  // Only update the elapsed time portion of header (right side)
  tft.fillRect(SCREEN_WIDTH - 60, 0, 60, HEADER_HEIGHT, COLOR_HEADER_BG);

  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(1);
  tft.setTextDatum(MR_DATUM);
  String elapsed = formatElapsedTime(millis() - lastScanTime);
  tft.drawString(elapsed, SCREEN_WIDTH - 5, HEADER_HEIGHT / 2);
}

int rssiToBars(int rssi) {
  if (rssi >= -50) return 10;
  if (rssi >= -55) return 9;
  if (rssi >= -60) return 8;
  if (rssi >= -65) return 7;
  if (rssi >= -70) return 6;
  if (rssi >= -75) return 5;
  if (rssi >= -80) return 4;
  if (rssi >= -85) return 3;
  if (rssi >= -90) return 2;
  return 1;
}

String formatElapsedTime(unsigned long ms) {
  unsigned long seconds = ms / 1000;
  if (seconds < 60) {
    return String(seconds) + "s ago";
  } else {
    unsigned long minutes = seconds / 60;
    return String(minutes) + "m ago";
  }
}

// ============================================================================
// Touch Handling
// ============================================================================

// Note: ESP32-3248S035C uses GT911 capacitive touch controller via I2C
// For full touch support, add GT911 library. For now, touch is placeholder.
// Touch pins: SDA=33, SCL=32, INT=21, RST=25

bool touchAvailable = false;
uint16_t lastTouchX = 0;
uint16_t lastTouchY = 0;

bool getTouchPoint(uint16_t* x, uint16_t* y) {
  // Placeholder for GT911 touch reading
  // TODO: Implement GT911 I2C touch reading or add GT911 library
  // For now, return false (no touch)
  //
  // To add touch support, install a GT911 library and implement:
  // - Initialize GT911 on I2C with SDA=33, SCL=32
  // - Read touch points from GT911 registers
  //
  return false;
}

void handleTouch() {
  uint16_t touchX, touchY;

  if (getTouchPoint(&touchX, &touchY)) {
    // Debounce
    static unsigned long lastTouch = 0;
    if (millis() - lastTouch < 300) return;
    lastTouch = millis();

    // Check if touch is in header (force rescan)
    if (touchY < HEADER_HEIGHT) {
      Serial.println("Header touched - forcing rescan");
      lastScanTime = 0;  // Force immediate scan
      return;
    }

    // Check if touch is on a device row
    int rowIndex = (touchY - ROW_START_Y) / DEVICE_ROW_HEIGHT;
    if (rowIndex >= 0 && rowIndex < MAX_VISIBLE_DEVICES) {
      int deviceIndex = rowIndex + scrollOffset;
      if (deviceIndex < deviceCount) {
        // Detect long press for whitelist toggle
        unsigned long pressStart = millis();
        while (getTouchPoint(&touchX, &touchY)) {
          if (millis() - pressStart > 1000) {
            // Long press - toggle whitelist
            if (devices[deviceIndex].isKnown) {
              removeFromWhitelist(deviceIndex);
            } else {
              addToWhitelist(deviceIndex);
            }
            return;
          }
          delay(50);
        }

        // Short tap - show device details (future feature)
        Serial.printf("Tapped device: %s\n", devices[deviceIndex].name.c_str());
      }
    }

    // Scroll handling
    static uint16_t lastTouchY = 0;
    if (lastTouchY > 0) {
      int deltaY = touchY - lastTouchY;
      if (abs(deltaY) > 30) {
        if (deltaY > 0 && scrollOffset > 0) {
          scrollOffset--;
          drawDisplay();
        } else if (deltaY < 0 && scrollOffset + MAX_VISIBLE_DEVICES < deviceCount) {
          scrollOffset++;
          drawDisplay();
        }
      }
    }
    lastTouchY = touchY;
  }
}

// ============================================================================
// SD Card Logging
// ============================================================================

void logDeviceToSD(BLEDeviceInfo& device) {
  if (!sdCardPresent) return;

  String filename = getLogFilename();
  bool newFile = !SD.exists(filename);

  File logFile = SD.open(filename, FILE_APPEND);
  if (!logFile) {
    Serial.println("Failed to open log file");
    return;
  }

  // Write header if new file
  if (newFile) {
    logFile.println("timestamp,mac,name,rssi,device_type,status,manufacturer");
  }

  // Get timestamp
  String timestamp;
  if (wifiConnected) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStr[25];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
      timestamp = String(timeStr);
    } else {
      timestamp = String(millis());
    }
  } else {
    timestamp = String(millis());
  }

  // Determine status
  String status;
  if (device.isKnown) status = "known";
  else if (device.isNew) status = "new";
  else status = "unknown";

  // Escape commas in name
  String safeName = device.name;
  safeName.replace(",", ";");

  // Write log entry
  logFile.printf("%s,%s,%s,%d,%s,%s,%s\n",
                 timestamp.c_str(),
                 device.mac.c_str(),
                 safeName.c_str(),
                 device.rssi,
                 device.deviceType.c_str(),
                 status.c_str(),
                 device.manufacturer.c_str());

  logFile.close();
}

String getLogFilename() {
  String filename = "/ble-logs/";

  if (wifiConnected) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char dateStr[12];
      strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &timeinfo);
      filename += String(dateStr);
    } else {
      filename += "unknown-date";
    }
  } else {
    // Use boot count or fixed name without NTP
    filename += "scan-log";
  }

  filename += ".csv";
  return filename;
}

// ============================================================================
// Audio Functions
// ============================================================================

void playTone(int frequency, int duration) {
  if (!audioEnabled) return;

  int audioPin = useSpeaker ? AUDIO_PIN : BUZZER_PIN;
  ledcWriteTone(audioPin, frequency);  // ESP32 Arduino Core 3.x API
  delay(duration);
  ledcWriteTone(audioPin, 0);    // Silence
}

void alertUnknownDevice() {
  if (!audioEnabled) return;

  // 3 short urgent beeps at 2kHz
  for (int i = 0; i < 3; i++) {
    playTone(2000, 100);
    delay(100);
  }
}

void alertNewDevice() {
  if (!audioEnabled) return;

  // Single short beep at 1kHz
  playTone(1000, 150);
}

void alertWhitelistAdded() {
  if (!audioEnabled) return;

  // Two ascending tones
  playTone(800, 150);
  delay(50);
  playTone(1200, 200);
}

// ============================================================================
// WiFi Webhook Alerts
// ============================================================================

void sendWebhookAlert(BLEDeviceInfo& device) {
  if (!wifiConnected) return;
  if (strlen(ALERT_WEBHOOK_URL) == 0 || strcmp(ALERT_WEBHOOK_URL, "") == 0) return;

  HTTPClient http;
  http.begin(ALERT_WEBHOOK_URL);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  StaticJsonDocument<512> doc;
  doc["event"] = "unknown_device";
  doc["mac"] = device.mac;
  doc["name"] = device.name;
  doc["rssi"] = device.rssi;
  doc["device_type"] = device.deviceType;
  doc["manufacturer"] = device.manufacturer;
  doc["scanner_id"] = SCANNER_ID;

  // Add timestamp
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    doc["timestamp"] = timeStr;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.printf("Webhook sent, response: %d\n", httpCode);
  } else {
    Serial.printf("Webhook failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}

// ============================================================================
// Server Log Posting
// ============================================================================

void postLogsToServer() {
  // Check if server URL is configured
  if (strlen(BLE_SERVER_URL) == 0 || strcmp(BLE_SERVER_URL, "") == 0) return;
  if (strlen(BLE_API_KEY) == 0 || strcmp(BLE_API_KEY, "CHANGE_ME_AFTER_DEPLOY") == 0) {
    Serial.println("WARNING: BLE_API_KEY not configured - skipping server post");
    return;
  }

  // Check WiFi status first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected - skipping server post");
    return;
  }

  Serial.printf("Server URL: %s\n", BLE_SERVER_URL);
  Serial.printf("WiFi RSSI: %d dBm, Gateway: %s\n", WiFi.RSSI(), WiFi.gatewayIP().toString().c_str());

  // Stop BLE scan during POST (but don't deinit - that breaks reinit!)
  Serial.println("Pausing BLE scan for HTTP POST...");
  pBLEScan->stop();
  delay(100);

  Serial.printf("Free heap: %d bytes, Largest block: %d bytes\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // Build JSON payload using StaticJsonDocument (stack, not heap)
  // Limit to 10 devices max to keep payload small
  int devicesToSend = min(deviceCount, 10);
  StaticJsonDocument<2048> doc;
  doc["scanner_id"] = SCANNER_ID;

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    doc["timestamp"] = timeStr;
  }

  JsonArray devicesArray = doc.createNestedArray("devices");

  for (int i = 0; i < devicesToSend; i++) {
    JsonObject devObj = devicesArray.createNestedObject();
    devObj["mac"] = devices[i].mac;
    devObj["name"] = devices[i].name;
    devObj["rssi"] = devices[i].rssi;
    devObj["type"] = devices[i].deviceType;
    devObj["status"] = devices[i].isKnown ? "known" : (devices[i].isNew ? "new" : "unknown");
  }

  // Serialize to stack-allocated buffer
  char payload[2048];
  serializeJson(doc, payload, sizeof(payload));

  Serial.printf("Posting %d devices to server (payload: %d bytes)...\n", devicesToSend, strlen(payload));

  // Use plain HTTP (no SSL) to avoid memory fragmentation issues
  HTTPClient http;
  if (!http.begin(BLE_SERVER_URL)) {
    Serial.println("http.begin() failed!");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + BLE_API_KEY);
  http.setTimeout(15000);

  int httpCode = http.POST(payload);
  if (httpCode == 200) {
    Serial.printf("Server post OK: %d devices sent\n", deviceCount);
  } else if (httpCode > 0) {
    Serial.printf("Server post failed: HTTP %d\n", httpCode);
    Serial.printf("Response: %s\n", http.getString().c_str());
  } else {
    Serial.printf("Server post error code: %d (%s)\n", httpCode, http.errorToString(httpCode).c_str());
  }

  http.end();

  // BLE scan will restart automatically on next loop iteration
  Serial.println("POST complete, BLE scan will resume.");
}

// ============================================================================
// Web Server Functions
// ============================================================================

void initWebServer() {
  Serial.println("=== Web Server Debug ===");

  server.on("/", handleRoot);
  Serial.println("  Route added: /");

  server.on("/logs", handleListLogs);
  Serial.println("  Route added: /logs");

  server.on("/download", handleDownloadLog);
  Serial.println("  Route added: /download");

  server.on("/status", handleStatus);
  Serial.println("  Route added: /status");

  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.println("=== Web Server Started ===");
  Serial.printf("  URL: http://%d.%d.%d.%d/\n", ip[0], ip[1], ip[2], ip[3]);
  Serial.printf("  Listening on port 80\n");
}

void handleRoot() {
  Serial.println("Web request: /");
  server.sendHeader("Connection", "close");
  String html = "<!DOCTYPE html><html><head><title>BLE Scanner</title>";
  html += "<style>body{font-family:monospace;background:#111;color:#0f0;padding:20px;}";
  html += "a{color:#0ff;}h1{color:#fff;}pre{background:#222;padding:10px;}</style></head>";
  html += "<body><h1>BLE Security Scanner</h1>";
  html += "<p>Scanner ID: " + String(SCANNER_ID) + "</p>";
  html += "<p>Devices tracked: " + String(deviceCount) + "</p>";
  html += "<p>SD Card: " + String(sdCardPresent ? "Present" : "Not found") + "</p>";
  html += "<h2>Endpoints:</h2><ul>";
  html += "<li><a href='/logs'>/logs</a> - List available log files (JSON)</li>";
  html += "<li><a href='/download?file=FILENAME'>/download?file=FILENAME</a> - Download a log file</li>";
  html += "<li><a href='/status'>/status</a> - Current scanner status (JSON)</li>";
  html += "</ul></body></html>";
  server.send(200, "text/html", html);
}

void handleListLogs() {
  Serial.println("Web request: /logs");
  server.sendHeader("Connection", "close");

  if (!sdCardPresent) {
    server.send(503, "application/json", "{\"error\":\"SD card not present\"}");
    return;
  }

  StaticJsonDocument<2048> doc;
  JsonArray files = doc.createNestedArray("files");

  File root = SD.open("/ble-logs");
  if (!root || !root.isDirectory()) {
    server.send(404, "application/json", "{\"error\":\"Log directory not found\"}");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      JsonObject fileObj = files.createNestedObject();
      fileObj["name"] = String(file.name());
      fileObj["size"] = file.size();
    }
    file = root.openNextFile();
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleDownloadLog() {
  Serial.println("Web request: /download");
  server.sendHeader("Connection", "close");

  if (!sdCardPresent) {
    server.send(503, "text/plain", "SD card not present");
    return;
  }

  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Missing 'file' parameter");
    return;
  }

  String filename = server.arg("file");

  // Security: prevent directory traversal
  if (filename.indexOf("..") >= 0) {
    server.send(403, "text/plain", "Invalid filename");
    return;
  }

  String filepath = "/ble-logs/" + filename;

  if (!SD.exists(filepath)) {
    server.send(404, "text/plain", "File not found: " + filename);
    return;
  }

  File file = SD.open(filepath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }

  // Stream the file
  server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
  server.streamFile(file, "text/csv");
  file.close();
}

void handleStatus() {
  Serial.println("Web request: /status");
  server.sendHeader("Connection", "close");

  StaticJsonDocument<1024> doc;

  doc["scanner_id"] = SCANNER_ID;
  doc["device_count"] = deviceCount;
  doc["whitelist_count"] = whitelistCount;
  doc["sd_card"] = sdCardPresent;
  doc["wifi_connected"] = wifiConnected;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["uptime_ms"] = millis();
  doc["scan_in_progress"] = scanInProgress;
  doc["last_scan_ms_ago"] = millis() - lastScanTime;

  // Add timestamp if NTP is available
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[25];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    doc["current_time"] = timeStr;
  }

  // List current devices
  JsonArray devicesArray = doc.createNestedArray("devices");
  for (int i = 0; i < min(deviceCount, 10); i++) {  // Limit to first 10
    JsonObject devObj = devicesArray.createNestedObject();
    devObj["mac"] = devices[i].mac;
    devObj["name"] = devices[i].name;
    devObj["rssi"] = devices[i].rssi;
    devObj["known"] = devices[i].isKnown;
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}
