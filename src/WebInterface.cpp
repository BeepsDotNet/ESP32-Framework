#include "WebInterface.h"
#include "GameController.h"
#include "GeminiAPI.h"
#include "SessionManager.h"
#include "config.h"
#include <SD.h>
// #include "esp_task_wdt.h"
#include "SDLogger.h"
#include "LEDControl.h"

// Global serial log event source
AsyncEventSource* g_serialLogEventSource = nullptr;

WebInterface::WebInterface() {
  server = nullptr;
  gameController = nullptr;
  geminiAPI = nullptr;
  sessionManager = nullptr;
  boardInitialized = false;
  processingMove = false;
  isWhiteTurn = true; // White starts

  // Initialize special move flags
  resetSpecialMoveFlags();

  // Initialize move history
  initializeMoveHistory();

  // Initialize chess board to starting position
  initializeBoard();
}

void WebInterface::begin(AsyncWebServer* webServer, SessionManager* sessMgr) {
  server = webServer;
  sessionManager = sessMgr;

  // Setup serial log SSE event source
  g_serialLogEventSource = new AsyncEventSource("/api/serial-stream");
  server->addHandler(g_serialLogEventSource);

  // Serve main chess app from SD card with chunked streaming
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    serveFileFromSD(request, HTML_FILE_PATH);
  });

  // Remove /app route - all content now served from SD card

  // Handle favicon.ico to prevent 500 errors
  server->on("/favicon.ico", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(204); // No content - prevents 500 error
  });

  // Chess piece images replaced with Unicode symbols - no longer serving image files

  // Serve Stockfish engine files from SD card with chunked streaming
  server->on("/stockfish.wasm.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!SD.exists("/stockfish.wasm.js")) {
      Serial.println("ERROR: stockfish.wasm.js not found on SD card");
      request->send(404, "text/plain", "stockfish.wasm.js not found");
      return;
    }

    // Use chunked streaming for this file
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/javascript",
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        static File file;
        static bool fileOpen = false;

        if (index == 0) {
          file = SD.open("/stockfish.wasm.js", FILE_READ);
          if (!file) {
            Serial.println("ERROR: Failed to open stockfish.wasm.js for streaming");
            return 0;
          }
          fileOpen = true;
          Serial.printf("Streaming stockfish.wasm.js: %d bytes\n", file.size());
        }

        if (!fileOpen || !file) {
          return 0;
        }

        size_t bytesRead = file.read(buffer, maxLen);
        if (bytesRead == 0 || !file.available()) {
          file.close();
          fileOpen = false;
          Serial.println("stockfish.wasm.js streaming complete");
        }

        return bytesRead;
      });

    response->addHeader("Cache-Control", "max-age=86400");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  server->on("/stockfish.wasm", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!SD.exists("/stockfish.wasm")) {
      Serial.println("ERROR: stockfish.wasm not found on SD card");
      request->send(404, "text/plain", "stockfish.wasm not found");
      return;
    }

    // Use chunked streaming for this large binary file
    AsyncWebServerResponse *response = request->beginChunkedResponse("application/wasm",
      [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        static File file;
        static bool fileOpen = false;

        if (index == 0) {
          file = SD.open("/stockfish.wasm", FILE_READ);
          if (!file) {
            Serial.println("ERROR: Failed to open stockfish.wasm for streaming");
            return 0;
          }
          fileOpen = true;
          Serial.printf("Streaming stockfish.wasm: %d bytes\n", file.size());
        }

        if (!fileOpen || !file) {
          return 0;
        }

        size_t bytesRead = file.read(buffer, maxLen);
        if (bytesRead == 0 || !file.available()) {
          file.close();
          fileOpen = false;
          Serial.println("stockfish.wasm streaming complete");
        }

        return bytesRead;
      });

    response->addHeader("Cache-Control", "max-age=86400");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  // API endpoints
  server->on("/api/board", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetBoard(request);
  });

  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetStatus(request);
  });

  server->on("/api/newgame", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleNewGame(request);
  });

  server->on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleResetGame(request);
  });

  server->on("/api/move", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleUserMove(request);
  });

  server->on("/api/undo", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleUndoMove(request);
  });

  server->on("/api/redo", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleRedoMove(request);
  });

  server->on("/api/request-ai-move", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleRequestAIMove(request);
  });

  server->on("/api/debug-button-color", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleDebugButtonColor(request);
  });

  // Log endpoint for browser console messages
  server->on("/api/log", HTTP_POST, [this](AsyncWebServerRequest* request) {}, NULL,
    [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      handleLogMessage(request, data, len, index, total);
    });

  // Client IP endpoint
  server->on("/api/client-ip", HTTP_GET, [](AsyncWebServerRequest* request) {
    String clientIP = request->client()->remoteIP().toString();
    request->send(200, "text/plain", clientIP);
  });

  // Log file management endpoints
  server->on("/api/logs/clear", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleClearLogs(request);
  });

  server->on("/api/logs/console", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetConsoleLog(request);
  });

  server->on("/api/logs/serial", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetSerialLog(request);
  });

  server->on("/api/logs/debug", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetDebugLog(request);
  });

  // Session Control endpoints
  server->on("/api/session/sd-write-status", HTTP_GET, [](AsyncWebServerRequest* request) {
    bool enabled = sdLogger ? sdLogger->getSDWriteEnabled() : true;
    String json = "{\"enabled\":" + String(enabled ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  server->on("/api/session/sd-write-toggle", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (sdLogger) {
      bool currentState = sdLogger->getSDWriteEnabled();
      sdLogger->setSDWriteEnabled(!currentState);
      String json = "{\"success\":true,\"enabled\":" + String(!currentState ? "true" : "false") + "}";
      request->send(200, "application/json", json);
    } else {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"SD logger not initialized\"}");
    }
  });

  server->on("/api/session/clear-all-logs", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (sdLogger) {
      sdLogger->clearAllLogs();
      request->send(200, "application/json", "{\"success\":true,\"message\":\"All logs cleared\"}");
    } else {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"SD logger not initialized\"}");
    }
  });

  // Get list of active sessions
  server->on("/api/session/list", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (sessionManager) {
      String json = sessionManager->getSessionsJSON();
      request->send(200, "application/json", json);
    } else {
      request->send(500, "application/json", "{\"error\":\"Session manager not initialized\"}");
    }
  });

  // Toggle debug logging for a specific session
  server->on("/api/session/toggle-logging", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!sessionManager) {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"Session manager not initialized\"}");
      return;
    }

    if (!request->hasParam("sessionId", true)) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing sessionId parameter\"}");
      return;
    }

    String sessionId = request->getParam("sessionId", true)->value();
    Session* session = sessionManager->getSession(sessionId);

    if (!session) {
      request->send(404, "application/json", "{\"success\":false,\"error\":\"Session not found\"}");
      return;
    }

    bool newState = !session->debugLogEnabled;
    sessionManager->setDebugLogEnabled(sessionId, newState);

    String json = "{\"success\":true,\"sessionId\":\"" + sessionId + "\",\"debugLogEnabled\":" + String(newState ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  // Get debug log state for a session
  server->on("/api/session/debug-state", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!sessionManager) {
      request->send(500, "application/json", "{\"success\":false,\"error\":\"Session manager not initialized\"}");
      return;
    }

    if (!request->hasParam("sessionId")) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing sessionId parameter\"}");
      return;
    }

    String sessionId = request->getParam("sessionId")->value();
    Session* session = sessionManager->getSession(sessionId);

    if (!session) {
      request->send(404, "application/json", "{\"success\":false,\"error\":\"Session not found\"}");
      return;
    }

    // Get current pendingRefresh state
    bool hadPendingRefresh = session->pendingRefresh;

    // Clear the flag immediately after reading it to prevent refresh loops
    if (hadPendingRefresh) {
      session->pendingRefresh = false;
    }

    String json = "{\"success\":true,\"sessionId\":\"" + sessionId + "\",\"debugLogEnabled\":" + String(session->debugLogEnabled ? "true" : "false") + ",\"pendingRefresh\":" + String(hadPendingRefresh ? "true" : "false") + "}";
    request->send(200, "application/json", json);
  });

  // Send refresh command to a specific session
  server->on("/api/session/send-refresh", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!sessionManager) {
      Serial.println("ERROR: send-refresh called but session manager not initialized");
      request->send(500, "application/json", "{\"success\":false,\"error\":\"Session manager not initialized\"}");
      return;
    }

    if (!request->hasParam("sessionId", true)) {
      Serial.println("ERROR: send-refresh called without sessionId parameter");
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing sessionId parameter\"}");
      return;
    }

    String sessionId = request->getParam("sessionId", true)->value();

    // Validate session ID is not empty
    if (sessionId.length() == 0) {
      Serial.println("ERROR: send-refresh called with empty sessionId");
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid sessionId (empty)\"}");
      return;
    }

    Session* session = sessionManager->getSession(sessionId);

    if (!session) {
      Serial.printf("ERROR: send-refresh - session not found: %s\n", sessionId.c_str());
      request->send(404, "application/json", "{\"success\":false,\"error\":\"Session not found\"}");
      return;
    }

    // Verify session pointer is valid before modifying
    if (session->sessionId.isEmpty() || session->sessionId != sessionId) {
      Serial.printf("ERROR: Session pointer validation failed (expected: %s, got: %s)\n",
                   sessionId.c_str(), session->sessionId.c_str());
      request->send(500, "application/json", "{\"success\":false,\"error\":\"Session validation failed\"}");
      return;
    }

    // Safety check: prevent refreshing yourself (could cause issues)
    String requestIP = request->client()->remoteIP().toString();
    if (session->ipAddress == requestIP) {
      Serial.printf("WARNING: Admin tried to refresh their own session %s (IP: %s) - ignoring\n",
                   sessionId.c_str(), requestIP.c_str());
      request->send(400, "application/json", "{\"success\":false,\"error\":\"Cannot refresh your own session\"}");
      return;
    }

    // Set pending refresh flag (safe to modify now)
    session->pendingRefresh = true;

    Serial.printf("Refresh command flagged for session %s (IP: %s)\n",
                 sessionId.c_str(), session->ipAddress.c_str());

    String json = "{\"success\":true,\"sessionId\":\"" + sessionId + "\"}";
    request->send(200, "application/json", json);
  });

  // Crash log endpoints
  server->on("/CrashLog.txt", HTTP_GET, [this](AsyncWebServerRequest* request) {
    const char* filename = "/CrashLog.txt";

    if (!SD.exists(filename)) {
      request->send(404, "text/plain", "Crash log not found");
      return;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "Failed to open crash log");
      return;
    }

    size_t fileSize = file.size();
    file.close();

    Serial.printf("CRASH LOG: Serving crash log (%d bytes) using chunked streaming\n", fileSize);

    // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
      [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        return streamFileChunk(buffer, maxLen, index, filename);
      });

    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // List all crash logs
  server->on("/api/crashlogs", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "[";
    File root = SD.open("/");
    bool first = true;

    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;

      String fileName = String(entry.name());
      if (fileName.startsWith("CrashLog_") && fileName.endsWith(".txt")) {
        if (!first) json += ",";
        json += "{\"name\":\"" + fileName + "\",\"size\":" + String(entry.size()) + "}";
        first = false;
      }
      entry.close();
    }
    root.close();

    json += "]";
    request->send(200, "application/json", json);
  });

  // Serve individual timestamped crash logs
  server->on("/api/crashlog", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing 'file' parameter");
      return;
    }

    String fileNameStr = "/" + request->getParam("file")->value();
    const char* filename = fileNameStr.c_str();

    if (!SD.exists(filename)) {
      request->send(404, "text/plain", "Crash log not found: " + fileNameStr);
      return;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "Failed to open crash log: " + fileNameStr);
      return;
    }

    size_t fileSize = file.size();
    file.close();

    Serial.printf("CRASH LOG: Serving crash log %s (%d bytes) using chunked streaming\n", filename, fileSize);

    // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
    // Need to capture fileNameStr by value in the lambda
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
      [this, fileNameStr](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        return streamFileChunk(buffer, maxLen, index, fileNameStr.c_str());
      });

    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
  });

  // Eject SD card endpoint
  server->on("/api/eject", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleEject(request);
  });

  // Reboot endpoint
  server->on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleReboot(request);
  });

  // File read/write endpoints
  server->on("/api/file/read", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleFileRead(request);
  });

  server->on("/api/file/write", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleFileWrite(request);
  });

