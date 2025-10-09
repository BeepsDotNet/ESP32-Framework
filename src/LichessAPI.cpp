#include "LichessAPI.h"
#include <ArduinoJson.h>
#include "SDLogger.h"

LichessAPI::LichessAPI()
    : _streaming(false), _streamClient(nullptr), _state(STATE_IDLE),
      _stateStartTime(0), _wasStreaming(false), _retryAttempt(0), _retryDelay(0),
      _operationSuccess(false), _pendingLevel(3), _pendingTimeLimit(600), _pendingIncrement(0),
      _lastHeartbeatTime(0), _heartbeatCount(0), _consecutiveFailures(0), _lastSuccessfulRequest(0) {
    // Configure SSL client for insecure mode (development)
    _client.setInsecure();
    // Set connection timeout to prevent hanging (15 seconds)
    _client.setTimeout(15);  // timeout in seconds for WiFiClientSecure
}

LichessAPI::~LichessAPI() {
    stopStream();
}

bool LichessAPI::begin(const char* apiToken) {
    if (!apiToken || strlen(apiToken) == 0) {
        setError("Invalid API token");
        return false;
    }
    _apiToken = String(apiToken);
    return true;
}

bool LichessAPI::testAccount(String& username) {
    String response;
    // Disable retry for testAccount - called from web handler, must not block too long
    if (!makeAPICall(API_ACCOUNT, "GET", "", response, false)) {
        return false;
    }

    // Parse JSON response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
        setError("Failed to parse account response");
        return false;
    }

    if (doc["username"].is<const char*>()) {
        username = doc["username"].as<String>();
        return true;
    }

    setError("No username in response");
    return false;
}

String LichessAPI::createAIGame(int level, int timeLimitSeconds, int incrementSeconds, const char* color) {
    // Check connection health and reset if needed
    if (!isConnectionHealthy()) {
        Serial.println("Connection unhealthy detected, performing automatic reset...");
        resetConnection();
    }

    // Validate parameters
    if (level < 1 || level > 8) {
        setError("AI level must be between 1 and 8");
        return "";
    }

    // Check if already busy with another operation
    if (_state != STATE_IDLE) {
        setError("API busy with another operation");
        return "";
    }

    // Store parameters for async processing
    _pendingLevel = level;
    _pendingTimeLimit = timeLimitSeconds;
    _pendingIncrement = incrementSeconds;
    _pendingColor = String(color);
    _createdGameId = "";
    _operationSuccess = false;

    // CRITICAL: Clear pending game/move state to ensure fresh game creation
    // Without this, old gameId causes state machine to think it's a resign operation
    _pendingGameId = "";
    _pendingMove = "";

    // Start async state machine - wait for SSL cleanup before creating
    _state = STATE_WAITING_SSL_CLEANUP;
    _stateStartTime = millis();
    Serial.println("Starting async game creation - waiting for SSL cleanup");

    return "";  // Will be populated in _createdGameId when complete
}

bool LichessAPI::makeMove(const String& gameId, const String& uciMove) {
    // Check connection health and reset if needed
    if (!isConnectionHealthy()) {
        Serial.println("Connection unhealthy detected, performing automatic reset...");
        resetConnection();
    }

    if (gameId.length() == 0 || uciMove.length() == 0) {
        setError("Invalid game ID or move");
        return false;
    }

    // Check if already busy with another operation
    if (_state != STATE_IDLE) {
        setError("API busy with another operation");
        return false;
    }

    // Store move details for async processing
    _pendingGameId = gameId;
    _pendingMove = uciMove;
    _wasStreaming = _streaming;

    // Start async state machine
    if (_wasStreaming) {
        // Set flag FIRST to stop processStreamEvents from accessing _streamClient
        _streaming = false;
        _state = STATE_WAITING_STREAM_STOP;
        _stateStartTime = millis();
        LOG_PRINTF("[%lu] MOVE START: %s (game: %s) - pausing stream\n", millis(), uciMove.c_str(), gameId.c_str());
    } else {
        // No stream to pause, go directly to making the move
        _state = STATE_MAKING_MOVE;
        LOG_PRINTF("[%lu] MOVE START: %s (game: %s) - no stream active\n", millis(), uciMove.c_str(), gameId.c_str());
    }

    return true;  // Move initiated, will complete in process()
}

