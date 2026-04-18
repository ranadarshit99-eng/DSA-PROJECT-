// ─────────────────────────────────────────────────────────
//  game.js  —  Game controller
// ─────────────────────────────────────────────────────────

// ── PARSE URL PARAMS ─────────────────────────────────────
const params      = new URLSearchParams(window.location.search);
const GAME_MODE   = params.get('mode')        || 'bot';
const DIFFICULTY  = parseInt(params.get('difficulty') || '2');
const RAW_COLOR   = params.get('playerColor') || 'white';

window.PLAYER_COLOR = RAW_COLOR;

const DEPTH_LABELS = { 1:'2 ply', 2:'3 ply', 3:'4 ply', 4:'5 ply' };

// ── STATE ────────────────────────────────────────────────
let board          = null;       // ChessBoard instance
let currentState   = null;       // last board state from server
let moveListData   = [];         // [{white:'e4', black:'e5'}, ...]
let moveNumber     = 1;
let isPlayerTurn   = true;
let pendingPromo   = null;
let gameOver       = false;
let startTime      = null;
let timerInterval  = null;
let nodesPerSearch = 0;

// ── INIT ─────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', async () => {
  board = new ChessBoard('chessBoard');
  board.onMoveRequested = handleMoveRequest;

  // Set up flipping for black player
  if(PLAYER_COLOR === 'black') {
    board.flipped = true;
  }

  // UI labels
  updatePlayerCards();
  document.getElementById('modeBadge').textContent =
    GAME_MODE === 'bot' ? `vs Bot (${getDiffLabel(DIFFICULTY)})` : 'vs Friend';
  document.getElementById('dsaDepth').textContent  = DEPTH_LABELS[DIFFICULTY] || '3 ply';

  await checkServerConnection();

  if(offlineMode) {
    showOfflineBanner();
    initOfflineMode();
  } else {
    await initGame();
  }
});

function getDiffLabel(d) {
  return ['','Beginner','Casual','Club','Master'][d] || 'Medium';
}

// ── GAME INIT ────────────────────────────────────────────
async function initGame() {
  try {
    await API.newGame(PLAYER_COLOR);
    const state = await API.getBoard();
    currentState = state;
    board.render(state, PLAYER_COLOR);
    board.updateCaptured(state);
    board._buildLabels();

    startTime = Date.now();
    updateStatus('white_turn');

    // If player is black, bot plays first
    if(GAME_MODE === 'bot' && PLAYER_COLOR === 'black') {
      isPlayerTurn = false;
      setTimeout(requestBotMove, 600);
    } else {
      isPlayerTurn = true;
    }
  } catch(e) {
    console.error('Failed to init game:', e);
    showOfflineBanner();
    initOfflineMode();
  }
}

// ── PLAYER MOVE ──────────────────────────────────────────
async function handleMoveRequest(fromRow, fromCol, toRow, toCol, promotion='q') {
  if(gameOver) return;
  if(GAME_MODE === 'bot' && !isPlayerTurn) return;

  try {
    const result = await API.makeMove(fromRow, fromCol, toRow, toCol, promotion);
    if(!result) { showToast('Illegal move!'); return; }

    // Animate last move
    board.lastFrom = { row: fromRow, col: fromCol };
    board.lastTo   = { row: toRow,   col: toCol   };

    currentState = result;
    board.render(result, PLAYER_COLOR);
    board.updateCaptured(result);

   addMoveToList(fromRow, fromCol, toRow, toCol, result.currentPlayer === 'white');

    if(checkGameOver(result)) return;

    // Bot responds
    if(GAME_MODE === 'bot') {
      isPlayerTurn = false;
      updateStatus('bot_thinking');
      setTimeout(requestBotMove, 300);
    }
  } catch(e) {
    console.error('Move error:', e);
    showToast('Server error — is the C++ server running?');
  }
}

// ── BOT MOVE ─────────────────────────────────────────────
async function requestBotMove() {
  if(gameOver) return;
  document.getElementById('thinkingOverlay').classList.add('active');

  const t0 = performance.now();
  try {
    const result = await API.getBotMove(DIFFICULTY);
    const elapsed = performance.now() - t0;

    if(result && result.botMove) {
      const bm = result.botMove;
      board.lastFrom = { row: bm.fromRow, col: bm.fromCol };
      board.lastTo   = { row: bm.toRow,   col: bm.toCol   };
    }

    currentState = result;
    board.render(result, PLAYER_COLOR);
    board.updateCaptured(result);

    if(result && result.botMove) {
      const bm = result.botMove;
      addMoveToList(bm.fromRow, bm.fromCol, bm.toRow, bm.toCol, PLAYER_COLOR === 'white');
    }

    // Update DSA panel
    document.getElementById('dsaNodes').textContent = `~${Math.round(2000/elapsed*1000)/1000}k`;

  } catch(e) {
    console.error('Bot error:', e);
  } finally {
    document.getElementById('thinkingOverlay').classList.remove('active');
  }

  if(!checkGameOver(currentState)) {
    isPlayerTurn = true;
    updateStatus('player_turn');
  }
}

