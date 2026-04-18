#include "chess_engine.h"
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <chrono>
#include <iostream>

// ============================================================
//  PIECE-SQUARE TABLES  (White's perspective, row 0 = white's back rank)
// ============================================================
const int ChessEngine::PIECE_VALUE[7] = { 0, 100, 320, 330, 500, 900, 20000 };

const int ChessEngine::PST_PAWN[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0 },
    { 50, 50, 50, 50, 50, 50, 50, 50 },
    { 10, 10, 20, 30, 30, 20, 10, 10 },
    {  5,  5, 10, 25, 25, 10,  5,  5 },
    {  0,  0,  0, 20, 20,  0,  0,  0 },
    {  5, -5,-10,  0,  0,-10, -5,  5 },
    {  5, 10, 10,-20,-20, 10, 10,  5 },
    {  0,  0,  0,  0,  0,  0,  0,  0 }
};
const int ChessEngine::PST_KNIGHT[8][8] = {
    {-50,-40,-30,-30,-30,-30,-40,-50},
    {-40,-20,  0,  0,  0,  0,-20,-40},
    {-30,  0, 10, 15, 15, 10,  0,-30},
    {-30,  5, 15, 20, 20, 15,  5,-30},
    {-30,  0, 15, 20, 20, 15,  0,-30},
    {-30,  5, 10, 15, 15, 10,  5,-30},
    {-40,-20,  0,  5,  5,  0,-20,-40},
    {-50,-40,-30,-30,-30,-30,-40,-50}
};
const int ChessEngine::PST_BISHOP[8][8] = {
    {-20,-10,-10,-10,-10,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5, 10, 10,  5,  0,-10},
    {-10,  5,  5, 10, 10,  5,  5,-10},
    {-10,  0, 10, 10, 10, 10,  0,-10},
    {-10, 10, 10, 10, 10, 10, 10,-10},
    {-10,  5,  0,  0,  0,  0,  5,-10},
    {-20,-10,-10,-10,-10,-10,-10,-20}
};
const int ChessEngine::PST_ROOK[8][8] = {
    {  0,  0,  0,  0,  0,  0,  0,  0},
    {  5, 10, 10, 10, 10, 10, 10,  5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    { -5,  0,  0,  0,  0,  0,  0, -5},
    {  0,  0,  0,  5,  5,  0,  0,  0}
};
const int ChessEngine::PST_QUEEN[8][8] = {
    {-20,-10,-10, -5, -5,-10,-10,-20},
    {-10,  0,  0,  0,  0,  0,  0,-10},
    {-10,  0,  5,  5,  5,  5,  0,-10},
    { -5,  0,  5,  5,  5,  5,  0, -5},
    {  0,  0,  5,  5,  5,  5,  0, -5},
    {-10,  5,  5,  5,  5,  5,  0,-10},
    {-10,  0,  5,  0,  0,  0,  0,-10},
    {-20,-10,-10, -5, -5,-10,-10,-20}
};
const int ChessEngine::PST_KING_MID[8][8] = {
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-30,-40,-40,-50,-50,-40,-40,-30},
    {-20,-30,-30,-40,-40,-30,-30,-20},
    {-10,-20,-20,-20,-20,-20,-20,-10},
    { 20, 20,  0,  0,  0,  0, 20, 20},
    { 20, 30, 10,  0,  0, 10, 30, 20}
};
const int ChessEngine::PST_KING_END[8][8] = {
    {-50,-40,-30,-20,-20,-30,-40,-50},
    {-30,-20,-10,  0,  0,-10,-20,-30},
    {-30,-10, 20, 30, 30, 20,-10,-30},
    {-30,-10, 30, 40, 40, 30,-10,-30},
    {-30,-10, 30, 40, 40, 30,-10,-30},
    {-30,-10, 20, 30, 30, 20,-10,-30},
    {-30,-30,  0,  0,  0,  0,-30,-30},
    {-50,-30,-30,-30,-30,-30,-30,-50}
};

// ============================================================
//  CONSTRUCTOR & BOARD INIT
// ============================================================
ChessEngine::ChessEngine() {
    initZobrist();
    resetBoard();
}