#if DEVELOPMENT_MODE
  // Development mode: Chunked HTML upload endpoints
  server->on("/api/upload-start", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleUploadStart(request);
  });
  server->on("/api/upload-chunk", HTTP_POST, [this](AsyncWebServerRequest* request) {
    // Response will be sent after body is received
  }, NULL,
  [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    handleUploadChunkBody(request, data, len, index, total);
  });
  server->on("/api/upload-finish", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleUploadFinish(request);
  });
  // Legacy single-file upload for smaller files
  server->on("/api/upload-html", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleHTMLUpload(request);
  });

  // File descriptor cleanup endpoint
  server->on("/api/cleanup-files", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleFileCleanup(request);
  });
#endif

}

void WebInterface::initializeBoard() {
  // Set up starting chess position
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      currentBoard[row][col] = "";
    }
  }

  // Black pieces (top of board)
  currentBoard[0][0] = "br"; currentBoard[0][1] = "bn"; currentBoard[0][2] = "bb"; currentBoard[0][3] = "bq";
  currentBoard[0][4] = "bk"; currentBoard[0][5] = "bb"; currentBoard[0][6] = "bn"; currentBoard[0][7] = "br";
  for (int col = 0; col < 8; col++) {
    currentBoard[1][col] = "bp";
  }

  // White pieces (bottom of board)
  for (int col = 0; col < 8; col++) {
    currentBoard[6][col] = "wp";
  }
  currentBoard[7][0] = "wr"; currentBoard[7][1] = "wn"; currentBoard[7][2] = "wb"; currentBoard[7][3] = "wq";
  currentBoard[7][4] = "wk"; currentBoard[7][5] = "wb"; currentBoard[7][6] = "wn"; currentBoard[7][7] = "wr";

  boardInitialized = true;
  isWhiteTurn = true; // White always starts
  resetSpecialMoveFlags();
}

void WebInterface::serveImageFile(AsyncWebServerRequest* request, const char* filename) {
  if (!SD.exists(filename)) {
    request->send(404, "text/plain", "Image not found");
    return;
  }

  // Add caching headers to reduce server load
  AsyncWebServerResponse *response = request->beginResponse(SD, filename, "image/png");
  response->addHeader("Cache-Control", "public, max-age=86400"); // Cache for 24 hours
  response->addHeader("Connection", "close"); // Close connection to free resources
  request->send(response);
}

// REMOVED: serveStreamedHTML function - no longer needed with SD card serving

// REMOVED: generateHTMLChunk function and htmlSections array - no longer needed
// REMOVED: generateHTMLChunk function and htmlSections array - no longer needed
size_t WebInterface::generateHTMLChunk(uint8_t *buffer, size_t maxLen, size_t index) {
  // Function removed - return empty to end chunked response
  return 0;
}


// REMOVED: All remaining embedded HTML/CSS content

String WebInterface::generateBoardJSON() {
  String json = "{\"board\":[";

  for (int row = 0; row < 8; row++) {
    if (row > 0) json += ",";
    json += "[";
    for (int col = 0; col < 8; col++) {
      if (col > 0) json += ",";
      json += "\"" + currentBoard[row][col] + "\"";
    }
    json += "]";
  }

  json += "]}";
  return json;
}

String WebInterface::generateChessBoard() {
  String html = "";

  for (int row = 0; row < 8; row++) {
    html += "<div class=\"row\">";
    for (int col = 0; col < 8; col++) {
      bool isLight = (row + col) % 2 == 0;
      String piece = currentBoard[row][col];
      html += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";
      if (piece.length() > 0) {
        html += "<div class=\"piece\">" + piece + "</div>";
      }
      html += "</div>";
    }
    html += "</div>";
  }

  return html;
}

String WebInterface::getPieceImageName(String pieceCode) {
  if (pieceCode == "wp") return "White-Pawn";
  if (pieceCode == "wr") return "White-Rook";
  if (pieceCode == "wn") return "White-Knight";
  if (pieceCode == "wb") return "White-Bishop";
  if (pieceCode == "wq") return "White-Queen";
  if (pieceCode == "wk") return "White-King";
  if (pieceCode == "bp") return "Black-Pawn";
  if (pieceCode == "br") return "Black-Rook";
  if (pieceCode == "bn") return "Black-Knight";
  if (pieceCode == "bb") return "Black-Bishop";
  if (pieceCode == "bq") return "Black-Queen";
  if (pieceCode == "bk") return "Black-King";
  return "";
}

