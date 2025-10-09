#include "StorageManager.h"
#include <SD_MMC.h>

StorageManager::StorageManager() {
  sdInitialized = false;
  currentGamePath = "/chess_games/games/current_game.json";
  configPath = "/chess_games/config/";
  logsPath = "/chess_games/logs/";
}

bool StorageManager::begin() {
  if (!SD_MMC.begin()) {
    Serial.println("SD_MMC card initialization failed in StorageManager");
    return false;
  }

  sdInitialized = true;
  Serial.println("StorageManager initialized successfully");

  // Log startup
  logInfo("StorageManager started");

  return true;
}

bool StorageManager::saveGameState(const GameData& gameData) {
  if (!sdInitialized) {
    Serial.println("SD_MMC card not initialized");
    return false;
  }

  JsonDocument doc;
  doc["gameId"] = gameData.gameId;
  doc["status"] = gameData.status;
  doc["currentPlayer"] = gameData.currentPlayer;
  doc["moveCount"] = gameData.moveCount;
  doc["board"] = gameData.board;
  doc["moves"] = gameData.moves;
  doc["ai1Data"] = gameData.ai1Data;
  doc["ai2Data"] = gameData.ai2Data;
  doc["timestamp"] = gameData.timestamp;

  String jsonString;
  serializeJson(doc, jsonString);

  bool success = writeFile(currentGamePath, jsonString);
  if (success) {
    Serial.printf("Game state saved: %s\n", gameData.gameId.c_str());
    logInfo("Game state saved: " + gameData.gameId);
  } else {
    Serial.println("Failed to save game state");
    logError("Failed to save game state: " + gameData.gameId);
  }

  return success;
}

bool StorageManager::loadGameState(GameData& gameData) {
  if (!sdInitialized || !hasCurrentGame()) {
    return false;
  }

  String jsonString = readFile(currentGamePath);
  if (jsonString.isEmpty()) {
    Serial.println("Current game file is empty");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.printf("JSON parsing failed: %s\n", error.c_str());
    logError("JSON parsing failed for current game: " + String(error.c_str()));
    return false;
  }

  gameData.gameId = doc["gameId"].as<String>();
  gameData.status = doc["status"].as<String>();
  gameData.currentPlayer = doc["currentPlayer"].as<String>();
  gameData.moveCount = doc["moveCount"].as<int>();
  gameData.board = doc["board"].as<String>();
  gameData.moves = doc["moves"].as<String>();
  gameData.ai1Data = doc["ai1Data"].as<String>();
  gameData.ai2Data = doc["ai2Data"].as<String>();
  gameData.timestamp = doc["timestamp"].as<String>();

  Serial.printf("Game state loaded: %s\n", gameData.gameId.c_str());
  logInfo("Game state loaded: " + gameData.gameId);

  return true;
}

bool StorageManager::hasCurrentGame() {
  if (!sdInitialized) return false;
  return SD_MMC.exists(currentGamePath);
}

bool StorageManager::deleteCurrentGame() {
  if (!sdInitialized) return false;

  if (SD_MMC.exists(currentGamePath)) {
    bool success = SD_MMC.remove(currentGamePath);
    if (success) {
      Serial.println("Current game deleted");
      logInfo("Current game deleted");
    } else {
      Serial.println("Failed to delete current game");
      logError("Failed to delete current game");
    }
    return success;
  }

  return true; // Already doesn't exist
}

bool StorageManager::saveAIConfig(int aiIndex, const JsonDocument& config) {
  if (!sdInitialized || aiIndex < 0 || aiIndex > 1) {
    return false;
  }

  String filename = configPath + "ai" + String(aiIndex + 1) + "_config.json";
  String jsonString;
  serializeJson(config, jsonString);

  bool success = writeFile(filename, jsonString);
  if (success) {
    Serial.printf("AI %d config saved\n", aiIndex + 1);
    logInfo("AI " + String(aiIndex + 1) + " config saved");
  } else {
    Serial.printf("Failed to save AI %d config\n", aiIndex + 1);
    logError("Failed to save AI " + String(aiIndex + 1) + " config");
  }

  return success;
}