void ChessEngine::initZobrist() {
    std::mt19937_64 rng(0xDEADBEEFCAFEBABE);
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++)
            for(int p=0;p<13;p++)
                zobristTable[r][c][p] = rng();
    zobristBlackToMove = rng();
}

int ChessEngine::pieceIndex(Piece p) {
    if(p.isEmpty()) return 0;
    return (p.color == WHITE ? 0 : 6) + (int)p.type;
}

uint64_t ChessEngine::computeZobrist() {
    uint64_t key = 0;
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++)
            key ^= zobristTable[r][c][pieceIndex(board[r][c])];
    if(currentPlayer == BLACK) key ^= zobristBlackToMove;
    return key;
}

void ChessEngine::resetBoard() {
    // Clear
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++)
            board[r][c] = Piece();

    // White pieces  (row 0 = rank 1 for white, row 7 = rank 8)
    // We store board so row 0 = rank 1, row 7 = rank 8 internally
    // White starts at rows 0,1 and Black at rows 6,7
    board[0][0]=Piece(ROOK,WHITE);   board[0][1]=Piece(KNIGHT,WHITE);
    board[0][2]=Piece(BISHOP,WHITE); board[0][3]=Piece(QUEEN,WHITE);
    board[0][4]=Piece(KING,WHITE);   board[0][5]=Piece(BISHOP,WHITE);
    board[0][6]=Piece(KNIGHT,WHITE); board[0][7]=Piece(ROOK,WHITE);
    for(int c=0;c<8;c++) board[1][c]=Piece(PAWN,WHITE);

    board[7][0]=Piece(ROOK,BLACK);   board[7][1]=Piece(KNIGHT,BLACK);
    board[7][2]=Piece(BISHOP,BLACK); board[7][3]=Piece(QUEEN,BLACK);
    board[7][4]=Piece(KING,BLACK);   board[7][5]=Piece(BISHOP,BLACK);
    board[7][6]=Piece(KNIGHT,BLACK); board[7][7]=Piece(ROOK,BLACK);
    for(int c=0;c<8;c++) board[6][c]=Piece(PAWN,BLACK);

    currentPlayer  = WHITE;
    whiteKingMoved = blackKingMoved = false;
    whiteRookAMoved= whiteRookHMoved= false;
    blackRookAMoved= blackRookHMoved= false;
    enPassantCol   = -1;
    enPassantRow   = -1;
    halfMoveClock  = 0;
    fullMoveNumber = 1;
    gameStatus     = PLAYING;
//stack 
    while(!moveHistory.empty()) {
         moveHistory.pop();
    }
    transpositionTable.clear();
    positionCount.clear();

    currentZobristKey = computeZobrist();
    positionCount[currentZobristKey] = 1;
}

// ============================================================
//  MOVE GENERATION
// ============================================================
std::vector<Move> ChessEngine::generatePawnMoves(int row, int col, Color color) {
    std::vector<Move> moves;
    int dir   = (color == WHITE) ? 1 : -1;
    int start = (color == WHITE) ? 1 : 6;
    int promo = (color == WHITE) ? 7 : 0;

    // One square forward
    int nr = row + dir;
    if(nr>=0 && nr<8 && board[nr][col].isEmpty()) {
        if(nr == promo) {
            moves.push_back(Move(row,col,nr,col,QUEEN));
            moves.push_back(Move(row,col,nr,col,ROOK));
            moves.push_back(Move(row,col,nr,col,BISHOP));
            moves.push_back(Move(row,col,nr,col,KNIGHT));
        } else {
            moves.push_back(Move(row,col,nr,col));
        }
        // Two squares from start
        if(row == start) {
            int nr2 = row + 2*dir;
            if(board[nr2][col].isEmpty()) {
                moves.push_back(Move(row,col,nr2,col));
            }
        }
    }
    // Captures
    for(int dc : {-1,1}) {
        int nc = col + dc;
        if(nc<0||nc>=8) continue;
        nr = row + dir;
        if(nr<0||nr>=8) continue;
        // Normal capture
        if(!board[nr][nc].isEmpty() && board[nr][nc].color != color) {
            if(nr == promo) {
                moves.push_back(Move(row,col,nr,nc,QUEEN));
                moves.push_back(Move(row,col,nr,nc,ROOK));
                moves.push_back(Move(row,col,nr,nc,BISHOP));
                moves.push_back(Move(row,col,nr,nc,KNIGHT));
            } else {
                moves.push_back(Move(row,col,nr,nc));
            }
        }
        // En passant
        if(nc == enPassantCol && row == enPassantRow) {
            Move m(row,col,nr,nc);
            m.isEnPassant = true;
            moves.push_back(m);
        }
    }
    return moves;
}

