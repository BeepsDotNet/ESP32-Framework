#include "ChessEngine.h"

ChessEngine::ChessEngine() {
  currentPlayer = WHITE;
  whiteKingMoved = blackKingMoved = false;
  whiteRookKingsideMoved = whiteRookQueensideMoved = false;
  blackRookKingsideMoved = blackRookQueensideMoved = false;
  halfMoveClock = 0;
  fullMoveNumber = 1;
}

void ChessEngine::begin() {
  initializeBoard();
  Serial.println("Chess engine initialized");
}

void ChessEngine::resetGame() {
  initializeBoard();
  currentPlayer = WHITE;
  whiteKingMoved = blackKingMoved = false;
  whiteRookKingsideMoved = whiteRookQueensideMoved = false;
  blackRookKingsideMoved = blackRookQueensideMoved = false;
  halfMoveClock = 0;
  fullMoveNumber = 1;
  moveHistory = "";
  Serial.println("Chess game reset");
}

void ChessEngine::initializeBoard() {
  // Clear board
  for (int row = 0; row < BOARD_SIZE; row++) {
    for (int col = 0; col < BOARD_SIZE; col++) {
      board[row][col].type = EMPTY;
      board[row][col].color = WHITE;
    }
  }

  // Place white pieces
  board[7][0] = {ROOK, WHITE};   board[7][1] = {KNIGHT, WHITE};
  board[7][2] = {BISHOP, WHITE}; board[7][3] = {QUEEN, WHITE};
  board[7][4] = {KING, WHITE};   board[7][5] = {BISHOP, WHITE};
  board[7][6] = {KNIGHT, WHITE}; board[7][7] = {ROOK, WHITE};

  for (int col = 0; col < BOARD_SIZE; col++) {
    board[6][col] = {PAWN, WHITE};
  }

  // Place black pieces
  board[0][0] = {ROOK, BLACK};   board[0][1] = {KNIGHT, BLACK};
  board[0][2] = {BISHOP, BLACK}; board[0][3] = {QUEEN, BLACK};
  board[0][4] = {KING, BLACK};   board[0][5] = {BISHOP, BLACK};
  board[0][6] = {KNIGHT, BLACK}; board[0][7] = {ROOK, BLACK};

  for (int col = 0; col < BOARD_SIZE; col++) {
    board[1][col] = {PAWN, BLACK};
  }
}

Move ChessEngine::parseMove(const String& moveStr) {
  Move move = {-1, -1, -1, -1, EMPTY, false};

  if (moveStr.isEmpty() || moveStr.length() < 2) {
    return move;
  }

  String cleanMove = moveStr;
  cleanMove.trim();
  cleanMove.toUpperCase();

  // Handle castling
  if (cleanMove == "O-O" || cleanMove == "0-0") {
    // Kingside castling
    if (currentPlayer == WHITE) {
      move = {7, 4, 7, 6, EMPTY, true}; // King e1 to g1
    } else {
      move = {0, 4, 0, 6, EMPTY, true}; // King e8 to g8
    }
    return move;
  }

  if (cleanMove == "O-O-O" || cleanMove == "0-0-0") {
    // Queenside castling
    if (currentPlayer == WHITE) {
      move = {7, 4, 7, 2, EMPTY, true}; // King e1 to c1
    } else {
      move = {0, 4, 0, 2, EMPTY, true}; // King e8 to c8
    }
    return move;
  }

  // Parse regular moves (simplified parsing)
  int len = cleanMove.length();
  char targetFile = cleanMove.charAt(len - 2);
  char targetRank = cleanMove.charAt(len - 1);

  if (targetFile < 'A' || targetFile > 'H' || targetRank < '1' || targetRank > '8') {
    return move;
  }

  int toCol = targetFile - 'A';
  int toRow = 8 - (targetRank - '0');

  // Determine piece type
  PieceType pieceType = PAWN;
  if (len > 2) {
    char firstChar = cleanMove.charAt(0);
    switch (firstChar) {
      case 'K': pieceType = KING; break;
      case 'Q': pieceType = QUEEN; break;
      case 'R': pieceType = ROOK; break;
      case 'B': pieceType = BISHOP; break;
      case 'N': pieceType = KNIGHT; break;
      default: pieceType = PAWN; break;
    }
  }

  // Find the piece that can make this move
  for (int fromRow = 0; fromRow < BOARD_SIZE; fromRow++) {
    for (int fromCol = 0; fromCol < BOARD_SIZE; fromCol++) {
      if (board[fromRow][fromCol].type == pieceType &&
          board[fromRow][fromCol].color == currentPlayer) {

        Move testMove = {fromRow, fromCol, toRow, toCol, EMPTY, false};
        if (isValidMove(testMove)) {
          testMove.isValid = true;
          return testMove;
        }
      }
    }
  }

  return move;
}

