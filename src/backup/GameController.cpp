#include "GameController.h"

GameController::GameController() {
  chess = nullptr;
  scraper = nullptr;
  storage = nullptr;
  currentState = GAME_IDLE;
  lastMoveTime = 0;
  moveTimeout = 60000; // 60 seconds timeout for AI moves
  totalMoves = 0;
}

void GameController::begin(ChessEngine* chessEngine, WebScraper* webScraper, StorageManager* storageManager) {
  chess = chessEngine;
  scraper = webScraper;
  storage = storageManager;

  // Initialize AI players with default settings
  ai1.name = "AI Player 1";
  ai1.endpoint = "";
  ai1.color = "white";
  ai1.lastMove = "";
  ai1.thinkTime = 0;
  ai1.moveCount = 0;

  ai2.name = "AI Player 2";
  ai2.endpoint = "";
  ai2.color = "black";
  ai2.lastMove = "";
  ai2.thinkTime = 0;
  ai2.moveCount = 0;

  // Load AI configurations if available
  if (storage && storage->isReady()) {
    JsonDocument ai1Config, ai2Config;

    if (storage->loadAIConfig(0, ai1Config)) {
      ai1.name = ai1Config["name"].as<String>();
      ai1.endpoint = ai1Config["endpoint"].as<String>();

      // Configure web scraper for AI1
      if (ai1.name == "ChatGPT") {
        scraper->setupChatGPT(0);
      } else if (ai1.name == "Claude") {
        scraper->setupClaude(0);
      }

      Serial.printf("AI1 configured: %s\n", ai1.name.c_str());
    } else {
      // Set up default AI1 (ChatGPT)
      scraper->setupChatGPT(0);
      ai1.name = "ChatGPT";
      Serial.println("AI1 using default ChatGPT configuration");
    }

    if (storage->loadAIConfig(1, ai2Config)) {
      ai2.name = ai2Config["name"].as<String>();
      ai2.endpoint = ai2Config["endpoint"].as<String>();

      // Configure web scraper for AI2
      if (ai2.name == "ChatGPT") {
        scraper->setupChatGPT(1);
      } else if (ai2.name == "Claude") {
        scraper->setupClaude(1);
      }

      Serial.printf("AI2 configured: %s\n", ai2.name.c_str());
    } else {
      // Set up default AI2 (Claude)
      scraper->setupClaude(1);
      ai2.name = "Claude";
      Serial.println("AI2 using default Claude configuration");
    }
  }

  Serial.println("GameController initialized");
}

void GameController::update() {
  if (!chess || !scraper || !storage) {
    return;
  }

  switch (currentState) {
    case GAME_IDLE:
      // Nothing to do when idle
      break;

    case GAME_WAITING_AI1:
      if (millis() - lastMoveTime > moveTimeout) {
        Serial.println("AI1 move timeout");
        currentState = GAME_ERROR;
        saveGameState();
      } else {
        // Request move from AI1
        requestAIMove(0); // AI1 index
      }
      break;

    case GAME_WAITING_AI2:
      if (millis() - lastMoveTime > moveTimeout) {
        Serial.println("AI2 move timeout");
        currentState = GAME_ERROR;
        saveGameState();
      } else {
        // Request move from AI2
        requestAIMove(1); // AI2 index
      }
      break;

    case GAME_PROCESSING_MOVE:
      // Move processing handled in processAIMove
      break;

    case GAME_FINISHED:
      // Game is over, save final state
      if (storage->isReady()) {
        storage->archiveGame(gameId);
      }
      currentState = GAME_IDLE;
      break;

    case GAME_ERROR:
      Serial.println("Game in error state");
      // Could implement error recovery logic here
      break;
  }
}

void GameController::startNewGame() {
  if (!chess || !scraper || !storage) {
    Serial.println("GameController not properly initialized");
    return;
  }

  Serial.println("Starting new chess game");

  // Reset chess engine
  chess->resetGame();

  // Generate new game ID
  gameId = generateGameId();

  // Reset AI states
  ai1.lastMove = "";
  ai1.thinkTime = 0;
  ai1.moveCount = 0;

  ai2.lastMove = "";
  ai2.thinkTime = 0;
  ai2.moveCount = 0;

  totalMoves = 0;
  lastMoveTime = millis();

  // Delete any existing current game
  if (storage->isReady()) {
    storage->deleteCurrentGame();
  }

  // Start with AI1 (white)
  currentState = GAME_WAITING_AI1;

  // Save initial game state
  saveGameState();

  Serial.printf("New game started: %s\n", gameId.c_str());
}