std::vector<Move> ChessEngine::generateKnightMoves(int row, int col, Color color) {
    std::vector<Move> moves;
    static const int dx[]={-2,-2,-1,-1, 1, 1, 2, 2};
    static const int dy[]={-1, 1,-2, 2,-2, 2,-1, 1};
    for(int i=0;i<8;i++){
        int nr=row+dx[i], nc=col+dy[i];
        if(nr<0||nr>=8||nc<0||nc>=8) continue;
        if(board[nr][nc].color == color) continue;
        moves.push_back(Move(row,col,nr,nc));
    }
    return moves;
}

std::vector<Move> ChessEngine::generateBishopMoves(int row, int col, Color color) {
    std::vector<Move> moves;
    static const int dx[]={-1,-1, 1, 1};
    static const int dy[]={-1, 1,-1, 1};
    for(int d=0;d<4;d++){
        for(int s=1;s<8;s++){
            int nr=row+s*dx[d], nc=col+s*dy[d];
            if(nr<0||nr>=8||nc<0||nc>=8) break;
            if(board[nr][nc].color == color) break;
            moves.push_back(Move(row,col,nr,nc));
            if(!board[nr][nc].isEmpty()) break;
        }
    }
    return moves;
}

std::vector<Move> ChessEngine::generateRookMoves(int row, int col, Color color) {
    std::vector<Move> moves;
    static const int dx[]={-1, 1, 0, 0};
    static const int dy[]={ 0, 0,-1, 1};
    for(int d=0;d<4;d++){
        for(int s=1;s<8;s++){
            int nr=row+s*dx[d], nc=col+s*dy[d];
            if(nr<0||nr>=8||nc<0||nc>=8) break;
            if(board[nr][nc].color == color) break;
            moves.push_back(Move(row,col,nr,nc));
            if(!board[nr][nc].isEmpty()) break;
        }
    }
    return moves;
}

std::vector<Move> ChessEngine::generateQueenMoves(int row, int col, Color color) {
    auto a = generateBishopMoves(row,col,color);
    auto b = generateRookMoves(row,col,color);
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

std::vector<Move> ChessEngine::generateKingMoves(int row, int col, Color color) {
    std::vector<Move> moves;
    for(int dr=-1;dr<=1;dr++)
        for(int dc=-1;dc<=1;dc++){
            if(dr==0&&dc==0) continue;
            int nr=row+dr, nc=col+dc;
            if(nr<0||nr>=8||nc<0||nc>=8) continue;
            if(board[nr][nc].color == color) continue;
            moves.push_back(Move(row,col,nr,nc));
        }

    // Castling
    bool kingMoved   = (color==WHITE) ? whiteKingMoved   : blackKingMoved;
    bool rookAMoved  = (color==WHITE) ? whiteRookAMoved  : blackRookAMoved;
    bool rookHMoved  = (color==WHITE) ? whiteRookHMoved  : blackRookHMoved;
    int  backRank    = (color==WHITE) ? 0 : 7;

    if(!kingMoved && !isInCheck(color)) {
        // Kingside
        if(!rookHMoved &&
           board[backRank][5].isEmpty() && board[backRank][6].isEmpty() &&
           !isSquareAttacked(backRank,5,opponent(color)) &&
           !isSquareAttacked(backRank,6,opponent(color))) {
            Move m(row,col,backRank,6);
            m.isCastleKingside = true;
            moves.push_back(m);
        }
        // Queenside
        if(!rookAMoved &&
           board[backRank][1].isEmpty() && board[backRank][2].isEmpty() && board[backRank][3].isEmpty() &&
           !isSquareAttacked(backRank,3,opponent(color)) &&
           !isSquareAttacked(backRank,2,opponent(color))) {
            Move m(row,col,backRank,2);
            m.isCastleQueenside = true;
            moves.push_back(m);
        }
    }
    return moves;
}

std::vector<Move> ChessEngine::getPseudoLegalMovesForPiece(int row, int col) {
    Piece& p = board[row][col];
    if(p.isEmpty()) return {};
    switch(p.type) {
        case PAWN:   return generatePawnMoves(row,col,p.color);
        case KNIGHT: return generateKnightMoves(row,col,p.color);
        case BISHOP: return generateBishopMoves(row,col,p.color);
        case ROOK:   return generateRookMoves(row,col,p.color);
        case QUEEN:  return generateQueenMoves(row,col,p.color);
        case KING:   return generateKingMoves(row,col,p.color);
        default:     return {};
    }
}

std::vector<Move> ChessEngine::getPseudoLegalMoves(Color color) {
    std::vector<Move> all;
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++)
            if(board[r][c].color == color){
                auto m = getPseudoLegalMovesForPiece(r,c);
                all.insert(all.end(), m.begin(), m.end());
            }
    return all;
}

