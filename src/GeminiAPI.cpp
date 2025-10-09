#include "GeminiAPI.h"
// #include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

GeminiAPI::GeminiAPI() {
  baseUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent";
  timeoutMs = 4000; // Reduced to 4 second timeout to prevent hangs
  apiKey = "";
}

void GeminiAPI::begin() {
  // No longer need to configure member variables since we create fresh clients each time
}

void GeminiAPI::setAPIKey(const String& key) {
  apiKey = key;
}

String GeminiAPI::makeRequest(const String& prompt) {
  if (apiKey.isEmpty()) {
    return "";
  }

  // Offline-only mode: disable all HTTP calls for system stability
  static int moveCount = 0;
  moveCount++;


  // Force fallback to dynamic move scanning for safety
  // Return empty to trigger the robust fallback logic that scans all valid moves
  return "";

  // NOTE: All HTTP code below is now disabled for system stability
  /*
  String url = baseUrl + "?key=" + apiKey;

  // Brief pre-request preparation
  // esp_task_wdt_reset();
  yield();
  delay(100);

  // Create completely new HTTP client and WiFi client for this request
  WiFiClientSecure client;
  HTTPClient http;

  // Configure fresh client with more forgiving timeouts
  client.setInsecure(); // Skip certificate verification
  client.setTimeout(10); // 10 second SSL timeout for reliability
  client.setHandshakeTimeout(8000); // 8 second handshake timeout

  // Initialize HTTP connection with proper configuration
  int retryCount = 0;
  while (!http.begin(client, url) && retryCount < 3) {
    retryCount++;
    delay(500 * retryCount); // Increasing delay
  }

  if (retryCount >= 3) {
    return "";
  }

  http.setTimeout(15000); // 15 second HTTP timeout for reliability
  http.setReuse(false); // Disable connection reuse to avoid SSL issues
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // Force connection cleanup
  http.addHeader("Connection", "close");
  http.addHeader("Cache-Control", "no-cache");

  // Set headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP32-Chess/1.0");

  // Create JSON payload
  JsonDocument doc;
  JsonObject contents = doc["contents"].add<JsonObject>();
  JsonObject parts = contents["parts"].add<JsonObject>();
  parts["text"] = prompt;

  String jsonString;
  serializeJson(doc, jsonString);

  // Free JSON document memory
  doc.clear();


  // Feed the watchdog before making the request
  // esp_task_wdt_reset();
  yield();
  delay(100);

  unsigned long requestStart = millis();

  int httpCode = -1;
  bool requestCompleted = false;

  // Aggressive task yielding HTTP POST with timeout monitoring

  // Conservative HTTP POST approach for stability

  // Conservative timeout settings
  http.setTimeout(8000); // 8 second timeout
  http.setConnectTimeout(5000); // 5 second connect timeout

  unsigned long postStartTime = millis();

  httpCode = http.POST(jsonString);

  unsigned long postDuration = millis() - postStartTime;


  unsigned long requestTime = millis() - requestStart;

  // Simple failure check with fallback move
  if (httpCode <= 0) {
    http.end();
    client.stop();
    // esp_task_wdt_reset();
    yield();
    delay(200); // Brief cleanup delay
    // Return a fallback move so the game can continue
    return "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"e5\"}]}}]}";
  }

  String response = "";

  // Feed watchdog before response processing
  // esp_task_wdt_reset();
  yield();
  delay(100);


  if (httpCode == HTTP_CODE_OK || httpCode == 200) {
    // esp_task_wdt_reset();
    yield();

    response = http.getString();

    // esp_task_wdt_reset();
    yield();

  } else if (httpCode > 0) {
    // Feed watchdog before getting error response
    // esp_task_wdt_reset();
    yield();

    String errorResponse = http.getString();

    // Feed watchdog after getting error response
    // esp_task_wdt_reset();
    yield();

  } else {
  }

  // Fast cleanup to prevent web server blocking
  http.end();
  client.stop();

  // esp_task_wdt_reset();
  yield();
  delay(100); // Brief cleanup delay

  return response;
  */
}

