#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "esp_task_wdt.h"
#include <Adafruit_NeoPixel.h>
#include "WebInterface.h"
#include "GeminiAPI.h"
#include "GameController.h"
#include "LichessAPI.h"
#include "LichessWebHandler.h"
#include "SessionManager.h"
#include "WebRTCHandler.h"
#include "SDLogger.h"

// Global SD logger - will be initialized after SD card is ready
SDLogger* sdLogger = nullptr;

// Pin definitions
// Adafruit QT Py ESP32 Pico has NeoPixel on GPIO 5, power on GPIO 8
#define RGB_LED_ENABLED false
#define NEOPIXEL_ENABLED true   // QT Py ESP32 Pico has NeoPixel
#define NEOPIXEL_PIN 5          // NeoPixel data on GPIO 5
#define NEOPIXEL_POWER 8        // NeoPixel power enable on GPIO 8
#define NEOPIXEL_COUNT 1
#define STATUS_LED_PIN 13  // Additional red LED
#define RESET_BUTTON_PIN 0 // Boot button on GPIO 0
#define SD_CS_PIN 15     // silkscreened A3 as chip‐select
#define SPI_SCK  14      // silkscreened SCK
#define SPI_MISO 13      // silkscreened MI
#define SPI_MOSI 12      // silkscreened MO

AsyncWebServer server(80);
WebInterface webInterface;
GeminiAPI geminiAPI;
GameController gameController;
LichessAPI lichessAPI;
LichessWebHandler lichessWebHandler;
SessionManager sessionManager;
WebRTCHandler webrtcHandler;

// NeoPixel LED (conditionally compiled)
#if NEOPIXEL_ENABLED
Adafruit_NeoPixel pixel(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif

// WiFi Configuration (loaded from SPIFFS)
String ssid = "";
String password = "";

// RGB LED helper functions
void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
#if RGB_LED_ENABLED
  // ESP32-PICO-V3 RGB LED uses active HIGH
  digitalWrite(RGB_LED_RED, r > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_GREEN, g > 0 ? HIGH : LOW);
  digitalWrite(RGB_LED_BLUE, b > 0 ? HIGH : LOW);
  Serial.printf("RGB LED Status: R=%d G=%d B=%d\n", r, g, b);
#elif NEOPIXEL_ENABLED
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
  Serial.printf("NeoPixel Status: RGB(%d,%d,%d)\n", r, g, b);
#else
  // Fallback to single LED
  if (r > 0 || g > 0 || b > 0) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    Serial.printf("LED ON (simulating RGB(%d,%d,%d))\n", r, g, b);
  } else {
    digitalWrite(STATUS_LED_PIN, LOW);
    Serial.printf("LED OFF\n");
  }
#endif
}

void setLEDWhite() { setLEDColor(255, 255, 255); }
void setLEDBlue() { setLEDColor(0, 0, 255); }
void setLEDGreen() { setLEDColor(0, 255, 0); }
void setLEDPurple() { setLEDColor(128, 0, 128); }
void setLEDYellow() { setLEDColor(255, 255, 0); }
void setLEDRed() { setLEDColor(255, 0, 0); }
void setLEDOff() { setLEDColor(0, 0, 0); }

void blinkLEDWhite(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    setLEDWhite();
    delay(delayMs);
    setLEDOff();
    delay(delayMs);
  }
}

void blinkLEDGreen(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    setLEDGreen();
    delay(delayMs);
    setLEDOff();
    delay(delayMs);
  }
}

void blinkLEDPurple(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    setLEDPurple();
    delay(delayMs);
    setLEDOff();
    delay(delayMs);
  }
}

void blinkLEDRed(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    setLEDRed();
    delay(delayMs);
    setLEDOff();
    delay(delayMs);
  }
}

// Function declarations
bool initializeSPIFFS();
bool initializeSDCard();
bool loadAPIKey();
bool loadWiFiCredentials();
void scanWiFiNetworks();
void connectToWiFi();
void setupWebServer();
void blinkError();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n=== Chess AI vs AI System Starting ===");

  // Initialize NeoPixel (if available)
#if NEOPIXEL_ENABLED
  // Enable NeoPixel power on GPIO 8
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);  // Turn on power to NeoPixel
  delay(20); // Small delay for power stabilization

  pixel.begin();
  pixel.setBrightness(50); // Set to 50% brightness (0-255)
  setLEDOff();
#endif

  // Blink white during initialization
  Serial.println("Initializing - Blinking White");
  blinkLEDWhite(3, 500);
  setLEDWhite();  // Keep white LED on

  // Reconfigure watchdog with longer timeout (60 seconds)
  esp_task_wdt_init(60, false); // 60 second timeout, don't panic
  Serial.println("Watchdog timer reconfigured with 60 second timeout");

  // Initialize RGB LED pins