bool ChessEngine::playMove(const String& moveStr) {
  Move move = parseMove(moveStr);
  return playMove(move);
}

bool ChessEngine::playMove(const Move& move) {
  if (!move.isValid || !isValidMove(move)) {
    Serial.printf("Invalid move attempted\n");
    return false;
  }

  makeMove(move);

  // Update move history
  if (!moveHistory.isEmpty()) {
    moveHistory += " ";
  }

  // Convert move back to algebraic notation for history
  String algebraicMove = "";

  // Handle castling
  if (board[move.toRow][move.toCol].type == KING &&
      abs(move.fromCol - move.toCol) == 2) {
    if (move.toCol > move.fromCol) {
      algebraicMove = "O-O";
    } else {
      algebraicMove = "O-O-O";
    }
  } else {
    // Regular move notation
    if (board[move.toRow][move.toCol].type != PAWN) {
      switch (board[move.toRow][move.toCol].type) {
        case KING: algebraicMove += "K"; break;
        case QUEEN: algebraicMove += "Q"; break;
        case ROOK: algebraicMove += "R"; break;
        case BISHOP: algebraicMove += "B"; break;
        case KNIGHT: algebraicMove += "N"; break;
      }
    }

    algebraicMove += char('a' + move.toCol);
    algebraicMove += char('1' + (7 - move.toRow));
  }

  moveHistory += algebraicMove;

  // Switch players
  currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;

  if (currentPlayer == WHITE) {
    fullMoveNumber++;
  }

  Serial.printf("Move played: %s\n", algebraicMove.c_str());
  return true;
}

bool ChessEngine::isValidMove(const Move& move) {
  // Basic bounds checking
  if (move.fromRow < 0 || move.fromRow >= BOARD_SIZE ||
      move.fromCol < 0 || move.fromCol >= BOARD_SIZE ||
      move.toRow < 0 || move.toRow >= BOARD_SIZE ||
      move.toCol < 0 || move.toCol >= BOARD_SIZE) {
    return false;
  }

  // Can't move to same square
  if (move.fromRow == move.toRow && move.fromCol == move.toCol) {
    return false;
  }

  Piece fromPiece = board[move.fromRow][move.fromCol];
  Piece toPiece = board[move.toRow][move.toCol];

  // Must have a piece to move
  if (fromPiece.type == EMPTY) {
    return false;
  }

  // Must be moving own piece
  if (fromPiece.color != currentPlayer) {
    return false;
  }

  // Can't capture own piece
  if (toPiece.type != EMPTY && toPiece.color == currentPlayer) {
    return false;
  }

  // Piece-specific movement validation
  int rowDiff = abs(move.toRow - move.fromRow);
  int colDiff = abs(move.toCol - move.fromCol);

  switch (fromPiece.type) {
    case PAWN: {
      int direction = (fromPiece.color == WHITE) ? -1 : 1;
      int startRow = (fromPiece.color == WHITE) ? 6 : 1;

      // Forward move
      if (move.toCol == move.fromCol) {
        if (toPiece.type != EMPTY) return false;
        if (move.toRow == move.fromRow + direction) return true;
        if (move.fromRow == startRow && move.toRow == move.fromRow + 2 * direction) {
          return board[move.fromRow + direction][move.fromCol].type == EMPTY;
        }
        return false;
      }
      // Diagonal capture
      else if (colDiff == 1 && move.toRow == move.fromRow + direction) {
        return toPiece.type != EMPTY;
      }
      return false;
    }

    case ROOK:
      if (rowDiff != 0 && colDiff != 0) return false;
      // Check path is clear
      // (Simplified - should check all squares in between)
      return true;

    case KNIGHT:
      return (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);

    case BISHOP:
      if (rowDiff != colDiff) return false;
      // Check diagonal path is clear
      // (Simplified - should check all squares in between)
      return true;

    case QUEEN:
      if (rowDiff != 0 && colDiff != 0 && rowDiff != colDiff) return false;
      // Check path is clear
      // (Simplified - should check all squares in between)
      return true;

    case KING:
      if (rowDiff > 1 || colDiff > 1) {
        // Castling
        if (rowDiff == 0 && colDiff == 2) {
          return canCastle(currentPlayer, move.toCol > move.fromCol);
        }
        return false;
      }
      return true;

    default:
      return false;
  }
}

