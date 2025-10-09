#ifndef LICHESS_API_H
#define LICHESS_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <vector>

/**
 * LichessAPI.h
 *
 * Handles Lichess Board API integration for human vs AI chess gameplay
 * Uses non-bot Board API endpoints for regular Lichess accounts
 *
 * Key Features:
 * - Create AI challenge games
 * - Stream game state (NDJSON format)
 * - Submit moves (UCI notation)
 * - Handle game events
 */

class LichessAPI {
public:
    LichessAPI();
    ~LichessAPI();

    // Initialize with API token
    bool begin(const char* apiToken);

    // Test account connection
    bool testAccount(String& username);

    // Create new AI game
    // Returns gameId on success, empty string on failure
    String createAIGame(int level = 3, int timeLimitSeconds = 600, int incrementSeconds = 0, const char* color = "white");

    // Make a move (UCI notation: e.g., "e2e4", "e7e8q" for promotion)
    // Non-blocking - call process() in loop to complete
    bool makeMove(const String& gameId, const String& uciMove);

    // Resign current game
    bool resignGame(const String& gameId);

    // Stream handling (non-blocking)
    bool startStream(const String& gameId);
    void stopStream();
    bool isStreaming() const { return _streaming; }

    // Process stream events (call regularly in loop)
    // Returns true if new event available
    bool processStreamEvents(String& eventJson);

    // Process async operations - call regularly from main loop
    void process();

    // Check if busy with async operation
    bool isBusy() const { return _state != STATE_IDLE; }

    // Get async operation results
    String getCreatedGameId() const { return _createdGameId; }
    bool wasOperationSuccessful() const { return _operationSuccess; }
    void clearCreatedGameId() { _createdGameId = ""; _operationSuccess = false; }

    // Get last error message
    String getLastError() const { return _lastError; }

    // Clear error message (used after handling errors)
    void clearError() { _lastError = ""; }

    // Check if API token is set
    bool hasToken() const { return _apiToken.length() > 0; }
    int getTokenLength() const { return _apiToken.length(); }

    // Queue management
    bool queueCreateGame(int level, int timeLimit, int increment, const String& color);
    int getQueueSize() const { return _requestQueue.size(); }

    // Emergency reset - clears all state and returns to IDLE
    void forceReset();

    // Connection health check and recovery
    bool isConnectionHealthy();
    void resetConnection();

private:
    // Request queue structure
    struct QueuedRequest {
        enum Type { CREATE_GAME, MAKE_MOVE, RESIGN_GAME };
        Type type;
        String gameId;
        String move;
        int level;
        int timeLimit;
        int increment;
        String color;
    };

    std::vector<QueuedRequest> _requestQueue;
    // Async state machine states
    enum State {
        STATE_IDLE,
        STATE_WAITING_STREAM_STOP,      // Waiting for stream to close before operation
        STATE_WAITING_SSL_CLEANUP,      // Waiting for SSL client cleanup
        STATE_MAKING_MOVE,              // Making API call for move
        STATE_RESUMING_STREAM,          // Resuming stream after move
        STATE_RESIGNING_GAME,           // Resigning the game
        STATE_CREATING_GAME,            // Creating a new game
        STATE_STARTING_STREAM,          // Starting stream for new game
        STATE_RETRYING_CONNECTION       // Waiting to retry failed connection
    };

    // Single SSL client shared between operations (ESP32 doesn't have enough RAM for 2)
    WiFiClientSecure _client;
    HTTPClient _http;
    HTTPClient _streamHttp;

    // Stream handling
    WiFiClient* _streamClient;
    bool _streaming;
    String _streamBuffer;

    // Async state machine
    State _state;
    unsigned long _stateStartTime;
    String _pendingGameId;
    String _pendingMove;
    bool _wasStreaming;
    int _retryAttempt;
    unsigned long _retryDelay;

    // Async operation results (for callbacks)
    String _createdGameId;
    bool _operationSuccess;

    // Pending game creation parameters
    int _pendingLevel;
    int _pendingTimeLimit;
    int _pendingIncrement;
    String _pendingColor;

    // Configuration
    String _apiToken;
    String _lastError;

    // Heartbeat tracking for diagnostics
    unsigned long _lastHeartbeatTime;
    unsigned long _heartbeatCount;

    // Connection health tracking
    int _consecutiveFailures;
    unsigned long _lastSuccessfulRequest;
    static constexpr int MAX_CONSECUTIVE_FAILURES = 3;

    // API endpoints
    static constexpr const char* LICHESS_HOST = "lichess.org";
    static constexpr const char* API_ACCOUNT = "https://lichess.org/api/account";
    static constexpr const char* API_CHALLENGE_AI = "https://lichess.org/api/challenge/ai";
    static constexpr const char* API_BOARD_GAME_STREAM = "https://lichess.org/api/board/game/stream/";
    static constexpr const char* API_BOARD_GAME_MOVE = "https://lichess.org/api/board/game/";

    // Timing constants (milliseconds)
    static constexpr unsigned long STREAM_STOP_DELAY = 50;
    static constexpr unsigned long SSL_CLEANUP_DELAY = 500;
    static constexpr unsigned long RETRY_INITIAL_DELAY = 1000;
    static constexpr unsigned long STREAM_RESUME_DELAY = 500;
    static constexpr unsigned long OPERATION_TIMEOUT = 30000;  // 30 second timeout for operations

    // Helper methods
    bool makeAPICall(const char* url, const char* method, const String& body, String& response, bool enableRetry = true);
    void setError(const String& error);
    void runNetworkDiagnostics();
    bool parseNDJSON(String& line);
    void processQueue();
    bool checkOperationTimeout();
};

#endif // LICHESS_API_H