std::vector<Move> ChessEngine::getLegalMoves(Color color) {
    auto pseudo = getPseudoLegalMoves(color);
    std::vector<Move> legal;
    for(auto& m : pseudo) {
        applyMove(m);
        if(!isInCheck(color)) legal.push_back(m);
        unapplyMove(m);
    }
    return legal;
}

std::vector<Move> ChessEngine::getLegalMovesForSquare(int row, int col) {
    if(board[row][col].isEmpty()) return {};
    Color c = board[row][col].color;
    auto pseudo = getPseudoLegalMovesForPiece(row,col);
    std::vector<Move> legal;
    for(auto& m : pseudo) {
        applyMove(m);
        if(!isInCheck(c)) legal.push_back(m);
        unapplyMove(m);
    }
    return legal;
}

void ChessEngine::applyMove(Move& m) {
    // Save state
    m.capturedPiece      = board[m.toRow][m.toCol];
    m.savedWhiteKingMoved = whiteKingMoved;
    m.savedBlackKingMoved = blackKingMoved;
    m.savedWhiteRookAMoved= whiteRookAMoved;
    m.savedWhiteRookHMoved= whiteRookHMoved;
    m.savedBlackRookAMoved= blackRookAMoved;
    m.savedBlackRookHMoved= blackRookHMoved;
    m.savedEnPassantCol   = enPassantCol;
    m.savedEnPassantRow   = enPassantRow;

    Piece moving = board[m.fromRow][m.fromCol];

    // En passant capture
    if(m.isEnPassant) {
        m.capturedPiece = board[m.fromRow][m.toCol];  // the pawn being captured
        board[m.fromRow][m.toCol] = Piece();
    }

    // Execute basic move
    board[m.toRow][m.toCol] = moving;
    board[m.fromRow][m.fromCol] = Piece();

    // Promotion
    if(m.promotion != EMPTY) {
        board[m.toRow][m.toCol] = Piece(m.promotion, moving.color);
    }

    // Castling – move rook
    if(m.isCastleKingside) {
        int r = m.fromRow;
        board[r][5] = board[r][7];
        board[r][7] = Piece();
    }
    if(m.isCastleQueenside) {
        int r = m.fromRow;
        board[r][3] = board[r][0];
        board[r][0] = Piece();
    }

    // Update flags
    if(moving.type == KING) {
        if(moving.color == WHITE) whiteKingMoved = true;
        else                       blackKingMoved  = true;
    }
    if(moving.type == ROOK) {
        if(moving.color == WHITE) {
            if(m.fromCol==0) whiteRookAMoved = true;
            if(m.fromCol==7) whiteRookHMoved = true;
        } else {
            if(m.fromCol==0) blackRookAMoved = true;
            if(m.fromCol==7) blackRookHMoved = true;
        }
    }

    // En-passant target square (set if pawn moved 2)
    enPassantCol = -1; enPassantRow = -1;
    if(moving.type == PAWN && abs(m.toRow - m.fromRow) == 2) {
        enPassantCol = m.toCol;
        enPassantRow = m.toRow;
    }

    currentPlayer = opponent(currentPlayer);
}