String WebInterface::getMinimalFallbackHTML() {
  return F(R"HTML(<!DOCTYPE html>
<html><head><title>Chess - SD Card Error</title></head>
<body style="font-family:Arial;padding:20px;text-align:center;background:#1a1a1a;color:white;">
<h1>Chess Application</h1>
<p>SD Card chess-app.html not found.</p>
<p>Please upload the chess application file.</p>
<button onclick="window.location.reload()">Refresh</button>
</body></html>)HTML");
}

// Board state management functions

void WebInterface::handleGetBoard(AsyncWebServerRequest* request) {
  String boardJson = generateBoardJSON();
  AsyncWebServerResponse *response = request->beginResponse(200, "application/json", boardJson);
  response->addHeader("Connection", "close");
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

void WebInterface::handleGetStatus(AsyncWebServerRequest* request) {
  String status = "{";
  status += "\"currentPlayer\":\"" + String(isWhiteTurn ? "White" : "Black") + "\",";
  status += "\"gameActive\":true,";
  status += "\"moveCount\":0,";
  status += "\"status\":\"ready\",";

  // Check if either king is in check
  bool whiteInCheck = isKingInCheck(true);
  bool blackInCheck = isKingInCheck(false);

  status += "\"whiteInCheck\":" + String(whiteInCheck ? "true" : "false") + ",";
  status += "\"blackInCheck\":" + String(blackInCheck ? "true" : "false") + ",";

  // Check for checkmate/stalemate
  bool whiteCheckmate = false;
  bool blackCheckmate = false;
  bool stalemate = false;

  if (!hasLegalMoves(true)) {
    if (whiteInCheck) {
      whiteCheckmate = true;
    } else {
      stalemate = true;
    }
  }

  if (!hasLegalMoves(false)) {
    if (blackInCheck) {
      blackCheckmate = true;
    } else {
      stalemate = true;
    }
  }

  status += "\"whiteCheckmate\":" + String(whiteCheckmate ? "true" : "false") + ",";
  status += "\"blackCheckmate\":" + String(blackCheckmate ? "true" : "false") + ",";
  status += "\"stalemate\":" + String(stalemate ? "true" : "false") + ",";
  status += "\"checkMessage\":\"\"";

  status += "}";

  request->send(200, "application/json", status);
}

void WebInterface::handleNewGame(AsyncWebServerRequest* request) {
  initializeBoard();
  request->send(200, "application/json", "{\"status\":\"new_game_started\"}");
}

void WebInterface::handleResetGame(AsyncWebServerRequest* request) {
  initializeBoard();
  request->send(200, "application/json", "{\"status\":\"game_reset\"}");
}

void WebInterface::handleUserMove(AsyncWebServerRequest* request) {

  // Check for move parameter
  if (request->hasParam("move", true)) {
    String move = request->getParam("move", true)->value();

    // Apply the move to the board
    if (applyMoveToBoard(move)) {

      // Switch turns
      isWhiteTurn = !isWhiteTurn;

      request->send(200, "application/json", "{\"status\":\"move_accepted\",\"move\":\"" + move + "\"}");

      // AI move will be triggered by client after animation delay via /api/request-ai-move
    } else {
      request->send(400, "application/json", "{\"status\":\"invalid_move\",\"move\":\"" + move + "\"}");
    }
  } else {
    request->send(400, "application/json", "{\"status\":\"missing_move_parameter\"}");
  }
}

void WebInterface::handleRequestAIMove(AsyncWebServerRequest* request) {

  // Only process AI move if it's AI's turn (black)
  if (isWhiteTurn) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Not AI turn\"}");
    return;
  }

  // Trigger AI move
  triggerAIMove();

  // Send success response
  request->send(200, "application/json", "{\"status\":\"ai_move_triggered\"}");
}


// REMOVED: Duplicate function definitions

// REMOVED: Duplicate getPieceImageName function

// REMOVED: Duplicate initializeBoard function

void WebInterface::setGameController(GameController* controller) {
  gameController = controller;
}

void WebInterface::setGeminiAPI(GeminiAPI* api) {
  geminiAPI = api;
}

// REMOVED: Floating HTML code

// REMOVED: getIndexHTML() function - all content now served from SD card

// REMOVED: getIndexHTML() function - all content now served from SD card


void WebInterface::applyMove(const String& move, bool isWhite) {
  // For v2.0 skeleton, just log the move - actual chess logic will be implemented later
}

bool WebInterface::applyMoveToBoard(const String& move) {
  // Parse coordinate move like "e2e4" or "6,4,4,4"
  int fromRow, fromCol, toRow, toCol;

  if (!parseMove(move, fromRow, fromCol, toRow, toCol)) {
    return false;
  }

  // Check if there's a piece at the from position
  if (currentBoard[fromRow][fromCol] == "") {
    return false;
  }

  // Validate the move according to chess rules
  if (!isValidMove(fromRow, fromCol, toRow, toCol)) {
    return false;
  }

  // Check if this move would leave the king in check
  if (wouldMoveLeaveKingInCheck(fromRow, fromCol, toRow, toCol)) {
    return false;
  }

  // Get piece info before moving
  String piece = currentBoard[fromRow][fromCol];
  bool isWhite = isPieceWhite(piece);

  // Save board state before white move for undo functionality
  if (isWhite) {
    saveCurrentBoardState();
  }

  // If the current player's king is in check, they MUST get out of check
  if (isKingInCheck(isWhite)) {
    // Player is in check - they can only make moves that get them out of check
    // We already validated above that this move doesn't leave king in check
    // So if king is currently in check and this move doesn't leave king in check,
    // then this move successfully gets out of check - allow it
  }

  //            " to " + String(toRow) + "," + String(toCol));

  // Handle special moves
  char pieceType = getPieceType(piece);

  if (pieceType == 'k' && isCastlingMove(fromRow, fromCol, toRow, toCol)) {
    // Castling
    bool kingside = toCol > fromCol;
    performCastle(isWhite, kingside);
  } else if (pieceType == 'p' && isEnPassantCapture(fromRow, fromCol, toRow, toCol)) {
    // En passant
    performEnPassant(fromRow, fromCol, toRow, toCol);
  } else if (pieceType == 'p' && isPawnPromotion(fromRow, fromCol, toRow, toCol)) {
    // Pawn promotion (default to Queen for now - could be enhanced for user choice)
    currentBoard[toRow][toCol] = piece;
    currentBoard[fromRow][fromCol] = "";
    promotePawn(toRow, toCol, 'q', isWhite);
  } else {
    // Normal move
    currentBoard[toRow][toCol] = piece;
    currentBoard[fromRow][fromCol] = "";
  }

  // Update special move tracking
  updateSpecialMoveTracking(fromRow, fromCol, toRow, toCol);

  // Save after-move state for redo functionality (only for white moves)
  if (isWhite) {
    saveAfterMoveState();
  }

  // Check if this move puts the opponent in check
  bool opponentInCheck = isKingInCheck(!isWhite);

  if (opponentInCheck) {
    String checkMsg = !isWhite ? "WHITE KING IN CHECK!" : "BLACK KING IN CHECK!";
  }

  return true;
}

