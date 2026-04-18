// #pragma once
// #include <vector>
// #include <stack>
// #include <string>
// #include <unordered_map>
// #include <algorithm>
// #include <climits>
// #include <random>
// #include <sstream>

// // ============================================================
// //  PIECE TYPES & COLORS  (DSA: Enum-based type system)
// // ============================================================
// enum PieceType { EMPTY=0, PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6 };
// enum Color     { NONE=0,  WHITE=1, BLACK=2 };

// inline Color opponent(Color c) { return (c == WHITE) ? BLACK : WHITE; }

// // ============================================================
// //  PIECE STRUCT
// // ============================================================
// struct Piece {
//     PieceType type;
//     Color     color;
//     Piece() : type(EMPTY), color(NONE) {}
//     Piece(PieceType t, Color c) : type(t), color(c) {}
//     bool isEmpty() const { return type == EMPTY; }
// };

// // ============================================================
// //  MOVE STRUCT  (DSA: stored in Stack for history)
// // ============================================================
// struct Move {
//     int  fromRow, fromCol, toRow, toCol;
//     PieceType promotion;        // used if pawn reaches last rank
//     bool isEnPassant;
//     bool isCastleKingside;
//     bool isCastleQueenside;

//     // Saved state for undo (DSA: full state snapshot in stack node)
//     Piece capturedPiece;
//     bool  savedWhiteKingMoved,  savedBlackKingMoved;
//     bool  savedWhiteRookAMoved, savedWhiteRookHMoved;
//     bool  savedBlackRookAMoved, savedBlackRookHMoved;
//     int   savedEnPassantCol;   // -1 = none
//     bool  savedEnPassantRow;

//     Move() : fromRow(0),fromCol(0),toRow(0),toCol(0),
//              promotion(EMPTY),isEnPassant(false),
//              isCastleKingside(false),isCastleQueenside(false),
//              savedEnPassantCol(-1),savedEnPassantRow(false) {}

//     Move(int fr,int fc,int tr,int tc,PieceType promo=EMPTY)
//         : fromRow(fr),fromCol(fc),toRow(tr),toCol(tc),
//           promotion(promo),isEnPassant(false),
//           isCastleKingside(false),isCastleQueenside(false),
//           savedEnPassantCol(-1),savedEnPassantRow(false) {}

//     std::string toAlgebraic() const;
// };

// // ============================================================
// //  TRANSPOSITION TABLE ENTRY  (DSA: Hash Map for memoization)
// // ============================================================
// enum NodeType { EXACT, LOWER_BOUND, UPPER_BOUND };
// struct TTEntry {
//     int      score;
//     int      depth;
//     NodeType type;
//     Move     bestMove;
// };

// // ============================================================
// //  CHESS ENGINE CLASS
// // ============================================================
// class ChessEngine {
// public:
//     // Board state
//     Piece board[8][8];
//     Color currentPlayer;
//     bool  whiteKingMoved,  blackKingMoved;
//     bool  whiteRookAMoved, whiteRookHMoved;   // a-file, h-file
//     bool  blackRookAMoved, blackRookHMoved;
//     int   enPassantCol;   // col where en-passant capture is possible, -1 = none
//     int   enPassantRow;   // row of the pawn that just moved 2 squares
//     int   halfMoveClock;  // for 50-move rule
//     int   fullMoveNumber;

//     // DSA: Stack for move history (undo/redo)
//     std::stack<Move> moveHistory;

//     // DSA: HashMap for transposition table (alpha-beta memoization)
//     std::unordered_map<uint64_t, TTEntry> transpositionTable;

//     // Zobrist keys for hashing (DSA: Random number table for position hashing)
//     uint64_t zobristTable[8][8][13]; // [row][col][piece_index]
//     uint64_t zobristBlackToMove;
//     uint64_t currentZobristKey;

//     // Game status
//     enum GameStatus { PLAYING, WHITE_WINS, BLACK_WINS, STALEMATE, DRAW_50_MOVE, DRAW_REPETITION };
//     GameStatus gameStatus;

//     // Repetition detection (DSA: HashMap for counting positions)
//     std::unordered_map<uint64_t, int> positionCount;

//     ChessEngine();
//     void resetBoard();
//     void initZobrist();

//     // ── Move Generation ──────────────────────────────────────
//     std::vector<Move> getLegalMoves(Color color);
//     std::vector<Move> getPseudoLegalMoves(Color color);
//     std::vector<Move> getPseudoLegalMovesForPiece(int row, int col);
//     std::vector<Move> getLegalMovesForSquare(int row, int col);

//     // ── Move Execution ───────────────────────────────────────
//     bool makeMove(int fromRow, int fromCol, int toRow, int toCol,
//                   PieceType promotion = QUEEN);
//     void undoLastMove();
//     bool isValidMove(int fromRow, int fromCol, int toRow, int toCol);

//     // ── Game State Queries ────────────────────────────────────
//     bool       isInCheck(Color color);
//     bool       isCheckmate(Color color);
//     bool       isStalemate(Color color);
//     GameStatus getGameStatus();
//     std::string getStatusString();

//     // ── AI Bot (Minimax + Alpha-Beta) ─────────────────────────
//     Move getBotMove(int difficulty);  // difficulty: 1=easy,2=medium,3=hard,4=expert

//     // ── Serialization for HTTP API ────────────────────────────
//     std::string getBoardJSON();
//     std::string getLegalMovesJSON(int row, int col);
//     std::string getGameStateJSON();

// private:
//     // Internal move execution (without legality checks)
//     void applyMove(Move& m);
//     void unapplyMove(Move& m);

//     // Square attack detection
//     bool isSquareAttacked(int row, int col, Color byColor);
//     std::vector<Move> generatePawnMoves(int row, int col, Color color);
//     std::vector<Move> generateKnightMoves(int row, int col, Color color);
//     std::vector<Move> generateBishopMoves(int row, int col, Color color);
//     std::vector<Move> generateRookMoves(int row, int col, Color color);
//     std::vector<Move> generateQueenMoves(int row, int col, Color color);
//     std::vector<Move> generateKingMoves(int row, int col, Color color);

//     // DSA: Minimax tree search with Alpha-Beta pruning
//     int minimax(int depth, int alpha, int beta, bool maximizing, Color rootColor);
//     int quiescenceSearch(int alpha, int beta, Color color, int qdepth);

//     // Evaluation function
//     int evaluate(Color perspective);
//     int evaluatePiece(PieceType type, Color color, int row, int col);

//     // Zobrist hashing
//     uint64_t computeZobrist();
//     void     updateZobristForMove(Move& m);
//     int      pieceIndex(Piece p);

//     // Move ordering (DSA: heuristic sort for better alpha-beta)
//     void orderMoves(std::vector<Move>& moves, Color color);
//     int  mvvLva(const Move& m);   // Most Valuable Victim – Least Valuable Attacker

//     // Piece-square tables for positional evaluation
//     static const int PST_PAWN[8][8];
//     static const int PST_KNIGHT[8][8];
//     static const int PST_BISHOP[8][8];
//     static const int PST_ROOK[8][8];
//     static const int PST_QUEEN[8][8];
//     static const int PST_KING_MID[8][8];
//     static const int PST_KING_END[8][8];
//     static const int PIECE_VALUE[7];
// };