// ── GAME OVER CHECK ──────────────────────────────────────
function checkGameOver(state) {
  if(!state) return false;
  const s = state.status;
  if(s === 'playing') {
    updateStatus(state.currentPlayer === 'white' ? 'white_turn' : 'black_turn');
    return false;
  }

  gameOver = true;
  let icon, title, sub;
  switch(s) {
    case 'white_wins':
      icon='♔'; title='White Wins!';
      sub = GAME_MODE==='bot'
        ? (PLAYER_COLOR==='white' ? '🎉 You won! Excellent play!' : '🤖 Bot wins this time.')
        : 'White player wins!';
      break;
    case 'black_wins':
      icon='♚'; title='Black Wins!';
      sub = GAME_MODE==='bot'
        ? (PLAYER_COLOR==='black' ? '🎉 You won! Excellent play!' : '🤖 Bot wins this time.')
        : 'Black player wins!';
      break;
    case 'stalemate': icon='🤝'; title='Stalemate!';  sub='No legal moves — it\'s a draw!'; break;
    case 'draw_50':   icon='⏱'; title='50-Move Draw'; sub='No captures or pawn moves in 50 moves.'; break;
    case 'draw_rep':  icon='🔁'; title='Draw by Repetition'; sub='Same position occurred 3 times.'; break;
    default:          icon='🏁'; title='Game Over'; sub=''; break;
  }

  setTimeout(() => showGameOver(icon, title, sub), 800);
  updateStatus(s);
  return true;
}

function showGameOver(icon, title, sub) {
  document.getElementById('goIcon').textContent  = icon;
  document.getElementById('goTitle').textContent = title;
  document.getElementById('goSub').textContent   = sub;
  document.getElementById('gameOverModal').classList.add('active');
}

// ── STATUS BAR ───────────────────────────────────────────
function updateStatus(status) {
  const bar  = document.getElementById('statusBar');
  const dot  = document.getElementById('statusDot');
  const text = document.getElementById('statusText');

  const msgs = {
    white_turn:   '⬜ White to move',
    black_turn:   '⬛ Black to move',
    //bot_thinking: '🤖 Bot is thinking…',
    player_turn:  PLAYER_COLOR==='white' ? '⬜ Your turn (White)' : '⬛ Your turn (Black)',
    white_wins:   '♔ White wins by checkmate!',
    black_wins:   '♚ Black wins by checkmate!',
    stalemate:    '🤝 Stalemate – Draw',
    draw_50:      '⏱ Draw (50-move rule)',
    draw_rep:     '🔁 Draw (threefold repetition)',
  };

  text.textContent = msgs[status] || status;
  dot.style.background = status.includes('win') ? '#ff4444' :
                         status.includes('draw') || status === 'stalemate' ? '#ffaa00' :
                         'var(--green-accent)';
}

// ── MOVE LIST (PGN-like) ─────────────────────────────────
const FILES = ['a','b','c','d','e','f','g','h'];
function squareName(row, col) {
  return FILES[col] + (8 - row);  // row 0 = rank 8
}
function moveToNotation(fromRow, fromCol, toRow, toCol) {
  // Basic algebraic — just source+dest for now
  return squareName(fromRow,fromCol) + squareName(toRow,toCol);
}

function addMoveToList(fromRow, fromCol, toRow, toCol, isBlack) {
  const notation = moveToNotation(fromRow, fromCol, toRow, toCol);
  const list = document.getElementById('moveList');

  if(!isBlack) {
    // White's move — new row
    moveListData.push({ white: notation, black: null, num: moveNumber });
    const row = document.createElement('div');
    row.className = 'move-row';
    row.dataset.index = moveListData.length - 1;
    row.innerHTML = `
      <span class="move-num">${moveNumber}.</span>
      <span class="move-white current">${notation}</span>
      <span class="move-black">…</span>`;
    list.appendChild(row);
    moveNumber++;
  } else {
    // Black's move — update last row
    if(moveListData.length > 0) {
      const last = moveListData[moveListData.length-1];
      last.black = notation;
      const rows = list.querySelectorAll('.move-row');
      if(rows.length > 0) {
        const lastRow = rows[rows.length-1];
        const bSpan = lastRow.querySelector('.move-black');
        if(bSpan) { bSpan.textContent = notation; bSpan.classList.add('current'); }
        const wSpan = lastRow.querySelector('.move-white');
        if(wSpan) wSpan.classList.remove('current');
      }
    }
  }

  // Update counter
  document.getElementById('moveCount').textContent = list.querySelectorAll('.move-row').length;

  // Scroll to bottom
  list.scrollTop = list.scrollHeight;
}