void ChessEngine::unapplyMove(Move& m) {
    currentPlayer = opponent(currentPlayer);

    Piece moving = board[m.toRow][m.toCol];
    if(m.promotion != EMPTY) moving = Piece(PAWN, moving.color);

    board[m.fromRow][m.fromCol] = moving;

    if(m.isEnPassant) {
        board[m.toRow][m.toCol] = Piece();
        board[m.fromRow][m.toCol] = m.capturedPiece;
    } else {
        board[m.toRow][m.toCol] = m.capturedPiece;
    }

    // Undo castling
    if(m.isCastleKingside) {
        int r = m.fromRow;
        board[r][7] = board[r][5];
        board[r][5] = Piece();
    }
    if(m.isCastleQueenside) {
        int r = m.fromRow;
        board[r][0] = board[r][3];
        board[r][3] = Piece();
    }

    // Restore flags
    whiteKingMoved  = m.savedWhiteKingMoved;
    blackKingMoved  = m.savedBlackKingMoved;
    whiteRookAMoved = m.savedWhiteRookAMoved;
    whiteRookHMoved = m.savedWhiteRookHMoved;
    blackRookAMoved = m.savedBlackRookAMoved;
    blackRookHMoved = m.savedBlackRookHMoved;
    enPassantCol    = m.savedEnPassantCol;
    enPassantRow    = m.savedEnPassantRow;
}

// ============================================================
//  PUBLIC MAKE MOVE
// ============================================================
bool ChessEngine::makeMove(int fromRow, int fromCol, int toRow, int toCol,
                           PieceType promotion) {
    auto legal = getLegalMovesForSquare(fromRow, fromCol);
    Move* chosen = nullptr;
    for(auto& m : legal) {
        if(m.toRow==toRow && m.toCol==toCol) {
            // Prefer matching promotion
            if(m.promotion != EMPTY) {
                if(m.promotion == promotion) { chosen = &m; break; }
            } else {
                chosen = &m; break;
            }
        }
    }
    if(!chosen) return false;

    Move copy = *chosen;
    applyMove(copy);
    moveHistory.push(copy);
    halfMoveClock = (copy.capturedPiece.isEmpty() && board[copy.toRow][copy.toCol].type != PAWN)
                    ? halfMoveClock+1 : 0;
    if(currentPlayer == WHITE) fullMoveNumber++;

    currentZobristKey = computeZobrist();
    positionCount[currentZobristKey]++;

    gameStatus = getGameStatus();
    return true;
}

void ChessEngine::undoLastMove() {
    if(moveHistory.empty()) return;
    Move m = moveHistory.top();
    moveHistory.pop();
    unapplyMove(m);
    positionCount[currentZobristKey]--;
    currentZobristKey = computeZobrist();
    gameStatus = PLAYING;
}