bool WebInterface::parseMove(const String& move, int& fromRow, int& fromCol, int& toRow, int& toCol) {
  // Handle coordinate notation like "6,4,4,4" (fromRow,fromCol,toRow,toCol)
  if (move.indexOf(',') >= 0) {
    int commaPos1 = move.indexOf(',');
    int commaPos2 = move.indexOf(',', commaPos1 + 1);
    int commaPos3 = move.indexOf(',', commaPos2 + 1);

    if (commaPos1 >= 0 && commaPos2 >= 0 && commaPos3 >= 0) {
      fromRow = move.substring(0, commaPos1).toInt();
      fromCol = move.substring(commaPos1 + 1, commaPos2).toInt();
      toRow = move.substring(commaPos2 + 1, commaPos3).toInt();
      toCol = move.substring(commaPos3 + 1).toInt();
      return true;
    }
  }

  // Handle castling moves
  if (move == "O-O" || move == "0-0") {
    // Kingside castling - find the king and move 2 squares right
    bool isWhite = isWhiteTurn;
    int kingRow = isWhite ? 7 : 0;
    fromRow = kingRow;
    fromCol = 4;
    toRow = kingRow;
    toCol = 6;
    return true;
  }
  if (move == "O-O-O" || move == "0-0-0") {
    // Queenside castling - find the king and move 2 squares left
    bool isWhite = isWhiteTurn;
    int kingRow = isWhite ? 7 : 0;
    fromRow = kingRow;
    fromCol = 4;
    toRow = kingRow;
    toCol = 2;
    return true;
  }

  // Handle full algebraic notation like "e2e4"
  if (move.length() == 4 && move.charAt(0) >= 'a' && move.charAt(0) <= 'h' &&
      move.charAt(1) >= '1' && move.charAt(1) <= '8') {
    char fromFile = move.charAt(0);
    char fromRank = move.charAt(1);
    char toFile = move.charAt(2);
    char toRank = move.charAt(3);

    // Convert to board coordinates (a1 = 7,0)
    fromCol = fromFile - 'a';
    fromRow = 8 - (fromRank - '0');
    toCol = toFile - 'a';
    toRow = 8 - (toRank - '0');

    // Validate coordinates
    if (fromRow >= 0 && fromRow < 8 && fromCol >= 0 && fromCol < 8 &&
        toRow >= 0 && toRow < 8 && toCol >= 0 && toCol < 8) {
      return true;
    }
  }

  // Handle standard algebraic notation like "d5", "Nc6", "Bxf7+"
  String cleanMove = move;
  cleanMove.trim();

  // Remove check/checkmate indicators
  if (cleanMove.endsWith("+") || cleanMove.endsWith("#")) {
    cleanMove = cleanMove.substring(0, cleanMove.length() - 1);
  }

  // Extract destination square (always the last 2 characters)
  if (cleanMove.length() >= 2) {
    String destSquare = cleanMove.substring(cleanMove.length() - 2);
    if (destSquare.charAt(0) >= 'a' && destSquare.charAt(0) <= 'h' &&
        destSquare.charAt(1) >= '1' && destSquare.charAt(1) <= '8') {

      toCol = destSquare.charAt(0) - 'a';
      toRow = 8 - (destSquare.charAt(1) - '0');

      // For simple pawn moves like "d5", "e6"
      if (cleanMove.length() == 2) {
        // Look for a pawn that can move to this square
        for (int r = 0; r < 8; r++) {
          for (int c = 0; c < 8; c++) {
            String piece = currentBoard[r][c];
            if (!piece.isEmpty() && isPieceWhite(piece) == isWhiteTurn && getPieceType(piece) == 'p') {
              if (isPawnMoveValid(r, c, toRow, toCol, isPieceWhite(piece))) {
                fromRow = r;
                fromCol = c;
                return true;
              }
            }
          }
        }
      }

      // For piece moves like "Nc6", "Bb5"
      if (cleanMove.length() >= 3) {
        char pieceType = tolower(cleanMove.charAt(0));
        if (pieceType == 'n' || pieceType == 'b' || pieceType == 'r' ||
            pieceType == 'q' || pieceType == 'k') {

          // Look for the piece that can move to this square
          for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
              String piece = currentBoard[r][c];
              if (!piece.isEmpty() && isPieceWhite(piece) == isWhiteTurn &&
                  tolower(getPieceType(piece)) == pieceType) {
                if (isValidMove(r, c, toRow, toCol)) {
                  fromRow = r;
                  fromCol = c;
                  return true;
                }
              }
            }
          }
        }
      }
    }
  }

  return false;
}

// Chess rule validation helper functions

bool WebInterface::isValidMove(int fromRow, int fromCol, int toRow, int toCol) {
  // Check bounds
  if (fromRow < 0 || fromRow >= 8 || fromCol < 0 || fromCol >= 8 ||
      toRow < 0 || toRow >= 8 || toCol < 0 || toCol >= 8) {
    return false;
  }

  // Can't move to same position
  if (fromRow == toRow && fromCol == toCol) {
    return false;
  }

  String movingPiece = currentBoard[fromRow][fromCol];
  String targetPiece = currentBoard[toRow][toCol];

  // Can't capture own piece
  if (!targetPiece.isEmpty() && isPieceWhite(movingPiece) == isPieceWhite(targetPiece)) {
    return false;
  }

  char pieceType = getPieceType(movingPiece);
  bool isWhite = isPieceWhite(movingPiece);

  // Check for special moves first
  if (pieceType == 'k' && isCastlingMove(fromRow, fromCol, toRow, toCol)) {
    bool kingside = toCol > fromCol;
    return canCastle(isWhite, kingside);
  }

  if (pieceType == 'p' && isEnPassantCapture(fromRow, fromCol, toRow, toCol)) {
    return true;
  }

  // Check piece-specific movement rules
  switch (pieceType) {
    case 'p': return isPawnMoveValid(fromRow, fromCol, toRow, toCol, isWhite);
    case 'r': return isRookMoveValid(fromRow, fromCol, toRow, toCol);
    case 'b': return isBishopMoveValid(fromRow, fromCol, toRow, toCol);
    case 'n': return isKnightMoveValid(fromRow, fromCol, toRow, toCol);
    case 'q': return isQueenMoveValid(fromRow, fromCol, toRow, toCol);
    case 'k': return isKingMoveValid(fromRow, fromCol, toRow, toCol);
    default: return false;
  }
}

bool WebInterface::isPawnMoveValid(int fromRow, int fromCol, int toRow, int toCol, bool isWhite) {
  int direction = isWhite ? -1 : 1; // White moves up (decreasing row), black moves down
  int startRow = isWhite ? 6 : 1;   // Starting row for pawns

  // Forward movement
  if (fromCol == toCol) {
    // Must be empty square ahead
    if (!currentBoard[toRow][toCol].isEmpty()) {
      return false;
    }

    // One square forward
    if (toRow == fromRow + direction) {
      return true;
    }

    // Two squares forward from starting position
    if (fromRow == startRow && toRow == fromRow + 2 * direction) {
      return true;
    }

    return false;
  }

  // Diagonal capture
  if (abs(fromCol - toCol) == 1 && toRow == fromRow + direction) {
    // Must be an enemy piece to capture
    return !currentBoard[toRow][toCol].isEmpty() &&
           isPieceWhite(currentBoard[toRow][toCol]) != isWhite;
  }

  return false;
}

bool WebInterface::isRookMoveValid(int fromRow, int fromCol, int toRow, int toCol) {
  // Rook moves horizontally or vertically
  if (fromRow != toRow && fromCol != toCol) {
    return false;
  }

  // Check if path is clear
  return isPathClear(fromRow, fromCol, toRow, toCol);
}

bool WebInterface::isBishopMoveValid(int fromRow, int fromCol, int toRow, int toCol) {
  // Bishop moves diagonally
  if (abs(fromRow - toRow) != abs(fromCol - toCol)) {
    return false;
  }

  // Check if path is clear
  return isPathClear(fromRow, fromCol, toRow, toCol);
}

bool WebInterface::isKnightMoveValid(int fromRow, int fromCol, int toRow, int toCol) {
  int rowDiff = abs(fromRow - toRow);
  int colDiff = abs(fromCol - toCol);

  // Knight moves in L-shape: 2+1 or 1+2
  return (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);
}

bool WebInterface::isQueenMoveValid(int fromRow, int fromCol, int toRow, int toCol) {
  // Queen combines rook and bishop movement
  return isRookMoveValid(fromRow, fromCol, toRow, toCol) ||
         isBishopMoveValid(fromRow, fromCol, toRow, toCol);
}

bool WebInterface::isKingMoveValid(int fromRow, int fromCol, int toRow, int toCol) {
  int rowDiff = abs(fromRow - toRow);
  int colDiff = abs(fromCol - toCol);

  // King moves one square in any direction
  return rowDiff <= 1 && colDiff <= 1;
}

bool WebInterface::isPathClear(int fromRow, int fromCol, int toRow, int toCol) {
  int rowStep = (toRow > fromRow) ? 1 : (toRow < fromRow) ? -1 : 0;
  int colStep = (toCol > fromCol) ? 1 : (toCol < fromCol) ? -1 : 0;

  int checkRow = fromRow + rowStep;
  int checkCol = fromCol + colStep;

  // Check each square along the path (excluding destination)
  while (checkRow != toRow || checkCol != toCol) {
    if (!currentBoard[checkRow][checkCol].isEmpty()) {
      return false; // Path is blocked
    }
    checkRow += rowStep;
    checkCol += colStep;
  }

  return true;
}

bool WebInterface::isPieceWhite(const String& piece) {
  if (piece.isEmpty()) return false;
  return piece.charAt(0) == 'w';
}

char WebInterface::getPieceType(const String& piece) {
  if (piece.isEmpty()) return ' ';
  return piece.charAt(1);
}

// Check detection functions