bool StorageManager::loadAIConfig(int aiIndex, JsonDocument& config) {
  if (!sdInitialized || aiIndex < 0 || aiIndex > 1) {
    return false;
  }

  String filename = configPath + "ai" + String(aiIndex + 1) + "_config.json";
  String jsonString = readFile(filename);

  if (jsonString.isEmpty()) {
    Serial.printf("AI %d config file not found or empty\n", aiIndex + 1);
    return false;
  }

  DeserializationError error = deserializeJson(config, jsonString);
  if (error) {
    Serial.printf("Failed to parse AI %d config: %s\n", aiIndex + 1, error.c_str());
    logError("Failed to parse AI " + String(aiIndex + 1) + " config: " + String(error.c_str()));
    return false;
  }

  Serial.printf("AI %d config loaded\n", aiIndex + 1);
  return true;
}

bool StorageManager::saveSystemConfig(const JsonDocument& config) {
  if (!sdInitialized) return false;

  String filename = configPath + "system_config.json";
  String jsonString;
  serializeJson(config, jsonString);

  bool success = writeFile(filename, jsonString);
  if (success) {
    Serial.println("System config saved");
    logInfo("System config saved");
  } else {
    Serial.println("Failed to save system config");
    logError("Failed to save system config");
  }

  return success;
}

bool StorageManager::loadSystemConfig(JsonDocument& config) {
  if (!sdInitialized) return false;

  String filename = configPath + "system_config.json";
  String jsonString = readFile(filename);

  if (jsonString.isEmpty()) {
    Serial.println("System config file not found, using defaults");
    return false;
  }

  DeserializationError error = deserializeJson(config, jsonString);
  if (error) {
    Serial.printf("Failed to parse system config: %s\n", error.c_str());
    logError("Failed to parse system config: " + String(error.c_str()));
    return false;
  }

  Serial.println("System config loaded");
  return true;
}

bool StorageManager::archiveGame(const String& gameId) {
  if (!sdInitialized || !hasCurrentGame()) {
    return false;
  }

  String archivePath = "/chess_games/games/game_" + gameId + ".json";
  String currentData = readFile(currentGamePath);

  if (currentData.isEmpty()) {
    Serial.println("No current game data to archive");
    return false;
  }

  bool success = writeFile(archivePath, currentData);
  if (success) {
    Serial.printf("Game archived: %s\n", gameId.c_str());
    logInfo("Game archived: " + gameId);
    deleteCurrentGame();
  } else {
    Serial.printf("Failed to archive game: %s\n", gameId.c_str());
    logError("Failed to archive game: " + gameId);
  }

  return success;
}

bool StorageManager::listGames(JsonDocument& gamesList) {
  if (!sdInitialized) return false;

  File dir = SD_MMC.open("/chess_games/games");
  if (!dir || !dir.isDirectory()) {
    Serial.println("Games directory not found");
    return false;
  }

  JsonArray games = gamesList["games"].to<JsonArray>();

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".json") && filename.startsWith("game_")) {
        JsonObject gameInfo = games.add<JsonObject>();
        gameInfo["filename"] = filename;
        gameInfo["size"] = file.size();

        // Try to extract basic info from the file
        String gameData = readFile("/chess_games/games/" + filename);
        if (!gameData.isEmpty()) {
          JsonDocument gameDoc;
          if (deserializeJson(gameDoc, gameData) == DeserializationError::Ok) {
            gameInfo["gameId"] = gameDoc["gameId"].as<String>();
            gameInfo["status"] = gameDoc["status"].as<String>();
            gameInfo["moveCount"] = gameDoc["moveCount"].as<int>();
            gameInfo["timestamp"] = gameDoc["timestamp"].as<String>();
          }
        }
      }
    }
    file = dir.openNextFile();
  }

  dir.close();
  return true;
}

