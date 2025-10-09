#include "LichessWebHandler.h"
#include <ArduinoJson.h>
#include "SDLogger.h"

// Global instance
LichessWebHandler* g_lichessWebHandler = nullptr;

LichessWebHandler::LichessWebHandler()
    : _server(nullptr), _lichessAPI(nullptr), _sessionManager(nullptr), _eventSource(nullptr) {
    g_lichessWebHandler = this;
}

LichessWebHandler::~LichessWebHandler() {
    if (_eventSource) {
        delete _eventSource;
    }
    g_lichessWebHandler = nullptr;
}

void LichessWebHandler::begin(AsyncWebServer* server, LichessAPI* api, SessionManager* sessionMgr) {
    _server = server;
    _lichessAPI = api;
    _sessionManager = sessionMgr;

    // Setup SSE event source
    _eventSource = new AsyncEventSource("/api/lichess/stream");
    _server->addHandler(_eventSource);

    // Setup endpoints (now session-aware)
    _server->on("/api/lichess/account", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleTestAccount(request);
    });

    _server->on("/api/session/create", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleCreateSession(request);
    });

    _server->on("/api/lichess/create-game", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleCreateGame(request);
    });

    _server->on("/api/lichess/move", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleMakeMove(request);
    });

    _server->on("/api/lichess/resign", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleResignGame(request);
    });

    _server->on("/api/lichess/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetGameStatus(request);
    });

    _server->on("/api/lichess/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        this->handleReset(request);
    });

    _server->on("/api/check-admin", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleCheckAdmin(request);
    });

    _server->on("/api/sessions", HTTP_GET, [this](AsyncWebServerRequest* request) {
        this->handleGetSessions(request);
    });

    Serial.println("Chess web handlers registered (multi-session mode)");
}

void LichessWebHandler::setAPIToken(const char* token) {
    _apiToken = String(token);
    if (_lichessAPI) {
        _lichessAPI->begin(token);
    }
}

void LichessWebHandler::handleTestAccount(AsyncWebServerRequest* request) {
    if (!_lichessAPI) {
        sendErrorResponse(request, 500, "Lichess API not initialized");
        return;
    }

    String username;
    if (_lichessAPI->testAccount(username)) {
        sendJSONResponse(request, 200, "{\"username\":\"" + username + "\"}", true);
    } else {
        sendErrorResponse(request, 401, _lichessAPI->getLastError());
    }
}

void LichessWebHandler::handleCreateGame(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    // Get session ID
    String sessionId = getSessionIdFromRequest(request);
    if (sessionId.length() == 0) {
        sendErrorResponse(request, 400, "Missing session ID");
        return;
    }

    // Validate session exists
    Session* session = _sessionManager->getSession(sessionId);
    if (!session) {
        sendErrorResponse(request, 404, "Session not found");
        return;
    }

    // Use session-specific API instance
    LichessAPI* sessionAPI = session->lichessAPI;
    if (!sessionAPI) {
        sendErrorResponse(request, 500, "Session API not initialized");
        return;
    }

    // Check if session already has active game
    if (session->gameActive) {
        sendErrorResponse(request, 400, "Session already has active game: " + session->gameId);
        return;
    }

    // Check if API token is configured
    if (!sessionAPI->hasToken()) {
        Serial.println("ERROR: Lichess API token not configured for session!");
        sendErrorResponse(request, 500, "Lichess API token not configured");
        return;
    }

    // Parse parameters
    int level = request->hasParam("level", true) ? request->getParam("level", true)->value().toInt() : 3;
    int timeLimit = request->hasParam("time", true) ? request->getParam("time", true)->value().toInt() : 600;
    int increment = request->hasParam("increment", true) ? request->getParam("increment", true)->value().toInt() : 0;
    String color = request->hasParam("color", true) ? request->getParam("color", true)->value() : "white";

    // Queue the request instead of rejecting when busy
    if (sessionAPI->isBusy()) {
        Serial.printf("Session %s: API busy - queueing game creation (queue size: %d)\n",
                      sessionId.c_str(), sessionAPI->getQueueSize());
        sessionAPI->queueCreateGame(level, timeLimit, increment, color);
        sendJSONResponse(request, 202, "{\"status\":\"queued\",\"queueSize\":" + String(sessionAPI->getQueueSize()) + "}", true);
        return;
    }

    Serial.printf("Session %s: Creating game (level=%d, time=%d, color=%s)\n",
                  sessionId.c_str(), level, timeLimit, color.c_str());

    // Store color in session for later reference
    session->playerColor = color;
    _sessionManager->updateActivity(sessionId);

    // createAIGame now initiates async process using session-specific API
    sessionAPI->createAIGame(level, timeLimit, increment, color.c_str());

    // Return immediately - game will be created in background
    sendJSONResponse(request, 202, "{\"status\":\"creating\"}", true);
}

