#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>

// Forward declaration
class LichessAPI;

/**
 * SessionManager.h
 *
 * Manages multiple browser sessions with IP tracking
 * Supports concurrent games from different browsers
 * Each session has its own LichessAPI instance for independent game management
 */

#define MAX_SESSIONS 3  // Allow 3 concurrent browser tabs/devices (each LichessAPI ~15-20KB RAM)
#define SESSION_TIMEOUT_MS 1800000  // 30 minutes in milliseconds (1800000ms = 30min)

struct Session {
    String sessionId;           // Unique session identifier
    String ipAddress;           // Client IP address
    String gameId;              // Current Lichess game ID
    String playerColor;         // "white" or "black"
    bool gameActive;            // Is game currently active
    bool loggingEnabled;        // Enable/disable server-side logging for this session
    bool debugLogEnabled;       // Enable/disable browser debug logging for this session
    bool pendingRefresh;        // Flag indicating admin requested browser refresh
    unsigned long createdAt;    // Session creation timestamp
    unsigned long lastActivity; // Last API activity timestamp
    unsigned long messageCount; // Number of messages sent from this session
    LichessAPI* lichessAPI;     // Dedicated API instance for this session

    Session() : gameActive(false), loggingEnabled(true), debugLogEnabled(false), pendingRefresh(false), createdAt(0), lastActivity(0), messageCount(0), lichessAPI(nullptr) {}
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    // Session lifecycle
    String createSession(const String& ipAddress);
    bool hasSession(const String& sessionId) const;
    Session* getSession(const String& sessionId);
    bool deleteSession(const String& sessionId);
    void cleanupExpiredSessions();

    // API token management (set once for all sessions)
    void setAPIToken(const char* token);

    // Session operations
    bool setGameId(const String& sessionId, const String& gameId, const String& color);
    bool setGameActive(const String& sessionId, bool active);
    bool updateActivity(const String& sessionId);
    bool setLoggingEnabled(const String& sessionId, bool enabled);
    bool setDebugLogEnabled(const String& sessionId, bool enabled);
    bool incrementMessageCount(const String& sessionId);

    // Process all session API instances (call from main loop)
    void processAllSessions();

    // Session queries
    int getActiveSessionCount() const;
    String getSessionByGameId(const String& gameId) const;
    std::vector<String> getSessionsByIP(const String& ipAddress) const;

    // Admin IP management (for admin button visibility)
    void addAdminIP(const String& ipAddress);
    void removeAdminIP(const String& ipAddress);
    bool isAdminIP(const String& ipAddress) const;
    void clearAdminIPs();
    bool loadAdminIPsFromSD();  // Load admin IPs from /admin-auth.md

    // Debug/logging
    void printActiveSessions() const;
    String getSessionsJSON() const;

    // Iterator access for processing all sessions (e.g., checking async results)
    const std::map<String, Session>& getAllSessions() const { return _sessions; }
    std::map<String, Session>& getAllSessions() { return _sessions; }

private:
    std::map<String, Session> _sessions;
    std::vector<String> _adminIPs;
    unsigned long _lastCleanup;
    String _apiToken;  // Shared API token for all sessions

    String generateSessionId();
    bool isSessionExpired(const Session& session) const;
    void createLichessAPIForSession(Session* session);
    void destroyLichessAPIForSession(Session* session);
};

#endif // SESSION_MANAGER_H
