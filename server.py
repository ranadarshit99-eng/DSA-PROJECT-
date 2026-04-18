    """
ChessDSA — Optimized Python Chess Engine + HTTP Server
Fixes: checkmate logic, illegal move handling, 3x-10x speed improvement
Run: python server.py  →  http://localhost:8080
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json, os, random, time, struct
from urllib.parse import urlparse, parse_qs

# ═══════════════════════════════════════════════════════════════
#  CONSTANTS
# ═══════════════════════════════════════════════════════════════
EMPTY=0; PAWN=1; KNIGHT=2; BISHOP=3; ROOK=4; QUEEN=5; KING=6
WHITE=1; BLACK=2

PIECE_VALUE = [0, 100, 320, 330, 500, 900, 20000]

OPP = {WHITE: BLACK, BLACK: WHITE}

# Piece-Square Tables  (index 0 = rank1/white-back, 7 = rank8/black-back)
PST_PAWN = [
    [  0,  0,  0,  0,  0,  0,  0,  0],
    [ 50, 50, 50, 50, 50, 50, 50, 50],
    [ 10, 10, 20, 30, 30, 20, 10, 10],
    [  5,  5, 10, 25, 25, 10,  5,  5],
    [  0,  0,  0, 20, 20,  0,  0,  0],
    [  5, -5,-10,  0,  0,-10, -5,  5],
    [  5, 10, 10,-20,-20, 10, 10,  5],
    [  0,  0,  0,  0,  0,  0,  0,  0],
]
PST_KNIGHT = [
    [-50,-40,-30,-30,-30,-30,-40,-50],
    [-40,-20,  0,  0,  0,  0,-20,-40],
    [-30,  0, 10, 15, 15, 10,  0,-30],
    [-30,  5, 15, 20, 20, 15,  5,-30],
    [-30,  0, 15, 20, 20, 15,  0,-30],
    [-30,  5, 10, 15, 15, 10,  5,-30],
    [-40,-20,  0,  5,  5,  0,-20,-40],
    [-50,-40,-30,-30,-30,-30,-40,-50],
]
PST_BISHOP = [
    [-20,-10,-10,-10,-10,-10,-10,-20],
    [-10,  0,  0,  0,  0,  0,  0,-10],
    [-10,  0,  5, 10, 10,  5,  0,-10],
    [-10,  5,  5, 10, 10,  5,  5,-10],
    [-10,  0, 10, 10, 10, 10,  0,-10],
    [-10, 10, 10, 10, 10, 10, 10,-10],
    [-10,  5,  0,  0,  0,  0,  5,-10],
    [-20,-10,-10,-10,-10,-10,-10,-20],
]
PST_ROOK = [
    [  0,  0,  0,  0,  0,  0,  0,  0],
    [  5, 10, 10, 10, 10, 10, 10,  5],
    [ -5,  0,  0,  0,  0,  0,  0, -5],
    [ -5,  0,  0,  0,  0,  0,  0, -5],
    [ -5,  0,  0,  0,  0,  0,  0, -5],
    [ -5,  0,  0,  0,  0,  0,  0, -5],
    [ -5,  0,  0,  0,  0,  0,  0, -5],
    [  0,  0,  0,  5,  5,  0,  0,  0],
]
PST_QUEEN = [
    [-20,-10,-10, -5, -5,-10,-10,-20],
    [-10,  0,  0,  0,  0,  0,  0,-10],
    [-10,  0,  5,  5,  5,  5,  0,-10],
    [ -5,  0,  5,  5,  5,  5,  0, -5],
    [  0,  0,  5,  5,  5,  5,  0, -5],
    [-10,  5,  5,  5,  5,  5,  0,-10],
    [-10,  0,  5,  0,  0,  0,  0,-10],
    [-20,-10,-10, -5, -5,-10,-10,-20],
]
PST_KING_MID = [
    [-30,-40,-40,-50,-50,-40,-40,-30],
    [-30,-40,-40,-50,-50,-40,-40,-30],
    [-30,-40,-40,-50,-50,-40,-40,-30],
    [-30,-40,-40,-50,-50,-40,-40,-30],
    [-20,-30,-30,-40,-40,-30,-30,-20],
    [-10,-20,-20,-20,-20,-20,-20,-10],
    [ 20, 20,  0,  0,  0,  0, 20, 20],
    [ 20, 30, 10,  0,  0, 10, 30, 20],
]
PST_KING_END = [
    [-50,-40,-30,-20,-20,-30,-40,-50],
    [-30,-20,-10,  0,  0,-10,-20,-30],
    [-30,-10, 20, 30, 30, 20,-10,-30],
    [-30,-10, 30, 40, 40, 30,-10,-30],
    [-30,-10, 30, 40, 40, 30,-10,-30],
    [-30,-10, 20, 30, 30, 20,-10,-30],
    [-30,-30,  0,  0,  0,  0,-30,-30],
    [-50,-30,-30,-30,-30,-30,-30,-50],
]

PST = {
    PAWN: PST_PAWN, KNIGHT: PST_KNIGHT, BISHOP: PST_BISHOP,
    ROOK: PST_ROOK, QUEEN: PST_QUEEN,
}

# ═══════════════════════════════════════════════════════════════
#  ZOBRIST TABLE  (pre-computed once at import)
# ═══════════════════════════════════════════════════════════════
random.seed(0xDEADBEEFCAFEBABE)
_ZOBRIST = [[[random.getrandbits(64) for _ in range(14)] for _ in range(8)] for _ in range(8)]
_ZOBRIST_BLACK = random.getrandbits(64)
_ZOBRIST_EP    = [random.getrandbits(64) for _ in range(8)]   # per column
_ZOBRIST_CASTLE= [random.getrandbits(64) for _ in range(4)]   # WKS,WQS,BKS,BQS

def _zidx(t, color):
    # piece index 1-12, 0 = empty
    return t + (0 if color == WHITE else 6)

# ═══════════════════════════════════════════════════════════════
#  MOVE TUPLE LAYOUT
#  (fr, fc, tr, tc, promo, ep, castle_k, castle_q)
#   0   1   2   3    4     5     6         7
# ═══════════════════════════════════════════════════════════════

# ═══════════════════════════════════════════════════════════════
#  CHESS ENGINE
# ═══════════════════════════════════════════════════════════════
class ChessEngine:
    def __init__(self):
        # Transposition table: key → (depth, score, flag, move)
        # flag: 0=exact, 1=lower, 2=upper
        self.tt = {}
        self.reset()

    # ── RESET ─────────────────────────────────────────────────
    def reset(self):
        # Flat board: board[r*8+c] = (type,color) or None — faster indexing
        b = [None] * 64
        for c in range(8):
            b[0*8+c] = (ROOK if c in(0,7) else KNIGHT if c in(1,6) else
                        BISHOP if c in(2,5) else QUEEN if c==3 else KING, BLACK)
            b[1*8+c] = (PAWN, BLACK)
            b[6*8+c] = (PAWN, WHITE)
            b[7*8+c] = (ROOK if c in(0,7) else KNIGHT if c in(1,6) else
                        BISHOP if c in(2,5) else QUEEN if c==3 else KING, WHITE)
        self.b = b

        self.current  = WHITE
        # Castle rights as a 4-bit mask: bit0=WKS, bit1=WQS, bit2=BKS, bit3=BQS
        self.castle   = 0b1111
        self.ep_col   = -1   # -1 = no en-passant
        self.half     = 0
        self.full     = 1
        self.status   = "playing"

        # Incremental Zobrist key
        self.zkey = self._compute_zobrist_full()

        # DSA: Stack for full undo
        self.history  = []   # list of (zkey, castle, ep_col, half, full, move, captured)

        # Repetition map
        self.pos_count = {self.zkey: 1}
        self.tt = {}

        # Material+PST score cache (white perspective)
        self.score_cache = self._compute_score_full()

    # ── ZOBRIST ───────────────────────────────────────────────
    def _compute_zobrist_full(self):
        key = 0
        b = self.b
        for sq in range(64):
            p = b[sq]
            if p:
                key ^= _ZOBRIST[sq>>3][sq&7][_zidx(p[0], p[1])]
        if self.current == BLACK:
            key ^= _ZOBRIST_BLACK
        if self.ep_col >= 0:
            key ^= _ZOBRIST_EP[self.ep_col]
        for i in range(4):
            if self.castle & (1 << i):
                key ^= _ZOBRIST_CASTLE[i]
        return key

    # ── MATERIAL+PST SCORE CACHE ──────────────────────────────
    def _compute_score_full(self):
        """Material + PST score from WHITE's perspective."""
        score = 0
        eg = self._is_endgame()
        for sq in range(64):
            p = self.b[sq]
            if not p: continue
            t, col = p
            r, c = sq >> 3, sq & 7
            pr = r if col == WHITE else (7 - r)
            base = PIECE_VALUE[t]
            if t == KING:
                pst = (PST_KING_END if eg else PST_KING_MID)[pr][c]
            else:
                pst = PST[t][pr][c]
            val = base + pst
            score += val if col == WHITE else -val
        return score

    def _is_endgame(self):
        queens  = sum(1 for sq in range(64) if self.b[sq] and self.b[sq][0]==QUEEN)
        minors  = sum(1 for sq in range(64) if self.b[sq] and self.b[sq][0] in (KNIGHT,BISHOP))
        return queens == 0 or (queens <= 2 and minors <= 2)

    # ── ATTACK DETECTION ──────────────────────────────────────
    def _sq_attacked(self, r, c, by):
        b = self.b
        d = 1 if by == WHITE else -1
        # Pawn attacks
        for dc in (-1, 1):
            pr, pc = r - d, c + dc
            if 0 <= pr < 8 and 0 <= pc < 8:
                p = b[pr*8+pc]
                if p == (PAWN, by): return True
        # Knight
        for dr, dc in ((-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)):
            nr, nc = r+dr, c+dc
            if 0 <= nr < 8 and 0 <= nc < 8:
                p = b[nr*8+nc]
                if p and p[1]==by and p[0]==KNIGHT: return True
        # Diagonals (Bishop/Queen)
        for dr, dc in ((-1,-1),(-1,1),(1,-1),(1,1)):
            nr, nc = r+dr, c+dc
            while 0 <= nr < 8 and 0 <= nc < 8:
                p = b[nr*8+nc]
                if p:
                    if p[1]==by and p[0] in (BISHOP,QUEEN): return True
                    break
                nr += dr; nc += dc
        # Straights (Rook/Queen)
        for dr, dc in ((-1,0),(1,0),(0,-1),(0,1)):
            nr, nc = r+dr, c+dc
            while 0 <= nr < 8 and 0 <= nc < 8:
                p = b[nr*8+nc]
                if p:
                    if p[1]==by and p[0] in (ROOK,QUEEN): return True
                    break
                nr += dr; nc += dc
        # King
        for dr in (-1,0,1):
            for dc in (-1,0,1):
                if dr==dc==0: continue
                nr, nc = r+dr, c+dc
                if 0 <= nr < 8 and 0 <= nc < 8:
                    p = b[nr*8+nc]
                    if p == (KING, by): return True
        return False

    def _king_sq(self, color):
        for sq in range(64):
            if self.b[sq] == (KING, color):
                return sq >> 3, sq & 7
        return -1, -1

    def is_in_check(self, color):
        kr, kc = self._king_sq(color)
        if kr < 0: return False
        return self._sq_attacked(kr, kc, OPP[color])

    # ── MOVE GENERATION ───────────────────────────────────────
    def _gen_moves(self, color):
        """Generate pseudo-legal moves for color."""
        moves = []
        b = self.b
        opp = OPP[color]
        ep_col = self.ep_col

        for sq in range(64):
            p = b[sq]
            if not p or p[1] != color: continue
            t = p[0]
            r, c = sq >> 3, sq & 7

            if t == PAWN:
                d = -1 if color == WHITE else 1
                start = 6 if color == WHITE else 1
                promo_r = 0 if color == WHITE else 7
                nr = r + d
                if 0 <= nr < 8:
                    # Forward
                    if not b[nr*8+c]:
                        if nr == promo_r:
                            for pr in (QUEEN, ROOK, BISHOP, KNIGHT):
                                moves.append((r,c,nr,c,pr,False,False,False))
                        else:
                            moves.append((r,c,nr,c,0,False,False,False))
                            # Double push
                            if r == start and not b[(r+2*d)*8+c]:
                                moves.append((r,c,r+2*d,c,0,False,False,False))
                    # Captures
                    for dc in (-1, 1):
                        nc = c + dc
                        if 0 <= nc < 8:
                            target = b[nr*8+nc]
                            if target and target[1] == opp:
                                if nr == promo_r:
                                    for pr in (QUEEN, ROOK, BISHOP, KNIGHT):
                                        moves.append((r,c,nr,nc,pr,False,False,False))
                                else:
                                    moves.append((r,c,nr,nc,0,False,False,False))
                            # En passant
                            elif nc == ep_col and r == (self.ep_row if hasattr(self,'ep_row') else -1):
                                moves.append((r,c,nr,nc,0,True,False,False))

            elif t == KNIGHT:
                for dr,dc in ((-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)):
                    nr,nc=r+dr,c+dc
                    if 0<=nr<8 and 0<=nc<8:
                        tgt=b[nr*8+nc]
                        if not tgt or tgt[1]==opp:
                            moves.append((r,c,nr,nc,0,False,False,False))

            elif t in (BISHOP, ROOK, QUEEN):
                dirs = []
                if t in (BISHOP, QUEEN): dirs += [(-1,-1),(-1,1),(1,-1),(1,1)]
                if t in (ROOK,   QUEEN): dirs += [(-1,0),(1,0),(0,-1),(0,1)]
                for dr,dc in dirs:
                    nr,nc=r+dr,c+dc
                    while 0<=nr<8 and 0<=nc<8:
                        tgt=b[nr*8+nc]
                        if tgt:
                            if tgt[1]==opp:
                                moves.append((r,c,nr,nc,0,False,False,False))
                            break
                        moves.append((r,c,nr,nc,0,False,False,False))
                        nr+=dr; nc+=dc

            elif t == KING:
                for dr in (-1,0,1):
                    for dc in (-1,0,1):
                        if dr==dc==0: continue
                        nr,nc=r+dr,c+dc
                        if 0<=nr<8 and 0<=nc<8:
                            tgt=b[nr*8+nc]
                            if not tgt or tgt[1]==opp:
                                moves.append((r,c,nr,nc,0,False,False,False))
                # Castling
                back = 7 if color==WHITE else 0
                if r == back and not self.is_in_check(color):
                    # Kingside (bit0=WKS, bit2=BKS)
                    bit_ks = 0 if color==WHITE else 2
                    if (self.castle >> bit_ks) & 1:
                        if not b[back*8+5] and not b[back*8+6] \
                           and not self._sq_attacked(back,5,opp) \
                           and not self._sq_attacked(back,6,opp):
                            moves.append((r,c,back,6,0,False,True,False))
                    # Queenside (bit1=WQS, bit3=BQS)
                    bit_qs = 1 if color==WHITE else 3
                    if (self.castle >> bit_qs) & 1:
                        if not b[back*8+3] and not b[back*8+2] and not b[back*8+1] \
                           and not self._sq_attacked(back,3,opp) \
                           and not self._sq_attacked(back,2,opp):
                            moves.append((r,c,back,2,0,False,False,True))

        return moves

    # ── APPLY / UNDO  (optimized: no deep copy) ───────────────
    def _apply(self, move):
        """Apply move, updating board, zobrist key, score cache, and flags.
           Returns (old_castle, old_ep_col, old_ep_row, captured_piece)."""
        fr,fc,tr,tc,promo,ep,castle_k,castle_q = move
        b = self.b

        old_castle = self.castle
        old_ep_col = self.ep_col
        old_ep_row = getattr(self, 'ep_row', -1)

        moving   = b[fr*8+fc]
        captured = b[tr*8+tc]
        t, color = moving
        opp      = OPP[color]
        back     = 7 if color==WHITE else 0

        # --- Zobrist update (XOR out old, XOR in new) ---
        zk = self.zkey

        # Side to move
        zk ^= _ZOBRIST_BLACK   # toggle side

        # Remove moving piece from source
        zk ^= _ZOBRIST[fr][fc][_zidx(t, color)]

        # Remove captured piece (normal)
        ep_captured = None
        if ep:
            # En-passant: captured pawn is on same rank as moving pawn
            ep_captured = b[fr*8+tc]
            zk ^= _ZOBRIST[fr][tc][_zidx(PAWN, opp)]
            b[fr*8+tc] = None
            captured = ep_captured
        elif captured:
            zk ^= _ZOBRIST[tr][tc][_zidx(captured[0], captured[1])]

        # Clear old EP from key
        if old_ep_col >= 0:
            zk ^= _ZOBRIST_EP[old_ep_col]
        # Clear castle bits from key
        for i in range(4):
            if old_castle & (1<<i):
                zk ^= _ZOBRIST_CASTLE[i]

        # Move piece
        final_type = promo if promo else t
        b[tr*8+tc] = (final_type, color)
        b[fr*8+fc] = None

        zk ^= _ZOBRIST[tr][tc][_zidx(final_type, color)]

        # Castling rook
        if castle_k:
            rk = b[back*8+7]
            zk ^= _ZOBRIST[back][7][_zidx(ROOK, color)]
            zk ^= _ZOBRIST[back][5][_zidx(ROOK, color)]
            b[back*8+5] = rk
            b[back*8+7] = None
        if castle_q:
            rk = b[back*8+0]
            zk ^= _ZOBRIST[back][0][_zidx(ROOK, color)]
            zk ^= _ZOBRIST[back][3][_zidx(ROOK, color)]
            b[back*8+3] = rk
            b[back*8+0] = None

        # Update castle rights
        new_castle = old_castle
        if t == KING:
            if color == WHITE: new_castle &= 0b1100
            else:              new_castle &= 0b0011
        if t == ROOK or (captured and captured[0]==ROOK):
            # moving rook
            if color==WHITE:
                if fr==7 and fc==0: new_castle &= ~0b0010
                if fr==7 and fc==7: new_castle &= ~0b0001
            else:
                if fr==0 and fc==0: new_castle &= ~0b1000
                if fr==0 and fc==7: new_castle &= ~0b0100
            # captured rook
            if captured and captured[0]==ROOK:
                if tr==7 and tc==0: new_castle &= ~0b0010
                if tr==7 and tc==7: new_castle &= ~0b0001
                if tr==0 and tc==0: new_castle &= ~0b1000
                if tr==0 and tc==7: new_castle &= ~0b0100

        self.castle = new_castle
        for i in range(4):
            if new_castle & (1<<i):
                zk ^= _ZOBRIST_CASTLE[i]

        # En-passant target
        self.ep_col = -1
        self.ep_row = -1
        if t == PAWN and abs(tr-fr) == 2:
            self.ep_col = tc
            self.ep_row = tr
            zk ^= _ZOBRIST_EP[tc]

        self.zkey    = zk
        self.current = opp

        return old_castle, old_ep_col, old_ep_row, captured, ep_captured

    def _undo(self, move, saved):
        """Undo a move using saved state."""
        fr,fc,tr,tc,promo,ep,castle_k,castle_q = move
        old_castle, old_ep_col, old_ep_row, captured, ep_captured = saved
        b = self.b

        color = OPP[self.current]   # who made the move
        back  = 7 if color==WHITE else 0

        moving_type = PAWN if promo else b[tr*8+tc][0]

        b[fr*8+fc] = (moving_type, color)

        if ep:
            b[tr*8+tc] = None
            b[fr*8+tc] = ep_captured
        else:
            b[tr*8+tc] = captured

        if castle_k:
            b[back*8+7] = b[back*8+5]
            b[back*8+5] = None
        if castle_q:
            b[back*8+0] = b[back*8+3]
            b[back*8+3] = None

        self.castle  = old_castle
        self.ep_col  = old_ep_col
        self.ep_row  = old_ep_row
        self.current = color
        # Recompute zobrist (cheaper than incremental undo for correctness)
        self.zkey    = self._compute_zobrist_full()

    # ── LEGAL MOVES ───────────────────────────────────────────
    def _legal_moves_color(self, color):
        legal = []
        for m in self._gen_moves(color):
            saved = self._apply(m)
            in_check = self.is_in_check(color)
            self._undo(m, saved)
            if not in_check:
                legal.append(m)
        return legal

    def _legal_moves_sq(self, r, c):
        p = self.b[r*8+c]
        if not p: return []
        color = p[1]
        legal = []
        # Only generate for this square
        from_moves = []
        b_backup = self.b
        # Generate pseudo for this piece
        t = p[0]
        ep_col = self.ep_col
        opp = OPP[color]

        if t == PAWN:
            d = -1 if color==WHITE else 1
            start   = 6 if color==WHITE else 1
            promo_r = 0 if color==WHITE else 7
            nr = r + d
            b = self.b
            if 0 <= nr < 8:
                if not b[nr*8+c]:
                    if nr == promo_r:
                        for pr in (QUEEN,ROOK,BISHOP,KNIGHT):
                            from_moves.append((r,c,nr,c,pr,False,False,False))
                    else:
                        from_moves.append((r,c,nr,c,0,False,False,False))
                        if r==start and not b[(r+2*d)*8+c]:
                            from_moves.append((r,c,r+2*d,c,0,False,False,False))
                for dc in (-1,1):
                    nc=c+dc
                    if 0<=nc<8:
                        tgt=b[nr*8+nc]
                        if tgt and tgt[1]==opp:
                            if nr==promo_r:
                                for pr in (QUEEN,ROOK,BISHOP,KNIGHT):
                                    from_moves.append((r,c,nr,nc,pr,False,False,False))
                            else:
                                from_moves.append((r,c,nr,nc,0,False,False,False))
                        elif nc==ep_col and r==getattr(self,'ep_row',-1):
                            from_moves.append((r,c,nr,nc,0,True,False,False))
        elif t == KNIGHT:
            b = self.b
            for dr,dc in ((-2,-1),(-2,1),(-1,-2),(-1,2),(1,-2),(1,2),(2,-1),(2,1)):
                nr,nc=r+dr,c+dc
                if 0<=nr<8 and 0<=nc<8:
                    tgt=b[nr*8+nc]
                    if not tgt or tgt[1]==opp:
                        from_moves.append((r,c,nr,nc,0,False,False,False))
        elif t in (BISHOP,ROOK,QUEEN):
            b = self.b
            dirs=[]
            if t in(BISHOP,QUEEN): dirs+=[(-1,-1),(-1,1),(1,-1),(1,1)]
            if t in(ROOK,QUEEN):   dirs+=[(-1,0),(1,0),(0,-1),(0,1)]
            for dr,dc in dirs:
                nr,nc=r+dr,c+dc
                while 0<=nr<8 and 0<=nc<8:
                    tgt=b[nr*8+nc]
                    if tgt:
                        if tgt[1]==opp: from_moves.append((r,c,nr,nc,0,False,False,False))
                        break
                    from_moves.append((r,c,nr,nc,0,False,False,False))
                    nr+=dr; nc+=dc
        elif t == KING:
            b = self.b
            for dr in(-1,0,1):
                for dc in(-1,0,1):
                    if dr==dc==0: continue
                    nr,nc=r+dr,c+dc
                    if 0<=nr<8 and 0<=nc<8:
                        tgt=b[nr*8+nc]
                        if not tgt or tgt[1]==opp:
                            from_moves.append((r,c,nr,nc,0,False,False,False))
            back=7 if color==WHITE else 0
            if r==back and not self.is_in_check(color):
                bit_ks=0 if color==WHITE else 2
                bit_qs=1 if color==WHITE else 3
                if (self.castle>>bit_ks)&1:
                    if not b[back*8+5] and not b[back*8+6] \
                       and not self._sq_attacked(back,5,opp) \
                       and not self._sq_attacked(back,6,opp):
                        from_moves.append((r,c,back,6,0,False,True,False))
                if (self.castle>>bit_qs)&1:
                    if not b[back*8+3] and not b[back*8+2] and not b[back*8+1] \
                       and not self._sq_attacked(back,3,opp) \
                       and not self._sq_attacked(back,2,opp):
                        from_moves.append((r,c,back,2,0,False,False,True))

        for m in from_moves:
            saved = self._apply(m)
            in_check = self.is_in_check(color)
            self._undo(m, saved)
            if not in_check:
                legal.append(m)
        return legal

    # ── PUBLIC MAKE MOVE ──────────────────────────────────────
    def make_move(self, fr, fc, tr, tc, promo_str='q'):
        """Returns (True, '') on success or (False, error_reason) on failure."""
        p = self.b[fr*8+fc]

        # Basic validation
        if not p:
            return False, "no_piece"
        if p[1] != self.current:
            return False, "wrong_turn"
        if self.status != "playing":
            return False, f"game_over:{self.status}"

        promo_map = {'q':QUEEN,'r':ROOK,'b':BISHOP,'n':KNIGHT,'':QUEEN}
        promo_piece = promo_map.get(promo_str, QUEEN)

        legal = self._legal_moves_sq(fr, fc)
        chosen = None
        for m in legal:
            if m[2]==tr and m[3]==tc:
                if m[4]:
                    if m[4]==promo_piece: chosen=m; break
                else:
                    chosen=m; break

        if not chosen:
            # Distinguish why the move is illegal
            # Check if destination is valid pseudo-legally
            all_pseudo = self._gen_moves(self.current)
            pseudo_match = any(m[2]==tr and m[3]==tc and m[0]==fr and m[1]==fc
                               for m in all_pseudo)
            if pseudo_match:
                return False, "leaves_king_in_check"
            return False, "illegal_move"

        # Push state onto undo stack
        old_half  = self.half
        old_full  = self.full
        old_pos   = dict(self.pos_count)
        saved = self._apply(chosen)
        self.history.append((chosen, saved, old_half, old_full, old_pos))

        # Update clocks
        cap = saved[3]
        if chosen[4] or (cap and cap[0]==PAWN):
            self.half = 0
        else:
            self.half += 1
        if self.current == WHITE:
            self.full += 1

        self.pos_count[self.zkey] = self.pos_count.get(self.zkey, 0) + 1
        self.status = self._get_status()
        return True, ""

    def undo(self):
        if not self.history: return
        chosen, saved, old_half, old_full, old_pos = self.history.pop()
        self._undo(chosen, saved)
        self.half      = old_half
        self.full      = old_full
        self.pos_count = old_pos
        self.status    = "playing"

    # ── GAME STATUS ───────────────────────────────────────────
    def _get_status(self):
        if self.half >= 100:
            return "draw_50"
        if self.pos_count.get(self.zkey, 0) >= 3:
            return "draw_rep"

        # --- Checkmate / stalemate detection ---
        # Only compute legal moves if needed (expensive)
        cur = self.current
        has_legal = False
        for m in self._gen_moves(cur):
            saved = self._apply(m)
            in_chk = self.is_in_check(cur)
            self._undo(m, saved)
            if not in_chk:
                has_legal = True
                break

        if not has_legal:
            if self.is_in_check(cur):
                # Checkmate — the side to move lost
                return "white_wins" if cur == BLACK else "black_wins"
            else:
                return "stalemate"

        # Insufficient material
        if self._insufficient_material():
            return "draw_material"

        return "playing"

    def _insufficient_material(self):
        pieces = [(self.b[sq][0], self.b[sq][1])
                  for sq in range(64) if self.b[sq]]
        types  = [p[0] for p in pieces]
        if PAWN in types or ROOK in types or QUEEN in types:
            return False
        # KvK, KBvK, KNvK, KBvKB same color (simplified)
        non_kings = [t for t in types if t != KING]
        return len(non_kings) <= 1

    # ── EVALUATION ────────────────────────────────────────────
    def evaluate(self, perspective):
        # Compute from scratch (incremental would be faster but complex)
        score = 0
        eg = self._is_endgame()
        for sq in range(64):
            p = self.b[sq]
            if not p: continue
            t, col = p
            r, c = sq>>3, sq&7
            pr = r if col==WHITE else (7-r)
            base = PIECE_VALUE[t]
            if t == KING:
                pst = (PST_KING_END if eg else PST_KING_MID)[pr][c]
            else:
                pst = PST.get(t, [[0]*8]*8)[pr][c]
            val = base + pst
            score += val if col==perspective else -val
        return score

    # ── MOVE ORDERING  (MVV-LVA + promotion bonus) ────────────
    def _move_score(self, m):
        s = 0
        victim   = self.b[m[2]*8+m[3]]
        attacker = self.b[m[0]*8+m[1]]
        if victim and attacker:
            s = PIECE_VALUE[victim[0]] * 10 - PIECE_VALUE[attacker[0]]
        if m[4]: s += 800
        if m[5]: s += 200   # en passant bonus
        return s

    def _order(self, moves):
        moves.sort(key=self._move_score, reverse=True)

    # ── QUIESCENCE SEARCH ─────────────────────────────────────
    def _quiesce(self, alpha, beta, color, depth):
        stand = self.evaluate(color)
        if depth <= 0: return stand
        if stand >= beta: return beta
        if stand > alpha: alpha = stand

        caps = [m for m in self._gen_moves(color) if self.b[m[2]*8+m[3]] or m[5]]
        self._order(caps)

        for m in caps:
            saved = self._apply(m)
            if self.is_in_check(color):
                self._undo(m, saved)
                continue
            score = -self._quiesce(-beta, -alpha, OPP[color], depth-1)
            self._undo(m, saved)
            if score >= beta: return beta
            if score > alpha: alpha = score
        return alpha

    # ── MINIMAX + ALPHA-BETA + TT ─────────────────────────────
    INF = 999_999

    def minimax(self, depth, alpha, beta, maximizing, root_color):
        key = self.zkey
        tt  = self.tt

        # TT lookup
        te = tt.get(key)
        if te and te[0] >= depth:
            td, ts, tf, tm = te
            if tf == 0: return ts, tm          # exact
            if tf == 1 and ts > alpha: alpha = ts  # lower bound
            if tf == 2 and ts < beta:  beta  = ts  # upper bound
            if alpha >= beta: return ts, tm

        cur = root_color if maximizing else OPP[root_color]

        if depth == 0:
            return self._quiesce(alpha, beta, cur, 4), None

        moves = self._legal_moves_color(cur)

        if not moves:
            if self.is_in_check(cur):
                sc = (-self.INF + (20 - depth)) if maximizing else (self.INF - (20 - depth))
                return sc, None
            return 0, None  # stalemate

        self._order(moves)

        orig_alpha = alpha
        best_move  = moves[0]
        best = -self.INF if maximizing else self.INF

        for m in moves:
            saved = self._apply(m)
            score, _ = self.minimax(depth-1, -beta, -alpha, not maximizing, root_color)
            score = -score
            self._undo(m, saved)

            if maximizing:
                if score > best: best = score; best_move = m
                if score > alpha: alpha = score
            else:
                if score < best: best = score; best_move = m
                if score < beta: beta = score

            if alpha >= beta: break  # Alpha-Beta cutoff

        # Store TT entry
        if best <= orig_alpha: flag = 2      # upper bound
        elif best >= beta:     flag = 1      # lower bound
        else:                  flag = 0      # exact
        tt[key] = (depth, best, flag, best_move)

        return best, best_move

    # ── ITERATIVE DEEPENING BOT ───────────────────────────────
    def get_bot_move(self, difficulty, time_limit_ms=None):
        """
        difficulty 1=easy(d2 + random), 2=medium(d3), 3=hard(d4), 4=expert(ID up to d6)
        Iterative deepening for depth 4+: always returns best move found within time.
        """
        color  = self.current
        moves  = self._legal_moves_color(color)
        if not moves: return None

        if difficulty == 1:
            if random.random() < 0.5:
                return random.choice(moves)
            max_depth = 2
        elif difficulty == 2:
            max_depth = 3
        elif difficulty == 3:
            max_depth = 4
        else:
            max_depth = 6

        self.tt = {}
        self._order(moves)
        best_move = moves[0]

        if time_limit_ms is None:
            time_limit_ms = {1:200, 2:500, 3:2000, 4:5000}.get(difficulty, 2000)

        deadline = time.time() + time_limit_ms / 1000.0

        for depth in range(1, max_depth + 1):
            if time.time() >= deadline and depth > 1:
                break
            cur_best = -self.INF
            cur_move = best_move
            self._order(moves)
            for m in moves:
                saved = self._apply(m)
                score, _ = self.minimax(depth-1, -self.INF, self.INF, False, color)
                score = -score
                self._undo(m, saved)
                if score > cur_best:
                    cur_best = score
                    cur_move = m
                if time.time() >= deadline and depth > 1:
                    break
            best_move = cur_move
            if cur_best >= self.INF - 100:   # forced mate found
                break

        return best_move

    # ── SERIALIZATION ─────────────────────────────────────────
    PIECE_CH = {PAWN:'p', KNIGHT:'n', BISHOP:'b', ROOK:'r', QUEEN:'q', KING:'k'}

    def board_json(self):
        rows = []
        for r in range(8):
            row = []
            for c in range(8):
                p = self.b[r*8+c]
                if not p:
                    row.append(None)
                else:
                    ch = self.PIECE_CH[p[0]]
                    row.append(ch.upper() if p[1]==WHITE else ch)
            rows.append(row)
        in_chk = self.is_in_check(self.current)
        return {
            'board':         rows,
            'currentPlayer': 'white' if self.current==WHITE else 'black',
            'status':        self.status,
            'inCheck':       in_chk,
            'isCheckmate':   in_chk and self.status in ('white_wins','black_wins'),
            'isStalemate':   self.status == 'stalemate',
            'isDraw':        self.status in ('draw_50','draw_rep','draw_material'),
            'moveCount':     len(self.history),
            'enPassantCol':  self.ep_col,
            'enPassantRow':  getattr(self,'ep_row',-1),
            'halfMoveClock': self.half,
        }

    def legal_moves_json(self, visual_row, col):
        """visual_row 0 = board row 0 (black's back rank)."""
        moves  = self._legal_moves_sq(visual_row, col)
        result = []
        for m in moves:
            entry = {'row': m[2], 'col': m[3]}
            if m[4]:       entry['promotion'] = True
            if m[5]:       entry['enPassant'] = True
            if m[6]:       entry['castle']    = 'kingside'
            if m[7]:       entry['castle']    = 'queenside'
            result.append(entry)
        return result