// ============================================================
//  ATTACK DETECTION
// ============================================================
bool ChessEngine::isSquareAttacked(int row, int col, Color byColor) {
    // Pawns
    int dir = (byColor==WHITE) ? 1 : -1;
    for(int dc : {-1,1}) {
        int pr = row - dir, pc = col + dc;
        if(pr>=0&&pr<8&&pc>=0&&pc<8)
            if(board[pr][pc].color==byColor && board[pr][pc].type==PAWN) return true;
    }
    // Knights
    static const int kx[]={-2,-2,-1,-1,1,1,2,2};
    static const int ky[]={-1,1,-2,2,-2,2,-1,1};
    for(int i=0;i<8;i++){
        int nr=row+kx[i],nc=col+ky[i];
        if(nr>=0&&nr<8&&nc>=0&&nc<8)
            if(board[nr][nc].color==byColor && board[nr][nc].type==KNIGHT) return true;
    }
    // Bishop/Queen diagonals
    static const int dx[]={-1,-1,1,1};
    static const int dy[]={-1,1,-1,1};
    for(int d=0;d<4;d++){
        for(int s=1;s<8;s++){
            int nr=row+s*dx[d],nc=col+s*dy[d];
            if(nr<0||nr>=8||nc<0||nc>=8) break;
            if(!board[nr][nc].isEmpty()){
                if(board[nr][nc].color==byColor &&
                   (board[nr][nc].type==BISHOP||board[nr][nc].type==QUEEN)) return true;
                break;
            }
        }
    }
    // Rook/Queen straights
    static const int rx[]={-1,1,0,0};
    static const int ry[]={0,0,-1,1};
    for(int d=0;d<4;d++){
        for(int s=1;s<8;s++){
            int nr=row+s*rx[d],nc=col+s*ry[d];
            if(nr<0||nr>=8||nc<0||nc>=8) break;
            if(!board[nr][nc].isEmpty()){
                if(board[nr][nc].color==byColor &&
                   (board[nr][nc].type==ROOK||board[nr][nc].type==QUEEN)) return true;
                break;
            }
        }
    }
    // King
    for(int dr=-1;dr<=1;dr++)
        for(int dc=-1;dc<=1;dc++){
            if(dr==0&&dc==0) continue;
            int nr=row+dr,nc=col+dc;
            if(nr>=0&&nr<8&&nc>=0&&nc<8)
                if(board[nr][nc].color==byColor && board[nr][nc].type==KING) return true;
        }
    return false;
}

bool ChessEngine::isInCheck(Color color) {
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++)
            if(board[r][c].color==color && board[r][c].type==KING)
                return isSquareAttacked(r,c,opponent(color));
    return false;
}

bool ChessEngine::isCheckmate(Color color) {
    return isInCheck(color) && getLegalMoves(color).empty();
}

bool ChessEngine::isStalemate(Color color) {
    return !isInCheck(color) && getLegalMoves(color).empty();
}

ChessEngine::GameStatus ChessEngine::getGameStatus() {
    if(halfMoveClock >= 100)                return DRAW_50_MOVE;
    if(positionCount[currentZobristKey]>=3) return DRAW_REPETITION;
    if(isCheckmate(WHITE))                  return BLACK_WINS;
    if(isCheckmate(BLACK))                  return WHITE_WINS;
    if(isStalemate(currentPlayer))          return STALEMATE;
    return PLAYING;
}

std::string ChessEngine::getStatusString() {
    switch(gameStatus) {
        case PLAYING:          return "playing";
        case WHITE_WINS:       return "white_wins";
        case BLACK_WINS:       return "black_wins";
        case STALEMATE:        return "stalemate";
        case DRAW_50_MOVE:     return "draw_50";
        case DRAW_REPETITION:  return "draw_rep";
        default:               return "unknown";
    }
}

// ============================================================
//  EVALUATION  (DSA: Heuristic scoring)
// ============================================================
bool isEndgame(const Piece board[8][8]) {
    int queens=0, minors=0;
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++){
            if(board[r][c].type==QUEEN) queens++;
            if(board[r][c].type==KNIGHT||board[r][c].type==BISHOP) minors++;
        }
    return queens==0 || (queens<=2 && minors<=2);
}

int ChessEngine::evaluatePiece(PieceType type, Color color, int row, int col) {
    // Flip row for black (PST is from white's perspective)
    int r = (color == WHITE) ? row : (7 - row);
    int base = PIECE_VALUE[(int)type];
    int pst = 0;
    bool eg = isEndgame(board);
    switch(type) {
        case PAWN:   pst = PST_PAWN[r][col];   break;
        case KNIGHT: pst = PST_KNIGHT[r][col];  break;
        case BISHOP: pst = PST_BISHOP[r][col];  break;
        case ROOK:   pst = PST_ROOK[r][col];    break;
        case QUEEN:  pst = PST_QUEEN[r][col];   break;
        case KING:   pst = eg ? PST_KING_END[r][col] : PST_KING_MID[r][col]; break;
        default:     break;
    }
    return base + pst;
}

