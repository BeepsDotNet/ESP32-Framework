# ESP32 Lichess Integration Architecture

## Overview
This document outlines the C++ backend architecture for integrating Lichess Board API with the ESP32 chess application.

## Architecture Layers

### 1. LichessAPI Class (`LichessAPI.h` / `LichessAPI.cpp`)
**Purpose:** Low-level Lichess API communication

**Responsibilities:**
- HTTP/HTTPS communication with Lichess servers
- NDJSON stream parsing for game events
- API token authentication
- Error handling and retry logic

**Key Methods:**
```cpp
bool begin(const char* apiToken)              // Initialize with token
String createAIGame(...)                       // POST /api/challenge/ai
bool makeMove(gameId, uciMove)                 // POST /api/board/game/{id}/move/{uci}
bool startStream(gameId)                       // GET /api/board/game/stream/{id}
bool processStreamEvents(String& eventJson)    // Parse NDJSON events
bool resignGame(gameId)                        // POST /api/board/game/{id}/resign
```

**Technical Details:**
- Uses `WiFiClientSecure` for HTTPS
- Non-blocking stream processing
- Buffered NDJSON parser (line-by-line)
- Memory-efficient (ESP32 RAM constraints)

---

### 2. WebInterface Updates (`WebInterface.cpp`)
**Purpose:** Add new API endpoints for browser-ESP32 communication

**New Endpoints:**
```
GET  /api/lichess/account         → Test account connection
POST /api/lichess/create-game     → Create AI game
POST /api/lichess/move            → Submit move
POST /api/lichess/resign          → Resign game
GET  /api/lichess/stream          → SSE stream of game events
```

**Implementation:**
```cpp
// In WebInterface::setupRoutes()
server.on("/api/lichess/create-game", HTTP_POST, handleCreateGame);
server.on("/api/lichess/move", HTTP_POST, handleMove);
server.on("/api/lichess/stream", HTTP_GET, handleSSEStream);
```

---

### 3. SSE (Server-Sent Events) Handler
**Purpose:** Relay Lichess NDJSON stream to browser as SSE

**Flow:**
1. Browser opens EventSource connection to `/api/lichess/stream`
2. ESP32 maintains long-lived connection to Lichess
3. ESP32 parses NDJSON events from Lichess
4. ESP32 forwards events to browser as SSE messages
5. Browser receives real-time game updates

**Implementation Approach:**
```cpp
void handleSSEStream() {
    // Set SSE headers
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/event-stream", "");

    // Keep connection open
    while (server.client().connected()) {
        String eventJson;
        if (lichessAPI.processStreamEvents(eventJson)) {
            // Forward to browser
            server.sendContent("data: " + eventJson + "\n\n");
        }
        delay(50); // Prevent busy-wait
    }
}
```

---

### 4. Game State Manager
**Purpose:** Track current game state

**State Variables:**
```cpp
String currentGameId;
String playerColor;
bool gameActive;
String lastFEN;
```

---

## Data Flow

### Creating a Game
```
Browser                ESP32                Lichess
   |                     |                     |
   |--POST /create-game->|                     |
   |                     |--POST /challenge/ai>|
   |                     |<--{gameId}----------|
   |<--{gameId}----------|                     |
   |                     |                     |
   |--GET /stream------->|                     |
   |                     |--GET /stream/{id}-->|
   |<--SSE: gameFull-----|<--NDJSON: gameFull--|
```

### Making a Move
```
Browser                ESP32                Lichess
   |                     |                     |
   |--POST /move {uci}-->|                     |
   |                     |--POST /move/{uci}-->|
   |                     |<--{ok: true}--------|
   |<--{success}---------|                     |
   |                     |                     |
   |<--SSE: gameState----|<--NDJSON: gameState-|
   |  (AI response)      |   (AI moved)        |
```

---

## Memory Considerations

### ESP32 Constraints:
- **RAM:** ~520KB total, ~200KB available for app
- **HTTPS Overhead:** ~40KB per connection
- **Stream Buffer:** ~2KB for NDJSON parsing

### Optimizations:
1. **Single Stream Connection:** Only one active game at a time
2. **Line-by-line Parsing:** Don't buffer entire stream
3. **Event Forwarding:** Immediately forward to browser, don't store
4. **Token Storage:** Store in SPIFFS/SD, not RAM

---

## Configuration (`config.h`)

```cpp
// Lichess API Configuration
#define LICHESS_API_TOKEN_FILE "/lichess-token.txt"
#define LICHESS_STREAM_BUFFER_SIZE 2048
#define LICHESS_CONNECT_TIMEOUT 10000
#define LICHESS_READ_TIMEOUT 30000
```

---

## Error Handling

### Connection Errors:
- **WiFi Down:** Return HTTP 503, retry connection
- **Lichess Timeout:** Return HTTP 504, suggest retry
- **Invalid Token:** Return HTTP 401, request new token

### Stream Interruptions:
- **Connection Lost:** Close browser SSE, show reconnect button
- **Parse Error:** Log error, skip malformed event
- **Buffer Overflow:** Clear buffer, request game state refresh

---

## Implementation Priority

### Phase 1: Basic API Integration ✓
- [x] Design architecture
- [ ] Implement LichessAPI class
- [ ] Add /api/lichess/account endpoint
- [ ] Add /api/lichess/create-game endpoint
- [ ] Test game creation

### Phase 2: Move Handling
- [ ] Add /api/lichess/move endpoint
- [ ] Implement move submission
- [ ] Test UCI move format

### Phase 3: Stream Integration
- [ ] Implement NDJSON parser
- [ ] Add SSE handler
- [ ] Forward game events to browser
- [ ] Test real-time updates

### Phase 4: Browser Interface
- [ ] Replace Gemini logic with Lichess calls
- [ ] Add chess.js for move validation
- [ ] Implement drag-drop to UCI converter
- [ ] Add game controls (resign, draw offer)

---

## Testing Strategy

1. **Unit Tests:** Test NDJSON parser with sample data
2. **Integration Tests:** Test API endpoints with Postman
3. **End-to-End Tests:** Play full game via browser
4. **Load Tests:** Verify memory usage during long games

---

## Next Steps

1. Implement `LichessAPI.cpp` with full API methods
2. Update `WebInterface.cpp` with new endpoints
3. Test with `lichess-test.html` interface
4. Build full `chess-app.html` with Lichess integration