void LichessWebHandler::handleMakeMove(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    // Get session ID
    String sessionId = getSessionIdFromRequest(request);
    if (sessionId.length() == 0) {
        sendErrorResponse(request, 400, "Missing session ID");
        return;
    }

    // Validate session exists
    Session* session = _sessionManager->getSession(sessionId);
    if (!session || !session->gameActive) {
        sendErrorResponse(request, 400, "No active game for this session");
        return;
    }

    // Use session-specific API instance
    LichessAPI* sessionAPI = session->lichessAPI;
    if (!sessionAPI) {
        sendErrorResponse(request, 500, "Session API not initialized");
        return;
    }

    if (!request->hasParam("move", true)) {
        sendErrorResponse(request, 400, "Missing 'move' parameter");
        return;
    }

    // Check if API is busy with another operation
    if (sessionAPI->isBusy()) {
        sendErrorResponse(request, 503, "API busy, try again");
        return;
    }

    String move = request->getParam("move", true)->value();
    Serial.printf("Session %s: Making move %s on game %s\n",
                  sessionId.c_str(), move.c_str(), session->gameId.c_str());

    _sessionManager->updateActivity(sessionId);

    // makeMove now returns immediately and processes asynchronously
    if (sessionAPI->makeMove(session->gameId, move)) {
        // Move initiated successfully - will complete in background
        sendJSONResponse(request, 202, "{\"success\":true,\"status\":\"processing\"}", true);
    } else {
        // Failed to initiate move
        sendErrorResponse(request, 400, sessionAPI->getLastError());
    }
}

void LichessWebHandler::handleResignGame(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    // Get session ID
    String sessionId = getSessionIdFromRequest(request);
    if (sessionId.length() == 0) {
        sendErrorResponse(request, 400, "Missing session ID");
        return;
    }

    // Validate session exists
    Session* session = _sessionManager->getSession(sessionId);
    if (!session || !session->gameActive) {
        sendErrorResponse(request, 400, "No active game for this session");
        return;
    }

    // Use session-specific API instance
    LichessAPI* sessionAPI = session->lichessAPI;
    if (!sessionAPI) {
        sendErrorResponse(request, 500, "Session API not initialized");
        return;
    }

    // Check if API is busy with another operation
    if (sessionAPI->isBusy()) {
        sendErrorResponse(request, 503, "API busy, try again");
        return;
    }

    Serial.printf("Session %s: Resigning game %s\n", sessionId.c_str(), session->gameId.c_str());

    _sessionManager->updateActivity(sessionId);

    // resignGame now initiates async process
    if (sessionAPI->resignGame(session->gameId)) {
        // Resign initiated - will complete in background
        sendJSONResponse(request, 202, "{\"success\":true,\"status\":\"resigning\"}", true);
    } else {
        sendErrorResponse(request, 400, sessionAPI->getLastError());
    }
}

void LichessWebHandler::handleGetGameStatus(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    // Get session ID
    String sessionId = getSessionIdFromRequest(request);
    if (sessionId.length() == 0) {
        sendErrorResponse(request, 400, "Missing session ID");
        return;
    }

    // Validate session exists
    Session* session = _sessionManager->getSession(sessionId);
    if (!session) {
        sendErrorResponse(request, 404, "Session not found");
        return;
    }

    JsonDocument doc;
    doc["gameActive"] = session->gameActive;
    doc["gameId"] = session->gameId;
    doc["playerColor"] = session->playerColor;
    doc["streaming"] = _lichessAPI ? _lichessAPI->isStreaming() : false;
    doc["sessionId"] = sessionId;

    _sessionManager->updateActivity(sessionId);

    String response;
    serializeJson(doc, response);

    request->send(200, "application/json", response);
}