bool WebInterface::hasLegalMoves(bool isWhite) {
  // Check if the player has any legal moves
  for (int fromR = 0; fromR < 8; fromR++) {
    for (int fromC = 0; fromC < 8; fromC++) {
      String piece = currentBoard[fromR][fromC];
      if (piece.isEmpty() || isPieceWhite(piece) != isWhite) {
        continue; // Not our piece
      }

      // Check all possible destination squares
      for (int toR = 0; toR < 8; toR++) {
        for (int toC = 0; toC < 8; toC++) {
          if (isValidMove(fromR, fromC, toR, toC) &&
              !wouldMoveLeaveKingInCheck(fromR, fromC, toR, toC)) {
            return true; // Found a legal move
          }
        }
      }
    }
  }
  return false; // No legal moves found
}

bool WebInterface::isKingInCheck(bool isWhiteKing) {
  int kingRow, kingCol;
  findKing(isWhiteKing, kingRow, kingCol);

  if (kingRow == -1 || kingCol == -1) {
    return false; // King not found (shouldn't happen)
  }

  return isSquareAttackedBy(kingRow, kingCol, !isWhiteKing);
}

bool WebInterface::wouldMoveLeaveKingInCheck(int fromRow, int fromCol, int toRow, int toCol) {
  // Make a temporary move
  String movingPiece = currentBoard[fromRow][fromCol];
  String capturedPiece = currentBoard[toRow][toCol];
  bool isWhite = isPieceWhite(movingPiece);

  // Apply the move temporarily
  currentBoard[toRow][toCol] = movingPiece;
  currentBoard[fromRow][fromCol] = "";

  // Check if our king would be in check after this move
  bool wouldBeInCheck = isKingInCheck(isWhite);

  // Restore the board
  currentBoard[fromRow][fromCol] = movingPiece;
  currentBoard[toRow][toCol] = capturedPiece;

  return wouldBeInCheck;
}

bool WebInterface::isSquareAttackedBy(int row, int col, bool byWhite) {
  // Check all squares for enemy pieces that can attack this square
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      String piece = currentBoard[r][c];
      if (!piece.isEmpty() && isPieceWhite(piece) == byWhite) {
        if (canPieceAttackSquare(r, c, row, col)) {
          return true;
        }
      }
    }
  }
  return false;
}

void WebInterface::findKing(bool isWhite, int& kingRow, int& kingCol) {
  String targetKing = isWhite ? "wk" : "bk";

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (currentBoard[r][c] == targetKing) {
        kingRow = r;
        kingCol = c;
        return;
      }
    }
  }

  // King not found
  kingRow = -1;
  kingCol = -1;
}

bool WebInterface::canPieceAttackSquare(int pieceRow, int pieceCol, int targetRow, int targetCol) {
  String piece = currentBoard[pieceRow][pieceCol];
  if (piece.isEmpty()) return false;

  char pieceType = getPieceType(piece);
  bool isWhite = isPieceWhite(piece);

  // Check if piece can attack the target square based on its movement rules
  switch (pieceType) {
    case 'p': // Pawn attacks diagonally
      {
        int direction = isWhite ? -1 : 1;
        return (targetRow == pieceRow + direction) &&
               (abs(targetCol - pieceCol) == 1);
      }
    case 'r': // Rook
      return isRookMoveValid(pieceRow, pieceCol, targetRow, targetCol);
    case 'b': // Bishop
      return isBishopMoveValid(pieceRow, pieceCol, targetRow, targetCol);
    case 'n': // Knight
      return isKnightMoveValid(pieceRow, pieceCol, targetRow, targetCol);
    case 'q': // Queen
      return isQueenMoveValid(pieceRow, pieceCol, targetRow, targetCol);
    case 'k': // King
      return isKingMoveValid(pieceRow, pieceCol, targetRow, targetCol);
    default:
      return false;
  }
}

// Special move implementations

void WebInterface::resetSpecialMoveFlags() {
  whiteKingMoved = false;
  blackKingMoved = false;
  whiteKingsideRookMoved = false;
  whiteQueensideRookMoved = false;
  blackKingsideRookMoved = false;
  blackQueensideRookMoved = false;
  enPassantColumn = -1;
  enPassantIsWhite = false;
}

bool WebInterface::isCastlingMove(int fromRow, int fromCol, int toRow, int toCol) {
  // Check if it's a king move of 2 squares horizontally
  if (fromRow != toRow || abs(fromCol - toCol) != 2) {
    return false;
  }

  String piece = currentBoard[fromRow][fromCol];
  char pieceType = getPieceType(piece);

  return pieceType == 'k';
}

bool WebInterface::canCastle(bool isWhite, bool kingside) {
  int kingRow = isWhite ? 7 : 0;
  int rookCol = kingside ? 7 : 0;

  // Check if king or rook has moved
  if (isWhite) {
    if (whiteKingMoved || (kingside ? whiteKingsideRookMoved : whiteQueensideRookMoved)) {
      return false;
    }
  } else {
    if (blackKingMoved || (kingside ? blackKingsideRookMoved : blackQueensideRookMoved)) {
      return false;
    }
  }

  // Check if king is in check
  if (isKingInCheck(isWhite)) {
    return false;
  }

  // Check if squares between king and rook are empty
  int startCol = kingside ? 5 : 1;
  int endCol = kingside ? 6 : 3;

  for (int col = startCol; col <= endCol; col++) {
    if (!currentBoard[kingRow][col].isEmpty()) {
      return false;
    }
  }

  // Check if king would pass through or land on attacked squares
  int kingDestCol = kingside ? 6 : 2;
  if (isSquareAttackedBy(kingRow, 5, !isWhite) ||
      isSquareAttackedBy(kingRow, kingDestCol, !isWhite)) {
    return false;
  }

  return true;
}

void WebInterface::performCastle(bool isWhite, bool kingside) {
  int row = isWhite ? 7 : 0;
  int rookFromCol = kingside ? 7 : 0;
  int rookToCol = kingside ? 5 : 3;
  int kingToCol = kingside ? 6 : 2;

  String king = currentBoard[row][4];
  String rook = currentBoard[row][rookFromCol];

  // Move pieces
  currentBoard[row][kingToCol] = king;
  currentBoard[row][rookToCol] = rook;
  currentBoard[row][4] = "";
  currentBoard[row][rookFromCol] = "";

  //             " " + String(kingside ? "kingside" : "queenside"));
}

bool WebInterface::isEnPassantCapture(int fromRow, int fromCol, int toRow, int toCol) {
  String piece = currentBoard[fromRow][fromCol];
  char pieceType = getPieceType(piece);
  bool isWhite = isPieceWhite(piece);

  // Must be a pawn
  if (pieceType != 'p') {
    return false;
  }

  // Must be diagonal move
  if (abs(fromCol - toCol) != 1 || abs(fromRow - toRow) != 1) {
    return false;
  }

  // Destination must be empty
  if (!currentBoard[toRow][toCol].isEmpty()) {
    return false;
  }

  // Must be capturing en passant pawn
  if (enPassantColumn == toCol && enPassantIsWhite != isWhite) {
    int capturedPawnRow = isWhite ? 3 : 4;
    if (fromRow == capturedPawnRow && toRow == (isWhite ? 2 : 5)) {
      return true;
    }
  }

  return false;
}

void WebInterface::performEnPassant(int fromRow, int fromCol, int toRow, int toCol) {
  String piece = currentBoard[fromRow][fromCol];
  bool isWhite = isPieceWhite(piece);
  int capturedPawnRow = isWhite ? 3 : 4;

  // Move pawn
  currentBoard[toRow][toCol] = piece;
  currentBoard[fromRow][fromCol] = "";

  // Remove captured pawn
  currentBoard[capturedPawnRow][toCol] = "";

}

bool WebInterface::isPawnPromotion(int fromRow, int fromCol, int toRow, int toCol) {
  String piece = currentBoard[fromRow][fromCol];
  char pieceType = getPieceType(piece);
  bool isWhite = isPieceWhite(piece);

  // Must be a pawn
  if (pieceType != 'p') {
    return false;
  }

  // Must reach promotion rank
  int promotionRank = isWhite ? 0 : 7;
  return toRow == promotionRank;
}

void WebInterface::promotePawn(int row, int col, char promoteTo, bool isWhite) {
  String newPiece = String(isWhite ? 'w' : 'b') + String(promoteTo);
  currentBoard[row][col] = newPiece;

}

