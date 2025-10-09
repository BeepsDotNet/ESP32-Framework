#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Project headers
#include "GameController.h"
#include "WebScraper.h"
#include "ChessEngine.h"
#include "StorageManager.h"
#include "WebInterface.h"
#include "NetworkManager.h"

// Pin definitions
#define SD_CS_PIN 5
#define STATUS_LED_PIN 2
#define RESET_BUTTON_PIN 0

// Global objects
GameController gameController;
WebScraper webScraper;
ChessEngine chessEngine;
StorageManager storageManager;
WebInterface webInterface;
NetworkManager networkManager;

AsyncWebServer server(80);

// WiFi Configuration (loaded from SD card)
String ssid = "";
String password = "";

// Function prototypes
bool initializeSDCard();
bool loadWiFiCredentials();
bool createDefaultWiFiFile();
void blinkError();

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Chess AI vs AI System Starting...");

  // Initialize pins
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Initialize SD card
  if (!initializeSDCard()) {
    Serial.println("SD Card initialization failed!");
    blinkError();
    return;
  }

  // Initialize storage manager
  storageManager.begin();

  // Load WiFi credentials from SD card
  if (!loadWiFiCredentials()) {
    Serial.println("Failed to load WiFi credentials!");
    blinkError();
    return;
  }

  // Connect to WiFi
  if (!networkManager.connect(ssid.c_str(), password.c_str())) {
    Serial.println("WiFi connection failed!");
    blinkError();
    return;
  }

  // Initialize chess engine
  chessEngine.begin();

  // Initialize web scraper
  webScraper.begin();

  // Initialize game controller
  gameController.begin(&chessEngine, &webScraper, &storageManager);

  // Initialize web interface
  webInterface.begin(&server);
  webInterface.setGameController(&gameController);

  // Try to restore previous game
  if (storageManager.hasCurrentGame()) {
    Serial.println("Restoring previous game...");
    gameController.restoreGame();
  } else {
    Serial.println("Starting new game...");
    gameController.startNewGame();
  }

  // Start web server
  server.begin();
  Serial.println("Web server started");
  Serial.printf("Access web interface at: http://%s\n", WiFi.localIP().toString().c_str());

  digitalWrite(STATUS_LED_PIN, HIGH);
  Serial.println("System ready!");
}

void loop() {
  // Handle reset button
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("Reset button pressed - starting new game");
      gameController.startNewGame();
      delay(1000); // Prevent multiple triggers
    }
  }

  // Update game controller
  gameController.update();

  // Update web interface
  webInterface.update();

  // Update network manager
  networkManager.update();

  // Small delay to prevent watchdog issues
  delay(10);
}

bool initializeSDCard() {
  Serial.println("Initializing SD card...");

  if (!SD_MMC.begin()) {
    Serial.println("SD card mount failed");
    return false;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return false;
  }

  Serial.printf("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // Create directory structure
  if (!SD_MMC.exists("/chess_games")) {
    SD_MMC.mkdir("/chess_games");
  }
  if (!SD_MMC.exists("/chess_games/config")) {
    SD_MMC.mkdir("/chess_games/config");
  }
  if (!SD_MMC.exists("/chess_games/games")) {
    SD_MMC.mkdir("/chess_games/games");
  }
  if (!SD_MMC.exists("/chess_games/logs")) {
    SD_MMC.mkdir("/chess_games/logs");
  }

  Serial.println("SD card initialized successfully");
  return true;
}

bool loadWiFiCredentials() {
  Serial.println("Loading WiFi credentials from SD card...");

  if (!SD_MMC.exists("/wifi.MD")) {
    Serial.println("WiFi credentials file '/wifi.MD' not found");
    Serial.println("Creating default wifi.MD file...");
    return createDefaultWiFiFile();
  }

  File file = SD_MMC.open("/wifi.MD");
  if (!file) {
    Serial.println("Failed to open WiFi credentials file");
    return false;
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
        Serial.printf("SSID found: %s\n", ssid.c_str());
      }
    }
    // Parse password line
    else if (line.startsWith("password = ")) {
      int startQuote = line.indexOf('"');
      int endQuote = line.lastIndexOf('"');
      if (startQuote != -1 && endQuote != -1 && startQuote != endQuote) {
        password = line.substring(startQuote + 1, endQuote);
        passwordFound = true;
        Serial.println("Password found: [HIDDEN]");
      }
    }
  }

  file.close();

  if (!ssidFound) {
    Serial.println("SSID not found in wifi.MD file");
    return false;
  }

  if (!passwordFound) {
    Serial.println("Password not found in wifi.MD file");
    return false;
  }

  Serial.println("WiFi credentials loaded successfully");
  return true;
}

bool createDefaultWiFiFile() {
  Serial.println("Creating default WiFi configuration file...");

  // Default WiFi credentials
  String defaultSSID = "dRouter";
  String defaultPassword = "WEWENTTOTHESTORE";

  // WiFi file content
  String wifiContent = "# WiFi Configuration File\n";
  wifiContent += "# Automatically created by Chess AI vs AI system\n";
  wifiContent += "# Edit this file to change WiFi credentials\n";
  wifiContent += "#\n";
  wifiContent += "# Format: key = \"value\"\n";
  wifiContent += "# Lines starting with # are comments and will be ignored\n";
  wifiContent += "\n";
  wifiContent += "ssid = \"" + defaultSSID + "\"\n";
  wifiContent += "password = \"" + defaultPassword + "\"\n";

  // Write file to SD card
  File file = SD_MMC.open("/wifi.MD", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create wifi.MD file");
    return false;
  }

  size_t bytesWritten = file.print(wifiContent);
  file.close();

  if (bytesWritten != wifiContent.length()) {
    Serial.println("Failed to write complete wifi.MD file");
    return false;
  }

  Serial.println("Default wifi.MD file created successfully");
  Serial.printf("SSID: %s\n", defaultSSID.c_str());
  Serial.println("Password: [HIDDEN]");

  // Now load the credentials we just created
  ssid = defaultSSID;
  password = defaultPassword;

  return true;
}

void blinkError() {
  for (int i = 0; i < 10; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
  }
}