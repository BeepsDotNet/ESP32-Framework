#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class GameController; // Forward declaration
class GeminiAPI; // Forward declaration
class SessionManager; // Forward declaration

// Global serial log event source (for broadcasting serial messages to browser)
extern AsyncEventSource* g_serialLogEventSource;

class WebInterface {
private:
  AsyncWebServer* server;
  GameController* gameController;
  GeminiAPI* geminiAPI;
  SessionManager* sessionManager;

  // Chess board state tracking
  String currentBoard[8][8]; // Current board state
  bool boardInitialized;
  bool processingMove; // Flag to prevent race conditions during move processing
  bool isWhiteTurn; // Track whose turn it is

  // Special move tracking
  bool whiteKingMoved;
  bool blackKingMoved;
  bool whiteKingsideRookMoved;
  bool whiteQueensideRookMoved;
  bool blackKingsideRookMoved;
  bool blackQueensideRookMoved;
  int enPassantColumn;    // -1 if no en passant available
  bool enPassantIsWhite;  // Which color can capture en passant

  // Captured pieces tracking
  String capturedWhitePieces[16]; // Max 16 pieces can be captured
  String capturedBlackPieces[16]; // Max 16 pieces can be captured
  int capturedWhiteCount;
  int capturedBlackCount;

  // Move history for undo/redo functionality
  struct MoveHistoryEntry {
    String beforeState[8][8];     // Board state before the move
    String afterState[8][8];      // Board state after the move
    bool beforeWhiteKingMoved;
    bool beforeBlackKingMoved;
    bool beforeWhiteKingsideRookMoved;
    bool beforeWhiteQueensideRookMoved;
    bool beforeBlackKingsideRookMoved;
    bool beforeBlackQueensideRookMoved;
    int beforeEnPassantColumn;
    bool beforeEnPassantIsWhite;
    bool afterWhiteKingMoved;
    bool afterBlackKingMoved;
    bool afterWhiteKingsideRookMoved;
    bool afterWhiteQueensideRookMoved;
    bool afterBlackKingsideRookMoved;
    bool afterBlackQueensideRookMoved;
    int afterEnPassantColumn;
    bool afterEnPassantIsWhite;
  };
  MoveHistoryEntry moveHistory[20];  // Store last 20 moves
  int historyCount;                  // Number of stored moves
  int currentHistoryIndex;           // Current position in history (-1 = latest)

  // File upload tracking
  String currentUploadFilename;      // Track the filename being uploaded

  // HTML generation methods - minimal fallback only
  String getMinimalFallbackHTML(); // New minimal fallback function
  String generateChessBoard();
  String generateBoardJSON();
  String getPieceImageName(String pieceCode);

  // Board state management
  void initializeBoard();
  void serveImageFile(AsyncWebServerRequest* request, const char* filename);
  // REMOVED: serveStreamedHTML() - no longer needed
  size_t generateHTMLChunk(uint8_t *buffer, size_t maxLen, size_t index); // Returns 0
  String generateCompactHTML(); // Redirects to minimal fallback
  void serveFileFromSD(AsyncWebServerRequest* request, const char* filename);
  size_t streamFileChunk(uint8_t *buffer, size_t maxLen, size_t index, const char* filename);
  void handleHTMLUpload(AsyncWebServerRequest* request);
  void handleFileCleanup(AsyncWebServerRequest* request);

  // Chunked upload methods
  void handleUploadStart(AsyncWebServerRequest* request);
  void handleUploadChunkBody(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleUploadFinish(AsyncWebServerRequest* request);

public:
  WebInterface();

  void begin(AsyncWebServer* webServer, SessionManager* sessMgr = nullptr);
  void setGameController(GameController* controller);
  void setGeminiAPI(GeminiAPI* api);

  // Public board management
  void applyMove(const String& move, bool isWhite);

  // API endpoints
  void handleGetStatus(AsyncWebServerRequest* request);
  void handleGetBoard(AsyncWebServerRequest* request);
  void handleNewGame(AsyncWebServerRequest* request);
  void handleResetGame(AsyncWebServerRequest* request);
  void handleUserMove(AsyncWebServerRequest* request);
  void handleRequestAIMove(AsyncWebServerRequest* request);
  void handleGetDebugOptions(AsyncWebServerRequest* request);
  void handleDebugButtonColor(AsyncWebServerRequest* request);
  void handleGetCapturedPieces(AsyncWebServerRequest* request);
  void handleLogMessage(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handleClearLogs(AsyncWebServerRequest* request);
  void handleGetConsoleLog(AsyncWebServerRequest* request);
  void handleGetSerialLog(AsyncWebServerRequest* request);
  void handleGetDebugLog(AsyncWebServerRequest* request);
  void handleEject(AsyncWebServerRequest* request);
  void handleReboot(AsyncWebServerRequest* request);
  void handleFileRead(AsyncWebServerRequest* request);
  void handleFileWrite(AsyncWebServerRequest* request);

  // AI move handling
  void triggerAIMove();

  // Move processing
  bool applyMoveToBoard(const String& move);
  bool parseMove(const String& move, int& fromRow, int& fromCol, int& toRow, int& toCol);

  // Chess rule validation helpers
  bool isValidMove(int fromRow, int fromCol, int toRow, int toCol);
  bool isPawnMoveValid(int fromRow, int fromCol, int toRow, int toCol, bool isWhite);
  bool isRookMoveValid(int fromRow, int fromCol, int toRow, int toCol);
  bool isBishopMoveValid(int fromRow, int fromCol, int toRow, int toCol);
  bool isKnightMoveValid(int fromRow, int fromCol, int toRow, int toCol);
  bool isQueenMoveValid(int fromRow, int fromCol, int toRow, int toCol);
  bool isKingMoveValid(int fromRow, int fromCol, int toRow, int toCol);
  bool isPathClear(int fromRow, int fromCol, int toRow, int toCol);
  bool isPieceWhite(const String& piece);
  char getPieceType(const String& piece);

  // Check detection functions
  bool isKingInCheck(bool isWhiteKing);
  bool wouldMoveLeaveKingInCheck(int fromRow, int fromCol, int toRow, int toCol);
  bool isSquareAttackedBy(int row, int col, bool byWhite);
  void findKing(bool isWhite, int& kingRow, int& kingCol);
  bool canPieceAttackSquare(int pieceRow, int pieceCol, int targetRow, int targetCol);
  bool hasLegalMoves(bool isWhite);

  // Special move functions
  bool isCastlingMove(int fromRow, int fromCol, int toRow, int toCol);
  bool canCastle(bool isWhite, bool kingside);
  void performCastle(bool isWhite, bool kingside);
  bool isEnPassantCapture(int fromRow, int fromCol, int toRow, int toCol);
  void performEnPassant(int fromRow, int fromCol, int toRow, int toCol);
  bool isPawnPromotion(int fromRow, int fromCol, int toRow, int toCol);
  void promotePawn(int row, int col, char promoteTo, bool isWhite);
  void updateSpecialMoveTracking(int fromRow, int fromCol, int toRow, int toCol);
  void resetSpecialMoveFlags();

  // Move history functions
  void saveCurrentBoardState();
  void saveAfterMoveState();
  bool undoLastWhiteMove();
  bool redoLastWhiteMove();
  void initializeMoveHistory();
  void handleUndoMove(AsyncWebServerRequest* request);
  void handleRedoMove(AsyncWebServerRequest* request);

  // Captured pieces functions
  void initializeCapturedPieces();
  void addCapturedPiece(const String& piece);
  String getPieceUnicode(String pieceCode);
  String generateCapturedPiecesHTML();
};

#endif