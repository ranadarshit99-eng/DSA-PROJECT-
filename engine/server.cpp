#include "chess_engine.h"
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <functional>
#include <thread>
#include <mutex>
#include <unordered_map>

// POSIX socket headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <signal.h>
#include <arpa/inet.h>

// ──────────────────────────────────────────────────────────
//  GLOBALS
// ──────────────────────────────────────────────────────────
static ChessEngine engine;
static std::mutex  engineMutex;
static int         serverFd = -1;

// ──────────────────────────────────────────────────────────
//  MINIMAL HTTP REQUEST PARSER
// ──────────────────────────────────────────────────────────
struct HttpRequest {
    std::string method, path, body;
    std::map<std::string,std::string> params;  // query params
    std::map<std::string,std::string> headers;
};

struct HttpResponse {
    int         status = 200;
    std::string contentType = "application/json";
    std::string body;
};

static std::string urlDecode(const std::string& s) {
    std::string out;
    for(size_t i=0;i<s.size();i++){
        if(s[i]=='%'&&i+2<s.size()){
            int v; sscanf(s.c_str()+i+1,"%2x",&v);
            out+=(char)v; i+=2;
        } else if(s[i]=='+') out+=' ';
        else out+=s[i];
    }
    return out;
}

static std::map<std::string,std::string> parseQuery(const std::string& query){
    std::map<std::string,std::string> m;
    std::istringstream ss(query);
    std::string token;
    while(std::getline(ss,token,'&')){
        auto eq = token.find('=');
        if(eq != std::string::npos)
            m[urlDecode(token.substr(0,eq))] = urlDecode(token.substr(eq+1));
    }
    return m;
}

static HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream ss(raw);
    std::string line;
    // First line: METHOD PATH HTTP/1.1
    std::getline(ss, line);
    if(!line.empty() && line.back()=='\r') line.pop_back();
    std::istringstream ls(line);
    std::string pathFull;
    ls >> req.method >> pathFull;

    auto qmark = pathFull.find('?');
    if(qmark != std::string::npos) {
        req.path   = pathFull.substr(0, qmark);
        req.params = parseQuery(pathFull.substr(qmark+1));
    } else {
        req.path = pathFull;
    }

    // Headers
    int contentLength = 0;
    while(std::getline(ss, line)){
        if(!line.empty()&&line.back()=='\r') line.pop_back();
        if(line.empty()) break;
        auto colon = line.find(':');
        if(colon!=std::string::npos){
            std::string key = line.substr(0,colon);
            std::string val = line.substr(colon+2);
            req.headers[key] = val;
            if(key=="Content-Length") contentLength=std::stoi(val);
        }
    }
    // Body
    if(contentLength > 0) {
        req.body.resize(contentLength);
        ss.read(&req.body[0], contentLength);
    }
    return req;
}

// ──────────────────────────────────────────────────────────
//  JSON HELPERS  (minimal, no external lib)
// ──────────────────────────────────────────────────────────
static std::string jsonStr(const std::string& key, const std::string& val) {
    return "\"" + key + "\":\"" + val + "\"";
}
static std::string jsonInt(const std::string& key, int val) {
    return "\"" + key + "\":" + std::to_string(val);
}
static std::string jsonBool(const std::string& key, bool val) {
    return "\"" + key + "\":" + (val?"true":"false");
}

static int getJsonInt(const std::string& body, const std::string& key) {
    auto pos = body.find("\""+key+"\"");
    if(pos==std::string::npos) return -1;
    auto colon = body.find(':', pos);
    if(colon==std::string::npos) return -1;
    return std::stoi(body.substr(colon+1));
}
static std::string getJsonStr(const std::string& body, const std::string& key) {
    auto pos = body.find("\""+key+"\"");
    if(pos==std::string::npos) return "";
    auto colon = body.find(':', pos);
    if(colon==std::string::npos) return "";
    auto q1 = body.find('"', colon+1);
    if(q1==std::string::npos) return "";
    auto q2 = body.find('"', q1+1);
    if(q2==std::string::npos) return "";
    return body.substr(q1+1, q2-q1-1);
}