void WebInterface::updateSpecialMoveTracking(int fromRow, int fromCol, int toRow, int toCol) {
  String piece = currentBoard[toRow][toCol];
  char pieceType = getPieceType(piece);
  bool isWhite = isPieceWhite(piece);

  // Track king moves
  if (pieceType == 'k') {
    if (isWhite) {
      whiteKingMoved = true;
    } else {
      blackKingMoved = true;
    }
  }

  // Track rook moves
  if (pieceType == 'r') {
    if (isWhite) {
      if (fromRow == 7 && fromCol == 0) whiteQueensideRookMoved = true;
      if (fromRow == 7 && fromCol == 7) whiteKingsideRookMoved = true;
    } else {
      if (fromRow == 0 && fromCol == 0) blackQueensideRookMoved = true;
      if (fromRow == 0 && fromCol == 7) blackKingsideRookMoved = true;
    }
  }

  // Reset en passant flag
  enPassantColumn = -1;

  // Set en passant flag for pawn double moves
  if (pieceType == 'p' && abs(fromRow - toRow) == 2) {
    enPassantColumn = fromCol;
    enPassantIsWhite = !isWhite; // Opponent can capture en passant
  }
}

// Move history implementation

void WebInterface::initializeMoveHistory() {
  historyCount = 0;
  currentHistoryIndex = -1;
}

void WebInterface::saveCurrentBoardState() {
  // When undo/redo operations happen, don't create new entries
  if (currentHistoryIndex < historyCount - 1) {
    // We're in the middle of history, truncate future moves
    historyCount = currentHistoryIndex + 1;
  }

  // Only save if we have space or shift history
  if (historyCount < 20) {
    currentHistoryIndex = historyCount;
  } else {
    // Shift history array left
    for (int i = 0; i < 19; i++) {
      moveHistory[i] = moveHistory[i + 1];
    }
    currentHistoryIndex = 19;
    historyCount = 20;
  }

  // Save current board state as "before" state
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      moveHistory[currentHistoryIndex].beforeState[row][col] = currentBoard[row][col];
    }
  }

  // Save current special move flags as "before" state
  moveHistory[currentHistoryIndex].beforeWhiteKingMoved = whiteKingMoved;
  moveHistory[currentHistoryIndex].beforeBlackKingMoved = blackKingMoved;
  moveHistory[currentHistoryIndex].beforeWhiteKingsideRookMoved = whiteKingsideRookMoved;
  moveHistory[currentHistoryIndex].beforeWhiteQueensideRookMoved = whiteQueensideRookMoved;
  moveHistory[currentHistoryIndex].beforeBlackKingsideRookMoved = blackKingsideRookMoved;
  moveHistory[currentHistoryIndex].beforeBlackQueensideRookMoved = blackQueensideRookMoved;
  moveHistory[currentHistoryIndex].beforeEnPassantColumn = enPassantColumn;
  moveHistory[currentHistoryIndex].beforeEnPassantIsWhite = enPassantIsWhite;

}

void WebInterface::saveAfterMoveState() {
  // Save current board state as "after" state for the current move
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      moveHistory[currentHistoryIndex].afterState[row][col] = currentBoard[row][col];
    }
  }

  // Save current special move flags as "after" state
  moveHistory[currentHistoryIndex].afterWhiteKingMoved = whiteKingMoved;
  moveHistory[currentHistoryIndex].afterBlackKingMoved = blackKingMoved;
  moveHistory[currentHistoryIndex].afterWhiteKingsideRookMoved = whiteKingsideRookMoved;
  moveHistory[currentHistoryIndex].afterWhiteQueensideRookMoved = whiteQueensideRookMoved;
  moveHistory[currentHistoryIndex].afterBlackKingsideRookMoved = blackKingsideRookMoved;
  moveHistory[currentHistoryIndex].afterBlackQueensideRookMoved = blackQueensideRookMoved;
  moveHistory[currentHistoryIndex].afterEnPassantColumn = enPassantColumn;
  moveHistory[currentHistoryIndex].afterEnPassantIsWhite = enPassantIsWhite;

  // Increment history count after completing the move
  historyCount = currentHistoryIndex + 1;

}

bool WebInterface::undoLastWhiteMove() {
  if (currentHistoryIndex >= 0 && currentHistoryIndex < historyCount) {
    // Restore board state to "before" state
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        currentBoard[row][col] = moveHistory[currentHistoryIndex].beforeState[row][col];
      }
    }

    // Restore special move flags to "before" state
    whiteKingMoved = moveHistory[currentHistoryIndex].beforeWhiteKingMoved;
    blackKingMoved = moveHistory[currentHistoryIndex].beforeBlackKingMoved;
    whiteKingsideRookMoved = moveHistory[currentHistoryIndex].beforeWhiteKingsideRookMoved;
    whiteQueensideRookMoved = moveHistory[currentHistoryIndex].beforeWhiteQueensideRookMoved;
    blackKingsideRookMoved = moveHistory[currentHistoryIndex].beforeBlackKingsideRookMoved;
    blackQueensideRookMoved = moveHistory[currentHistoryIndex].beforeBlackQueensideRookMoved;
    enPassantColumn = moveHistory[currentHistoryIndex].beforeEnPassantColumn;
    enPassantIsWhite = moveHistory[currentHistoryIndex].beforeEnPassantIsWhite;

    currentHistoryIndex--;
    return true;
  }

  return false;
}

bool WebInterface::redoLastWhiteMove() {
  if (currentHistoryIndex + 1 < historyCount) {
    currentHistoryIndex++;

    // Restore board state to "after" state
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        currentBoard[row][col] = moveHistory[currentHistoryIndex].afterState[row][col];
      }
    }

    // Restore special move flags to "after" state
    whiteKingMoved = moveHistory[currentHistoryIndex].afterWhiteKingMoved;
    blackKingMoved = moveHistory[currentHistoryIndex].afterBlackKingMoved;
    whiteKingsideRookMoved = moveHistory[currentHistoryIndex].afterWhiteKingsideRookMoved;
    whiteQueensideRookMoved = moveHistory[currentHistoryIndex].afterWhiteQueensideRookMoved;
    blackKingsideRookMoved = moveHistory[currentHistoryIndex].afterBlackKingsideRookMoved;
    blackQueensideRookMoved = moveHistory[currentHistoryIndex].afterBlackQueensideRookMoved;
    enPassantColumn = moveHistory[currentHistoryIndex].afterEnPassantColumn;
    enPassantIsWhite = moveHistory[currentHistoryIndex].afterEnPassantIsWhite;

    return true;
  }

  return false;
}

void WebInterface::handleUndoMove(AsyncWebServerRequest* request) {

  if (undoLastWhiteMove()) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Move undone\"}");
  } else {
    request->send(200, "application/json", "{\"success\":false,\"message\":\"No moves to undo\"}");
  }
}

void WebInterface::handleRedoMove(AsyncWebServerRequest* request) {

  if (redoLastWhiteMove()) {
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Move redone\"}");
  } else {
    request->send(200, "application/json", "{\"success\":false,\"message\":\"No moves to redo\"}");
  }
}

void WebInterface::handleDebugButtonColor(AsyncWebServerRequest* request) {
  // Get the POST parameters
  String buttonName = "";
  String colorValue = "";

  if (request->hasParam("buttonName", true)) {
    buttonName = request->getParam("buttonName", true)->value();
  }
  if (request->hasParam("colorValue", true)) {
    colorValue = request->getParam("colorValue", true)->value();
  }

  // Log to serial monitor

  request->send(200, "application/json", "{\"success\":true}");
}

void WebInterface::triggerAIMove() {
  if (!geminiAPI || processingMove) {
    return;
  }

  processingMove = true;

  // Request move from AI (currently in offline mode)
  String aiMove = geminiAPI->requestMove("", "", "black");

  // Try to apply the AI move if one was suggested
  bool moveApplied = false;
  if (!aiMove.isEmpty()) {
    if (applyMoveToBoard(aiMove)) {
      isWhiteTurn = true;
      moveApplied = true;
    }
  }

  // If no AI move was suggested or it failed, end the game with error
  if (!moveApplied) {
    Serial.println("ERROR: Game control API failure - no valid AI move received");
    Serial.println("Game ended due to AI failure");

    processingMove = false;
    return;
  }


  processingMove = false;
}

// REMOVED: generateCompactHTML() function - replaced with minimal fallback
// Function redirects to getMinimalFallbackHTML()
String WebInterface::generateCompactHTML() {
  return getMinimalFallbackHTML();
}

// REMOVED: All remaining malformed HTML/CSS/JavaScript content

