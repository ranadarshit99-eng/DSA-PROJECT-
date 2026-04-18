// ─────────────────────────────────────────────────────────
//  api.js  —  Communication with C++ backend
// ─────────────────────────────────────────────────────────
const API_BASE = 'http://localhost:8080/api';

const API = {
  async newGame(playerColor) {
    const r = await fetch(`${API_BASE}/new-game`, {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ playerColor })
    });
    return r.json();
  },

  async getBoard() {
    const r = await fetch(`${API_BASE}/board`);
    return r.json();
  },

  async getLegalMoves(row, col) {
    const r = await fetch(`${API_BASE}/legal-moves?row=${row}&col=${col}`);
    return r.json();
  },

  async makeMove(fromRow, fromCol, toRow, toCol, promotion='q') {
    const r = await fetch(`${API_BASE}/move`, {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ fromRow, fromCol, toRow, toCol, promotion })
    });
    if(!r.ok) return null;
    return r.json();
  },

  async getBotMove(difficulty) {
    const r = await fetch(`${API_BASE}/bot-move`, {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ difficulty: parseInt(difficulty) })
    });
    return r.json();
  },

  async undoMove() {
    const r = await fetch(`${API_BASE}/undo`, {
      method: 'POST',
      headers: {'Content-Type':'application/json'},
      body: '{}'
    });
    return r.json();
  },

  async getState() {
    const r = await fetch(`${API_BASE}/state`);
    return r.json();
  }
};

// ─────────────────────────────────────────────────────────
//  Offline / Demo Mode (when C++ server not running)
// ─────────────────────────────────────────────────────────
const DEMO_BOARD = [
  ['r','n','b','q','k','b','n','r'],
  ['p','p','p','p','p','p','p','p'],
  [null,null,null,null,null,null,null,null],
  [null,null,null,null,null,null,null,null],
  [null,null,null,null,null,null,null,null],
  [null,null,null,null,null,null,null,null],
  ['P','P','P','P','P','P','P','P'],
  ['R','N','B','Q','K','B','N','R']
];

let offlineMode = false;

async function checkServerConnection() {
  try {
    const r = await fetch(`${API_BASE}/state`, { signal: AbortSignal.timeout(1500) });
    offlineMode = !r.ok;
  } catch {
    offlineMode = true;
    console.warn('C++ server not running — using offline JS mode');
  }
}