// ──────────────────────────────────────────────────────────
//  HTTP RESPONSE BUILDER
// ──────────────────────────────────────────────────────────
static std::string buildResponse(const HttpResponse& res) {
    std::ostringstream ss;
    ss << "HTTP/1.1 " << res.status << " OK\r\n";
    ss << "Content-Type: " << res.contentType << "\r\n";
    ss << "Content-Length: " << res.body.size() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    ss << "Access-Control-Allow-Headers: Content-Type\r\n";
    ss << "Connection: close\r\n\r\n";
    ss << res.body;
    return ss.str();
}

// ──────────────────────────────────────────────────────────
//  ROUTE HANDLERS
// ──────────────────────────────────────────────────────────
static HttpResponse handleNewGame(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    std::string playerColor = getJsonStr(req.body, "playerColor");
    engine.resetBoard();
    HttpResponse res;
    res.body = "{" + jsonStr("status","ok") + "," + jsonStr("playerColor", playerColor.empty()?"white":playerColor) + "}";
    return res;
}

static HttpResponse handleGetBoard(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    HttpResponse res;
    res.body = engine.getBoardJSON();
    return res;
}

static HttpResponse handleLegalMoves(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    int row = -1, col = -1;
    auto it = req.params.find("row"); if(it!=req.params.end()) row=std::stoi(it->second);
    it = req.params.find("col");      if(it!=req.params.end()) col=std::stoi(it->second);
    HttpResponse res;
    if(row<0||row>=8||col<0||col>=8){ res.body="[]"; return res; }
    res.body = engine.getLegalMovesJSON(row, col);
    return res;
}

static HttpResponse handleMakeMove(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    // Body: {"fromRow":r,"fromCol":c,"toRow":r,"toCol":c,"promotion":"q"}
    int fromRow = getJsonInt(req.body,"fromRow");
    int fromCol = getJsonInt(req.body,"fromCol");
    int toRow   = getJsonInt(req.body,"toRow");
    int toCol   = getJsonInt(req.body,"toCol");
    std::string promoStr = getJsonStr(req.body,"promotion");

    // Convert visual row (0=rank8) to internal row
    int iFromRow = 7 - fromRow;
    int iToRow   = 7 - toRow;

    PieceType promo = QUEEN;
    if(promoStr=="r") promo=ROOK;
    else if(promoStr=="b") promo=BISHOP;
    else if(promoStr=="n") promo=KNIGHT;

    bool ok = engine.makeMove(iFromRow, fromCol, iToRow, toCol, promo);
    HttpResponse res;
    if(!ok){ res.status=400; res.body="{\"error\":\"illegal move\"}"; return res; }
    res.body = engine.getBoardJSON();
    return res;
}

static HttpResponse handleBotMove(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    int difficulty = getJsonInt(req.body,"difficulty");
    if(difficulty < 1) difficulty = 2;
    if(difficulty > 4) difficulty = 4;

    // Check game is still going
    if(engine.gameStatus != ChessEngine::PLAYING) {
        HttpResponse res;
        res.body = engine.getBoardJSON();
        return res;
    }

    Move m = engine.getBotMove(difficulty);
    // Apply the bot's move
    engine.makeMove(m.fromRow, m.fromCol, m.toRow, m.toCol, m.promotion!=EMPTY?m.promotion:QUEEN);

    HttpResponse res;
    // Include the move in response
    std::ostringstream ss;
    ss << engine.getBoardJSON();
    // Inject bot move info
    std::string bj = ss.str();
    // Remove last }
    bj.pop_back();
    bj += ",\"botMove\":{";
    bj += "\"fromRow\":" + std::to_string(7-m.fromRow) + ",";
    bj += "\"fromCol\":" + std::to_string(m.fromCol) + ",";
    bj += "\"toRow\":"   + std::to_string(7-m.toRow) + ",";
    bj += "\"toCol\":"   + std::to_string(m.toCol);
    bj += "}}";
    res.body = bj;
    return res;
}

static HttpResponse handleUndoMove(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    engine.undoLastMove();
    HttpResponse res;
    res.body = engine.getBoardJSON();
    return res;
}