void LichessWebHandler::forwardLichessEvents() {
    if (!_sessionManager || !_eventSource) {
        return;
    }

    // Process stream events from ALL sessions
    for (auto& pair : _sessionManager->getAllSessions()) {
        Session* session = &pair.second;
        if (!session || !session->lichessAPI) continue;

        LichessAPI* api = session->lichessAPI;
        if (!api->isStreaming()) continue;

        String eventJson;
        if (api->processStreamEvents(eventJson)) {
            // Validate JSON before forwarding
            if (eventJson.length() > 2 && (eventJson.startsWith("{") || eventJson.startsWith("["))) {
                // Wrap event with sessionId so browsers can filter
                String wrappedEvent = "{\"sessionId\":\"" + session->sessionId + "\",\"event\":" + eventJson + "}";

                // Forward wrapped event to all connected SSE clients
                _eventSource->send(wrappedEvent.c_str(), "lichess-event", millis());
                Serial.println("Forwarded valid event: " + eventJson.substring(0, 100));
            } else {
                // Lichess sends periodic heartbeat/keepalive messages (single chars like "1", "\n")
                // to keep the HTTP stream connection alive. These are normal and expected.
                // They are silently ignored here to avoid log spam.
                // Serial.println("Lichess heartbeat: [" + eventJson + "]");  // Uncomment for debugging
            }
        }
    }
}

void LichessWebHandler::sendJSONResponse(AsyncWebServerRequest* request, int code, const String& json, bool success) {
    request->send(code, "application/json", json);
}

void LichessWebHandler::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = error;

    String response;
    serializeJson(doc, response);

    request->send(code, "application/json", response);
}

void LichessWebHandler::processAsyncResults() {
    if (!_sessionManager) {
        return;
    }

    // Check ALL sessions for completed async operations
    // Use a temporary map to avoid modifying session state during iteration
    std::vector<std::pair<String, String>> completedGames;  // sessionId, gameId
    std::vector<std::pair<String, String>> failedGames;     // sessionId, error
    std::vector<std::pair<String, String>> recoveredGames;  // sessionId, gameId (timeout recovery)

    for (auto& pair : _sessionManager->getAllSessions()) {
        Session* session = &pair.second;
        if (!session || !session->lichessAPI) continue;

        LichessAPI* api = session->lichessAPI;
        if (api->isBusy()) continue;

        // Check for timeout recovery (error message contains "timeout" and stream is active)
        // This should only trigger once, so we must clear the error immediately
        String lastError = api->getLastError();
        if (lastError.indexOf("timeout") >= 0 && api->isStreaming() && session->gameId.length() > 0) {
            Serial.printf("Timeout recovery detected for session %s (game: %s)\n",
                         session->sessionId.c_str(), session->gameId.c_str());
            recoveredGames.push_back({session->sessionId, session->gameId});

            // CRITICAL: Clear the error immediately to prevent infinite loop
            // The error persists until cleared, causing this check to trigger repeatedly
            api->clearError();
        }

        // Check if a game creation just completed for this session
        String createdGameId = api->getCreatedGameId();
        if (createdGameId.length() > 0) {
            if (api->wasOperationSuccessful()) {
                // Success - game created
                completedGames.push_back({session->sessionId, createdGameId});
            } else {
                // Failure - game creation failed
                failedGames.push_back({session->sessionId, api->getLastError()});
            }

            // Clear the created game ID from this API instance
            api->clearCreatedGameId();
        }
    }

    // Process completed games
    for (const auto& entry : completedGames) {
        String sessionId = entry.first;
        String gameId = entry.second;

        Serial.printf("Async game creation completed for session %s: %s\n",
                      sessionId.c_str(), gameId.c_str());

        // Update session state
        Session* session = _sessionManager->getSession(sessionId);
        if (session) {
            session->gameId = gameId;
            session->gameActive = true;
            Serial.printf("Session %s: Game state updated (gameId=%s, color=%s)\n",
                          sessionId.c_str(), gameId.c_str(), session->playerColor.c_str());
        }

        // Send SSE event to notify all browsers
        if (_eventSource) {
            String eventData = "{\"type\":\"gameCreated\",\"gameId\":\"" + gameId + "\"}";
            _eventSource->send(eventData.c_str(), "game-created", millis());
            Serial.printf("Sent game-created SSE event: %s\n", eventData.c_str());
        }
    }

    // Process failed games
    for (const auto& entry : failedGames) {
        String sessionId = entry.first;
        String error = entry.second;

        Serial.printf("Async game creation failed for session %s: %s\n",
                      sessionId.c_str(), error.c_str());

        // Send SSE event to notify all browsers of failure
        if (_eventSource) {
            String eventData = "{\"type\":\"gameCreationFailed\",\"error\":\"" + error + "\"}";
            _eventSource->send(eventData.c_str(), "game-error", millis());
            Serial.printf("Sent game-error SSE event: %s\n", eventData.c_str());
        }
    }

    // Process recovered games (timeout recovery)
    for (const auto& entry : recoveredGames) {
        String sessionId = entry.first;
        String gameId = entry.second;

        Serial.printf("Game recovered from timeout for session %s (game: %s)\n",
                      sessionId.c_str(), gameId.c_str());

        // Send SSE event to notify browser of recovery
        if (_eventSource) {
            String wrappedEvent = "{\"sessionId\":\"" + sessionId + "\",\"event\":{\"type\":\"connectionRecovered\",\"message\":\"Connection timeout recovered - game stream restored\"}}";
            _eventSource->send(wrappedEvent.c_str(), "lichess-event", millis());
            Serial.printf("Sent connection-recovered SSE event for session %s\n", sessionId.c_str());
        }
    }
}