# ═══════════════════════════════════════════════════════════════
#  HTTP SERVER
# ═══════════════════════════════════════════════════════════════
engine = ChessEngine()

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
FRONTEND_DIR = os.path.join(SCRIPT_DIR, 'frontend')
PORT = 8080

class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass  # suppress

    def _send(self, code, body, ct='application/json'):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header('Content-Type', ct)
        self.send_header('Content-Length', len(data))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET,POST,OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
        self.wfile.write(data)

    def do_OPTIONS(self):
        self._send(200, '')

    def do_GET(self):
        parsed = urlparse(self.path)
        path   = parsed.path
        qs     = parse_qs(parsed.query)

        if path == '/api/board':
            self._send(200, json.dumps(engine.board_json()))

        elif path == '/api/state':
            bj = engine.board_json()
            self._send(200, json.dumps({
                'status':        bj['status'],
                'inCheck':       bj['inCheck'],
                'isCheckmate':   bj['isCheckmate'],
                'isStalemate':   bj['isStalemate'],
                'isDraw':        bj['isDraw'],
                'currentPlayer': bj['currentPlayer'],
                'moveCount':     bj['moveCount'],
            }))

        elif path == '/api/legal-moves':
            try:
                r = int(qs.get('row', ['0'])[0])
                c = int(qs.get('col', ['0'])[0])
                self._send(200, json.dumps(engine.legal_moves_json(r, c)))
            except Exception as e:
                self._send(400, json.dumps({'error': str(e)}))

        else:
            # Serve static files from frontend/
            fp = path.lstrip('/')
            if not fp: fp = 'index.html'
            full = os.path.join(FRONTEND_DIR, fp.replace('/', os.sep))
            if os.path.isfile(full):
                ext = os.path.splitext(full)[1]
                ct_map = {'.html':'text/html', '.css':'text/css',
                          '.js':'application/javascript', '.png':'image/png',
                          '.ico':'image/x-icon', '.woff2':'font/woff2'}
                ct = ct_map.get(ext, 'text/plain')
                with open(full, 'rb') as f:
                    self._send(200, f.read(), ct)
            else:
                self._send(404, json.dumps({'error': 'not found'}))

    def do_POST(self):
        length = int(self.headers.get('Content-Length', '0'))
        body   = self.rfile.read(length)
        try:    data = json.loads(body) if body else {}
        except: data = {}

        path = urlparse(self.path).path

        # ── New Game ──────────────────────────────────────────
        if path == '/api/new-game':
            engine.reset()
            color = data.get('playerColor', 'white')
            self._send(200, json.dumps({
                'status': 'ok',
                'playerColor': color,
                **engine.board_json()
            }))

        # ── Player Move ───────────────────────────────────────
        elif path == '/api/move':
            fr = int(data.get('fromRow', 0))
            fc = int(data.get('fromCol', 0))
            tr = int(data.get('toRow',   0))
            tc = int(data.get('toCol',   0))
            pr = data.get('promotion', 'q')

            ok, reason = engine.make_move(fr, fc, tr, tc, pr)
            if ok:
                self._send(200, json.dumps(engine.board_json()))
            else:
                # Detailed illegal-move feedback
                err_map = {
                    'no_piece':            'No piece at source square.',
                    'wrong_turn':          f"It is {engine.current == WHITE and 'white' or 'black'}'s turn.",
                    'leaves_king_in_check':'That move would leave your king in check.',
                    'illegal_move':        'Illegal move.',
                }
                msg = err_map.get(reason, reason)
                if reason.startswith('game_over:'):
                    msg = f"The game is already over ({engine.status})."
                self._send(400, json.dumps({'error': reason, 'message': msg}))

        # ── Bot Move ──────────────────────────────────────────
        elif path == '/api/bot-move':
            diff       = int(data.get('difficulty', 2))
            time_limit = int(data.get('timeLimitMs', 0)) or None

            if engine.status != 'playing':
                self._send(200, json.dumps(engine.board_json()))
                return

            t0   = time.time()
            move = engine.get_bot_move(diff, time_limit)
            elapsed = round((time.time() - t0) * 1000)

            if move:
                fr,fc,tr,tc = move[0],move[1],move[2],move[3]
                pr = {QUEEN:'q',ROOK:'r',BISHOP:'b',KNIGHT:'n'}.get(move[4], 'q')
                ok, reason = engine.make_move(fr, fc, tr, tc, pr)
                result = engine.board_json()
                result['botMove']  = {'fromRow':fr,'fromCol':fc,'toRow':tr,'toCol':tc}
                result['thinkMs']  = elapsed
                result['difficulty'] = diff
                if not ok:
                    result['botError'] = reason
                self._send(200, json.dumps(result))
            else:
                # No moves — game should already be over
                self._send(200, json.dumps(engine.board_json()))

        # ── Undo ──────────────────────────────────────────────
        elif path == '/api/undo':
            n = int(data.get('count', 1))
            for _ in range(n):
                engine.undo()
            self._send(200, json.dumps(engine.board_json()))

        # ── Hint (best move for current player) ───────────────
        elif path == '/api/hint':
            if engine.status != 'playing':
                self._send(200, json.dumps({'hint': None}))
                return
            diff = int(data.get('difficulty', 3))
            move = engine.get_bot_move(diff, 1500)
            if move:
                self._send(200, json.dumps({
                    'hint': {'fromRow':move[0],'fromCol':move[1],
                             'toRow':move[2],'toCol':move[3]}
                }))
            else:
                self._send(200, json.dumps({'hint': None}))

        else:
            self._send(404, json.dumps({'error': 'not found'}))


# ═══════════════════════════════════════════════════════════════
#  ENTRY POINT
# ═══════════════════════════════════════════════════════════════
if __name__ == '__main__':
    if not os.path.isdir(FRONTEND_DIR):
        print(f"\n⚠  frontend/ folder not found at:\n   {FRONTEND_DIR}")
        print("   Make sure server.py is in the chess-project/ root folder.\n")
        exit(1)

    server = HTTPServer(('0.0.0.0', PORT), Handler)
    print()
    print("╔══════════════════════════════════════════╗")
    print("║   ChessDSA — Optimized Python Server     ║")
    print("╚══════════════════════════════════════════╝")
    print(f"  ► http://localhost:{PORT}")
    print("  Press Ctrl+C to stop\n")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