static HttpResponse handleGetState(const HttpRequest& req) {
    std::lock_guard<std::mutex> lock(engineMutex);
    HttpResponse res;
    res.body = engine.getGameStateJSON();
    return res;
}

// ──────────────────────────────────────────────────────────
//  STATIC FILE SERVER (serves frontend/)
// ──────────────────────────────────────────────────────────
#include <fstream>
static std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if(!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
static std::string getMimeType(const std::string& path) {
    if(path.size()>=5 && path.substr(path.size()-5)==".html") return "text/html";
    if(path.size()>=4 && path.substr(path.size()-4)==".css")  return "text/css";
    if(path.size()>=3 && path.substr(path.size()-3)==".js")   return "application/javascript";
    if(path.size()>=4 && path.substr(path.size()-4)==".png")  return "image/png";
    if(path.size()>=4 && path.substr(path.size()-4)==".ico")  return "image/x-icon";
    return "text/plain";
}

static HttpResponse serveStatic(const std::string& urlPath) {
    HttpResponse res;
    std::string filePath = "../frontend" + urlPath;
    if(urlPath == "/" || urlPath.empty()) filePath = "../frontend/index.html";
    std::string content = readFile(filePath);
    if(content.empty()) {
        res.status = 404;
        res.contentType = "text/plain";
        res.body = "404 Not Found";
        return res;
    }
    res.contentType = getMimeType(filePath);
    res.body = content;
    return res;
}

// ──────────────────────────────────────────────────────────
//  REQUEST DISPATCHER
// ──────────────────────────────────────────────────────────
static HttpResponse dispatch(const HttpRequest& req) {
    // CORS preflight
    if(req.method == "OPTIONS") {
        HttpResponse res; res.body = ""; return res;
    }
    // API routes
    if(req.path == "/api/new-game"   && req.method == "POST") return handleNewGame(req);
    if(req.path == "/api/board"      && req.method == "GET")  return handleGetBoard(req);
    if(req.path == "/api/legal-moves"&& req.method == "GET")  return handleLegalMoves(req);
    if(req.path == "/api/move"       && req.method == "POST") return handleMakeMove(req);
    if(req.path == "/api/bot-move"   && req.method == "POST") return handleBotMove(req);
    if(req.path == "/api/undo"       && req.method == "POST") return handleUndoMove(req);
    if(req.path == "/api/state"      && req.method == "GET")  return handleGetState(req);
    // Static files
    return serveStatic(req.path);
}

// ──────────────────────────────────────────────────────────
//  CLIENT HANDLER (runs in separate thread)
// ──────────────────────────────────────────────────────────
static void handleClient(int clientFd) {
    char buf[65536] = {0};
    ssize_t bytes = recv(clientFd, buf, sizeof(buf)-1, 0);
    if(bytes <= 0) { close(clientFd); return; }
    buf[bytes] = '\0';

    std::string raw(buf, bytes);
    HttpRequest  req  = parseRequest(raw);
    HttpResponse resp = dispatch(req);
    std::string  out  = buildResponse(resp);

    send(clientFd, out.c_str(), out.size(), 0);
    close(clientFd);
}

// ──────────────────────────────────────────────────────────
//  MAIN
// ──────────────────────────────────────────────────────────
void signalHandler(int) {
    if(serverFd != -1) close(serverFd);
    exit(0);
}

int main(int argc, char** argv) {
    int port = 8080;
    if(argc > 1) port = std::atoi(argv[1]);

    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if(bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if(listen(serverFd, 16) < 0) {
        perror("listen"); return 1;
    }

    std::cout << "\n╔══════════════════════════════════════╗" << std::endl;
    std::cout << "║   Chess Engine Server  (C++ DSA)     ║" << std::endl;
    std::cout << "╚══════════════════════════════════════╝" << std::endl;
    std::cout << "  Listening on http://localhost:" << port << std::endl;
    std::cout << "  Open browser at http://localhost:" << port << std::endl;
    std::cout << "  Press Ctrl+C to stop\n" << std::endl;

    while(true) {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if(clientFd < 0) continue;
        // Handle each client in a thread
        std::thread(handleClient, clientFd).detach();
    }
    return 0;
}
