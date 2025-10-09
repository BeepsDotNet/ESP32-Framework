#ifndef GEMINI_API_H
#define GEMINI_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

class GeminiAPI {
private:
  String apiKey;
  String baseUrl;
  int timeoutMs;

  // API request methods
  String makeRequest(const String& prompt);
  String extractTextFromResponse(const String& response);
  String formatChessPrompt(const String& position, const String& moves, const String& color);
  bool validateMove(const String& move);
  String extractMoveFromText(const String& text);

public:
  GeminiAPI();

  void begin();
  void setAPIKey(const String& key);
  bool isConfigured() const { return !apiKey.isEmpty(); }

  // Chess-specific methods
  String requestMove(const String& currentPosition, const String& moveHistory, const String& color);
  bool testConnection();

  // Utility methods
  void setTimeout(int timeout) { timeoutMs = timeout; }
};

#endif