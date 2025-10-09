#include "WebRTCHandler.h"
#include "SDLogger.h"
#include <ArduinoJson.h>

WebRTCHandler::WebRTCHandler() : _server(nullptr), _lastCleanup(0) {
}

WebRTCHandler::~WebRTCHandler() {
}

void WebRTCHandler::begin(AsyncWebServer* server) {
    _server = server;

    Serial.printf("Initializing WebRTC signaling server\n");

    // POST /api/webrtc/signal - Send signaling message
    _server->on("/api/webrtc/signal", HTTP_POST,
        [](AsyncWebServerRequest *request){
            // Request handler - will be called after body handler completes
            // Body handler will have already sent the response
        },
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Body handler - accumulate and process complete message
            static String bodyBuffer = "";

            // Append this chunk to buffer
            for (size_t i = 0; i < len; i++) {
                bodyBuffer += (char)data[i];
            }

            // Check if this is the last chunk
            if (index + len == total) {
                // Process complete body
                DynamicJsonDocument doc(2048);
                DeserializationError error = deserializeJson(doc, bodyBuffer);

                // Clear buffer for next request
                bodyBuffer = "";

                if (error) {
                    Serial.printf("Failed to parse signaling JSON: %s\n", error.c_str());
                    sendErrorResponse(request, 400, "Invalid JSON");
                    return;
                }

                String gameCode = doc["gameCode"] | "";
                String type = doc["type"] | "";
                String fromPeer = doc["fromPeer"] | "";

                if (gameCode.isEmpty() || type.isEmpty() || fromPeer.isEmpty()) {
                    sendErrorResponse(request, 400, "Missing gameCode, type, or fromPeer");
                    return;
                }

                // Serialize the entire message to store
                String messageData;
                serializeJson(doc, messageData);

                // Store message in queue
                SignalingMessage msg;
                msg.type = type;
                msg.data = messageData;
                msg.fromPeer = fromPeer;
                msg.timestamp = millis();

                _messageQueues[gameCode].push_back(msg);

                Serial.printf("Stored %s message from %s for game %s\n", type.c_str(), fromPeer.c_str(), gameCode.c_str());

                sendJSONResponse(request, 200, "Message stored", true);
            }
        }
    );

    // GET /api/webrtc/poll - Poll for messages
    _server->on("/api/webrtc/poll", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handlePoll(request);
    });

    Serial.printf("WebRTC signaling endpoints registered\n");
}


void WebRTCHandler::handlePoll(AsyncWebServerRequest* request) {
    if (!request->hasParam("gameCode") || !request->hasParam("asPeer")) {
        Serial.printf("Poll missing params - gameCode: %d, asPeer: %d\n",
            request->hasParam("gameCode"), request->hasParam("asPeer"));
        sendErrorResponse(request, 400, "Missing gameCode or asPeer parameter");
        return;
    }

    String gameCode = request->getParam("gameCode")->value();
    String asPeer = request->getParam("asPeer")->value();

    Serial.printf("Poll from peer '%s' for game %s\n", asPeer.c_str(), gameCode.c_str());

    // Check if we have messages for this game code
    if (_messageQueues.find(gameCode) == _messageQueues.end()) {
        // No messages yet, return empty array
        Serial.printf("No message queue found for game %s\n", gameCode.c_str());
        DynamicJsonDocument doc(512);
        doc["success"] = true;
        JsonArray messages = doc.createNestedArray("messages");

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
        return;
    }

    // Return messages NOT sent by this peer, and remove them
    std::vector<SignalingMessage>& messages = _messageQueues[gameCode];
    Serial.printf("Queue has %d total messages\n", messages.size());

    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    JsonArray msgArray = doc.createNestedArray("messages");

    // Collect messages for this peer (sent by the OTHER peer)
    auto it = messages.begin();
    while (it != messages.end()) {
        Serial.printf("Message fromPeer='%s', asPeer='%s', match=%d\n",
            it->fromPeer.c_str(), asPeer.c_str(), (it->fromPeer == asPeer));

        if (it->fromPeer != asPeer) {
            // This message is for us (sent by other peer)
            DynamicJsonDocument msgDoc(2048);
            deserializeJson(msgDoc, it->data);
            msgArray.add(msgDoc.as<JsonObject>());

            // Remove this message after adding to response
            it = messages.erase(it);
        } else {
            // This message was sent by us, keep it for the other peer
            ++it;
        }
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);

    Serial.printf("Sent %d messages for game %s to peer %s\n", msgArray.size(), gameCode.c_str(), asPeer.c_str());
}

void WebRTCHandler::processCleanup() {
    unsigned long now = millis();

    if (now - _lastCleanup > CLEANUP_INTERVAL) {
        cleanupOldMessages();
        _lastCleanup = now;
    }
}

void WebRTCHandler::cleanupOldMessages() {
    unsigned long now = millis();
    int totalCleaned = 0;

    // Iterate through all game codes
    auto it = _messageQueues.begin();
    while (it != _messageQueues.end()) {
        std::vector<SignalingMessage>& messages = it->second;

        // Remove old messages
        auto msgIt = messages.begin();
        while (msgIt != messages.end()) {
            if (now - msgIt->timestamp > MESSAGE_TTL) {
                msgIt = messages.erase(msgIt);
                totalCleaned++;
            } else {
                ++msgIt;
            }
        }

        // If no messages left for this game code, remove the game code
        if (messages.empty()) {
            it = _messageQueues.erase(it);
        } else {
            ++it;
        }
    }

    if (totalCleaned > 0) {
        Serial.printf("Cleaned up %d old signaling messages\n", totalCleaned);
    }
}

void WebRTCHandler::sendJSONResponse(AsyncWebServerRequest* request, int code, const String& message, bool success) {
    DynamicJsonDocument doc(256);
    doc["success"] = success;
    doc["message"] = message;

    String response;
    serializeJson(doc, response);
    request->send(code, "application/json", response);
}

void WebRTCHandler::sendErrorResponse(AsyncWebServerRequest* request, int code, const String& error) {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["error"] = error;

    String response;
    serializeJson(doc, response);
    request->send(code, "application/json", response);
}
