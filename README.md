# ♟ ChessDSA — Full Chess Game (C++ Engine + HTML/CSS/JS Frontend)

A complete chess game powered by a **C++ backend engine** with full DSA algorithms, served to a beautiful chess.com-inspired frontend.

---

## 🏗 Project Structure

```
chess-project/
├── engine/
│   ├── chess_engine.h       ← Chess engine header (DSA data structures)
│   ├── chess_engine.cpp     ← Full engine: move gen, minimax, alpha-beta
│   └── server.cpp           ← POSIX HTTP server (no external deps!)
├── frontend/
│   ├── index.html           ← Landing page with animated showcase
│   ├── game.html            ← Chess game page (chess.com layout)
│   ├── css/style.css        ← Full UI styles (green/grey theme)
│   └── js/
│       ├── api.js           ← API communication layer
│       ├── board.js         ← Board rendering + drag & drop
│       └── game.js          ← Game controller
├── Makefile                 ← Build system
└── README.md
```

---

## 🚀 How to Run in VS Code

### Prerequisites
Install these once:
```bash
# Ubuntu/Debian/WSL
sudo apt update && sudo apt install g++ make

# macOS (with Homebrew)
brew install gcc

# Windows → use WSL2 or MinGW
```

### Step 1: Open in VS Code
```bash
cd chess-project
code .
```

### Step 2: Open a Terminal in VS Code
Press **Ctrl + `** (backtick) or go to **Terminal → New Terminal**

### Step 3: Build the C++ Server
```bash
make
```
This compiles `engine/chess_engine.cpp` + `engine/server.cpp` into `./chess_server`.

Expected output:
```
g++ -std=c++17 -O2 -Wall -Wextra -pthread -o chess_server engine/chess_engine.cpp engine/server.cpp
✅  Build successful!
   Run: ./chess_server
```

### Step 4: Run the Server
```bash
./chess_server
```
Output:
```
╔══════════════════════════════════════╗
║   Chess Engine Server  (C++ DSA)     ║
╚══════════════════════════════════════╝
  Listening on http://localhost:8080
  Open browser at http://localhost:8080
```

### Step 5: Open in Browser
Visit **http://localhost:8080** in your browser.

---

## 🎮 How to Play

1. **Landing Page**: Choose "Play vs Bot" or "Play vs Friend"
2. **Bot Game**: Select difficulty and your color
3. **Game Controls**:
   - Click a piece → see legal moves (green dots)
   - Click destination → make the move
   - Drag and drop also works!
   - **Ctrl+Z** — Undo last move
   - **F** key — Flip board
4. **Pawn Promotion**: A modal appears when your pawn reaches the last rank

---

## 🧠 DSA Algorithms Used (C++)

| Algorithm | Where | Description |
|-----------|-------|-------------|
| **Stack** (`std::stack`) | Move history | Stores full game state for undo/redo |
| **Minimax** | Bot AI | Recursive game tree search |
| **Alpha-Beta Pruning** | Bot AI | Cuts branches that can't affect outcome — O(b^d/2) vs O(b^d) |
| **Quiescence Search** | Bot AI | Extends search at captures to avoid horizon effect |
| **Zobrist Hashing** | Position tracking | Random XOR keys for fast position comparison |
| **Transposition Table** (`unordered_map`) | Bot AI | Memoizes evaluated positions |
| **MVV-LVA Move Ordering** | Bot AI | Sorts captures by victim/attacker value for better pruning |
| **Piece-Square Tables** | Evaluation | 2D lookup tables for positional scoring |

---

## 🤖 Bot Difficulty Levels

| Level | Rating | Depth | Description |
|-------|--------|-------|-------------|
| Beginner | ~400 | 2 ply | 50% random, depth-2 otherwise |
| Casual   | ~800 | 3 ply | Depth-3 minimax |
| Club     | ~1400| 4 ply | Depth-4 + quiescence search |
| Master   | ~1900| 5 ply | Depth-5 + full transposition table |

---

## ♟ Chess Rules Implemented

- ✅ All piece movements (Pawn, Knight, Bishop, Rook, Queen, King)
- ✅ Pawn double-push from starting rank
- ✅ En passant capture
- ✅ Castling (kingside & queenside)
- ✅ Pawn promotion (Queen/Rook/Bishop/Knight)
- ✅ Check detection
- ✅ Checkmate detection
- ✅ Stalemate detection
- ✅ 50-move draw rule
- ✅ Threefold repetition draw

---

## 🛠 VS Code Recommended Extensions

Install these in VS Code for a better experience:
- **C/C++** (ms-vscode.cpptools) — Syntax highlighting & IntelliSense
- **C/C++ Extension Pack** — Full C++ tooling
- **Live Server** — Serve frontend without the C++ server
- **Better C++ Syntax** — Enhanced syntax coloring

### tasks.json (Optional: Build with Ctrl+Shift+B)
Create `.vscode/tasks.json`:
```json
{
  "version": "2.0.0",
  "tasks": [{
    "label": "Build Chess Server",
    "type": "shell",
    "command": "make",
    "group": { "kind": "build", "isDefault": true },
    "presentation": { "reveal": "always" }
  }]
}
```

---

## 🔌 API Reference (C++ Server)

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/new-game` | Start new game `{"playerColor":"white"}` |
| GET  | `/api/board` | Get full board state |
| GET  | `/api/legal-moves?row=6&col=4` | Legal moves for a square |
| POST | `/api/move` | Make a move `{"fromRow":6,"fromCol":4,"toRow":4,"toCol":4}` |
| POST | `/api/bot-move` | Request bot move `{"difficulty":2}` |
| POST | `/api/undo` | Undo last move |
| GET  | `/api/state` | Get game status |

---

## 🎨 Theme

- **Board**: Classic cream/brown squares
- **UI**: Deep forest green (`#1a2f1a`) + ivory text
- **Accents**: Bright lime green (`#6aaa4a`)
- **Fonts**: Playfair Display (headings) + DM Sans (body) + JetBrains Mono (numbers)

---

## 🐛 Troubleshooting

**"Permission denied"**
```bash
chmod +x chess_server
```

**Port 8080 already in use**
```bash
./chess_server 9090    # Use different port
# Then open http://localhost:9090
```

**Frontend shows "C++ server not running"**
- Make sure you ran `./chess_server` in the terminal
- Check browser console for CORS errors
- Try opening `http://localhost:8080` directly

**Segfault / crash**
```bash
make clean && make    # Recompile from scratch
```
