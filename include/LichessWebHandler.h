#ifndef LICHESS_WEB_HANDLER_H
#define LICHESS_WEB_HANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "LichessAPI.h"
#include "SessionManager.h"

/**
 * LichessWebHandler.h
 *
 * Handles web endpoints for Lichess integration with multi-session support
 * Provides REST API and SSE streaming between browsers and Lichess
 * Supports multiple concurrent games from different browsers
 */

class LichessWebHandler {
public:
    LichessWebHandler();
    ~LichessWebHandler();

    // Initialize with web server and API instance
    void begin(AsyncWebServer* server, LichessAPI* api, SessionManager* sessionMgr);

    // Set API token
    void setAPIToken(const char* token);

    // Legacy methods (deprecated - kept for compatibility)
    bool isGameActive() const { return false; }  // Now per-session
    String getCurrentGameId() const { return ""; }  // Now per-session

private:
    AsyncWebServer* _server;
    LichessAPI* _lichessAPI;
    SessionManager* _sessionManager;

    String _apiToken;

    // SSE clients tracking
    AsyncEventSource* _eventSource;

    // Handler methods (now session-aware)
    void handleTestAccount(AsyncWebServerRequest* request);
    void handleCreateSession(AsyncWebServerRequest* request);
    void handleCreateGame(AsyncWebServerRequest* request);
    void handleMakeMove(AsyncWebServerRequest* request);
    void handleResignGame(AsyncWebServerRequest* request);
    void handleGetGameStatus(AsyncWebServerRequest* request);
    void handleReset(AsyncWebServerRequest* request);
    void handleCheckAdmin(AsyncWebServerRequest* request);
    void handleGetSessions(AsyncWebServerRequest* request);

    // SSE stream management
    void setupSSEStream();

    // Helper methods
    String getClientIP(AsyncWebServerRequest* request);
    String getSessionIdFromRequest(AsyncWebServerRequest* request);
    void sendJSONResponse(AsyncWebServerRequest* request, int code, const String& message, bool success = true);
    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error);

public:
    // Public method for main loop access
    void forwardLichessEvents();

    // Check for completed async operations and update game state
    void processAsyncResults();

    // Session cleanup (call from main loop)
    void cleanupSessions();
};

// Global instance for loop() access
extern LichessWebHandler* g_lichessWebHandler;

// Helper function to call from main loop
void processLichessStreamEvents();

#endif // LICHESS_WEB_HANDLER_H
