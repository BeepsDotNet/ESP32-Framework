# Chess AI vs AI Setup Guide

## Hardware Requirements

- **Adafruit Feather ESP32** (original ESP32, not S3)
- **MicroSD card** (8GB or larger, FAT32 formatted)
- **MicroSD card adapter/reader** for ESP32
- **WiFi network** with internet access

## Pin Connections

### SD Card Module
```
SD Card    ESP32 Pin
VCC    ->  3.3V
GND    ->  GND
MISO   ->  GPIO 19 (MISO)
MOSI   ->  GPIO 23 (MOSI)
SCK    ->  GPIO 18 (SCK)
CS     ->  GPIO 5  (configurable)
```

### Status LED
- **GPIO 2** - Built-in LED for status indication

### Reset Button
- **GPIO 0** - Reset/new game button (built-in)

## Software Setup

### 1. Install PlatformIO
```bash
# Install PlatformIO Core
pip install platformio

# Or use PlatformIO IDE extension in VS Code
```

### 2. Clone and Build
```bash
# Clone the project
git clone <repository-url>
cd chess-ai-vs-ai

# Build the project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor
```

### 3. WiFi Configuration
1. Copy `examples/wifi.MD` to the root of your SD card as `/wifi.MD`
2. Edit the file with your WiFi credentials:
```
ssid = "YourWiFiNetwork"
password = "YourWiFiPassword"
```

**Example wifi.MD content:**
```
# WiFi Configuration
ssid = "dRouter"
password = "WEWENTTOTHESTORE"
```

### 4. AI API Configuration
Set up API keys for the AI services:

#### Option A: In Code (Quick Start)
Edit `src/wifi_config.h`:
```cpp
#define OPENAI_API_KEY "your-openai-api-key"
#define ANTHROPIC_API_KEY "your-anthropic-api-key"
```

#### Option B: SD Card (Secure)
1. Create AI configuration files on SD card:
   - `/chess_games/config/ai1_config.json`
   - `/chess_games/config/ai2_config.json`
2. Use the format from `examples/ai_config_example.json`

### 5. SD Card Setup
1. Format microSD card as FAT32
2. Insert into ESP32 SD card slot
3. The system will automatically create the directory structure:
```
/chess_games/
  ├── config/
  ├── games/
  └── logs/
```

## First Run

1. **Power on** the ESP32
2. **Check serial monitor** for WiFi connection and IP address
3. **Access web interface** at `http://[ESP32-IP-ADDRESS]`
4. **Start a new game** using the web interface

## Web Interface

### Game Controls
- **Start New Game** - Begin a fresh game
- **Pause** - Pause the current game
- **Resume** - Resume a paused game
- **Reset** - Start over (confirms before resetting)

### Live Monitoring
- **Chess board** - Real-time board position
- **Game status** - Current player, move count, game state
- **AI information** - Response times, last moves
- **Move history** - Complete game notation

## Troubleshooting

### Common Issues

#### WiFi Connection Failed
- Check SSID and password in `wifi_config.h`
- Ensure ESP32 is within WiFi range
- Check serial monitor for specific error codes

#### SD Card Not Detected
- Verify SD card is FAT32 formatted
- Check wiring connections
- Try a different SD card (some cards are incompatible)

#### AI Not Responding
- Verify API keys are correct
- Check internet connectivity
- Monitor serial output for HTTP error codes
- Ensure AI service is not rate-limited

#### Web Interface Not Loading
- Check IP address in serial monitor
- Verify ESP32 and computer are on same network
- Try accessing `http://[ip]/api/status` directly

### LED Status Codes
- **Solid ON** - System ready and running
- **Fast blink** - Error condition (check serial monitor)
- **Slow blink** - Connecting to WiFi

### Serial Monitor Commands
Monitor at 115200 baud for debug information:
- Game state changes
- AI responses
- Network status
- Error messages

## API Endpoints

The ESP32 exposes a REST API for integration:

- `GET /api/status` - Current game status
- `GET /api/board` - Chess board HTML
- `GET /api/moves` - Move history
- `POST /api/start` - Start new game
- `POST /api/pause` - Pause game
- `POST /api/resume` - Resume game
- `POST /api/reset` - Reset game

## Game Persistence

Games are automatically saved to SD card:
- **Current game** - `/chess_games/games/current_game.json`
- **Archived games** - `/chess_games/games/game_[timestamp].json`
- **Logs** - `/chess_games/logs/game_log_[date].txt`

Games automatically resume after power loss or network interruption.

## Performance Tuning

### AI Response Timeout
Adjust in `src/wifi_config.h`:
```cpp
#define AI_MOVE_TIMEOUT_MS 60000  // 60 seconds
```

### Game Save Frequency
```cpp
#define GAME_SAVE_INTERVAL_MS 5000  // 5 seconds
```

### Memory Management
- The system maintains up to 10 archived games by default
- Logs are rotated to prevent SD card overflow
- JSON responses are limited to prevent memory issues