// Local HTTP server: serves the static web UI and a JSON equity API backed by the
// existing C++ engine. Run:  poker_server.exe [--port 8080] [--web web]
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "card.h"
#include "equity.h"
#include "http_server.h"
#include "serialize.h"
#include "showdown.h"

using namespace poker;

namespace {

std::string g_web_dir = "web";
constexpr uint64_t MAX_TRIALS = 5'000'000;  // guard against runaway requests

std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

// Parse concatenated 2-char cards, e.g. "2c7d9h" -> [2c,7d,9h].
std::vector<Card> parse_card_run(const std::string& s) {
    std::string t;
    for (char c : s)
        if (!std::isspace(static_cast<unsigned char>(c))) t += c;
    if (t.size() % 2 != 0)
        throw std::invalid_argument("card list has odd length: '" + s + "'");
    std::vector<Card> out;
    for (size_t i = 0; i < t.size(); i += 2)
        out.push_back(parse_card(t.substr(i, 2)));
    return out;
}

void validate(const std::vector<PlayerHole>& players, const std::vector<Card>& board) {
    if (players.size() < MIN_PLAYERS || players.size() > MAX_PLAYERS)
        throw std::invalid_argument("need between 2 and 9 players");
    size_t bs = board.size();
    if (!(bs == 0 || bs == 3 || bs == 4 || bs == 5))
        throw std::invalid_argument("board must be 0, 3, 4 or 5 cards");
    bool seen[NUM_CARDS] = {false};
    auto mark = [&](Card c) {
        if (seen[c]) throw std::invalid_argument("duplicate card: " + card_to_string(c));
        seen[c] = true;
    };
    for (const auto& p : players) { mark(p.a); mark(p.b); }
    for (Card c : board) mark(c);
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}

const std::string* find(const http::Request& req, const char* key) {
    auto it = req.query.find(key);
    return it == req.query.end() ? nullptr : &it->second;
}

http::Response api_equity(const http::Request& req) {
    http::Response res;
    res.content_type = "application/json";
    try {
        const std::string* players_s = find(req, "players");
        if (!players_s || players_s->empty())
            throw std::invalid_argument("missing 'players' parameter");

        std::vector<PlayerHole> players;
        for (const std::string& tok : split(*players_s, ',')) {
            if (tok.empty()) continue;
            auto cs = parse_card_run(tok);
            if (cs.size() != 2)
                throw std::invalid_argument("each player needs exactly 2 cards: '" + tok + "'");
            players.push_back({cs[0], cs[1]});
        }

        std::vector<Card> board;
        if (const std::string* b = find(req, "board"); b && !b->empty())
            board = parse_card_run(*b);

        uint64_t trials = 100000, seed = 1;
        bool force_mc = false;
        if (const std::string* t = find(req, "trials"); t) trials = std::stoull(*t);
        if (const std::string* s = find(req, "seed"); s) seed = std::stoull(*s);
        if (const std::string* m = find(req, "mc"); m) force_mc = (*m == "1" || *m == "true");
        if (trials > MAX_TRIALS) trials = MAX_TRIALS;

        validate(players, board);

        bool also_mc = board.empty() || force_mc;
        Result ex = enumerate(players, board);
        Result mc;
        if (also_mc) mc = monte_carlo(players, board, trials, seed);

        res.status = 200;
        res.body = results_to_json(players, board, ex, also_mc ? &mc : nullptr);
    } catch (const std::exception& e) {
        res.status = 400;
        res.body = std::string("{\"error\":\"") + json_escape(e.what()) + "\"}";
    }
    return res;
}

std::string content_type_for(const std::string& path) {
    auto ends = [&](const char* e) {
        size_t n = std::strlen(e);
        return path.size() >= n && path.compare(path.size() - n, n, e) == 0;
    };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".css"))  return "text/css; charset=utf-8";
    if (ends(".js"))   return "application/javascript; charset=utf-8";
    if (ends(".json")) return "application/json";
    if (ends(".svg"))  return "image/svg+xml";
    if (ends(".png"))  return "image/png";
    if (ends(".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

http::Response serve_static(const std::string& path) {
    http::Response res;
    std::string rel = (path == "/" || path.empty()) ? "/index.html" : path;
    if (rel.find("..") != std::string::npos) {
        res.status = 400;
        res.body = "bad path";
        return res;
    }
    std::string full = g_web_dir + rel;
    std::ifstream f(full, std::ios::binary);
    if (!f) {
        res.status = 404;
        res.body = "404 Not Found: " + rel;
        return res;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    res.status = 200;
    res.content_type = content_type_for(rel);
    res.body = ss.str();
    return res;
}

http::Response handle(const http::Request& req) {
    if (req.path == "/api/equity") return api_equity(req);
    return serve_static(req.path);
}

} // namespace

int main(int argc, char** argv) {
    uint16_t port = 8090;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) port = static_cast<uint16_t>(std::stoi(argv[++i]));
        else if (a == "--web" && i + 1 < argc) g_web_dir = argv[++i];
        else {
            std::cerr << "usage: poker_server [--port N] [--web DIR]\n";
            return 1;
        }
    }

    http::Server server(handle);
    std::cout << "Poker odds server: http://127.0.0.1:" << port
              << "   (serving '" << g_web_dir << "')\n";
    std::cout << "Press Ctrl+C to stop.\n";
    if (!server.listen("127.0.0.1", port)) {
        std::cerr << "Error: could not bind to port " << port
                  << " (is it already in use?)\n";
        return 1;
    }
    return 0;
}
