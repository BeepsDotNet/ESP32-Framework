# Chess AI vs AI - Project Context Restoration

## Project Overview
This is **Malcolm's Chess v4.3** - an ESP32-based chess application that displays on mobile devices and interacts with the Lichess API for human vs AI chess gameplay. The project recently transitioned from Gemini AI vs AI gameplay to Lichess-based human vs AI gameplay.

**Current Git Branch:** v4.3
**Display Version:** Malcolm's Chess v4.3 - Lichess

## Hardware Environment

### ESP32 Board
- **Board:** Adafruit Feather ESP32
- **Upload Port:** COM6 (commented out in platformio.ini - user manages uploads manually)
- **Status LED:** Pin 2
- **Reset Button:** Pin 0
- **Network IP:** 192.168.1.229

### SD Card Configuration
- **CS Pin:** 15 (silkscreened A3)
- **SPI SCK:** 14
- **SPI MISO:** 13
- **SPI MOSI:** 12
- **Speed:** 4MHz for stability

## Software Environment

### PlatformIO Configuration
```ini
platform = espressif32
board = featheresp32
framework = arduino
build_src_filter = +<*> -<backup/>
```

### Dependencies
- `esphome/ESPAsyncWebServer-esphome@^3.0.0`
- `esphome/AsyncTCP-esphome@^2.0.0`
- `bblanchon/ArduinoJson@^7.0.0`

### Development Mode
- `DEVELOPMENT_MODE = true` in `include/config.h`
- Enables HTML file upload via HTTP to ESP32
- Production mode would use existing SD card file

## Project Structure

### Core C++ Files

