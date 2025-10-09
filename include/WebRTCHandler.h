#ifndef WEBRTC_HANDLER_H
#define WEBRTC_HANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <map>
#include <vector>

/**
 * WebRTCHandler.h
 *
 * Simple WebRTC signaling server for P2P chess games
 * Stores and forwards SDP offers/answers and ICE candidates
 */

struct SignalingMessage {
    String type;          // "offer", "answer", "ice-candidate"
    String data;          // JSON string containing the message
    String fromPeer;      // "host" or "client" - who sent this message
    unsigned long timestamp;
};

class WebRTCHandler {
public:
    WebRTCHandler();
    ~WebRTCHandler();

    // Initialize with web server
    void begin(AsyncWebServer* server);

private:
    AsyncWebServer* _server;

    // Store messages by game code
    std::map<String, std::vector<SignalingMessage>> _messageQueues;

    // Cleanup old messages periodically
    unsigned long _lastCleanup;
    static const unsigned long CLEANUP_INTERVAL = 60000; // 1 minute
    static const unsigned long MESSAGE_TTL = 300000;     // 5 minutes

    // Handler methods
    void handlePoll(AsyncWebServerRequest* request);

    // Helper methods
    void cleanupOldMessages();
    void sendJSONResponse(AsyncWebServerRequest* request, int code, const String& message, bool success = true);
    void sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error);

public:
    // Call from main loop to cleanup old messages
    void processCleanup();
};

#endif // WEBRTC_HANDLER_H
