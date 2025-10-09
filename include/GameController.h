#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "ChessEngine.h"
#include "GeminiAPI.h"
#include "StorageManager.h"

class WebInterface; // Forward declaration

enum GameState {
  GAME_IDLE,
  GAME_WAITING_USER,
  GAME_WAITING_AI,
  GAME_PROCESSING_MOVE,
  GAME_FINISHED,
  GAME_ERROR
};

struct Player {
  String name;
  String type; // "user" or "ai"
  String endpoint; // only for AI players
  String color;
  String lastMove;
  unsigned long thinkTime;
  int moveCount;
};

class GameController {
private:
  ChessEngine* chess;
  GeminiAPI* geminiAPI;
  StorageManager* storage;
  WebInterface* webInterface;

  GameState currentState;
  Player player1; // User player
  Player player2; // AI player

  String gameId;
  unsigned long lastMoveTime;
  unsigned long moveTimeout;
  int totalMoves;

  void processMove(const String& move);
  void requestAIMove();
  void switchPlayer();
  void handleGameEnd();
  String generateGameId();

public:
  GameController();

  void begin(ChessEngine* chessEngine, GeminiAPI* api, StorageManager* storageManager);
  void setWebInterface(WebInterface* webInterface) { this->webInterface = webInterface; }
  void update();

  void startNewGame();
  void restoreGame();
  void pauseGame();
  void resumeGame();

  GameState getState() const { return currentState; }
  String getCurrentPlayer() const;
  String getStateString() const;
  String getGameId() const { return gameId; }
  int getMoveCount() const { return totalMoves; }

  Player getPlayer1() const { return player1; }
  Player getPlayer2() const { return player2; }

  // User move handling
  bool submitUserMove(const String& move);
  bool isWaitingForUser() const { return currentState == GAME_WAITING_USER; }

  void saveGameState();
  JsonDocument getGameStatus();
};

#endif