void LichessWebHandler::handleReset(AsyncWebServerRequest* request) {
    Serial.println("=== RESET REQUEST RECEIVED ===");

    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    // Get session ID
    String sessionId = getSessionIdFromRequest(request);
    if (sessionId.length() == 0) {
        sendErrorResponse(request, 400, "Missing session ID");
        return;
    }

    // Validate session exists
    Session* session = _sessionManager->getSession(sessionId);
    if (!session) {
        sendErrorResponse(request, 404, "Session not found");
        return;
    }

    // Clear this session's game state
    session->gameActive = false;
    session->gameId = "";
    session->playerColor = "white";

    Serial.printf("Session %s: Reset complete\n", sessionId.c_str());

    // Send success response
    sendJSONResponse(request, 200, "{\"success\":true,\"message\":\"Session reset successfully\"}", true);
}

// Helper: Get client IP address
String LichessWebHandler::getClientIP(AsyncWebServerRequest* request) {
    if (request->hasHeader("X-Forwarded-For")) {
        return request->header("X-Forwarded-For");
    }
    return request->client()->remoteIP().toString();
}

// Helper: Get session ID from request header or parameter
String LichessWebHandler::getSessionIdFromRequest(AsyncWebServerRequest* request) {
    // Check header first (preferred method)
    if (request->hasHeader("X-Session-ID")) {
        return request->header("X-Session-ID");
    }
    // Fallback to query parameter
    if (request->hasParam("sessionId")) {
        return request->getParam("sessionId")->value();
    }
    // Fallback to POST body parameter
    if (request->hasParam("sessionId", true)) {
        return request->getParam("sessionId", true)->value();
    }
    return "";
}

// Handler: Create new session
void LichessWebHandler::handleCreateSession(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    String clientIP = getClientIP(request);
    String sessionId = _sessionManager->createSession(clientIP);

    if (sessionId.length() == 0) {
        sendErrorResponse(request, 503, "Maximum sessions reached");
        return;
    }

    JsonDocument doc;
    doc["success"] = true;
    doc["sessionId"] = sessionId;
    doc["ipAddress"] = clientIP;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    Serial.printf("Session created: %s for IP: %s\n", sessionId.c_str(), clientIP.c_str());
}

// Handler: Check if client IP is admin
void LichessWebHandler::handleCheckAdmin(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    String clientIP = getClientIP(request);
    bool isAdmin = _sessionManager->isAdminIP(clientIP);

    JsonDocument doc;
    doc["isAdmin"] = isAdmin;
    doc["ipAddress"] = clientIP;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Handler: Get all active sessions (debug endpoint)
void LichessWebHandler::handleGetSessions(AsyncWebServerRequest* request) {
    if (!_sessionManager) {
        sendErrorResponse(request, 500, "Session manager not initialized");
        return;
    }

    String json = _sessionManager->getSessionsJSON();
    request->send(200, "application/json", json);
}

// Session cleanup (call from main loop)
void LichessWebHandler::cleanupSessions() {
    if (_sessionManager) {
        _sessionManager->cleanupExpiredSessions();
    }
}

// Global helper function for main loop
void processLichessStreamEvents() {
    if (g_lichessWebHandler) {
        g_lichessWebHandler->forwardLichessEvents();
        g_lichessWebHandler->processAsyncResults();
        g_lichessWebHandler->cleanupSessions();
    }
}