void StorageManager::logMove(const String& gameId, const String& move, const String& player) {
  String timestamp = String(millis());
  String logEntry = timestamp + " [MOVE] Game:" + gameId + " Player:" + player + " Move:" + move;
  logEvent(logEntry);
}

void StorageManager::logError(const String& error) {
  String timestamp = String(millis());
  String logEntry = timestamp + " [ERROR] " + error;
  logEvent(logEntry);

  // Also write to error log
  String errorLogPath = logsPath + "error_log.txt";
  String existingLog = readFile(errorLogPath);
  String newLog = existingLog + "\n" + logEntry;
  writeFile(errorLogPath, newLog);
}

void StorageManager::logInfo(const String& info) {
  String timestamp = String(millis());
  String logEntry = timestamp + " [INFO] " + info;
  logEvent(logEntry);
}

void StorageManager::logEvent(const String& event) {
  if (!sdInitialized) return;

  // Create daily log file
  String today = String(millis() / 86400000); // Simple day counter
  String logFile = logsPath + "game_log_" + today + ".txt";

  String existingLog = readFile(logFile);
  String newLog = existingLog + "\n" + event;

  // Keep log files manageable (last 100 lines)
  int lineCount = 0;
  for (int i = 0; i < newLog.length(); i++) {
    if (newLog.charAt(i) == '\n') lineCount++;
  }

  if (lineCount > 100) {
    // Keep only last 50 lines
    int linesToKeep = 50;
    int currentLines = 0;
    int startPos = newLog.length();

    for (int i = newLog.length() - 1; i >= 0; i--) {
      if (newLog.charAt(i) == '\n') {
        currentLines++;
        if (currentLines >= linesToKeep) {
          startPos = i + 1;
          break;
        }
      }
    }
    newLog = newLog.substring(startPos);
  }

  writeFile(logFile, newLog);
  Serial.println("LOG: " + event);
}

bool StorageManager::writeFile(const String& path, const String& data) {
  if (!sdInitialized) return false;

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to open file for writing: %s\n", path.c_str());
    return false;
  }

  size_t bytesWritten = file.print(data);
  file.close();

  return bytesWritten == data.length();
}

String StorageManager::readFile(const String& path) {
  if (!sdInitialized) return "";

  File file = SD_MMC.open(path);
  if (!file) {
    return "";
  }

  String content = "";
  while (file.available()) {
    content += char(file.read());
  }
  file.close();

  return content;
}

uint64_t StorageManager::getAvailableSpace() {
  if (!sdInitialized) return 0;

  // ESP32 SD_MMC library doesn't provide direct free space query
  // Return card size as approximation
  return SD_MMC.cardSize();
}

bool StorageManager::cleanupOldGames(int keepCount) {
  if (!sdInitialized) return false;

  File dir = SD_MMC.open("/chess_games/games");
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  // Count game files
  int gameCount = 0;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory() && String(file.name()).startsWith("game_") && String(file.name()).endsWith(".json")) {
      gameCount++;
    }
    file = dir.openNextFile();
  }
  dir.close();

  if (gameCount <= keepCount) {
    return true; // No cleanup needed
  }

  // Simple cleanup - remove oldest files
  // In a more sophisticated implementation, you'd sort by timestamp
  dir = SD_MMC.open("/chess_games/games");
  int filesToDelete = gameCount - keepCount;
  int deletedCount = 0;

  file = dir.openNextFile();
  while (file && deletedCount < filesToDelete) {
    String filename = file.name();
    if (!file.isDirectory() && filename.startsWith("game_") && filename.endsWith(".json")) {
      String fullPath = "/chess_games/games/" + filename;
      file.close();
      if (SD_MMC.remove(fullPath)) {
        deletedCount++;
        Serial.printf("Deleted old game file: %s\n", filename.c_str());
        logInfo("Deleted old game file: " + filename);
      }
      file = dir.openNextFile();
    } else {
      file = dir.openNextFile();
    }
  }

  dir.close();
  return deletedCount == filesToDelete;
}