// Serve file from SD card with chunked streaming
void WebInterface::serveFileFromSD(AsyncWebServerRequest* request, const char* filename) {
  if (!SD.begin(SD_CS_PIN)) {
    request->send(500, "text/plain", "SD Card initialization failed");
    return;
  }

  File file = SD.open(filename);
  if (!file) {
    // If file doesn't exist, serve minimal fallback HTML
    String fallbackHTML = getMinimalFallbackHTML();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", fallbackHTML);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    return;
  }

  size_t fileSize = file.size();
  file.close();

  // Log file sizing information
  Serial.printf("SIZING INFO: File: %s, Size: %d bytes\n", filename, fileSize);
  Serial.printf("SIZING INFO: Using chunked transfer encoding (no Content-Length header)\n");

  // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
  // Use chunked streaming for all requests
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/html",
    [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return streamFileChunk(buffer, maxLen, index, filename);
    });

  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

size_t WebInterface::streamFileChunk(uint8_t *buffer, size_t maxLen, size_t index, const char* filename) {
  static File file;
  static bool fileOpen = false;
  static size_t fileOffset = 0;
  static unsigned long firstChunkTime = 0;

  // CELLULAR OPTIMIZATION: Send just "Chess" as first tiny chunk for instant feedback
  if (index == 0) {
    // Open the file for streaming
    if (SD.exists(filename)) {
      file = SD.open(filename, FILE_READ);
      if (file) {
        fileOpen = true;
        fileOffset = 0;
        firstChunkTime = millis();
        Serial.printf("FAST CHUNK: File opened, size: %d bytes\n", file.size());
      } else {
        Serial.println("FAST CHUNK: Failed to open file");
        return 0;
      }
    } else {
      Serial.println("FAST CHUNK: File not found");
      return 0;
    }

    // Skip fast chunk - just stream the file directly to avoid document conflicts
    // The cellular delay below will still ensure slower networks get time to connect
    Serial.println("FAST CHUNK: Skipping fast chunk, will stream file directly");
    // Don't return here - fall through to stream the actual file
  }

  // CELLULAR OPTIMIZATION: Wait at least 1500ms after first chunk to ensure "Chess" displays on slow networks
  if (index > 0 && index <= 3) {
    unsigned long elapsed = millis() - firstChunkTime;
    if (elapsed < 1500) {
      Serial.printf("CELLULAR DELAY: Waiting %dms more for Chess display (elapsed: %dms)\n", 1500 - elapsed, elapsed);
      delay(1500 - elapsed);
    }
  }

  // For all subsequent chunks, stream the actual file content
  if (!fileOpen || !file) {
    Serial.println("CHUNK: File not open, ending stream");
    return 0; // End of stream
  }

  // Read next chunk from file
  size_t bytesRead = file.read(buffer, maxLen);
  fileOffset += bytesRead;

  Serial.printf("CHUNK %d: Read %d bytes (offset: %d/%d)\n", index, bytesRead, fileOffset, file.size());

  // Close file when done
  if (bytesRead == 0 || fileOffset >= file.size()) {
    file.close();
    fileOpen = false;
    fileOffset = 0;
    firstChunkTime = 0;
    // Serial.println("CHUNK: File streaming complete, file closed");  // Reduced verbosity
  }

  return bytesRead;
}

// Handle HTML file upload from development client
void WebInterface::handleHTMLUpload(AsyncWebServerRequest* request) {
  Serial.println("=== HTML Upload Handler Started ===");
  String response = "{\"success\":false,\"message\":\"No HTML data received\"}";

  Serial.printf("Request params count: %d\n", request->params());
  for (int i = 0; i < request->params(); i++) {
    AsyncWebParameter* p = request->getParam(i);
    Serial.printf("Param %d: name='%s', value length=%d, isPost=%s\n",
                  i, p->name().c_str(), p->value().length(), p->isPost() ? "true" : "false");
  }

  if (request->hasParam("html", true)) {
    Serial.println("Found 'html' parameter in POST data");
    String htmlContent = request->getParam("html", true)->value();
    Serial.printf("HTML content length: %d bytes\n", htmlContent.length());

    if (htmlContent.length() > 0) {
      Serial.println("HTML content is not empty, proceeding with SD card operations");

      // Initialize SD card
      Serial.printf("Initializing SD card with CS pin: %d\n", SD_CS_PIN);
      if (!SD.begin(SD_CS_PIN)) {
        Serial.println("ERROR: SD Card initialization failed!");
        response = "{\"success\":false,\"message\":\"SD Card initialization failed\"}";
        request->send(500, "application/json", response);
        return;
      }
      Serial.println("SD card initialized successfully");

      // Write file to SD card
      Serial.printf("Opening file for writing: %s\n", HTML_FILE_PATH);
      File file = SD.open(HTML_FILE_PATH, FILE_WRITE);
      if (file) {
        Serial.println("File opened successfully, writing content...");
        size_t bytesWritten = file.print(htmlContent);
        file.close();
        Serial.printf("File write completed. Bytes written: %d\n", bytesWritten);

        response = "{\"success\":true,\"message\":\"HTML file saved to SD card\",\"filename\":\"" + String(HTML_FILE_PATH) + "\",\"size\":" + String(htmlContent.length()) + "}";
        Serial.println("SUCCESS: HTML file saved to SD card");
      } else {
        Serial.println("ERROR: Failed to open file for writing on SD card");
        response = "{\"success\":false,\"message\":\"Failed to create file on SD card\"}";
      }
    } else {
      Serial.println("ERROR: HTML content is empty (length = 0)");
      response = "{\"success\":false,\"message\":\"HTML content is empty\"}";
    }
  } else {
    Serial.println("ERROR: No 'html' parameter found in POST data");
    response = "{\"success\":false,\"message\":\"No HTML data received\"}";
  }

  Serial.printf("Sending response: %s\n", response.c_str());
  request->send(200, "application/json", response);
  Serial.println("=== HTML Upload Handler Finished ===");
}

// Handle file descriptor cleanup
void WebInterface::handleFileCleanup(AsyncWebServerRequest* request) {
  // Gentle cleanup - just add a small delay to allow pending operations to complete
  delay(200);

  // Force yield to allow any pending file operations to finish
  yield();

  // Simple success response - avoid aggressive SD.end()/begin() cycle
  String response = "File cleanup completed";
  request->send(200, "text/plain", response);
}

// Handle start of chunked HTML upload
void WebInterface::handleUploadStart(AsyncWebServerRequest* request) {
  Serial.println("=== Upload Start Handler ===");
  String response = "{\"success\":false,\"message\":\"Upload start failed\"}";

  if (request->hasParam("filename", true) && request->hasParam("filesize", true)) {
    String filename = request->getParam("filename", true)->value();
    int filesize = request->getParam("filesize", true)->value().toInt();

    Serial.printf("Upload request: filename=%s, size=%d\n", filename.c_str(), filesize);

    // SD card should already be initialized from setup()
    // Just verify it's accessible
    Serial.println("Checking SD card access...");

    // Delete existing file if it exists
    if (SD.exists(filename.c_str())) {
      Serial.printf("Deleting existing file: %s\n", filename.c_str());
      if (SD.remove(filename.c_str())) {
        Serial.println("File deleted successfully");
      } else {
        Serial.println("WARNING: Failed to delete existing file");
      }
    }

    // Store filename for chunk and finish handlers
    currentUploadFilename = filename;

    // Create new empty file
    Serial.printf("Creating new file: %s\n", filename.c_str());
    File file = SD.open(filename.c_str(), FILE_WRITE);
    if (file) {
      Serial.println("File created successfully");
      file.close();
      response = "{\"success\":true,\"message\":\"Upload session started\",\"filename\":\"" + filename + "\",\"expectedSize\":" + String(filesize) + "}";
    } else {
      Serial.println("ERROR: Failed to create file on SD card");
      response = "{\"success\":false,\"message\":\"Failed to create file on SD card\"}";
    }
  } else {
    Serial.println("ERROR: Missing filename or filesize parameter");
    response = "{\"success\":false,\"message\":\"Missing filename or filesize parameter\"}";
  }

  Serial.printf("Sending response: %s\n", response.c_str());
  request->send(200, "application/json", response);
}

// Handle individual chunk upload (binary body data)
void WebInterface::handleUploadChunkBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  String response = "{\"success\":false,\"message\":\"Chunk upload failed\"}";

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    response = "{\"success\":false,\"message\":\"SD Card initialization failed\"}";
    request->send(500, "application/json", response);
    return;
  }

  // Append chunk to file (use currentUploadFilename)
  File file = SD.open(currentUploadFilename.c_str(), FILE_APPEND);
  if (file) {
    size_t written = file.write(data, len);
    file.close();

    if (written == len) {
      response = "{\"success\":true,\"message\":\"Chunk appended\",\"chunkSize\":" + String(len) + "}";
    } else {
      response = "{\"success\":false,\"message\":\"Failed to write all bytes\"}";
    }
  } else {
    response = "{\"success\":false,\"message\":\"Failed to open file for append\"}";
  }

  // Only send response when all body data is received
  if (index + len == total) {
    request->send(200, "application/json", response);
  }
}