bool LichessAPI::resignGame(const String& gameId) {
    if (gameId.length() == 0) {
        setError("Invalid game ID");
        return false;
    }

    // Check if already busy with another operation
    if (_state != STATE_IDLE) {
        setError("API busy with another operation");
        return false;
    }

    // Store game ID for async processing
    _pendingGameId = gameId;
    _wasStreaming = _streaming;

    // Start async state machine
    if (_wasStreaming) {
        // Stop stream first
        _streaming = false;
        _state = STATE_WAITING_STREAM_STOP;
        _stateStartTime = millis();
        Serial.println("Starting async resign - stopping stream");
    } else {
        // No stream, wait for SSL cleanup before resign
        _state = STATE_WAITING_SSL_CLEANUP;
        _stateStartTime = millis();
        Serial.println("Starting async resign - waiting for SSL cleanup");
    }

    return true;  // Resign initiated, will complete in process()
}

bool LichessAPI::startStream(const String& gameId) {
    if (_streaming) {
        setError("Stream already active");
        return false;
    }

    if (gameId.length() == 0) {
        setError("Invalid game ID");
        return false;
    }

    String url = String(API_BOARD_GAME_STREAM) + gameId;

    // Start HTTPS connection with extended timeout
    _streamHttp.begin(_client, url);
    _streamHttp.addHeader("Authorization", "Bearer " + _apiToken);
    _streamHttp.addHeader("Accept", "application/x-ndjson");
    _streamHttp.setTimeout(30000); // 30 second timeout

    Serial.println("Starting Lichess stream connection...");
    yield(); // Feed watchdog

    int httpCode = _streamHttp.GET();

    yield(); // Feed watchdog after GET
    Serial.printf("Stream connection response: %d\n", httpCode);

    if (httpCode != HTTP_CODE_OK) {
        setError("Stream connection failed: " + String(httpCode));
        _streamHttp.end();
        return false;
    }

    // Get stream client pointer
    _streamClient = _streamHttp.getStreamPtr();
    _streaming = true;
    _streamBuffer = "";

    Serial.println("Lichess stream started successfully");
    return true;
}

void LichessAPI::stopStream() {
    if (_streaming) {
        _streamHttp.end();
        _streamClient = nullptr;
        _streaming = false;
        _streamBuffer = "";
        Serial.println("Lichess stream stopped");
    }
}

bool LichessAPI::processStreamEvents(String& eventJson) {
    if (!_streaming || !_streamClient) {
        return false;
    }

    // Read available data from stream client
    while (_streamClient->available()) {
        char c = _streamClient->read();

        if (c == '\n') {
            // Complete line received
            if (_streamBuffer.length() > 0) {
                // Return the complete JSON event
                eventJson = _streamBuffer;
                // Only log actual JSON events, not heartbeat packets.
                // Lichess sends periodic heartbeat/keepalive messages (single chars like "1", "\n")
                // to keep the HTTP stream connection alive. These are filtered out here.
                if (eventJson.length() > 2 && (eventJson.startsWith("{") || eventJson.startsWith("["))) {
                    LOG_PRINTF("[%lu] STREAM EVENT: %s\n", millis(), eventJson.substring(0, 100).c_str());
                } else {
                    // Track heartbeat packets for diagnostics
                    _lastHeartbeatTime = millis();
                    _heartbeatCount++;
                }
                _streamBuffer = "";
                return true;
            }
        } else if (c != '\r') {
            // Add to buffer (skip carriage return)
            _streamBuffer += c;

            // Prevent buffer overflow
            if (_streamBuffer.length() > 4096) {
                LOG_PRINTF("[%lu] WARNING: Stream buffer overflow, clearing\n", millis());
                _streamBuffer = "";
                setError("Stream buffer overflow");
                return false;
            }
        }
    }

    // Check if connection is still alive
    if (!_streamClient->connected()) {
        Serial.println("Stream connection lost");
        stopStream();
        setError("Stream connection lost");
        return false;
    }

    return false;
}

