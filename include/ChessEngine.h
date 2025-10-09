#ifndef CHESS_ENGINE_H
#define CHESS_ENGINE_H

#include <Arduino.h>

#define BOARD_SIZE 8

enum PieceType {
  EMPTY = 0,
  PAWN = 1,
  ROOK = 2,
  KNIGHT = 3,
  BISHOP = 4,
  QUEEN = 5,
  KING = 6
};

enum PieceColor {
  WHITE = 0,
  BLACK = 1
};

struct Piece {
  PieceType type;
  PieceColor color;
};

struct Move {
  int fromRow, fromCol;
  int toRow, toCol;
  PieceType promotion;
  bool isValid;
};

enum GameResult {
  GAME_ONGOING,
  WHITE_WINS,
  BLACK_WINS,
  DRAW_STALEMATE,
  DRAW_INSUFFICIENT,
  DRAW_50MOVE,
  DRAW_REPETITION
};

class ChessEngine {
private:
  Piece board[BOARD_SIZE][BOARD_SIZE];
  PieceColor currentPlayer;
  bool whiteKingMoved, blackKingMoved;
  bool whiteRookKingsideMoved, whiteRookQueensideMoved;
  bool blackRookKingsideMoved, blackRookQueensideMoved;
  int halfMoveClock;
  int fullMoveNumber;
  String moveHistory;

  void initializeBoard();
  bool isValidMove(const Move& move);
  bool isInCheck(PieceColor color);
  bool isCheckmate(PieceColor color);
  bool isStalemate(PieceColor color);
  bool canCastle(PieceColor color, bool kingside);
  void makeMove(const Move& move);
  void undoMove(const Move& move);

public:
  ChessEngine();

  void begin();
  void resetGame();

  Move parseMove(const String& moveStr);
  bool playMove(const String& moveStr);
  bool playMove(const Move& move);

  String getFEN() const;
  String getPGN() const;
  String getPositionString() const;
  String getMoveHistory() const { return moveHistory; }

  PieceColor getCurrentPlayer() const { return currentPlayer; }
  GameResult getGameResult();
  bool isGameOver();

  // Board state queries
  Piece getPiece(int row, int col) const;
  bool isSquareAttacked(int row, int col, PieceColor attackingColor);
  void printBoard() const;
};

#endif