#include "SessionManager.h"
#include "LichessAPI.h"
#include "SDLogger.h"
#include <ArduinoJson.h>
#include <SD.h>

SessionManager::SessionManager() : _lastCleanup(0), _apiToken("") {
    Serial.println("SessionManager initialized");
}

SessionManager::~SessionManager() {
    // Clean up all LichessAPI instances
    for (auto& pair : _sessions) {
        destroyLichessAPIForSession(&pair.second);
    }
}

void SessionManager::setAPIToken(const char* token) {
    _apiToken = String(token);
    Serial.printf("API token set for all sessions (length: %d)\n", _apiToken.length());
}

void SessionManager::createLichessAPIForSession(Session* session) {
    if (!session) return;

    if (session->lichessAPI) {
        Serial.printf("Session %s: LichessAPI already exists\n", session->sessionId.c_str());
        return;
    }

    session->lichessAPI = new LichessAPI();
    if (_apiToken.length() > 0) {
        session->lichessAPI->begin(_apiToken.c_str());
    }
    Serial.printf("Session %s: LichessAPI instance created\n", session->sessionId.c_str());
}

void SessionManager::destroyLichessAPIForSession(Session* session) {
    if (!session) return;

    if (session->lichessAPI) {
        delete session->lichessAPI;
        session->lichessAPI = nullptr;
        Serial.printf("Session %s: LichessAPI instance destroyed\n", session->sessionId.c_str());
    }
}

// Generate a simple unique session ID based on time and random value
String SessionManager::generateSessionId() {
    char sessionId[17];
    unsigned long timestamp = millis();
    int randomValue = random(0, 65536);
    snprintf(sessionId, sizeof(sessionId), "%08lx%04x", timestamp, randomValue);
    return String(sessionId);
}

// Check if session has expired based on last activity
bool SessionManager::isSessionExpired(const Session& session) const {
    unsigned long now = millis();
    // Handle millis() rollover (occurs every ~49 days)
    if (now < session.lastActivity) {
        return false; // Don't expire during rollover
    }
    return (now - session.lastActivity) > SESSION_TIMEOUT_MS;
}

// Create new session
String SessionManager::createSession(const String& ipAddress) {
    // Check for existing sessions from the same IP with pendingRefresh flag
    std::vector<String> sessionsToDelete;
    for (auto& pair : _sessions) {
        if (pair.second.ipAddress == ipAddress && pair.second.pendingRefresh) {
            sessionsToDelete.push_back(pair.first);
            Serial.printf("Marking old session %s for deletion (pending refresh from %s)\n",
                         pair.first.c_str(), ipAddress.c_str());
        }
    }

    // Delete old sessions from the same IP that were flagged for refresh
    // Use safe iteration to avoid issues during deletion
    for (const String& oldSessionId : sessionsToDelete) {
        // Double-check session still exists before deleting (safety check)
        if (hasSession(oldSessionId)) {
            bool deleted = deleteSession(oldSessionId);
            if (deleted) {
                Serial.printf("Successfully deleted old session %s (refresh cleanup)\n", oldSessionId.c_str());
            } else {
                Serial.printf("WARNING: Failed to delete session %s during refresh cleanup\n", oldSessionId.c_str());
            }
        } else {
            Serial.printf("WARNING: Session %s already deleted, skipping\n", oldSessionId.c_str());
        }
    }

    // Check if we've hit the session limit
    if (_sessions.size() >= MAX_SESSIONS) {
        // Try cleaning up expired sessions first
        cleanupExpiredSessions();

        // Still at limit? Fail
        if (_sessions.size() >= MAX_SESSIONS) {
            Serial.printf("ERROR: Maximum session limit reached (%d sessions)\n", MAX_SESSIONS);
            return "";
        }
    }

    // Generate unique session ID
    String sessionId = generateSessionId();

    // Ensure it's truly unique (handle collision)
    while (_sessions.find(sessionId) != _sessions.end()) {
        sessionId = generateSessionId();
    }

    // Create new session
    Session session;
    session.sessionId = sessionId;
    session.ipAddress = ipAddress;
    session.gameActive = false;
    session.createdAt = millis();
    session.lastActivity = millis();
    session.lichessAPI = nullptr;

    // Enable debug logging by default for IPs NOT in the 192.168.1.x range
    if (!ipAddress.startsWith("192.168.1.")) {
        session.debugLogEnabled = true;
        Serial.printf("Session %s: Debug logging enabled (external IP: %s)\n", sessionId.c_str(), ipAddress.c_str());
    } else {
        session.debugLogEnabled = false;
    }

    _sessions[sessionId] = session;

    // Create LichessAPI instance for this session
    createLichessAPIForSession(&_sessions[sessionId]);

    Serial.printf("Session created: %s from IP %s (total sessions: %d)\n",
                  sessionId.c_str(), ipAddress.c_str(), _sessions.size());

    return sessionId;
}