**include/**
- `config.h` - Development mode flag, pin definitions, timeouts
- `DebugUtils.h` - Debug message utilities
- `GeminiAPI.h` - Legacy Gemini API (still present but not primary)
- `GameController.h` - Chess game logic controller
- `WebInterface.h` - Main web server interface
- `LichessAPI.h` - **NEW** Lichess Board API integration
- `LichessWebHandler.h` - **NEW** Lichess web endpoints and SSE
- `ChessEngine.h`, `NetworkManager.h`, `StorageManager.h`, `SPI_PINS.h`

**src/**
- `main.cpp` - **RECENTLY UPDATED** with Lichess integration
  - Initializes both GeminiAPI and LichessAPI
  - Loads API keys from `/API_Keys.MD` on SD card
  - Calls `processLichessStreamEvents()` in loop()
  - Sets up web server with Lichess endpoints
- `LichessAPI.cpp` - Implements Lichess Board API calls
- `LichessWebHandler.cpp` - Implements `/api/lichess/*` endpoints
- `GeminiAPI.cpp`, `GameController.cpp`, `WebInterface.cpp`, `DebugUtils.cpp`

### Frontend Files

**chess-app.html** (18,867 bytes)
- Title: "Malcolm's Chess v4.3 - Lichess"
- Uses chess.js library from CDN: `https://cdn.jsdelivr.net/npm/chess.js@0.12.0/chess.min.js`
- Implements Lichess integration with SSE (Server-Sent Events)
- Key endpoints called:
  - `POST /api/lichess/create-game` - Creates AI game on Lichess
  - `POST /api/lichess/move` - Submits moves
  - `POST /api/lichess/resign` - Resigns game
  - `GET /api/lichess/stream` - SSE stream for game events
  - `GET /api/lichess/status` - Check stream status
  - `GET /api/lichess/account` - Verify Lichess account

**Backup Files:**
- `chess-app-backup.html` - Previous version backup
- `chess-app-gemini-backup.html` - Original Gemini AI version (59KB)

### Documentation Files

**docs/**
- `PromptHistory.MD` - Complete chronological prompt history (3100+ lines)
- `ESP32-Lichess-Architecture.md` - Technical architecture for Lichess integration
- `Lichess-Integration-Guide.md` - Step-by-step integration instructions
- `ARCHITECTURE.md`, `SETUP.md` - Original project documentation
- `GEMINI-API-KEY.MD` - Gemini API key storage format (local only, not in git)
- `Restore.MD` - **THIS FILE** - Session restoration prompt

### Configuration Files (Local, Not in Git)
- `include/API_Keys.MD` - Stores both API keys:
  ```
  GeminiAPI-Key = <key>
  LichessAPI-Key = <key>
  ```
- `WIFI.MD` - WiFi credentials
- `Lichess API Key.png` - API key screenshot

### Upload Scripts
- `upload_html.py` - Primary HTTP upload to ESP32 at 192.168.1.229
- `upload_html_serial.py` - Backup serial upload via COM6
- `upload_gittest.py`, `upload_gittest_chunked.py` - Test scripts

## Recent Development Work

### Session Summary
1. **Started with:** Lichess API test interface on api-test branch
2. **Restored:** v4.2 branch files, then created v4.3 branch
3. **Created:** Complete Lichess integration architecture and code
4. **Implemented:** LichessAPI and LichessWebHandler classes
5. **Fixed:** Deprecated ArduinoJson v7 syntax issues
   - Changed `DynamicJsonDocument` → `JsonDocument`
   - Changed `containsKey()` → `.is<T>()`
   - Made `forwardLichessEvents()` public
6. **Converted:** chess-app.html from Gemini (59KB) to Lichess (18KB)
7. **Integrated:** Lichess components into main.cpp

### Latest Changes to main.cpp
```cpp
// Added includes (lines 12-13)
#include "LichessAPI.h"
#include "LichessWebHandler.h"

// Added global instances (lines 28-29)
LichessAPI lichessAPI;
LichessWebHandler lichessWebHandler;

// Modified loadAPIKey() to parse LichessAPI-Key (lines 524-548)
// Added lichessAPI.begin(lichessApiKey.c_str());

// Added to setupWebServer() (line 445)
lichessWebHandler.begin(&server, &lichessAPI);

// Added to loop() (line 109)
processLichessStreamEvents();
```

## Current Status

### What Works
- ✅ C++ backend fully integrated into main.cpp
- ✅ All Lichess API endpoints implemented
- ✅ HTML interface converted to Lichess
- ✅ ArduinoJson v7 compatibility issues fixed
- ✅ Compile-ready (no known errors)

### What Needs Testing
- ⏳ ESP32 firmware compilation and upload
- ⏳ HTML upload to ESP32 (currently ESP32 offline at 192.168.1.229)
- ⏳ Lichess game creation from browser
- ⏳ Move submission and AI response handling
- ⏳ SSE event stream functionality

### Known Issues
- ESP32 device not responding at 192.168.1.229 (connection timeout)
- User needs to compile and upload firmware to ESP32
- User needs to ensure Lichess API key is in `/API_Keys.MD` on SD card

## Workflow Commands

### Update Version Text
```bash
# Updates version numbers throughout codebase to match git branch
```

### Upload HTML to ESP32
```bash
python upload_html.py                    # Upload to 192.168.1.229
python upload_html_serial.py COM6        # Upload via serial
python upload_html.py 192.168.1.229      # Custom IP
```

### Git Branch Management
- **Current branch:** v4.3
- **Previous branch:** v4.2
- **Test branch:** api-test (contains Lichess notes and test files)

### File Management Notes
- User manually performs compiles and uploads unless directed otherwise
- Never upload to git: `WIFI.MD`, `API_Keys.MD`, `Lichess API Key.png`, `GEMINI-API-KEY.MD`
- Always append prompts to `PromptHistory.MD` automatically

## Lichess Integration Architecture

### API Flow
1. **Browser** → `POST /api/lichess/create-game` → **ESP32** → Lichess API
2. **ESP32** opens stream to Lichess `/api/board/game/stream/{gameId}`
3. **ESP32** forwards NDJSON events via SSE to **Browser** at `/api/lichess/stream`
4. **Browser** displays moves, sends user moves via `POST /api/lichess/move`
5. **ESP32** submits moves to Lichess, AI responds, events forwarded back

### Key Technical Details
- Lichess uses NDJSON (newline-delimited JSON) streaming
- ESP32 processes line-by-line in `processStreamEvents()`
- SSE (Server-Sent Events) relays to browser via AsyncEventSource
- chess.js validates moves client-side before submission
- UCI notation used for all moves (e.g., "e2e4", "e7e5")

## Next Steps

When resuming this session, you can immediately:
- Upload firmware to ESP32
- Test Lichess game creation
- Debug any runtime issues
- Optimize performance or add features
- Continue with any git operations or file modifications

**No fundamental questions needed** - the project is fully integrated and ready for testing/deployment.