#if RGB_LED_ENABLED
  pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);
  setLEDOff(); // Start with LED off
#endif

  // Initialize pins
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Initialize SPIFFS
  Serial.println("Initializing SPIFFS...");
  if (!initializeSPIFFS()) {
    Serial.println("SPIFFS initialization failed!");
    blinkError();
    return;
  }

  // Initialize SD Card (retry until successful with red LED blinking)
  bool sdCardAvailable = false;
  while (!sdCardAvailable) {
    sdCardAvailable = initializeSDCard();
    if (!sdCardAvailable) {
      Serial.println("SD Card initialization failed - retrying...");
      blinkLEDRed(3, 500);  // Blink red 3 times
      delay(1000);  // Wait 1 second before retry
    }
  }

  // SD Card successfully initialized
  Serial.println("SD Card initialized successfully!");

  // Archive old log with timestamp BEFORE initializing SDLogger
  // This must happen before sdLogger is created to avoid file conflicts
  File oldLog = SD.open("/DebugMessages.log", FILE_READ);
  if (oldLog) {
    // Generate timestamped filename using millis()
    char crashFileName[32];
    snprintf(crashFileName, sizeof(crashFileName), "/CrashLog_%lu.txt", millis());

    Serial.print("Found previous session log - archiving to ");
    Serial.println(crashFileName);

    File crashLog = SD.open(crashFileName, FILE_WRITE);
    if (crashLog) {
      crashLog.println("========================================");
      crashLog.print("=== Crash Log Archived at: ");
      crashLog.print(millis());
      crashLog.println(" ms ===");
      crashLog.println("========================================");

      // Copy last session to crash log
      int bytesCopied = 0;
      while (oldLog.available()) {
        crashLog.write(oldLog.read());
        bytesCopied++;
      }
      crashLog.close();
      Serial.print("Previous session archived (");
      Serial.print(bytesCopied);
      Serial.println(" bytes)");
    }
    oldLog.close();
  } else {
    Serial.println("No previous log file to archive");
  }

  // Now initialize SD logger (which will use the macro-redirected Serial)
  sdLogger = new SDLogger(Serial);
  sdLogger->begin(115200);

  // Clear log file to start fresh session
  sdLogger->clearLog();

  // Write session header
  sdLogger->println("========================================");
  sdLogger->println("=== New Session Started ===");
  sdLogger->print("=== Timestamp: ");
  sdLogger->print(millis());
  sdLogger->println(" ms ===");

  // Report crash log status (list all crash log files)
  File root = SD.open("/");
  bool foundCrashLogs = false;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    String fileName = String(entry.name());
    if (fileName.startsWith("CrashLog_") && fileName.endsWith(".txt")) {
      if (!foundCrashLogs) {
        sdLogger->println("=== Crash Logs Found ===");
        foundCrashLogs = true;
      }
      sdLogger->print("  ");
      sdLogger->print(entry.name());
      sdLogger->print(" (");
      sdLogger->print(entry.size());
      sdLogger->println(" bytes)");
    }
    entry.close();
  }
  root.close();

  if (!foundCrashLogs) {
    sdLogger->println("=== No crash logs found ===");
  } else {
    sdLogger->println("=========================");
  }

  sdLogger->println("========================================");
  sdLogger->println("SD Card initialized successfully!");

  // Load API key from SD card (optional)
  if (sdCardAvailable) {
    bool apiKeysLoaded = loadAPIKey();
    if (!apiKeysLoaded) {
      Serial.println("Failed to load API keys from SD card - continuing without API keys");
    } else {
      Serial.println("API keys loaded successfully from SD card");
    }
  } else {
    Serial.println("No SD card - API keys not available");
  }

  // Load WiFi credentials from SD card
  if (!loadWiFiCredentials()) {
    Serial.println("FATAL ERROR: Failed to load WiFi credentials from SD card");
    Serial.println("System cannot continue without WiFi credentials");
    while(1) {
      blinkLEDRed(1, 500);
      delay(500);
    }
  }

  // SD Card file reads completed successfully - blink purple 3 times and stay purple
  Serial.println("SD Card file reads completed successfully!");
  blinkLEDPurple(3, 500);
  setLEDPurple();  // Keep purple LED on

  // Scan for available WiFi networks
  scanWiFiNetworks();

  // Connect to WiFi (retry until successful)
  connectToWiFi();

  // Initialize web interface
  setupWebServer();

  // Start web server
  server.begin();
  Serial.println("Web server started");
  Serial.println("Access web interface at: http://" + WiFi.localIP().toString());

  // System fully initialized - set LED to solid blue
  setLEDBlue();
  Serial.println("System ready! LED showing solid blue.");
}