int ChessEngine::evaluate(Color perspective) {
    int score = 0;
    for(int r=0;r<8;r++)
        for(int c=0;c<8;c++){
            Piece& p = board[r][c];
            if(p.isEmpty()) continue;
            int val = evaluatePiece(p.type, p.color, r, c);
            score += (p.color == perspective) ? val : -val;
        }
    return score;
}

// ============================================================
//  MOVE ORDERING  (DSA: Heuristic sort for better alpha-beta)
// ============================================================
int ChessEngine::mvvLva(const Move& m) {
    int victim   = PIECE_VALUE[(int)board[m.toRow][m.toCol].type];
    int attacker = PIECE_VALUE[(int)board[m.fromRow][m.fromCol].type];
    return victim * 10 - attacker;   // prioritize capturing big pieces with small ones
}

void ChessEngine::orderMoves(std::vector<Move>& moves, Color color) {
    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b){
        int sa = 0, sb = 0;
        if(!board[a.toRow][a.toCol].isEmpty()) sa = mvvLva(a);
        if(!board[b.toRow][b.toCol].isEmpty()) sb = mvvLva(b);
        if(a.promotion != EMPTY) sa += 800;
        if(b.promotion != EMPTY) sb += 800;
        return sa > sb;
    });
}

// ============================================================
//  QUIESCENCE SEARCH  (DSA: extended search for tactical stability)
// ============================================================
int ChessEngine::quiescenceSearch(int alpha, int beta, Color color, int qdepth) {
    int stand_pat = evaluate(color);
    if(qdepth <= 0) return stand_pat;
    if(stand_pat >= beta) return beta;
    if(alpha < stand_pat) alpha = stand_pat;

    auto moves = getPseudoLegalMoves(color);
    orderMoves(moves, color);

    for(auto& m : moves) {
        if(board[m.toRow][m.toCol].isEmpty() && !m.isEnPassant) continue; // only captures
        applyMove(m);
        if(isInCheck(color)) { unapplyMove(m); continue; }
        int score = -quiescenceSearch(-beta, -alpha, opponent(color), qdepth-1);
        unapplyMove(m);
        if(score >= beta) return beta;
        if(score > alpha) alpha = score;
    }
    return alpha;
}

// ============================================================
//  MINIMAX WITH ALPHA-BETA PRUNING  (DSA: core AI algorithm)
//  Complexity: O(b^(d/2)) with good move ordering vs O(b^d) naive
// ============================================================
int ChessEngine::minimax(int depth, int alpha, int beta, bool maximizing, Color rootColor) {
    // Check transposition table (DSA: HashMap lookup)
    uint64_t key = computeZobrist();
    auto it = transpositionTable.find(key);
    if(it != transpositionTable.end() && it->second.depth >= depth) {
        auto& entry = it->second;
        if(entry.type == EXACT) return entry.score;
        if(entry.type == LOWER_BOUND && entry.score > alpha) alpha = entry.score;
        if(entry.type == UPPER_BOUND && entry.score < beta)  beta  = entry.score;
        if(alpha >= beta) return entry.score;
    }

    Color current = maximizing ? rootColor : opponent(rootColor);

    if(depth == 0) return quiescenceSearch(alpha, beta, current, 3);

    auto moves = getLegalMoves(current);
    if(moves.empty()) {
        if(isInCheck(current)) return maximizing ? -20000 + (100-depth) : 20000 - (100-depth);
        return 0; // stalemate
    }

    orderMoves(moves, current);

    int origAlpha = alpha;
    int best = maximizing ? INT_MIN : INT_MAX;
    Move bestMove;

    for(auto& m : moves) {
        applyMove(m);
        int score = minimax(depth-1, alpha, beta, !maximizing, rootColor);
        unapplyMove(m);

        if(maximizing) {
            if(score > best) { best = score; bestMove = m; }
            alpha = std::max(alpha, best);
        } else {
            if(score < best) { best = score; bestMove = m; }
            beta = std::min(beta, best);
        }
        if(alpha >= beta) break; // Alpha-Beta cutoff
    }

    // Store in transposition table
    TTEntry entry;
    entry.score = best; entry.depth = depth; entry.bestMove = bestMove;
    if(best <= origAlpha)     entry.type = UPPER_BOUND;
    else if(best >= beta)     entry.type = LOWER_BOUND;
    else                      entry.type = EXACT;
    transpositionTable[key] = entry;

    return best;
}