// ── UNDO ─────────────────────────────────────────────────
async function requestUndo() {
  if(gameOver) return;
  try {
    // Undo twice in bot mode (undo player + bot move)
    const times = GAME_MODE === 'bot' ? 2 : 1;
    for(let i=0;i<times;i++) await API.undoMove();
    const state = await API.getBoard();
    currentState = state;
    board.lastFrom = null; board.lastTo = null;
    board.render(state, PLAYER_COLOR);
    board.updateCaptured(state);

    // Remove last move row(s) from list
    const list = document.getElementById('moveList');
    for(let i=0;i<times;i++) {
      const rows = list.querySelectorAll('.move-row');
      if(!rows.length) break;
      const last = rows[rows.length-1];
      const bSpan = last.querySelector('.move-black');
      if(i===0 && bSpan && bSpan.textContent !== '…') {
        bSpan.textContent = '…'; bSpan.classList.remove('current');
      } else {
        last.remove(); moveNumber--;
        moveListData.pop();
      }
    }
    document.getElementById('moveCount').textContent = list.querySelectorAll('.move-row').length;

    isPlayerTurn = true;
    gameOver = false;
    updateStatus('player_turn');
  } catch(e) { showToast('Undo failed'); }
}

// ── CONTROLS ─────────────────────────────────────────────
function flipBoard() { board.flip(); }
function newGame()   { window.location.reload(); }
function resignGame() {
  if(confirm('Resign this game?')) {
    const winner = PLAYER_COLOR === 'white' ? 'black_wins' : 'white_wins';
    showGameOver('🏳', 'You Resigned', 'Better luck next time!');
    gameOver = true;
  }
}

// ── PLAYER CARDS ─────────────────────────────────────────
function updatePlayerCards() {
  const isWhite = PLAYER_COLOR !== 'black';
  const botName = ['','Beginner Bot','Casual Bot','Club Bot','Master Bot'][DIFFICULTY] || 'Bot';

  if(isWhite) {
    document.getElementById('bottomAvatar').textContent = '♔';
    document.getElementById('bottomName').textContent   = 'You';
    document.getElementById('bottomSub').textContent    = 'White · Move first';
    document.getElementById('topAvatar').textContent    = GAME_MODE==='bot' ? '🤖' : '♚';
    document.getElementById('topName').textContent      = GAME_MODE==='bot' ? botName : 'Player 2';
    document.getElementById('topSub').textContent       = GAME_MODE==='bot' ? `Difficulty ${DIFFICULTY}` : 'Black';
  } else {
    document.getElementById('bottomAvatar').textContent = '♚';
    document.getElementById('bottomName').textContent   = 'You';
    document.getElementById('bottomSub').textContent    = 'Black · Wait for bot';
    document.getElementById('topAvatar').textContent    = GAME_MODE==='bot' ? '🤖' : '♔';
    document.getElementById('topName').textContent      = GAME_MODE==='bot' ? botName : 'Player 1';
    document.getElementById('topSub').textContent       = GAME_MODE==='bot' ? `Difficulty ${DIFFICULTY}` : 'White';
  }
}

// ── OFFLINE MODE ─────────────────────────────────────────
function showOfflineBanner() {
  const banner = document.createElement('div');
  banner.style.cssText = `
    position:fixed;top:0;left:0;right:0;z-index:9999;
    background:rgba(200,80,20,0.9);color:#fff;
    text-align:center;padding:10px;font-size:13px;
    backdrop-filter:blur(4px);
  `;
  banner.innerHTML = `
    ⚠️ <strong>C++ server not running.</strong>
    Showing demo board only.
    Run <code style="background:rgba(0,0,0,0.3);padding:2px 6px;border-radius:3px">make && ./chess_server</code>
    in the project folder to play.`;
  document.body.prepend(banner);
}

function initOfflineMode() {
  // Render static demo board
  const demoState = {
    board: [
      ['r','n','b','q','k','b','n','r'],
      ['p','p','p','p','p','p','p','p'],
      [null,null,null,null,null,null,null,null],
      [null,null,null,null,null,null,null,null],
      [null,null,null,null,null,null,null,null],
      [null,null,null,null,null,null,null,null],
      ['P','P','P','P','P','P','P','P'],
      ['R','N','B','Q','K','B','N','R']
    ],
    currentPlayer: 'white',
    status: 'playing',
    inCheck: false,
    moveCount: 0,
    enPassantCol: -1, enPassantRow: -1
  };
  currentState = demoState;
  board.render(demoState, PLAYER_COLOR);
  board.updateCaptured(demoState);
  updateStatus('white_turn');
}

// ── TOAST ────────────────────────────────────────────────
function showToast(msg) {
  const t = document.createElement('div');
  t.style.cssText = `
    position:fixed;bottom:24px;left:50%;transform:translateX(-50%);
    background:var(--bg-card);border:1px solid var(--border-bright);
    color:var(--text-primary);padding:10px 20px;border-radius:8px;
    font-size:14px;z-index:9999;animation:slideUp .3s ease;
  `;
  t.textContent = msg;
  document.body.appendChild(t);
  setTimeout(() => t.remove(), 2500);
}

// ── KEYBOARD SHORTCUTS ───────────────────────────────────
document.addEventListener('keydown', e => {
  if(e.key === 'z' && (e.ctrlKey||e.metaKey)) requestUndo();
  if(e.key === 'f') flipBoard();
  if(e.key === 'Escape') {
    document.querySelectorAll('.modal-overlay').forEach(m => m.classList.remove('active'));
    board._clearSelection?.();
  }
});