/*
 * LED Status Indicator Summary:
 * 1. Blink white 3 times + solid white - Initial startup/initialization
 * 2. Blink red 3 times (loop) - SD Card failed, retrying
 * 3. Blink purple 3 times + solid purple - SD Card files read successfully
 * 4. Green blinking - WiFi connecting (alternates with OFF every 500ms)
 * 5. Solid blue - System fully ready and operational
 */

void loop() {
  // Feed watchdog in main loop
  // esp_task_wdt_reset();
  yield();

  // Process Lichess async operations for all sessions (non-blocking SSL cleanup, moves, etc)
  sessionManager.processAllSessions();

  // Process Lichess stream events
  processLichessStreamEvents();

  // Process WebRTC signaling cleanup
  webrtcHandler.processCleanup();

  // Handle reset button
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      digitalWrite(STATUS_LED_PIN, LOW);
      delay(500);
      digitalWrite(STATUS_LED_PIN, HIGH);
      delay(1000); // Prevent multiple triggers
    }
  }

  // Keep main loop running frequently for async tasks
  delay(10);
  // esp_task_wdt_reset(); // Feed watchdog again
  yield(); // Allow async tasks to run
}

bool initializeSPIFFS() {

  if (!SPIFFS.begin(true)) {
    if (!SPIFFS.format()) {
      return false;
    }

    if (!SPIFFS.begin(true)) {
      return false;
    }
  }


  return true;
}

bool loadWiFiCredentials() {
  Serial.println("Loading WiFi credentials from SD card...");

  int retryCount = 0;

  while (true) {
    retryCount++;
    Serial.println("SD card read attempt #" + String(retryCount));

    // Check if SD card is available first
    if (SD.cardType() == CARD_NONE) {
      Serial.println("SD card not available - reinitializing...");
      // Disable and re-enable SD card
      SD.end();
      delay(1000);
      if (!SD.begin(SD_CS_PIN)) {
        Serial.println("SD card reinitialization failed, retrying...");
        delay(2000);
        continue;
      }
      Serial.println("SD card reinitialized successfully");
    }

    if (!SD.exists("/WIFI.MD")) {
      Serial.println("ERROR: WiFi credentials file '/WIFI.MD' not found on SD card!");
      Serial.println("Please create WIFI.MD file with your WiFi credentials");
      delay(5000);
      continue;
    }

    File file = SD.open("/WIFI.MD", FILE_READ);
    if (!file) {
      Serial.println("Failed to open WiFi credentials file - retrying SD card read...");
      delay(1000);
      continue;
    }

    String line;
    bool ssidFound = false, passwordFound = false;

    while (file.available()) {
      line = file.readStringUntil('\n');
      line.trim(); // Remove whitespace and newline characters

      // Skip empty lines and comments
      if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) {
        continue;
      }

      // Parse ssid line
      if (line.startsWith("ssid = ")) {
        int startQuote = line.indexOf('"');
        int endQuote = line.lastIndexOf('"');
        if (startQuote != -1 && endQuote != -1 && startQuote != endQuote) {
          ssid = line.substring(startQuote + 1, endQuote);
          ssidFound = true;
          String maskedSSID = ssid.substring(0, 3) + "******";
          Serial.println("SSID found: " + maskedSSID);
        }
      }
      // Parse password line
      else if (line.startsWith("password = ")) {
        int startQuote = line.indexOf('"');
        int endQuote = line.lastIndexOf('"');
        if (startQuote != -1 && endQuote != -1 && startQuote != endQuote) {
          password = line.substring(startQuote + 1, endQuote);
          passwordFound = true;
          Serial.println("Password found (length: " + String(password.length()) + " chars)");
        }
      }
    }

    file.close();

    if (!ssidFound) {
      Serial.println("SSID not found in wifi.MD file - retrying SD card read...");
      delay(1000);
      continue;
    }

    if (!passwordFound) {
      Serial.println("Password not found in wifi.MD file - retrying SD card read...");
      delay(1000);
      continue;
    }

    if (password.length() == 0) {
      Serial.println("Password is empty - retrying SD card read...");
      delay(1000);
      continue;
    }

    Serial.println("WiFi credentials loaded successfully from SD card");
    return true;
  } // End of while loop
}