// Check if session exists
bool SessionManager::hasSession(const String& sessionId) const {
    return _sessions.find(sessionId) != _sessions.end();
}

// Get session pointer (returns nullptr if not found)
Session* SessionManager::getSession(const String& sessionId) {
    auto it = _sessions.find(sessionId);
    if (it != _sessions.end()) {
        return &(it->second);
    }
    return nullptr;
}

// Delete session
bool SessionManager::deleteSession(const String& sessionId) {
    auto it = _sessions.find(sessionId);
    if (it != _sessions.end()) {
        // Safety check: ensure session ID matches (paranoid validation)
        if (it->second.sessionId.isEmpty() || it->second.sessionId != sessionId) {
            Serial.printf("WARNING: Session ID mismatch during deletion (expected: %s, got: %s)\n",
                         sessionId.c_str(), it->second.sessionId.c_str());
        }

        Serial.printf("Session deleted: %s (IP: %s, game: %s)\n",
                      sessionId.c_str(),
                      it->second.ipAddress.c_str(),
                      it->second.gameId.c_str());

        // Destroy LichessAPI instance for this session (handles nullptr safely)
        destroyLichessAPIForSession(&it->second);

        // Erase session from map
        _sessions.erase(it);

        // Verify deletion
        if (_sessions.find(sessionId) != _sessions.end()) {
            Serial.printf("ERROR: Session %s still exists after erase!\n", sessionId.c_str());
            return false;
        }

        return true;
    }
    Serial.printf("WARNING: Attempted to delete non-existent session: %s\n", sessionId.c_str());
    return false;
}

// Clean up expired sessions
void SessionManager::cleanupExpiredSessions() {
    unsigned long now = millis();

    // Only cleanup every 60 seconds to avoid overhead
    if (now - _lastCleanup < 60000) {
        return;
    }

    _lastCleanup = now;

    std::vector<String> expiredSessions;

    // Find expired sessions
    for (auto& pair : _sessions) {
        if (isSessionExpired(pair.second)) {
            expiredSessions.push_back(pair.first);
        }
    }

    // Delete expired sessions
    for (const String& sessionId : expiredSessions) {
        Serial.printf("Cleaning up expired session: %s\n", sessionId.c_str());
        deleteSession(sessionId);
    }
}

// Set game ID and color for session
bool SessionManager::setGameId(const String& sessionId, const String& gameId, const String& color) {
    Session* session = getSession(sessionId);
    if (session) {
        session->gameId = gameId;
        session->playerColor = color;
        session->lastActivity = millis();
        Serial.printf("Session %s: game set to %s (color: %s)\n",
                      sessionId.c_str(), gameId.c_str(), color.c_str());
        return true;
    }
    return false;
}

// Set game active status
bool SessionManager::setGameActive(const String& sessionId, bool active) {
    Session* session = getSession(sessionId);
    if (session) {
        session->gameActive = active;
        session->lastActivity = millis();
        Serial.printf("Session %s: game active = %s\n",
                      sessionId.c_str(), active ? "true" : "false");
        return true;
    }
    return false;
}

// Update last activity timestamp
bool SessionManager::updateActivity(const String& sessionId) {
    Session* session = getSession(sessionId);
    if (session) {
        session->lastActivity = millis();
        return true;
    }
    return false;
}

// Process all session API instances (call from main loop)
void SessionManager::processAllSessions() {
    for (auto& pair : _sessions) {
        if (pair.second.lichessAPI) {
            pair.second.lichessAPI->process();
        }
    }
}

// Get count of active sessions
int SessionManager::getActiveSessionCount() const {
    return _sessions.size();
}

// Find session by game ID
String SessionManager::getSessionByGameId(const String& gameId) const {
    for (const auto& pair : _sessions) {
        if (pair.second.gameId == gameId) {
            return pair.first;
        }
    }
    return "";
}

// Get all sessions from specific IP
std::vector<String> SessionManager::getSessionsByIP(const String& ipAddress) const {
    std::vector<String> sessions;
    for (const auto& pair : _sessions) {
        if (pair.second.ipAddress == ipAddress) {
            sessions.push_back(pair.first);
        }
    }
    return sessions;
}

// Admin IP management
void SessionManager::addAdminIP(const String& ipAddress) {
    // Check if already in list
    for (const String& ip : _adminIPs) {
        if (ip == ipAddress) {
            return;
        }
    }
    _adminIPs.push_back(ipAddress);
    Serial.printf("Admin IP added: %s\n", ipAddress.c_str());
}