void GameController::restoreGame() {
  if (!storage || !storage->isReady() || !storage->hasCurrentGame()) {
    Serial.println("No game to restore");
    return;
  }

  GameData gameData;
  if (!storage->loadGameState(gameData)) {
    Serial.println("Failed to load game state");
    return;
  }

  gameId = gameData.gameId;
  totalMoves = gameData.moveCount;

  // Parse AI data
  JsonDocument ai1Doc, ai2Doc;

  if (deserializeJson(ai1Doc, gameData.ai1Data) == DeserializationError::Ok) {
    ai1.name = ai1Doc["name"].as<String>();
    ai1.lastMove = ai1Doc["lastMove"].as<String>();
    ai1.thinkTime = ai1Doc["thinkTime"].as<unsigned long>();
    ai1.moveCount = ai1Doc["moveCount"].as<int>();
  }

  if (deserializeJson(ai2Doc, gameData.ai2Data) == DeserializationError::Ok) {
    ai2.name = ai2Doc["name"].as<String>();
    ai2.lastMove = ai2Doc["lastMove"].as<String>();
    ai2.thinkTime = ai2Doc["thinkTime"].as<unsigned long>();
    ai2.moveCount = ai2Doc["moveCount"].as<int>();
  }

  // Restore chess position (this would require implementing FEN parsing in ChessEngine)
  // For now, we'll start from the beginning and replay moves
  // This is a simplified approach

  if (gameData.status == "in_progress") {
    if (gameData.currentPlayer == "white") {
      currentState = GAME_WAITING_AI1;
    } else {
      currentState = GAME_WAITING_AI2;
    }
  } else {
    currentState = GAME_IDLE;
  }

  lastMoveTime = millis();

  Serial.printf("Game restored: %s (Move %d)\n", gameId.c_str(), totalMoves);
}

void GameController::pauseGame() {
  if (currentState != GAME_IDLE && currentState != GAME_FINISHED) {
    currentState = GAME_IDLE;
    saveGameState();
    Serial.println("Game paused");
  }
}

void GameController::resumeGame() {
  if (currentState == GAME_IDLE) {
    // Determine whose turn it is
    if (chess->getCurrentPlayer() == WHITE) {
      currentState = GAME_WAITING_AI1;
    } else {
      currentState = GAME_WAITING_AI2;
    }
    lastMoveTime = millis();
    Serial.println("Game resumed");
  }
}

void GameController::requestAIMove(int aiIndex) {
  if (currentState != GAME_WAITING_AI1 && currentState != GAME_WAITING_AI2) {
    return;
  }

  AIPlayer* currentAI = (aiIndex == 0) ? &ai1 : &ai2;

  Serial.printf("Requesting move from %s...\n", currentAI->name.c_str());

  unsigned long startTime = millis();

  String currentPosition = chess->getFEN();
  String moveHistory = chess->getMoveHistory();
  String color = currentAI->color;

  String move = scraper->requestMove(aiIndex, currentPosition, moveHistory, color);

  unsigned long endTime = millis();
  currentAI->thinkTime = endTime - startTime;

  if (!move.isEmpty()) {
    Serial.printf("%s suggested move: %s (took %lums)\n",
      currentAI->name.c_str(), move.c_str(), currentAI->thinkTime);

    processAIMove(move);
  } else {
    Serial.printf("No valid move received from %s\n", currentAI->name.c_str());
    currentState = GAME_ERROR;
  }
}

void GameController::processAIMove(const String& move) {
  currentState = GAME_PROCESSING_MOVE;

  bool moveValid = chess->playMove(move);

  if (moveValid) {
    totalMoves++;

    // Update current AI's data
    AIPlayer* currentAI = (chess->getCurrentPlayer() == BLACK) ? &ai1 : &ai2; // Player switched after move
    currentAI->lastMove = move;
    currentAI->moveCount++;

    // Log the move
    if (storage && storage->isReady()) {
      storage->logMove(gameId, move, currentAI->name);
    }

    Serial.printf("Move accepted: %s by %s\n", move.c_str(), currentAI->name.c_str());

    // Check for game end
    if (chess->isGameOver()) {
      handleGameEnd();
    } else {
      // Switch to next player
      switchPlayer();
    }

    // Save game state after each move
    saveGameState();

  } else {
    Serial.printf("Invalid move rejected: %s\n", move.c_str());
    currentState = GAME_ERROR;
    saveGameState();
  }
}

void GameController::switchPlayer() {
  if (chess->getCurrentPlayer() == WHITE) {
    currentState = GAME_WAITING_AI1;
    Serial.println("Waiting for AI1 (White) move");
  } else {
    currentState = GAME_WAITING_AI2;
    Serial.println("Waiting for AI2 (Black) move");
  }

  lastMoveTime = millis();
}

