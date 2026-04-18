// ─────────────────────────────────────────────────────────
//  board.js  —  Chess board rendering, drag & drop
// ─────────────────────────────────────────────────────────

const PIECE_UNICODE = {
  'K':'♔','Q':'♕','R':'♖','B':'♗','N':'♘','P':'♙',
  'k':'♚','q':'♛','r':'♜','b':'♝','n':'♞','p':'♟',
};
const PIECE_NAMES = {
  'K':'King','Q':'Queen','R':'Rook','B':'Bishop','N':'Knight','P':'Pawn',
  'k':'King','q':'Queen','r':'Rook','b':'Bishop','n':'Knight','p':'Pawn',
};
const PIECE_VALUES = { p:1, n:3, b:3, r:5, q:9, k:0 };

class ChessBoard {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.squares   = [];      // [row][col] DOM elements
    this.boardData = null;    // last fetched board state
    this.flipped   = false;
    this.selectedRow = -1;
    this.selectedCol = -1;
    this.legalMoves  = [];    // [{row,col,promotion?,enPassant?,castle?}]
    this.lastFrom    = null;  // {row,col}
    this.lastTo      = null;
    this.dragGhost   = null;
    this.onMoveRequested = null; // callback(fromRow,fromCol,toRow,toCol)
    this._buildDOM();
  }

  // ── DOM BUILD ────────────────────────────────────────
  _buildDOM() {
    this.container.innerHTML = '';
    this.squares = [];
    for(let vr=0; vr<8; vr++) {
      this.squares[vr] = [];
      for(let vc=0; vc<8; vc++) {
        const sq = document.createElement('div');
        sq.className = 'sq';
        sq.addEventListener('click',  e => this._onSquareClick(vr, vc, e));
        sq.addEventListener('mousedown', e => this._onMouseDown(vr, vc, e));
        this.container.appendChild(sq);
        this.squares[vr][vc] = sq;
      }
    }
    this._buildLabels();
  }

  _buildLabels() {
    const ranks = document.getElementById('rankLabels');
    const files = document.getElementById('fileLabels');
    if(!ranks || !files) return;
    ranks.innerHTML = '';
    files.innerHTML = '';
    const fileNames = ['a','b','c','d','e','f','g','h'];
    const rankNums  = ['8','7','6','5','4','3','2','1'];
    for(let i=0;i<8;i++) {
      const rs = document.createElement('span');
      rs.textContent = this.flipped ? (i+1).toString() : rankNums[i];
      ranks.appendChild(rs);
      const fs = document.createElement('span');
      fs.textContent = this.flipped ? fileNames[7-i] : fileNames[i];
      files.appendChild(fs);
    }
  }

  // ── COORDINATE HELPERS ───────────────────────────────
  // "visual" row 0 = top of screen; "data" row depends on flip
  visualToData(vr, vc) {
    return this.flipped
      ? { row: 7-vr, col: 7-vc }
      : { row: vr,   col: vc };
  }
  dataToVisual(row, col) {
    return this.flipped
      ? { vr: 7-row, vc: 7-col }
      : { vr: row,   vc: col };
  }

  // ── RENDER BOARD STATE ───────────────────────────────
  render(boardState, playerColor) {
    this.boardData = boardState;
    if(!boardState || !boardState.board) return;

    const board   = boardState.board; // 8x8 array, row 0 = rank8 visual
    const current = boardState.currentPlayer;

    for(let vr=0; vr<8; vr++) {
      for(let vc=0; vc<8; vc++) {
        const sq = this.squares[vr][vc];
        const {row, col} = this.visualToData(vr, vc);

        // Square color
        sq.className = 'sq ' + ((vr+vc)%2===0 ? 'light' : 'dark');

        // Highlight last move
        if(this.lastFrom && this.lastFrom.row===row && this.lastFrom.col===col)
          sq.classList.add('last-from');
        if(this.lastTo && this.lastTo.row===row && this.lastTo.col===col)
          sq.classList.add('last-to');

        // Legal move hints
        const lm = this.legalMoves.find(m => m.row===row && m.col===col);
        if(lm) {
          const piece = board[row][col];
          sq.classList.add(piece ? 'legal-capture' : 'legal-move');
        }

        // Selected square
        if(this.selectedRow===row && this.selectedCol===col)
          sq.classList.add('selected');

        // Place piece
        sq.innerHTML = '';
        const pieceKey = board[row][col];
        if(pieceKey) {
          const span = document.createElement('span');
          span.className = 'piece ' + (pieceKey===pieceKey.toUpperCase() ? 'white-p piece-white' : 'black-p piece-black');
          span.textContent = PIECE_UNICODE[pieceKey] || '?';
          span.draggable = false;
          sq.appendChild(span);
        }

        // Check highlight
        if(boardState.inCheck && pieceKey && (pieceKey==='K'||pieceKey==='k')) {
          const inCheckColor = boardState.currentPlayer === 'white' ? 'K' : 'k';
          if(pieceKey === inCheckColor) sq.classList.add('in-check');
        }
      }
    }

    // Active player highlight on player cards
    document.querySelectorAll('.player-card').forEach(c => c.classList.remove('active-turn'));
    if(current === 'white') {
      const card = playerColor==='black' ? document.getElementById('topPlayer') : document.getElementById('bottomPlayer');
      if(card) card.classList.add('active-turn');
    } else {
      const card = playerColor==='black' ? document.getElementById('bottomPlayer') : document.getElementById('topPlayer');
      if(card) card.classList.add('active-turn');
    }

    // Update DSA stack count
    const stackEl = document.getElementById('dsaStack');
    if(stackEl) stackEl.textContent = boardState.moveCount || 0;
  }

  // ── CLICK HANDLER ────────────────────────────────────
  _onSquareClick(vr, vc, e) {
    e.stopPropagation();
    const {row, col} = this.visualToData(vr, vc);
    this._handleSquareInteraction(row, col);
  }

  _handleSquareInteraction(row, col) {
    // Is this a legal move destination?
    const lm = this.legalMoves.find(m => m.row===row && m.col===col);
    if(lm && this.selectedRow >= 0) {
      this._requestMove(this.selectedRow, this.selectedCol, row, col, lm);
      return;
    }

    // Select new piece
    if(this.boardData) {
      const piece = this.boardData.board[row][col];
      if(piece) {
        this.selectedRow = row;
        this.selectedCol = col;
        this._fetchAndShowLegal(row, col);
        return;
      }
    }

    // Deselect
    this._clearSelection();
  }

  async _fetchAndShowLegal(row, col) {
    if(offlineMode) {
      this.legalMoves = this._getOfflineLegal(row, col);
      this.render(this.boardData, window.PLAYER_COLOR);
      return;
    }
    try {
      this.legalMoves = await API.getLegalMoves(row, col);
    } catch { this.legalMoves = []; }
    this.render(this.boardData, window.PLAYER_COLOR);
  }

  _requestMove(fromRow, fromCol, toRow, toCol, lm) {
    if(lm.promotion) {
      this._showPromotionModal(fromRow, fromCol, toRow, toCol);
    } else {
      if(this.onMoveRequested) this.onMoveRequested(fromRow, fromCol, toRow, toCol, 'q');
    }
    this._clearSelection();
  }

  _clearSelection() {
    this.selectedRow = -1;
    this.selectedCol = -1;
    this.legalMoves  = [];
    if(this.boardData) this.render(this.boardData, window.PLAYER_COLOR);
  }

  // ── DRAG & DROP ──────────────────────────────────────
  _onMouseDown(vr, vc, e) {
    if(e.button !== 0) return;
    const {row, col} = this.visualToData(vr, vc);
    if(!this.boardData) return;
    const piece = this.boardData.board[row][col];
    if(!piece) return;

    // Set selection
    this.selectedRow = row;
    this.selectedCol = col;
    this._fetchAndShowLegal(row, col);

    // Create ghost
    const ghost = document.createElement('div');
    ghost.className = 'drag-ghost piece ' + (piece===piece.toUpperCase()?'piece-white':'piece-black');
    ghost.textContent = PIECE_UNICODE[piece] || '?';
    ghost.style.left = e.clientX + 'px';
    ghost.style.top  = e.clientY + 'px';
    document.body.appendChild(ghost);
    this.dragGhost = ghost;

    // Dim the original square piece
    const sq = this.squares[vr][vc];
    const origPiece = sq.querySelector('.piece');
    if(origPiece) origPiece.classList.add('dragging');

    const onMouseMove = (ev) => {
      ghost.style.left = ev.clientX + 'px';
      ghost.style.top  = ev.clientY + 'px';
    };
    const onMouseUp = (ev) => {
      ghost.remove(); this.dragGhost = null;
      if(origPiece) origPiece.classList.remove('dragging');
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup',   onMouseUp);

      // Find target square
      const el = document.elementFromPoint(ev.clientX, ev.clientY);
      if(!el) return;
      const target = el.closest('.sq');
      if(!target) { this._clearSelection(); return; }
      // Find position
      for(let tvr=0;tvr<8;tvr++) for(let tvc=0;tvc<8;tvc++) {
        if(this.squares[tvr][tvc] === target) {
          const {row:tr, col:tc} = this.visualToData(tvr, tvc);
          const lm = this.legalMoves.find(m => m.row===tr && m.col===tc);
          if(lm) this._requestMove(row, col, tr, tc, lm);
          else   this._clearSelection();
          return;
        }
      }
      this._clearSelection();
    };
    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup',   onMouseUp);
  }

  // ── PROMOTION MODAL ──────────────────────────────────
  _showPromotionModal(fromRow, fromCol, toRow, toCol) {
    const modal = document.getElementById('promoModal');
    const opts  = document.getElementById('promoOptions');
    const color = this.boardData.currentPlayer;
    const pieces = color === 'white'
      ? [['q','♕'],['r','♖'],['b','♗'],['n','♘']]
      : [['q','♛'],['r','♜'],['b','♝'],['n','♞']];

    opts.innerHTML = '';
    pieces.forEach(([key, sym]) => {
      const btn = document.createElement('div');
      btn.className = 'promo-btn ' + (color==='white'?'piece-white':'piece-black');
      btn.textContent = sym;
      btn.onclick = () => {
        modal.classList.remove('active');
        if(this.onMoveRequested) this.onMoveRequested(fromRow, fromCol, toRow, toCol, key);
      };
      opts.appendChild(btn);
    });
    modal.classList.add('active');
  }

  // ── CAPTURED PIECES & MATERIAL ───────────────────────
  updateCaptured(boardData) {
    const initial = {p:8,n:2,b:2,r:2,q:1,k:1,P:8,N:2,B:2,R:2,Q:1,K:1};
    const current = {p:0,n:0,b:0,r:0,q:0,k:0,P:0,N:0,B:0,R:0,Q:0,K:0};
    boardData.board.forEach(row => row.forEach(p => { if(p && current[p]!==undefined) current[p]++; }));

    const topCap    = document.getElementById('topCaptured');
    const bottomCap = document.getElementById('bottomCaptured');
    const topMat    = document.getElementById('topMaterial');
    const botMat    = document.getElementById('bottomMaterial');
    if(!topCap) return;

    let whiteCap='', blackCap='', wMat=0, bMat=0;
    for(const key of ['p','n','b','r','q']) {
      const lost = initial[key] - current[key];
      if(lost > 0) {
        whiteCap += PIECE_UNICODE[key].repeat(lost);
        bMat += lost * PIECE_VALUES[key];
      }
    }
    for(const key of ['P','N','B','R','Q']) {
      const lost = initial[key] - current[key];
      if(lost > 0) {
        blackCap += PIECE_UNICODE[key.toLowerCase()].repeat(lost);
        wMat += lost * PIECE_VALUES[key.toLowerCase()];
      }
    }

    const isPlayerWhite = window.PLAYER_COLOR !== 'black';
    topCap.textContent    = isPlayerWhite ? blackCap : whiteCap;
    bottomCap.textContent = isPlayerWhite ? whiteCap : blackCap;

    const diff = isPlayerWhite ? (wMat - bMat) : (bMat - wMat);
    topMat.textContent    = diff < 0 ? `+${-diff}` : '';
    botMat.textContent    = diff > 0 ? `+${diff}`  : '';
  }

  // ── FLIP BOARD ───────────────────────────────────────
  flip() {
    this.flipped = !this.flipped;
    this._buildLabels();
    if(this.boardData) this.render(this.boardData, window.PLAYER_COLOR);
  }

  // ── OFFLINE LEGAL MOVES (simplified, when no server) ─
  _getOfflineLegal(row, col) {
    return []; // In offline mode, moves aren't validated
  }
}
