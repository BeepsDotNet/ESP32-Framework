#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

struct GameData {
  String gameId;
  String status;
  String currentPlayer;
  int moveCount;
  String board;
  String moves;
  String ai1Data;
  String ai2Data;
  String timestamp;
};

class StorageManager {
private:
  bool sdInitialized;
  String currentGamePath;
  String configPath;
  String logsPath;

  bool writeFile(const String& path, const String& data);
  String readFile(const String& path);
  void logEvent(const String& event);

public:
  StorageManager();

  bool begin();
  bool isReady() const { return sdInitialized; }

  // Game state management
  bool saveGameState(const GameData& gameData);
  bool loadGameState(GameData& gameData);
  bool hasCurrentGame();
  bool deleteCurrentGame();

  // Configuration management
  bool saveAIConfig(int aiIndex, const JsonDocument& config);
  bool loadAIConfig(int aiIndex, JsonDocument& config);
  bool saveSystemConfig(const JsonDocument& config);
  bool loadSystemConfig(JsonDocument& config);

  // Game history
  bool archiveGame(const String& gameId);
  bool listGames(JsonDocument& gamesList);

  // Logging
  void logMove(const String& gameId, const String& move, const String& player);
  void logError(const String& error);
  void logInfo(const String& info);

  // Utility
  uint64_t getAvailableSpace();
  bool cleanupOldGames(int keepCount = 10);
};

#endif