// Handle finish of chunked upload
void WebInterface::handleUploadFinish(AsyncWebServerRequest* request) {
  String response = "{\"success\":false,\"message\":\"Upload finish failed\"}";

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    response = "{\"success\":false,\"message\":\"SD Card initialization failed\"}";
    request->send(500, "application/json", response);
    return;
  }

  // Check if file exists and get final size (use currentUploadFilename)
  if (SD.exists(currentUploadFilename.c_str())) {
    File file = SD.open(currentUploadFilename.c_str(), FILE_READ);
    if (file) {
      size_t finalSize = file.size();
      file.close();
      response = "{\"success\":true,\"message\":\"Upload completed successfully\",\"filename\":\"" + currentUploadFilename + "\",\"finalSize\":" + String(finalSize) + "}";
    } else {
      response = "{\"success\":false,\"message\":\"File exists but cannot be opened\"}";
    }
  } else {
    response = "{\"success\":false,\"message\":\"File does not exist after upload\"}";
  }

  request->send(200, "application/json", response);
}

void WebInterface::handleLogMessage(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  // Append browser console log to unified DebugMessages.log file with IP address
  String modifiedLog;
  if (len > 0) {
    File logFile = SD.open("/DebugMessages.log", FILE_APPEND);
    if (logFile) {
      // Get client IP address
      String clientIP = request->client()->remoteIP().toString();

      // Convert incoming data to string to manipulate it
      String logMessage = String((char*)data).substring(0, len);

      // Find the timestamp end (after ']')
      int timestampEnd = logMessage.indexOf(']');
      if (timestampEnd != -1) {
        // Insert IP address after timestamp: [timestamp] [IP] message
        modifiedLog = logMessage.substring(0, timestampEnd + 1) + " [" + clientIP + "]" + logMessage.substring(timestampEnd + 1);
        logFile.print(modifiedLog);
      } else {
        // If no timestamp found, just write as-is
        logFile.write(data, len);
        modifiedLog = String((char*)data).substring(0, len);
      }

      logFile.close();

      // Broadcast to all connected Session Control viewers via serial stream
      if (g_serialLogEventSource && modifiedLog.length() > 0) {
        // Remove trailing newline for cleaner display
        modifiedLog.trim();
        g_serialLogEventSource->send(modifiedLog.c_str(), "serial-log", millis());
      }
    }
  }

  if (index + len == total) {
    // Increment session message count if sessionId is provided
    if (sessionManager && request->hasParam("sessionId")) {
      String sessionId = request->getParam("sessionId")->value();
      sessionManager->incrementMessageCount(sessionId);
    }

    request->send(200, "text/plain", "OK");
  }
}

void WebInterface::handleClearLogs(AsyncWebServerRequest* request) {
  bool debugCleared = false;

  if (SD.exists("/DebugMessages.log")) {
    SD.remove("/DebugMessages.log");
    debugCleared = true;
  }

  String response = "{\"success\":true,\"debugCleared\":" + String(debugCleared ? "true" : "false") + "}";
  request->send(200, "application/json", response);
}

void WebInterface::handleGetConsoleLog(AsyncWebServerRequest* request) {
  // Now serves unified DebugMessages.log (contains both browser and serial logs)
  const char* filename = "/DebugMessages.log";

  if (!SD.exists(filename)) {
    request->send(404, "text/plain", "Debug log file not found");
    return;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    request->send(500, "text/plain", "Failed to open debug log file");
    return;
  }

  size_t fileSize = file.size();
  file.close();

  Serial.printf("CONSOLE LOG: Serving debug log file (%d bytes) using chunked streaming\n", fileSize);

  // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
    [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return streamFileChunk(buffer, maxLen, index, filename);
    });

  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void WebInterface::handleGetSerialLog(AsyncWebServerRequest* request) {
  // Now serves unified DebugMessages.log (contains both browser and serial logs)
  const char* filename = "/DebugMessages.log";

  if (!SD.exists(filename)) {
    request->send(404, "text/plain", "Debug log file not found");
    return;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    request->send(500, "text/plain", "Failed to open debug log file");
    return;
  }

  size_t fileSize = file.size();
  file.close();

  Serial.printf("SERIAL LOG: Serving debug log file (%d bytes) using chunked streaming\n", fileSize);

  // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
    [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return streamFileChunk(buffer, maxLen, index, filename);
    });

  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void WebInterface::handleGetDebugLog(AsyncWebServerRequest* request) {
  // Serves the DebugMessages.log file for the Debug Log Viewer
  const char* filename = "/DebugMessages.log";

  if (!SD.exists(filename)) {
    request->send(404, "text/plain", "Debug log file not found");
    return;
  }

  File file = SD.open(filename, FILE_READ);
  if (!file) {
    request->send(500, "text/plain", "Failed to open debug log file");
    return;
  }

  size_t fileSize = file.size();
  file.close();

  Serial.printf("DEBUG LOG: Serving debug log file (%d bytes) using chunked streaming\n", fileSize);

  // RULE: ALWAYS use chunked streaming for files - ESP32 has limited RAM
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
    [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
      return streamFileChunk(buffer, maxLen, index, filename);
    });

  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void WebInterface::handleEject(AsyncWebServerRequest* request) {
  Serial.println("SD card eject requested via web interface");

  // Flush any pending writes
  if (sdLogger) {
    sdLogger->flush();
  }

  // Close any open file handles
  // SD.end() will safely unmount the SD card
  SD.end();

  // Set LED to yellow to indicate SD card ejected
  setLEDYellow();

  Serial.println("SD card safely ejected - you can now remove it");
  Serial.println("NOTE: You must reboot the ESP32 after reinserting the card");

  String response = "{\"success\":true,\"message\":\"SD card safely ejected\"}";
  request->send(200, "application/json", response);
}

void WebInterface::handleReboot(AsyncWebServerRequest* request) {
  Serial.println("Reboot requested via web interface");

  String response = "{\"success\":true,\"message\":\"ESP32 rebooting...\"}";
  request->send(200, "application/json", response);

  // Delay to allow response to be sent
  delay(500);

  // Reboot the ESP32
  ESP.restart();
}

void WebInterface::handleFileRead(AsyncWebServerRequest* request) {
  if (!request->hasParam("path")) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing path parameter\"}");
    return;
  }

  String path = request->getParam("path")->value();

  // Security: only allow reading from root directory files starting with /
  if (!path.startsWith("/")) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Path must start with /\"}");
    return;
  }

  Serial.printf("File read request: %s\n", path.c_str());

  if (!SD.exists(path)) {
    request->send(404, "application/json", "{\"success\":false,\"error\":\"File not found\"}");
    return;
  }

  File file = SD.open(path, FILE_READ);
  if (!file) {
    request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to open file\"}");
    return;
  }

  // Read entire file content
  String content = file.readString();
  file.close();

  Serial.printf("File read successfully: %s (%d bytes)\n", path.c_str(), content.length());

  // Return as plain text (not JSON) to preserve exact formatting
  request->send(200, "text/plain", content);
}

void WebInterface::handleFileWrite(AsyncWebServerRequest* request) {
  if (!request->hasParam("path", true)) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing path parameter\"}");
    return;
  }

  if (!request->hasParam("content", true)) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Missing content parameter\"}");
    return;
  }

  String path = request->getParam("path", true)->value();
  String content = request->getParam("content", true)->value();

  // Security: only allow writing to root directory files starting with /
  if (!path.startsWith("/")) {
    request->send(400, "application/json", "{\"success\":false,\"error\":\"Path must start with /\"}");
    return;
  }

  Serial.printf("File write request: %s (%d bytes)\n", path.c_str(), content.length());

  // Delete existing file if it exists
  if (SD.exists(path)) {
    SD.remove(path);
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to open file for writing\"}");
    return;
  }

  size_t written = file.print(content);
  file.close();

  if (written != content.length()) {
    request->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to write complete file\"}");
    return;
  }

  Serial.printf("File written successfully: %s (%d bytes)\n", path.c_str(), written);

  String response = "{\"success\":true,\"path\":\"" + path + "\",\"size\":" + String(written) + "}";
  request->send(200, "application/json", response);
}