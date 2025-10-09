#include "GameController.h"
#include "WebInterface.h"
// #include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>

// Static variable to track consecutive failures
static int consecutiveFailures = 0;

GameController::GameController() {
  currentState = GAME_IDLE;
  gameId = "";
  totalMoves = 0;
  webInterface = nullptr;
}

void GameController::begin(ChessEngine* chessEngine, GeminiAPI* api, StorageManager* storageManager) {
  // Initialize game controller with required components
  chess = chessEngine;
  geminiAPI = api;
  storage = storageManager;

  currentState = GAME_IDLE;
}

void GameController::startNewGame() {
  // Initialize players
  player1.name = "User";
  player1.type = "user";
  player1.color = "white";
  player1.lastMove = "";
  player1.thinkTime = 0;
  player1.moveCount = 0;

  player2.name = "Gemini AI";
  player2.type = "ai";
  player2.color = "black";
  player2.lastMove = "";
  player2.thinkTime = 0;
  player2.moveCount = 0;

  currentState = GAME_WAITING_USER; // Start with user's turn (white goes first)
  gameId = generateGameId();
  totalMoves = 0;
}

void GameController::pauseGame() {
  if (currentState != GAME_IDLE && currentState != GAME_FINISHED) {
  }
}

void GameController::resumeGame() {
  if (currentState != GAME_IDLE && currentState != GAME_FINISHED) {
  }
}

void GameController::update() {
  // Game update logic would go here
}

JsonDocument GameController::getGameStatus() {
  JsonDocument doc;

  doc["gameId"] = gameId;
  doc["status"] = getStateString();
  doc["currentPlayer"] = getCurrentPlayer();
  doc["moveCount"] = totalMoves;

  JsonObject p1 = doc["player1"].to<JsonObject>();
  p1["name"] = player1.name;
  p1["type"] = player1.type;
  p1["color"] = player1.color;
  p1["lastMove"] = player1.lastMove;
  p1["thinkTime"] = player1.thinkTime;
  p1["moveCount"] = player1.moveCount;

  JsonObject p2 = doc["player2"].to<JsonObject>();
  p2["name"] = player2.name;
  p2["type"] = player2.type;
  p2["color"] = player2.color;
  p2["lastMove"] = player2.lastMove;
  p2["thinkTime"] = player2.thinkTime;
  p2["moveCount"] = player2.moveCount;

  return doc;
}

String GameController::getCurrentPlayer() const {
  switch (currentState) {
    case GAME_WAITING_USER:
      return "User";
    case GAME_WAITING_AI:
      return "Gemini AI";
    default:
      return "None";
  }
}

String GameController::getStateString() const {
  switch (currentState) {
    case GAME_IDLE:
      return "Idle";
    case GAME_WAITING_USER:
      return "Waiting for User Move";
    case GAME_WAITING_AI:
      return "Waiting for AI Move";
    case GAME_PROCESSING_MOVE:
      return "Processing Move";
    case GAME_FINISHED:
      return "Game Finished";
    case GAME_ERROR:
      return "Error";
    default:
      return "Unknown";
  }
}

String GameController::generateGameId() {
  return "game_" + String(millis());
}

bool GameController::submitUserMove(const String& move) {
  if (currentState != GAME_WAITING_USER) {
    return false;
  }

  if (move.isEmpty()) {
    return false;
  }


  // Process the user's move
  player1.lastMove = move;
  player1.moveCount++;
  totalMoves++;

  // Update visual board with user's move
  if (webInterface) {
    // esp_task_wdt_reset(); // Feed watchdog before web update
    webInterface->applyMove(move, true); // User is white
    // esp_task_wdt_reset(); // Feed watchdog after web update
    yield(); // Allow other tasks to run
    delay(50); // Give async_tcp time to process
  }

  // Switch to AI's turn
  currentState = GAME_WAITING_AI;
  lastMoveTime = millis();

  // Request AI move (this would trigger AI response)
  requestAIMove();

  return true;
}

void GameController::requestAIMove() {
  if (currentState != GAME_WAITING_AI) {
    return;
  }

  if (!geminiAPI || !geminiAPI->isConfigured()) {
    currentState = GAME_ERROR;
    return;
  }

  currentState = GAME_PROCESSING_MOVE;

  // Feed watchdog before long operation
  yield();
  delay(10);

  unsigned long startTime = millis();

  // Build current position and move history strings
  // For now, use simple format - could be enhanced with FEN notation
  String position = "starting position"; // Placeholder
  String moveHistory = "";

  // Build move history from player moves
  if (player1.moveCount > 0) {
    for (int i = 1; i <= player1.moveCount; i++) {
      if (i > 1) moveHistory += " ";
      moveHistory += String(i) + ". " + player1.lastMove;
      if (player2.moveCount >= i) {
        moveHistory += " " + player2.lastMove;
      }
    }
  }

  // Allow time for web server to settle before API call
  delay(1000);
  yield();
  // esp_task_wdt_reset();

  // Temporarily disable watchdog for this task during API call
  // esp_task_wdt_delete(xTaskGetCurrentTaskHandle());

  // Request move from Gemini API with timeout handling
  String aiMove = "";
  unsigned long apiStartTime = millis();
  const unsigned long API_TIMEOUT = 30000; // 30 second hard timeout

  // Try to get AI move with timeout
  aiMove = geminiAPI->requestMove(position, moveHistory, "black");

  // Check if we timed out
  unsigned long apiDuration = millis() - apiStartTime;
  if (apiDuration > API_TIMEOUT || aiMove.isEmpty()) {

    // Try to recover by checking WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      delay(5000); // Wait for reconnection
    }

    aiMove = ""; // Force empty to trigger error handling
    consecutiveFailures++;

    // If we've had 3 consecutive failures, restart the ESP32
    if (consecutiveFailures >= 3) {
      delay(2000);
      ESP.restart();
    }
  }

  // Re-enable watchdog for this task
  // esp_task_wdt_add(xTaskGetCurrentTaskHandle());

  unsigned long thinkTime = millis() - startTime;

  if (!aiMove.isEmpty()) {
    // Reset failure counter on success
    consecutiveFailures = 0;

    player2.lastMove = aiMove;
    player2.moveCount++;
    player2.thinkTime = thinkTime;
    totalMoves++;

    // Update visual board with AI's move
    if (webInterface) {
      // esp_task_wdt_reset(); // Feed watchdog before web update
      webInterface->applyMove(aiMove, false); // AI is black
      // esp_task_wdt_reset(); // Feed watchdog after web update
      yield(); // Allow other tasks to run
      delay(100); // Give async_tcp time to process
    }

    // Switch back to user's turn
    currentState = GAME_WAITING_USER;


    // Aggressive yielding to prevent async_tcp watchdog timeout
    for (int i = 0; i < 10; i++) {
      // esp_task_wdt_reset();
      yield();
      delay(50); // Give async_tcp tasks priority
      vTaskDelay(1); // FreeRTOS task switch
    }

  } else {
    currentState = GAME_ERROR;
  }
}