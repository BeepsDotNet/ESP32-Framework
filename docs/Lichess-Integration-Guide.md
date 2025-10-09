# Lichess Integration Guide for main.cpp

## Overview
This guide shows how to integrate the Lichess API components into the existing ESP32 chess application.

## Files Added

### Header Files (include/)
- `LichessAPI.h` - Core Lichess API communication
- `LichessWebHandler.h` - Web endpoints for Lichess

### Source Files (src/)
- `LichessAPI.cpp` - Implementation of Lichess API
- `LichessWebHandler.cpp` - Implementation of web handlers

## Integration Steps

### Step 1: Update platformio.ini

Add required libraries:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps =
    ESP Async WebServer
    ArduinoJson
    HTTPClient    ; <-- Add if not present
```

### Step 2: Update config.h

Add Lichess configuration:

```cpp
// Lichess API Configuration
#define LICHESS_API_TOKEN_FILE "/api_keys.txt"
#define LICHESS_ENABLED true
```

### Step 3: Update main.cpp

#### Include Headers
```cpp
#include <Arduino.h>
#include "config.h"
#include "NetworkManager.h"
#include "StorageManager.h"
#include "WebInterface.h"
#include "GameController.h"
#include "GeminiAPI.h"
#include "LichessAPI.h"           // <-- Add
#include "LichessWebHandler.h"    // <-- Add
```

#### Declare Global Instances
```cpp
// Existing globals
NetworkManager networkManager;
StorageManager storageManager;
GameController gameController;
GeminiAPI geminiAPI;
WebInterface webInterface;
AsyncWebServer server(80);

// Add Lichess globals
LichessAPI lichessAPI;                 // <-- Add
LichessWebHandler lichessWebHandler;   // <-- Add
```

#### Load API Token in setup()
```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("Starting ESP32 Chess Application...");

    // Initialize SD card
    if (!storageManager.begin()) {
        Serial.println("SD Card initialization failed!");
        return;
    }

    // Initialize network
    if (!networkManager.begin()) {
        Serial.println("Network initialization failed!");
        return;
    }

    // Load API keys from SD card
    File apiFile = SD.open("/api_keys.txt");
    if (apiFile) {
        String geminiKey = "";
        String lichessKey = "";

        while (apiFile.available()) {
            String line = apiFile.readStringUntil('\n');
            line.trim();

            if (line.startsWith("GeminiAPI-Key")) {
                int eqPos = line.indexOf('=');
                if (eqPos > 0) {
                    geminiKey = line.substring(eqPos + 1);
                    geminiKey.trim();
                }
            }
            else if (line.startsWith("LichessAPI-Key")) {
                int eqPos = line.indexOf('=');
                if (eqPos > 0) {
                    lichessKey = line.substring(eqPos + 1);
                    lichessKey.trim();
                }
            }
        }
        apiFile.close();

        // Initialize APIs
        if (geminiKey.length() > 0) {
            geminiAPI.begin(geminiKey.c_str());
            Serial.println("Gemini API initialized");
        }

        if (lichessKey.length() > 0) {
            lichessAPI.begin(lichessKey.c_str());
            lichessWebHandler.begin(&server, &lichessAPI);
            lichessWebHandler.setAPIToken(lichessKey.c_str());
            Serial.println("Lichess API initialized");
        }
    } else {
        Serial.println("Warning: API keys file not found");
    }

    // Initialize game controller
    gameController.begin();

    // Initialize web interface
    webInterface.begin(&server);
    webInterface.setGameController(&gameController);
    webInterface.setGeminiAPI(&geminiAPI);

    // Start web server
    server.begin();
    Serial.println("Web server started");
    Serial.print("Access at: http://");
    Serial.println(networkManager.getIPAddress());
}
```

#### Process Events in loop()
```cpp
void loop() {
    // Existing loop code
    networkManager.update();

    // Add Lichess stream processing
    processLichessStreamEvents();   // <-- Add this

    delay(10);
}
```

## API Endpoints Available

Once integrated, these endpoints will be available:

### Lichess Endpoints
- `GET /api/lichess/account` - Test account connection
- `POST /api/lichess/create-game` - Create AI game
  - Parameters: level, time, increment, color
- `POST /api/lichess/move` - Make a move
  - Parameters: move (UCI notation)
- `POST /api/lichess/resign` - Resign game
- `GET /api/lichess/status` - Get game status
- `GET /api/lichess/stream` - SSE stream of game events

### Existing Chess Endpoints (unchanged)
- `GET /api/board` - Get board state
- `POST /api/move` - Make move (Gemini mode)
- `POST /api/newgame` - Start new game
- `POST /api/reset` - Reset game
- etc.

## Testing

### 1. Test with lichess-test.html
- Upload `lichess-test.html` to ESP32
- Open in browser: `http://[ESP32-IP]/lichess-test.html`
- Click "Test Account Connection"
- Create game and test moves

### 2. Monitor Serial Output
```
Lichess API initialized
Lichess web handlers registered
Game created and stream started: abc123def
Forwarded event: {"type":"gameFull"...
```

### 3. Check Memory Usage
```cpp
// Add to loop() for monitoring
if (millis() % 10000 == 0) {
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}
```

## Troubleshooting

### "Lichess API not initialized"
- Check that API token file exists: `/api_keys.txt`
- Verify format: `LichessAPI-Key = lip_xxxxx`
- Ensure `lichessAPI.begin()` was called

### "Stream connection failed"
- Check WiFi connection
- Verify Lichess API token is valid
- Check free heap memory (needs >100KB)

### Memory Issues
- Close unused streams
- Limit number of SSE clients
- Reduce NDJSON buffer size if needed

## Next Steps

1. Compile and upload to ESP32
2. Test with `lichess-test.html`
3. Build full chess UI with chess.js
4. Replace Gemini game mode with Lichess mode

## Notes

- The Gemini API code remains functional
- Both modes can coexist (switch via UI)
- Lichess mode is lighter on memory (no chess engine needed)
- Stream connection auto-closes on game end