void SessionManager::removeAdminIP(const String& ipAddress) {
    for (auto it = _adminIPs.begin(); it != _adminIPs.end(); ++it) {
        if (*it == ipAddress) {
            _adminIPs.erase(it);
            Serial.printf("Admin IP removed: %s\n", ipAddress.c_str());
            return;
        }
    }
}

bool SessionManager::isAdminIP(const String& ipAddress) const {
    for (const String& ip : _adminIPs) {
        if (ip == ipAddress) {
            return true;
        }
    }
    return false;
}

void SessionManager::clearAdminIPs() {
    _adminIPs.clear();
    Serial.println("All admin IPs cleared");
}

// Load admin IPs from /admin-auth.md on SD card
bool SessionManager::loadAdminIPsFromSD() {
    File file = SD.open("/admin-auth.md", FILE_READ);
    if (!file) {
        Serial.println("admin-auth.md not found on SD card - no admin IPs loaded");
        return false;
    }

    clearAdminIPs();
    int count = 0;

    Serial.println("Loading admin IPs from /admin-auth.md...");

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        // Skip empty lines and comments (lines starting with #)
        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }

        // Validate IP format (basic check for xxx.xxx.xxx.xxx pattern)
        bool validIP = false;
        int dotCount = 0;
        for (unsigned int i = 0; i < line.length(); i++) {
            if (line[i] == '.') dotCount++;
            else if (!isDigit(line[i])) break;
        }

        if (dotCount == 3) {
            validIP = true;
        }

        if (validIP) {
            addAdminIP(line);
            count++;
        } else {
            Serial.printf("Skipping invalid IP: %s\n", line.c_str());
        }
    }

    file.close();
    Serial.printf("Loaded %d admin IP(s) from admin-auth.md\n", count);

    return count > 0;
}

// Print active sessions for debugging
void SessionManager::printActiveSessions() const {
    Serial.printf("=== Active Sessions (%d) ===\n", _sessions.size());
    for (const auto& pair : _sessions) {
        const Session& s = pair.second;
        Serial.printf("  Session: %s\n", s.sessionId.c_str());
        Serial.printf("    IP: %s\n", s.ipAddress.c_str());
        Serial.printf("    Game: %s (%s)\n", s.gameId.c_str(), s.playerColor.c_str());
        Serial.printf("    Active: %s\n", s.gameActive ? "Yes" : "No");
        unsigned long age = (millis() - s.createdAt) / 1000;
        Serial.printf("    Age: %lu seconds\n", age);
    }
}

// Get sessions as JSON
String SessionManager::getSessionsJSON() const {
    JsonDocument doc;
    JsonArray sessionsArray = doc["sessions"].to<JsonArray>();

    for (const auto& pair : _sessions) {
        const Session& s = pair.second;
        JsonObject sessionObj = sessionsArray.add<JsonObject>();
        sessionObj["sessionId"] = s.sessionId;
        sessionObj["ipAddress"] = s.ipAddress;
        sessionObj["gameId"] = s.gameId;
        sessionObj["playerColor"] = s.playerColor;
        sessionObj["gameActive"] = s.gameActive;
        sessionObj["loggingEnabled"] = s.loggingEnabled;
        sessionObj["debugLogEnabled"] = s.debugLogEnabled;
        sessionObj["messageCount"] = s.messageCount;
        sessionObj["createdAt"] = s.createdAt;
        sessionObj["lastActivity"] = s.lastActivity;

        // Calculate session age in seconds
        unsigned long ageSeconds = (millis() - s.createdAt) / 1000;
        sessionObj["ageSeconds"] = ageSeconds;
    }

    doc["count"] = _sessions.size();
    doc["maxSessions"] = MAX_SESSIONS;

    String json;
    serializeJson(doc, json);
    return json;
}

// Set logging enabled for a session
bool SessionManager::setLoggingEnabled(const String& sessionId, bool enabled) {
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) {
        return false;
    }

    it->second.loggingEnabled = enabled;
    Serial.printf("Session %s: Logging %s\n", sessionId.c_str(), enabled ? "enabled" : "disabled");
    return true;
}

// Set debug log enabled for a session
bool SessionManager::setDebugLogEnabled(const String& sessionId, bool enabled) {
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) {
        return false;
    }

    it->second.debugLogEnabled = enabled;
    Serial.printf("Session %s: Debug log %s\n", sessionId.c_str(), enabled ? "enabled" : "disabled");
    return true;
}

// Increment message count for a session
bool SessionManager::incrementMessageCount(const String& sessionId) {
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) {
        return false;
    }

    it->second.messageCount++;
    return true;
}