void ChessEngine::makeMove(const Move& move) {
  Piece movingPiece = board[move.fromRow][move.fromCol];

  // Handle castling
  if (movingPiece.type == KING && abs(move.fromCol - move.toCol) == 2) {
    // Move king
    board[move.toRow][move.toCol] = movingPiece;
    board[move.fromRow][move.fromCol] = {EMPTY, WHITE};

    // Move rook
    if (move.toCol > move.fromCol) { // Kingside
      board[move.toRow][5] = board[move.toRow][7];
      board[move.toRow][7] = {EMPTY, WHITE};
    } else { // Queenside
      board[move.toRow][3] = board[move.toRow][0];
      board[move.toRow][0] = {EMPTY, WHITE};
    }
  } else {
    // Regular move
    board[move.toRow][move.toCol] = movingPiece;
    board[move.fromRow][move.fromCol] = {EMPTY, WHITE};
  }

  // Update castling rights
  if (movingPiece.type == KING) {
    if (movingPiece.color == WHITE) {
      whiteKingMoved = true;
    } else {
      blackKingMoved = true;
    }
  } else if (movingPiece.type == ROOK) {
    if (movingPiece.color == WHITE) {
      if (move.fromCol == 0) whiteRookQueensideMoved = true;
      if (move.fromCol == 7) whiteRookKingsideMoved = true;
    } else {
      if (move.fromCol == 0) blackRookQueensideMoved = true;
      if (move.fromCol == 7) blackRookKingsideMoved = true;
    }
  }
}

bool ChessEngine::canCastle(PieceColor color, bool kingside) {
  if (color == WHITE && whiteKingMoved) return false;
  if (color == BLACK && blackKingMoved) return false;

  if (kingside) {
    if (color == WHITE && whiteRookKingsideMoved) return false;
    if (color == BLACK && blackRookKingsideMoved) return false;
  } else {
    if (color == WHITE && whiteRookQueensideMoved) return false;
    if (color == BLACK && blackRookQueensideMoved) return false;
  }

  // Check if squares between king and rook are empty
  // (Simplified check)
  return true;
}

bool ChessEngine::isInCheck(PieceColor color) {
  // Find king position
  int kingRow = -1, kingCol = -1;
  for (int row = 0; row < BOARD_SIZE; row++) {
    for (int col = 0; col < BOARD_SIZE; col++) {
      if (board[row][col].type == KING && board[row][col].color == color) {
        kingRow = row;
        kingCol = col;
        break;
      }
    }
    if (kingRow != -1) break;
  }

  if (kingRow == -1) return false; // No king found

  return isSquareAttacked(kingRow, kingCol, (color == WHITE) ? BLACK : WHITE);
}