// Private helper methods

bool LichessAPI::makeAPICall(const char* url, const char* method, const String& body, String& response, bool enableRetry) {
    // Use shared _http client for API calls
    // Note: This will work because the stream uses _streamHttp, not _http

    Serial.printf("API Call: %s %s\n", method, url);
    Serial.printf("API Token: %s (length: %d)\n", _apiToken.c_str(), _apiToken.length());

    // Retry logic for SSL connection issues (max 3 attempts)
    // Disabled for web handler context to avoid watchdog timeout
    int maxRetries = enableRetry ? 3 : 1;
    int retryDelay = 1000;  // Start with 1 second delay
    int httpCode = -1;

    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        // Begin connection
        if (!_http.begin(_client, url)) {
            if (attempt < maxRetries) {
                Serial.printf("Begin failed (attempt %d/%d), retrying in %dms...\n",
                             attempt, maxRetries, retryDelay);
                delay(retryDelay);
                retryDelay *= 2;
                continue;
            } else {
                setError("Failed to connect to Lichess after " + String(maxRetries) + " attempts");
                return false;
            }
        }

        // Set headers
        _http.addHeader("Authorization", "Bearer " + _apiToken);
        if (body.length() > 0) {
            _http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        }
        _http.setTimeout(15000);

        // Make request
        if (strcmp(method, "POST") == 0) {
            httpCode = _http.POST(body);
        } else if (strcmp(method, "GET") == 0) {
            httpCode = _http.GET();
        } else {
            setError("Unsupported HTTP method");
            _http.end();
            return false;
        }

        // Check if we got connection error and should retry
        // httpCode < 0: SSL/connection failure
        // httpCode 1-99: Invalid HTTP status codes indicating connection issues
        if (httpCode < 0 || (httpCode > 0 && httpCode < 100)) {
            _http.end();  // Clean up before retry

            if (attempt < maxRetries) {
                Serial.printf("HTTP request failed (code: %d, attempt %d/%d), retrying in %dms...\n",
                             httpCode, attempt, maxRetries, retryDelay);
                delay(retryDelay);
                retryDelay *= 2;  // Exponential backoff: 1s, 2s
                continue;
            } else {
                // Run network diagnostics
                runNetworkDiagnostics();

                if (httpCode < 0) {
                    setError("HTTP request failed: " + _http.errorToString(httpCode));
                } else {
                    setError("HTTP connection error: " + String(httpCode));
                }
                return false;
            }
        }

        // Success! Break out of retry loop
        break;
    }

    // Check response code
    if (httpCode < 0) {
        setError("HTTP request failed after retries: " + _http.errorToString(httpCode));
        _http.end();
        return false;
    }

    // Treat invalid HTTP codes (1-99) as connection errors - these should have been caught in retry loop
    if (httpCode > 0 && httpCode < 100) {
        setError("HTTP connection error: " + String(httpCode) + " (invalid status code)");
        _http.end();
        return false;
    }

    if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_CREATED) {
        response = _http.getString();
        Serial.printf("Lichess API Error Details:\n");
        Serial.printf("  HTTP Status: %d\n", httpCode);
        Serial.printf("  Request URL: %s\n", url);
        Serial.printf("  Request Method: %s\n", method);
        Serial.printf("  API Token: %s (length: %d)\n", _apiToken.c_str(), _apiToken.length());
        Serial.printf("  Authorization Header: Bearer %s\n", _apiToken.c_str());
        Serial.printf("  Request Body: %s\n", body.c_str());
        Serial.printf("  Response Body: %s\n", response.c_str());
        setError("HTTP error: " + String(httpCode));
        _http.end();
        return false;
    }

    // Get response
    response = _http.getString();
    _http.end();

    // Track successful request
    _consecutiveFailures = 0;
    _lastSuccessfulRequest = millis();

    return true;
}