void scanWiFiNetworks() {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  int numNetworks = WiFi.scanNetworks();

  if (numNetworks == 0) {
  } else {

    bool targetNetworkFound = false;
    bool preferWPA2Found = false;
    for (int i = 0; i < numNetworks; ++i) {
      String networkSSID = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      wifi_auth_mode_t encryptionType = WiFi.encryptionType(i);

      String encType = "";
      switch(encryptionType) {
        case WIFI_AUTH_OPEN: encType = "OPEN"; break;
        case WIFI_AUTH_WEP: encType = "WEP"; break;
        case WIFI_AUTH_WPA_PSK: encType = "WPA"; break;
        case WIFI_AUTH_WPA2_PSK: encType = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: encType = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: encType = "WPA2_ENT"; break;
        default: encType = "UNKNOWN"; break;
      }


      if (networkSSID == ssid) {
        targetNetworkFound = true;

        // Prefer WPA2 networks over others
        if (encryptionType == WIFI_AUTH_WPA2_PSK || encryptionType == WIFI_AUTH_WPA_WPA2_PSK) {
          preferWPA2Found = true;
        }
      }
    }

    if (targetNetworkFound && preferWPA2Found) {
    }

    if (!targetNetworkFound) {
    }
  }

  WiFi.scanDelete();
  delay(1000);
}

void connectToWiFi() {
  int retryCount = 0;

  while (true) {
    retryCount++;
    String maskedSSID = ssid.substring(0, 3) + "******";
    Serial.println("WiFi connection attempt #" + String(retryCount) + " - Connecting to: " + maskedSSID);

    // Disable and re-enable WiFi adapter
    Serial.println("Disabling WiFi adapter...");
    WiFi.mode(WIFI_OFF);
    delay(1000);

    Serial.println("Re-enabling WiFi adapter...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(2000);

    // Show WiFi configuration details (SSID masked for security)
    Serial.println("WiFi Config - SSID: '" + maskedSSID + "', Password length: " + String(password.length()) + " chars");

    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("WiFi connection initiated...");

    int attempts = 0;
    const int maxAttempts = 40; // 20 seconds timeout

    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
      delay(500);
      Serial.print(".");

      // Toggle green LED with each dot
      if (attempts % 2 == 0) {
        setLEDGreen();
      } else {
        setLEDOff();
      }

      attempts++;

      // Enhanced debugging every 5 seconds
      if (attempts % 10 == 0) {
        wl_status_t status = WiFi.status();
        String statusText = "";
        switch(status) {
          case WL_NO_SHIELD: statusText = "WL_NO_SHIELD"; break;
          case WL_IDLE_STATUS: statusText = "WL_IDLE_STATUS"; break;
          case WL_NO_SSID_AVAIL: statusText = "WL_NO_SSID_AVAIL (Network not found)"; break;
          case WL_SCAN_COMPLETED: statusText = "WL_SCAN_COMPLETED"; break;
          case WL_CONNECTED: statusText = "WL_CONNECTED"; break;
          case WL_CONNECT_FAILED: statusText = "WL_CONNECT_FAILED (Bad password/auth)"; break;
          case WL_CONNECTION_LOST: statusText = "WL_CONNECTION_LOST"; break;
          case WL_DISCONNECTED: statusText = "WL_DISCONNECTED"; break;
          default: statusText = "UNKNOWN_STATUS"; break;
        }
        Serial.println("\nWiFi status: " + String(status) + " (" + statusText + "), attempts: " + String(attempts) + "/" + String(maxAttempts));
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n\nWiFi connected successfully!");
      Serial.println("IP address: " + WiFi.localIP().toString());
      Serial.println("Gateway: " + WiFi.gatewayIP().toString());
      Serial.println("DNS: " + WiFi.dnsIP().toString());
      Serial.println("Signal strength: " + String(WiFi.RSSI()) + "dBm");
      return; // Exit function on successful connection
    } else {
      wl_status_t finalStatus = WiFi.status();
      Serial.println("\nWiFi connection attempt #" + String(retryCount) + " failed");
      Serial.println("Final status: " + String(finalStatus));

      if (finalStatus == WL_NO_SSID_AVAIL) {
        String maskedSSID = ssid.substring(0, 3) + "******";
        Serial.println("ERROR: Network '" + maskedSSID + "' not found. Retrying...");
      } else if (finalStatus == WL_CONNECT_FAILED) {
        Serial.println("ERROR: Connection failed. Check password and network security settings. Retrying...");
      } else {
        Serial.println("ERROR: Unknown connection failure. Retrying...");
      }

      Serial.println("Waiting 3 seconds before next attempt...");
      delay(3000);
    }
  }
}

void setupWebServer() {
  // Initialize Gemini API
  geminiAPI.begin();

  // Initialize GameController
  gameController.begin(nullptr, &geminiAPI, nullptr);

  // Initialize WebInterface
  webInterface.setGeminiAPI(&geminiAPI);
  webInterface.setGameController(&gameController);
  webInterface.begin(&server, &sessionManager);

  // Connect GameController to WebInterface for board updates
  gameController.setWebInterface(&webInterface);

  // Initialize Lichess Web Handler with SessionManager
  lichessWebHandler.begin(&server, &lichessAPI, &sessionManager);

  // Initialize WebRTC Handler
  webrtcHandler.begin(&server);

  // Load admin IPs from SD card /admin-auth.md file
  // File format: one IP address per line, lines starting with # are comments
  Serial.println("Loading admin IPs from SD card...");
  if (sessionManager.loadAdminIPsFromSD()) {
    Serial.println("Admin IPs loaded successfully");
  } else {
    Serial.println("No admin IPs loaded (admin-auth.md not found or empty)");
  }

}

bool initializeSDCard() {
  Serial.println("Initializing SD card...");

  // Initialize SPI with custom pins
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS_PIN);
  delay(100);

  if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {  // Use 4MHz SPI speed for stability
    Serial.println("SD card initialization failed!");
    Serial.println("Check if SD card is inserted and wired correctly");
    return false;
  }

  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }

  String cardTypeStr;
  if (cardType == CARD_MMC) {
    cardTypeStr = "MMC";
  } else if (cardType == CARD_SD) {
    cardTypeStr = "SDSC";
  } else if (cardType == CARD_SDHC) {
    cardTypeStr = "SDHC";
  } else {
    cardTypeStr = "UNKNOWN";
  }
  Serial.println("SD Card Type: " + cardTypeStr);

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.println("SD Card Size: " + String((unsigned long)cardSize) + "MB");

  Serial.println("SD card initialized successfully");
  return true;
}