// ============================================================
//  BOT MOVE SELECTION
// ============================================================
Move ChessEngine::getBotMove(int difficulty) {
    // difficulty: 1=easy(d2), 2=medium(d3), 3=hard(d4), 4=expert(d5)
    int depths[] = {0,2,3,4,5};
    int depth = depths[std::min(difficulty, 4)];

    Color botColor = currentPlayer;

    // Easy: sometimes play random move
    if(difficulty == 1) {
        auto moves = getLegalMoves(botColor);
        if(moves.empty()) return Move();
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        // 50% chance random
        if(rng() % 2 == 0) {
            return moves[rng() % moves.size()];
        }
        depth = 2;
    }

    transpositionTable.clear();
    auto moves = getLegalMoves(botColor);
    if(moves.empty()) return Move();

    orderMoves(moves, botColor);
    int best = INT_MIN;
    Move bestMove = moves[0];

    for(auto& m : moves) {
        applyMove(m);
        int score = minimax(depth-1, INT_MIN+1, INT_MAX-1, false, botColor);
        unapplyMove(m);
        if(score > best) { best = score; bestMove = m; }
    }

    return bestMove;
}

// ============================================================
//  JSON SERIALIZATION
// ============================================================
static std::string pieceChar(Piece p) {
    if(p.isEmpty()) return "null";
    static const char* names[] = {"","p","n","b","r","q","k"};
    std::string s = names[(int)p.type];
    if(p.color == WHITE) s[0] = toupper(s[0]);
    return "\"" + s + "\"";
}

std::string ChessEngine::getBoardJSON() {
    std::ostringstream ss;
    ss << "{\"board\":[";
    for(int r=7;r>=0;r--) {  // row 7 = rank 8 at top
        ss << "[";
        for(int c=0;c<8;c++) {
            ss << pieceChar(board[r][c]);
            if(c<7) ss << ",";
        }
        ss << "]";
        if(r>0) ss << ",";
    }
    ss << "],";
    ss << "\"currentPlayer\":\"" << (currentPlayer==WHITE?"white":"black") << "\",";
    ss << "\"status\":\"" << getStatusString() << "\",";
    ss << "\"inCheck\":" << (isInCheck(currentPlayer)?"true":"false") << ",";
    ss << "\"moveCount\":" << moveHistory.size() << ",";
    ss << "\"enPassantCol\":" << enPassantCol << ",";
    ss << "\"enPassantRow\":" << enPassantRow;
    ss << "}";
    return ss.str();
}

std::string ChessEngine::getLegalMovesJSON(int row, int col) {
    // row/col in visual coords (row 0 = rank 8 from frontend)
    int internalRow = 7 - row;
    auto moves = getLegalMovesForSquare(internalRow, col);
    std::ostringstream ss;
    ss << "[";
    for(size_t i=0;i<moves.size();i++) {
        ss << "{\"row\":" << (7-moves[i].toRow) << ",\"col\":" << moves[i].toCol;
        if(moves[i].promotion != EMPTY) ss << ",\"promotion\":true";
        if(moves[i].isEnPassant)        ss << ",\"enPassant\":true";
        if(moves[i].isCastleKingside)   ss << ",\"castle\":\"kingside\"";
        if(moves[i].isCastleQueenside)  ss << ",\"castle\":\"queenside\"";
        ss << "}";
        if(i+1<moves.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

std::string ChessEngine::getGameStateJSON() {
    std::ostringstream ss;
    ss << "{\"status\":\"" << getStatusString() << "\",";
    ss << "\"inCheck\":" << (isInCheck(currentPlayer)?"true":"false") << ",";
    ss << "\"currentPlayer\":\"" << (currentPlayer==WHITE?"white":"black") << "\",";
    ss << "\"moveCount\":" << moveHistory.size() << "}";
    return ss.str();
}