void LichessAPI::setError(const String& error) {
    _lastError = error;
    Serial.print("LichessAPI Error: ");
    Serial.println(error);

    // Track consecutive failures
    _consecutiveFailures++;
    Serial.printf("Consecutive failures: %d/%d\n", _consecutiveFailures, MAX_CONSECUTIVE_FAILURES);
}

void LichessAPI::runNetworkDiagnostics() {
    Serial.println("\n========== NETWORK DIAGNOSTICS ==========");

    // 0. Check Lichess heartbeat status (if streaming)
    if (_streaming) {
        unsigned long timeSinceHeartbeat = millis() - _lastHeartbeatTime;
        Serial.printf("0. Lichess Stream Status: ACTIVE\n");
        Serial.printf("   Total heartbeats received: %lu\n", _heartbeatCount);
        Serial.printf("   Last heartbeat: %lu ms ago\n", timeSinceHeartbeat);
        if (timeSinceHeartbeat < 10000) {
            Serial.println("   ✓ Lichess connection is ALIVE (heartbeats recent)");
        } else {
            Serial.println("   ⚠ WARNING: No recent heartbeats (stream may be stalled)");
        }
    } else {
        Serial.println("0. Lichess Stream Status: NOT STREAMING");
    }

    // 1. Check WiFi status
    wl_status_t wifiStatus = WiFi.status();
    Serial.printf("1. WiFi Status: %d ", wifiStatus);
    switch(wifiStatus) {
        case WL_CONNECTED: Serial.println("(CONNECTED)"); break;
        case WL_NO_SHIELD: Serial.println("(NO_SHIELD)"); break;
        case WL_IDLE_STATUS: Serial.println("(IDLE)"); break;
        case WL_NO_SSID_AVAIL: Serial.println("(NO_SSID)"); break;
        case WL_SCAN_COMPLETED: Serial.println("(SCAN_COMPLETED)"); break;
        case WL_CONNECT_FAILED: Serial.println("(CONNECT_FAILED)"); break;
        case WL_CONNECTION_LOST: Serial.println("(CONNECTION_LOST)"); break;
        case WL_DISCONNECTED: Serial.println("(DISCONNECTED)"); break;
        default: Serial.println("(UNKNOWN)"); break;
    }

    if (wifiStatus != WL_CONNECTED) {
        Serial.println("WiFi not connected! Skipping further tests.");
        Serial.println("=========================================\n");
        return;
    }

    // 2. Check local network info
    Serial.printf("2. Local IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("   DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("   Signal: %d dBm\n", WiFi.RSSI());

    // 3. Test DNS resolution for lichess.org
    Serial.print("3. DNS Lookup (lichess.org): ");
    IPAddress lichessIP;
    if (WiFi.hostByName("lichess.org", lichessIP)) {
        Serial.printf("SUCCESS -> %s\n", lichessIP.toString().c_str());
    } else {
        Serial.println("FAILED - Cannot resolve lichess.org");
        Serial.println("=========================================\n");
        return;
    }

    // 4. Test DNS resolution for Google
    Serial.print("4. DNS Lookup (google.com): ");
    IPAddress googleIP;
    if (WiFi.hostByName("google.com", googleIP)) {
        Serial.printf("SUCCESS -> %s\n", googleIP.toString().c_str());
    } else {
        Serial.println("FAILED - Cannot resolve google.com");
    }

    // 5. Test HTTP connection to Lichess
    Serial.print("5. HTTP Connect Test (lichess.org:443): ");
    WiFiClientSecure testClient;
    testClient.setInsecure();
    if (testClient.connect("lichess.org", 443)) {
        Serial.println("SUCCESS - Can connect to Lichess");
        testClient.stop();
    } else {
        Serial.println("FAILED - Cannot connect to Lichess");
    }

    Serial.println("=========================================\n");
}

bool LichessAPI::queueCreateGame(int level, int timeLimit, int increment, const String& color) {
    QueuedRequest req;
    req.type = QueuedRequest::CREATE_GAME;
    req.level = level;
    req.timeLimit = timeLimit;
    req.increment = increment;
    req.color = color;

    _requestQueue.push_back(req);
    Serial.printf("Queued game creation request (queue size: %d)\n", _requestQueue.size());
    return true;
}

void LichessAPI::processQueue() {
    // Only process queue if we're idle and not streaming
    if (_state != STATE_IDLE || _requestQueue.empty()) {
        return;
    }

    // If streaming is active, stop it before processing queue
    // (especially for CREATE_GAME requests which need exclusive SSL access)
    if (_streaming) {
        Serial.println("Stopping active stream to process queued request");
        stopStream();
        // Wait a moment for cleanup
        delay(100);
    }

    // Get next request from queue
    QueuedRequest req = _requestQueue.front();
    _requestQueue.erase(_requestQueue.begin());

    Serial.printf("Processing queued request (type: %d, remaining: %d)\n", req.type, _requestQueue.size());

    // Process the request
    switch (req.type) {
        case QueuedRequest::CREATE_GAME:
            createAIGame(req.level, req.timeLimit, req.increment, req.color.c_str());
            break;
        case QueuedRequest::MAKE_MOVE:
            makeMove(req.gameId, req.move);
            break;
        case QueuedRequest::RESIGN_GAME:
            resignGame(req.gameId);
            break;
    }
}

bool LichessAPI::checkOperationTimeout() {
    if (_state == STATE_IDLE) {
        return false;
    }

    unsigned long elapsed = millis() - _stateStartTime;
    if (elapsed >= OPERATION_TIMEOUT) {
        Serial.printf("=== OPERATION TIMEOUT DETECTED ===\n");
        Serial.printf("State: %d, Elapsed: %lu ms (limit: %lu ms)\n", _state, elapsed, OPERATION_TIMEOUT);

        // Save game context before reset (for recovery)
        String savedGameId = _pendingGameId;
        bool hadActiveGame = (savedGameId.length() > 0);

        setError("Operation timeout - attempting automatic recovery");

        // Perform full connection reset for better recovery
        Serial.println("Performing connection reset due to timeout...");
        resetConnection();
        Serial.println("Connection reset complete - ready for new requests");

        // Attempt to resume game stream if we had an active game
        if (hadActiveGame) {
            Serial.printf("Attempting to resume game stream for: %s\n", savedGameId.c_str());
            _pendingGameId = savedGameId;

            // Try to restart the stream
            if (startStream(savedGameId)) {
                Serial.println("Game stream resumed successfully after timeout recovery");
                _operationSuccess = true;  // Recovery succeeded
            } else {
                Serial.println("Failed to resume game stream - user will need to refresh");
                _operationSuccess = false;
            }
        }

        _state = STATE_IDLE;

        return true;
    }
    return false;
}

void LichessAPI::process() {
    // Check for operation timeout
    if (checkOperationTimeout()) {
        Serial.println("Operation timed out, processing queue...");
        processQueue();
        return;
    }

    // Process async state machine
    if (_state == STATE_IDLE) {
        // Process next queued request if available
        processQueue();
        return;
    }

    unsigned long elapsed = millis() - _stateStartTime;

    switch (_state) {
        case STATE_WAITING_STREAM_STOP:
            if (elapsed >= STREAM_STOP_DELAY) {
                // Safe to clean up stream connection now
                _streamHttp.end();
                _client.stop();
                _streamClient = nullptr;
                _streamBuffer = "";
                Serial.println("Stream stopped, waiting for SSL cleanup");

                // Move to SSL cleanup state
                _state = STATE_WAITING_SSL_CLEANUP;
                _stateStartTime = millis();
            }
            break;

        case STATE_WAITING_SSL_CLEANUP:
            if (elapsed >= SSL_CLEANUP_DELAY) {
                Serial.println("SSL cleanup complete");
                // Determine next state based on pending operation
                if (_pendingMove.length() > 0) {
                    Serial.println("Proceeding to make move");
                    _state = STATE_MAKING_MOVE;
                } else if (_pendingGameId.length() > 0 && _pendingMove.length() == 0) {
                    // Resign operation (has gameId but no move)
                    Serial.println("Proceeding to resign");
                    _state = STATE_RESIGNING_GAME;
                } else {
                    // Game creation (no gameId, no move)
                    Serial.println("Proceeding to create game");
                    _state = STATE_CREATING_GAME;
                }
            }
            break;

        case STATE_MAKING_MOVE: {
            // Make the API call for the move
            String url = String(API_BOARD_GAME_MOVE) + _pendingGameId + "/move/" + _pendingMove;
            String response;

            bool success = makeAPICall(url.c_str(), "POST", "", response);

            if (!success) {
                // Move failed, return to idle
                Serial.println("Move failed: " + _lastError);
                _state = STATE_IDLE;
                break;
            }

            // Parse response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);

            if (error) {
                setError("Failed to parse move response");
                _state = STATE_IDLE;
                break;
            }

            if (doc["ok"].is<bool>() && doc["ok"].as<bool>()) {
                LOG_PRINTF("[%lu] MOVE ACCEPTED: %s\n", millis(), _pendingMove.c_str());

                // Resume stream if it was active before
                if (_wasStreaming) {
                    _state = STATE_RESUMING_STREAM;
                    _stateStartTime = millis();
                    LOG_PRINTF("[%lu] Move complete, waiting to resume stream\n", millis());
                } else {
                    // No stream to resume, done
                    _state = STATE_IDLE;
                    LOG_PRINTF("[%lu] Move complete (no stream)\n", millis());
                }
            } else {
                setError("Move rejected by server");
                _state = STATE_IDLE;
            }
            break;
        }

        case STATE_RESUMING_STREAM:
            if (elapsed >= STREAM_RESUME_DELAY) {
                LOG_PRINTF("[%lu] Resuming stream for game %s (waiting for opponent move)\n", millis(), _pendingGameId.c_str());
                if (startStream(_pendingGameId)) {
                    LOG_PRINTF("[%lu] Stream resumed - listening for opponent\n", millis());
                } else {
                    LOG_PRINTF("[%lu] WARNING: Failed to resume stream after move\n", millis());
                }
                _state = STATE_IDLE;
                _pendingMove = "";  // Clear pending move
            }
            break;

        case STATE_RESIGNING_GAME: {
            // Make the API call to resign
            String url = String(API_BOARD_GAME_MOVE) + _pendingGameId + "/resign";
            String response;

            Serial.println("Resigning game: " + _pendingGameId);
            bool success = makeAPICall(url.c_str(), "POST", "", response, false);

            if (success) {
                Serial.println("Game resigned successfully");
                _operationSuccess = true;
            } else {
                Serial.println("Resign failed: " + _lastError);
                _operationSuccess = false;
            }

            _state = STATE_IDLE;
            _pendingGameId = "";
            break;
        }

        case STATE_CREATING_GAME: {
            // Build form data for game creation
            String body = "level=" + String(_pendingLevel) +
                          "&clock.limit=" + String(_pendingTimeLimit) +
                          "&clock.increment=" + String(_pendingIncrement) +
                          "&color=" + _pendingColor;

            String response;
            Serial.printf("Creating game: level=%d, time=%d, color=%s\n",
                         _pendingLevel, _pendingTimeLimit, _pendingColor.c_str());

            bool success = makeAPICall(API_CHALLENGE_AI, "POST", body, response, false);

            if (!success) {
                Serial.println("Game creation failed: " + _lastError);
                _operationSuccess = false;
                _createdGameId = "";
                _state = STATE_IDLE;
                break;
            }

            // Parse JSON response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, response);

            if (error) {
                setError("Failed to parse game creation response");
                _operationSuccess = false;
                _createdGameId = "";
                _state = STATE_IDLE;
                break;
            }

            if (doc["id"].is<const char*>()) {
                _createdGameId = doc["id"].as<String>();
                _operationSuccess = true;
                Serial.println("Game created: " + _createdGameId);

                // Start streaming the new game
                _state = STATE_STARTING_STREAM;
                _stateStartTime = millis();
            } else {
                setError("No game ID in response");
                _operationSuccess = false;
                _createdGameId = "";
                _state = STATE_IDLE;
            }
            break;
        }

        case STATE_STARTING_STREAM:
            if (elapsed >= STREAM_RESUME_DELAY) {
                Serial.println("Starting stream for new game: " + _createdGameId);
                if (startStream(_createdGameId)) {
                    Serial.println("Stream started successfully");
                } else {
                    Serial.println("WARNING: Failed to start stream for new game");
                }
                _state = STATE_IDLE;
            }
            break;

        case STATE_RETRYING_CONNECTION:
            // For future use with async retry logic
            if (elapsed >= _retryDelay) {
                Serial.println("Retry delay complete");
                _state = STATE_IDLE;
            }
            break;

        default:
            break;
    }
}

void LichessAPI::forceReset() {
    Serial.println("=== FORCE RESET: Clearing all LichessAPI state ===");

    // Stop any active stream
    if (_streaming) {
        Serial.println("Stopping active stream...");
        stopStream();
    }

    // Clear operation queue
    int queueSize = _requestQueue.size();
    _requestQueue.clear();
    if (queueSize > 0) {
        Serial.printf("Cleared %d queued operations\n", queueSize);
    }

    // Reset state machine to IDLE
    _state = STATE_IDLE;
    Serial.println("State machine reset to IDLE");

    // Clear all pending operations
    _pendingGameId = "";
    _pendingMove = "";
    _createdGameId = "";
    _operationSuccess = false;
    _wasStreaming = false;

    // Close any open connections
    if (_client.connected()) {
        _client.stop();
        Serial.println("Closed SSL client connection");
    }
    if (_http.connected()) {
        _http.end();
        Serial.println("Closed HTTP client connection");
    }

    // Clear error state
    _lastError = "";

    // Reset health tracking
    _consecutiveFailures = 0;
    _lastSuccessfulRequest = millis();

    Serial.println("=== FORCE RESET COMPLETE ===");
}

bool LichessAPI::isConnectionHealthy() {
    // Check if we've had too many consecutive failures
    if (_consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
        Serial.printf("Connection unhealthy: %d consecutive failures\n", _consecutiveFailures);
        return false;
    }

    // Check if connection has been idle too long (10 minutes)
    unsigned long now = millis();
    if (_lastSuccessfulRequest > 0 && (now - _lastSuccessfulRequest) > 600000) {
        Serial.printf("Connection stale: %lu ms since last success\n", now - _lastSuccessfulRequest);
        return false;
    }

    // Don't check client.connected() state here - it's normal for the stream
    // to be connected but idle while waiting for user input or opponent moves.
    // The timeout mechanism in checkOperationTimeout() will handle actual hangs.

    return true;
}

void LichessAPI::resetConnection() {
    Serial.println("=== RESETTING CONNECTION (without full system reboot) ===");

    // Stop streaming
    if (_streaming) {
        Serial.println("Stopping stream...");
        stopStream();
        delay(100);  // Give it time to clean up
    }

    // Force close all connections
    if (_client.connected()) {
        Serial.println("Closing stale SSL connection...");
        _client.stop();
        delay(100);
    }

    if (_http.connected()) {
        Serial.println("Closing HTTP connection...");
        _http.end();
        delay(100);
    }

    if (_streamHttp.connected()) {
        Serial.println("Closing stream HTTP connection...");
        _streamHttp.end();
        delay(100);
    }

    // Recreate SSL client with fresh configuration
    Serial.println("Reinitializing SSL client...");
    _client.setInsecure();
    _client.setTimeout(15);

    // Reset health tracking
    _consecutiveFailures = 0;
    _lastSuccessfulRequest = millis();

    // Reset state machine
    _state = STATE_IDLE;
    _pendingGameId = "";
    _pendingMove = "";
    _wasStreaming = false;

    Serial.println("=== CONNECTION RESET COMPLETE ===");
}
