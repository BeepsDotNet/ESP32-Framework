#include "WebScraper.h"

WebScraper::WebScraper() {
  timeoutMs = 30000; // 30 second timeout
}

void WebScraper::begin() {
  client.setInsecure(); // Skip SSL verification for simplicity
  Serial.println("WebScraper initialized");
}

void WebScraper::setEndpoint(int aiIndex, const AIEndpoint& endpoint) {
  if (aiIndex >= 0 && aiIndex < 2) {
    endpoints[aiIndex] = endpoint;
    Serial.printf("AI %d endpoint set: %s\n", aiIndex + 1, endpoint.name.c_str());
  }
}

String WebScraper::requestMove(int aiIndex, const String& currentPosition, const String& moveHistory, const String& color) {
  if (aiIndex < 0 || aiIndex >= 2) {
    Serial.println("Invalid AI index");
    return "";
  }

  AIEndpoint& endpoint = endpoints[aiIndex];
  if (endpoint.baseUrl.isEmpty()) {
    Serial.printf("AI %d endpoint not configured\n", aiIndex + 1);
    return "";
  }

  // Format the chess prompt
  String prompt = formatChessPrompt(currentPosition, moveHistory, color);

  Serial.printf("Requesting move from %s...\n", endpoint.name.c_str());
  Serial.printf("Prompt: %s\n", prompt.c_str());

  http.begin(client, endpoint.baseUrl + endpoint.chatEndpoint);
  http.setTimeout(timeoutMs);

  // Set headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ChessAI-ESP32/1.0");

  // Add custom headers from endpoint configuration
  if (!endpoint.headers.isEmpty()) {
    // Parse and add custom headers
    // Format: "Header1:Value1|Header2:Value2"
    int startPos = 0;
    int delimPos = endpoint.headers.indexOf('|', startPos);

    while (delimPos != -1 || startPos < endpoint.headers.length()) {
      String headerPair = (delimPos != -1) ?
        endpoint.headers.substring(startPos, delimPos) :
        endpoint.headers.substring(startPos);

      int colonPos = headerPair.indexOf(':');
      if (colonPos != -1) {
        String headerName = headerPair.substring(0, colonPos);
        String headerValue = headerPair.substring(colonPos + 1);
        http.addHeader(headerName, headerValue);
      }

      if (delimPos == -1) break;
      startPos = delimPos + 1;
      delimPos = endpoint.headers.indexOf('|', startPos);
    }
  }

  // Create JSON payload
  JsonDocument jsonDoc;
  jsonDoc["model"] = "gpt-3.5-turbo"; // Default, should be configurable

  JsonArray messages = jsonDoc["messages"].to<JsonArray>();

  JsonObject systemMsg = messages.add<JsonObject>();
  systemMsg["role"] = "system";
  systemMsg["content"] = "You are a chess master. Respond only with the next move in algebraic notation (e.g., 'e4', 'Nf3', 'O-O'). No explanations.";

  JsonObject userMsg = messages.add<JsonObject>();
  userMsg["role"] = "user";
  userMsg["content"] = prompt;

  jsonDoc["max_tokens"] = 10;
  jsonDoc["temperature"] = 0.7;

  String payload;
  serializeJson(jsonDoc, payload);

  int httpResponseCode = http.POST(payload);
  String response = "";

  if (httpResponseCode > 0) {
    response = http.getString();
    Serial.printf("HTTP Response Code: %d\n", httpResponseCode);

    if (httpResponseCode == 200) {
      String move = extractMoveFromResponse(response, endpoint.name);
      http.end();

      if (validateMove(move)) {
        Serial.printf("Valid move received: %s\n", move.c_str());
        return move;
      } else {
        Serial.printf("Invalid move received: %s\n", move.c_str());
        return "";
      }
    } else {
      Serial.printf("HTTP Error: %s\n", response.c_str());
    }
  } else {
    Serial.printf("HTTP Request failed: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  return "";
}

String WebScraper::extractMoveFromResponse(const String& response, const String& aiName) {
  Serial.printf("Raw response from %s: %s\n", aiName.c_str(), response.c_str());

  // Parse JSON response
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  if (error) {
    Serial.printf("JSON parsing failed: %s\n", error.c_str());
    return "";
  }

  // Extract move from different AI response formats
  String move = "";

  // OpenAI format
  if (doc["choices"].is<JsonArray>() && doc["choices"].size() > 0) {
    JsonObject choice = doc["choices"][0];
    if (choice["message"]["content"].is<const char*>()) {
      move = choice["message"]["content"].as<String>();
    }
  }
  // Claude format (if different)
  else if (doc["content"].is<JsonArray>() && doc["content"].size() > 0) {
    JsonObject content = doc["content"][0];
    if (content["text"].is<const char*>()) {
      move = content["text"].as<String>();
    }
  }
  // Generic text response
  else if (doc["text"].is<const char*>()) {
    move = doc["text"].as<String>();
  }

  // Clean up the move string
  move.trim();
  move.replace(".", "");
  move.replace("\"", "");
  move.replace("'", "");

  // Extract only the move part (remove explanations)
  int spacePos = move.indexOf(' ');
  if (spacePos != -1) {
    move = move.substring(0, spacePos);
  }

  return move;
}

String WebScraper::formatChessPrompt(const String& position, const String& moves, const String& color) {
  String prompt = "Current chess position (FEN): " + position + "\n";

  if (!moves.isEmpty()) {
    prompt += "Move history: " + moves + "\n";
  }

  prompt += "You are playing as " + color + ". What is your next move? ";
  prompt += "Respond with only the move in algebraic notation (e.g., e4, Nf3, O-O).";

  return prompt;
}

bool WebScraper::validateMove(const String& move) {
  if (move.isEmpty() || move.length() > 7) {
    return false;
  }

  // Basic move format validation
  // This is a simplified check - real validation happens in ChessEngine

  // Check for basic patterns
  if (move == "O-O" || move == "O-O-O") {
    return true; // Castling
  }

  // Check for piece moves (e.g., Nf3, Be5, Qh5)
  if (move.length() >= 2) {
    char lastChar = move.charAt(move.length() - 1);
    char secondLastChar = move.charAt(move.length() - 2);

    // Should end with a valid square (a-h, 1-8)
    if (lastChar >= '1' && lastChar <= '8' &&
        secondLastChar >= 'a' && secondLastChar <= 'h') {
      return true;
    }
  }

  return false;
}

bool WebScraper::testConnection(int aiIndex) {
  if (aiIndex < 0 || aiIndex >= 2) {
    return false;
  }

  AIEndpoint& endpoint = endpoints[aiIndex];
  if (endpoint.baseUrl.isEmpty()) {
    return false;
  }

  http.begin(client, endpoint.baseUrl);
  http.setTimeout(5000); // 5 second timeout for test

  int httpResponseCode = http.GET();
  bool success = (httpResponseCode > 0 && httpResponseCode < 400);

  http.end();

  Serial.printf("Connection test for %s: %s\n",
    endpoint.name.c_str(), success ? "SUCCESS" : "FAILED");

  return success;
}

void WebScraper::setupChatGPT(int aiIndex) {
  AIEndpoint endpoint;
  endpoint.name = "ChatGPT";
  endpoint.baseUrl = "https://api.openai.com";
  endpoint.chatEndpoint = "/v1/chat/completions";
  endpoint.headers = "Authorization:Bearer YOUR_OPENAI_API_KEY";
  endpoint.promptTemplate = "chess_standard";
  endpoint.responseParser = "openai_json";

  setEndpoint(aiIndex, endpoint);
}

void WebScraper::setupClaude(int aiIndex) {
  AIEndpoint endpoint;
  endpoint.name = "Claude";
  endpoint.baseUrl = "https://api.anthropic.com";
  endpoint.chatEndpoint = "/v1/messages";
  endpoint.headers = "x-api-key:YOUR_ANTHROPIC_API_KEY|anthropic-version:2023-06-01";
  endpoint.promptTemplate = "chess_standard";
  endpoint.responseParser = "anthropic_json";

  setEndpoint(aiIndex, endpoint);
}

void WebScraper::setupCustomAI(int aiIndex, const AIEndpoint& endpoint) {
  setEndpoint(aiIndex, endpoint);
}