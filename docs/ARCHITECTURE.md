# Chess AI vs AI System Architecture

## Overview
An ESP32-based system that orchestrates chess games between two AI chat applications through web scraping, with real-time monitoring via web interface and persistent game state storage on SD card.

## System Components

### 1. Core Modules
- **GameController**: Main orchestrator managing game flow
- **WebScraper**: HTTP client for AI chat applications
- **ChessEngine**: Game logic, move validation, and board state
- **StorageManager**: SD card operations for persistence
- **WebInterface**: Real-time game monitoring dashboard
- **NetworkManager**: WiFi and HTTP connection management

### 2. Data Flow
```
[AI Chat App 1] ←→ [WebScraper] ←→ [GameController] ←→ [ChessEngine]
                                        ↓
[Web Interface] ←→ [ESP32] ←→ [StorageManager] ←→ [SD Card]
                                        ↑
[AI Chat App 2] ←→ [WebScraper] ←→ [GameController]
```

### 3. Storage Structure (SD Card)
```
/chess_games/
  ├── config/
  │   ├── ai1_config.json
  │   ├── ai2_config.json
  │   └── system_config.json
  ├── games/
  │   ├── game_YYYYMMDD_HHMMSS.json
  │   └── current_game.json
  └── logs/
      ├── game_log_YYYYMMDD.txt
      └── error_log.txt
```

### 4. Game State JSON Structure
```json
{
  "gameId": "game_20240921_154500",
  "status": "in_progress",
  "currentPlayer": "white",
  "moveCount": 15,
  "board": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
  "moves": ["e2e4", "e7e5", "Ng1f3"],
  "ai1": {
    "name": "ChatGPT",
    "color": "white",
    "endpoint": "https://chat.openai.com",
    "lastMove": "Ng1f3",
    "thinkTime": 5000
  },
  "ai2": {
    "name": "Claude",
    "color": "black",
    "endpoint": "https://claude.ai",
    "lastMove": "",
    "thinkTime": 4500
  },
  "timestamp": "2024-09-21T15:45:00Z"
}
```

### 5. Web Interface Features
- Live board visualization
- Game progress and statistics
- AI response times
- Move history
- Game control (pause/resume/restart)
- System status and logs

### 6. Recovery Mechanism
- Automatic save after each move
- Game state restoration on startup
- Network failure handling
- AI response timeout management

## Pin Configuration (ESP32)
- SD Card: SPI pins (MISO, MOSI, SCK, CS)
- Status LED: GPIO 2
- Reset Button: GPIO 0

## Communication Protocols
- WiFi for internet connectivity
- HTTPS for AI chat applications
- HTTP for web interface
- SPI for SD card communication