void GameController::handleGameEnd() {
  GameResult result = chess->getGameResult();

  String resultStr = "Unknown";
  switch (result) {
    case WHITE_WINS:
      resultStr = ai1.name + " (White) wins";
      break;
    case BLACK_WINS:
      resultStr = ai2.name + " (Black) wins";
      break;
    case DRAW_STALEMATE:
      resultStr = "Draw (Stalemate)";
      break;
    case DRAW_INSUFFICIENT:
      resultStr = "Draw (Insufficient material)";
      break;
    case DRAW_50MOVE:
      resultStr = "Draw (50-move rule)";
      break;
    case DRAW_REPETITION:
      resultStr = "Draw (Repetition)";
      break;
    default:
      resultStr = "Game over";
      break;
  }

  Serial.printf("Game finished: %s\n", resultStr.c_str());

  if (storage && storage->isReady()) {
    storage->logInfo("Game finished: " + resultStr + " (Game ID: " + gameId + ")");
  }

  currentState = GAME_FINISHED;
}

String GameController::getCurrentPlayer() const {
  if (chess->getCurrentPlayer() == WHITE) {
    return ai1.name + " (White)";
  } else {
    return ai2.name + " (Black)";
  }
}

String GameController::generateGameId() {
  // Generate timestamp-based game ID
  unsigned long timestamp = millis();
  return "game_" + String(timestamp);
}

void GameController::saveGameState() {
  if (!storage || !storage->isReady()) {
    return;
  }

  GameData gameData;
  gameData.gameId = gameId;

  switch (currentState) {
    case GAME_IDLE:
      gameData.status = "idle";
      break;
    case GAME_WAITING_AI1:
    case GAME_WAITING_AI2:
    case GAME_PROCESSING_MOVE:
      gameData.status = "in_progress";
      break;
    case GAME_FINISHED:
      gameData.status = "finished";
      break;
    case GAME_ERROR:
      gameData.status = "error";
      break;
  }

  gameData.currentPlayer = (chess->getCurrentPlayer() == WHITE) ? "white" : "black";
  gameData.moveCount = totalMoves;
  gameData.board = chess->getFEN();
  gameData.moves = chess->getMoveHistory();
  gameData.timestamp = String(millis());

  // Serialize AI data
  JsonDocument ai1Doc, ai2Doc;

  ai1Doc["name"] = ai1.name;
  ai1Doc["color"] = ai1.color;
  ai1Doc["lastMove"] = ai1.lastMove;
  ai1Doc["thinkTime"] = ai1.thinkTime;
  ai1Doc["moveCount"] = ai1.moveCount;
  serializeJson(ai1Doc, gameData.ai1Data);

  ai2Doc["name"] = ai2.name;
  ai2Doc["color"] = ai2.color;
  ai2Doc["lastMove"] = ai2.lastMove;
  ai2Doc["thinkTime"] = ai2.thinkTime;
  ai2Doc["moveCount"] = ai2.moveCount;
  serializeJson(ai2Doc, gameData.ai2Data);

  bool saved = storage->saveGameState(gameData);
  if (!saved) {
    Serial.println("Failed to save game state");
  }
}

JsonDocument GameController::getGameStatus() {
  JsonDocument status;

  status["gameId"] = gameId;
  status["currentPlayer"] = getCurrentPlayer();
  status["moveCount"] = totalMoves;

  switch (currentState) {
    case GAME_IDLE:
      status["status"] = "idle";
      break;
    case GAME_WAITING_AI1:
      status["status"] = "waiting_ai1";
      break;
    case GAME_WAITING_AI2:
      status["status"] = "waiting_ai2";
      break;
    case GAME_PROCESSING_MOVE:
      status["status"] = "processing";
      break;
    case GAME_FINISHED:
      status["status"] = "finished";
      break;
    case GAME_ERROR:
      status["status"] = "error";
      break;
  }

  JsonObject ai1Status = status["ai1"].to<JsonObject>();
  ai1Status["name"] = ai1.name;
  ai1Status["color"] = ai1.color;
  ai1Status["lastMove"] = ai1.lastMove;
  ai1Status["thinkTime"] = ai1.thinkTime;
  ai1Status["moveCount"] = ai1.moveCount;

  JsonObject ai2Status = status["ai2"].to<JsonObject>();
  ai2Status["name"] = ai2.name;
  ai2Status["color"] = ai2.color;
  ai2Status["lastMove"] = ai2.lastMove;
  ai2Status["thinkTime"] = ai2.thinkTime;
  ai2Status["moveCount"] = ai2.moveCount;

  if (chess) {
    status["board"] = chess->getFEN();
    status["moves"] = chess->getMoveHistory();
  }

  return status;
}