bool loadAPIKey() {
  Serial.println("Loading API keys from SD card...");

  if (!SD.exists("/API_Keys.MD")) {
    Serial.println("WARNING: API_Keys.MD file not found on SD card");
    return false;
  }
  Serial.println("API_Keys.MD file found");

  File file = SD.open("/API_Keys.MD", FILE_READ);
  if (!file) {
    Serial.println("ERROR: Failed to open API_Keys.MD file");
    return false;
  }
  Serial.println("API_Keys.MD file opened successfully");

  String geminiApiKey = "";
  String lichessApiKey = "";
  bool geminiKeyFound = false;
  bool lichessKeyFound = false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim(); // Remove whitespace and newline characters

    // Skip empty lines and comments
    if (line.length() == 0 || line.startsWith("#") || line.startsWith("//")) {
      continue;
    }

    // Parse GeminiAPI-Key line
    if (line.startsWith("GeminiAPI-Key = ")) {
      int startPos = line.indexOf("= ") + 2;
      if (startPos > 2) {
        geminiApiKey = line.substring(startPos);
        geminiApiKey.trim();
        geminiKeyFound = true;
      }
    }

    // Parse LichessAPI-Key line
    if (line.startsWith("LichessAPI-Key = ")) {
      int startPos = line.indexOf("= ") + 2;
      if (startPos > 2) {
        lichessApiKey = line.substring(startPos);
        lichessApiKey.trim();
        // Remove backslash escape characters (Windows 11 Notepad adds these)
        lichessApiKey.replace("\\", "");
        lichessKeyFound = true;
      }
    }
  }

  file.close();

  // Configure Gemini API if key found
  if (geminiKeyFound && !geminiApiKey.isEmpty()) {
    geminiAPI.setAPIKey(geminiApiKey);
  }

  // Configure Chess API if key found
  if (lichessKeyFound && !lichessApiKey.isEmpty()) {
    lichessAPI.begin(lichessApiKey.c_str());
    sessionManager.setAPIToken(lichessApiKey.c_str());  // Set API token for all session instances
    Serial.println("Chess API key loaded and configured successfully");

    // Mask API key for security (show first 3 chars + asterisks)
    String maskedKey = lichessApiKey.substring(0, 3);
    for (int i = 3; i < lichessApiKey.length(); i++) {
      maskedKey += "*";
    }
    Serial.printf("Chess API key: %s (length: %d chars)\n", maskedKey.c_str(), lichessApiKey.length());
  } else {
    Serial.println("WARNING: No Chess API key found in API_Keys.MD!");
  }

  // Return true if at least one key was found
  return (geminiKeyFound || lichessKeyFound);
}

void blinkError() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
  }
}