String GeminiAPI::extractTextFromResponse(const String& response) {
  if (response.isEmpty()) {
    return "";
  }

  // Feed watchdog before JSON parsing
  // esp_task_wdt_reset();
  yield();

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, response);

  // Feed watchdog after JSON parsing
  // esp_task_wdt_reset();
  yield();

  if (error) {
    return "";
  }

  // Extract text from response structure
  if (doc["candidates"].is<JsonArray>()) {
    JsonArray candidates = doc["candidates"];
    if (candidates.size() > 0) {
      JsonObject candidate = candidates[0];
      if (candidate["content"]["parts"].is<JsonArray>()) {
        JsonArray parts = candidate["content"]["parts"];
        if (parts.size() > 0) {
          String text = parts[0]["text"];
          return text;
        }
      }
    }
  }

  return "";
}

String GeminiAPI::formatChessPrompt(const String& position, const String& moves, const String& color) {
  String prompt = "You are playing chess as " + color + ". ";

  if (!position.isEmpty()) {
    prompt += "Current position: " + position + " ";
  }

  if (!moves.isEmpty()) {
    prompt += "Move history: " + moves + " ";
  } else {
    prompt += "This is the start of the game. ";
  }

  prompt += "Please respond with ONLY your next move in standard algebraic notation. ";
  prompt += "Examples: e4, Nf3, Bxf7+, O-O, Qh5#. ";
  prompt += "Do not include any explanation, just the move.";

  return prompt;
}

bool GeminiAPI::validateMove(const String& move) {
  if (move.length() < 2 || move.length() > 7) return false;

  // Remove whitespace
  String cleanMove = move;
  cleanMove.trim();

  // Castling
  if (cleanMove == "O-O" || cleanMove == "O-O-O" || cleanMove == "0-0" || cleanMove == "0-0-0") {
    return true;
  }

  // Basic move validation
  char firstChar = cleanMove.charAt(0);

  // Should start with piece or file
  if (!((firstChar >= 'a' && firstChar <= 'h') ||
        (firstChar == 'K' || firstChar == 'Q' || firstChar == 'R' ||
         firstChar == 'B' || firstChar == 'N'))) {
    return false;
  }

  // Should contain a valid square
  bool hasValidSquare = false;
  for (int i = 0; i < cleanMove.length() - 1; i++) {
    char file = cleanMove.charAt(i);
    char rank = cleanMove.charAt(i + 1);
    if ((file >= 'a' && file <= 'h') && (rank >= '1' && rank <= '8')) {
      hasValidSquare = true;
      break;
    }
  }

  return hasValidSquare;
}

String GeminiAPI::extractMoveFromText(const String& text) {
  if (text.isEmpty()) {
    return "";
  }

  // Split text into words
  String words[50];
  int wordCount = 0;
  int start = 0;

  String cleanText = text;
  cleanText.trim();

  for (int i = 0; i <= cleanText.length() && wordCount < 50; i++) {
    if (i == cleanText.length() || cleanText.charAt(i) == ' ' ||
        cleanText.charAt(i) == '\n' || cleanText.charAt(i) == '\t' ||
        cleanText.charAt(i) == '.' || cleanText.charAt(i) == ',') {
      if (i > start) {
        String word = cleanText.substring(start, i);
        word.trim();
        if (!word.isEmpty()) {
          words[wordCount++] = word;
        }
      }
      start = i + 1;
    }
  }

  // Find the first valid chess move
  for (int i = 0; i < wordCount; i++) {
    if (validateMove(words[i])) {
      return words[i];
    }
  }

  return "";
}

String GeminiAPI::requestMove(const String& currentPosition, const String& moveHistory, const String& color) {
  if (!isConfigured()) {
    return "";
  }

  String prompt = formatChessPrompt(currentPosition, moveHistory, color);
  String response = makeRequest(prompt);

  if (response.isEmpty()) {
    return "";
  }

  String text = extractTextFromResponse(response);
  if (text.isEmpty()) {
    return "";
  }


  return extractMoveFromText(text);
}

bool GeminiAPI::testConnection() {
  if (!isConfigured()) {
    return false;
  }

  String testPrompt = "Hello, please respond with just the word 'test'";
  String response = makeRequest(testPrompt);

  return !response.isEmpty();
}