bool ChessEngine::isSquareAttacked(int row, int col, PieceColor attackingColor) {
  // Simplified attack checking
  // In a full implementation, this would check all possible attacks
  return false;
}

GameResult ChessEngine::getGameResult() {
  if (isCheckmate(WHITE)) return BLACK_WINS;
  if (isCheckmate(BLACK)) return WHITE_WINS;
  if (isStalemate(WHITE) || isStalemate(BLACK)) return DRAW_STALEMATE;
  if (halfMoveClock >= 100) return DRAW_50MOVE;

  return GAME_ONGOING;
}

bool ChessEngine::isCheckmate(PieceColor color) {
  // Simplified checkmate detection
  return false;
}

bool ChessEngine::isStalemate(PieceColor color) {
  // Simplified stalemate detection
  return false;
}

bool ChessEngine::isGameOver() {
  return getGameResult() != GAME_ONGOING;
}

String ChessEngine::getFEN() const {
  String fen = "";

  // Piece placement
  for (int row = 0; row < BOARD_SIZE; row++) {
    int emptyCount = 0;
    for (int col = 0; col < BOARD_SIZE; col++) {
      Piece p = board[row][col];
      if (p.type == EMPTY) {
        emptyCount++;
      } else {
        if (emptyCount > 0) {
          fen += String(emptyCount);
          emptyCount = 0;
        }

        char pieceChar = ' ';
        switch (p.type) {
          case PAWN: pieceChar = 'p'; break;
          case ROOK: pieceChar = 'r'; break;
          case KNIGHT: pieceChar = 'n'; break;
          case BISHOP: pieceChar = 'b'; break;
          case QUEEN: pieceChar = 'q'; break;
          case KING: pieceChar = 'k'; break;
        }

        if (p.color == WHITE) {
          pieceChar = toupper(pieceChar);
        }

        fen += pieceChar;
      }
    }
    if (emptyCount > 0) {
      fen += String(emptyCount);
    }
    if (row < 7) fen += "/";
  }

  // Active color
  fen += (currentPlayer == WHITE) ? " w " : " b ";

  // Castling rights
  String castling = "";
  if (!whiteKingMoved && !whiteRookKingsideMoved) castling += "K";
  if (!whiteKingMoved && !whiteRookQueensideMoved) castling += "Q";
  if (!blackKingMoved && !blackRookKingsideMoved) castling += "k";
  if (!blackKingMoved && !blackRookQueensideMoved) castling += "q";
  if (castling.isEmpty()) castling = "-";
  fen += castling;

  // En passant target square (simplified)
  fen += " -";

  // Halfmove clock and fullmove number
  fen += " " + String(halfMoveClock) + " " + String(fullMoveNumber);

  return fen;
}

String ChessEngine::getPGN() const {
  // Simplified PGN generation
  return moveHistory;
}

String ChessEngine::getPositionString() const {
  return getFEN();
}

Piece ChessEngine::getPiece(int row, int col) const {
  if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
    return board[row][col];
  }
  return {EMPTY, WHITE};
}

void ChessEngine::printBoard() const {
  Serial.println("  a b c d e f g h");
  for (int row = 0; row < BOARD_SIZE; row++) {
    Serial.printf("%d ", 8 - row);
    for (int col = 0; col < BOARD_SIZE; col++) {
      char piece = '.';
      Piece p = board[row][col];

      if (p.type != EMPTY) {
        switch (p.type) {
          case PAWN: piece = 'P'; break;
          case ROOK: piece = 'R'; break;
          case KNIGHT: piece = 'N'; break;
          case BISHOP: piece = 'B'; break;
          case QUEEN: piece = 'Q'; break;
          case KING: piece = 'K'; break;
        }
        if (p.color == BLACK) {
          piece = tolower(piece);
        }
      }
      Serial.printf("%c ", piece);
    }
    Serial.printf("%d\n", 8 - row);
  }
  Serial.println("  a b c d e f g h");
}