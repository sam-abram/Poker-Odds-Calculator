#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "card.h"
#include "equity.h"
#include "serialize.h"
#include "showdown.h"

using namespace poker;

namespace {

struct Config {
    std::vector<PlayerHole> players;
    std::vector<Card> board;
    uint64_t trials = 100000;
    uint64_t seed = 1;
    bool json = false;       // reserved for JSON output (wired in a later step)
    bool force_mc = false;   // also run Monte Carlo post-flop
};

// Parse a run of concatenated cards (spaces allowed), e.g. "As Ks" or "2c7d9h".
std::vector<Card> parse_card_list(const std::string& s) {
    std::string t;
    for (char c : s)
        if (!std::isspace(static_cast<unsigned char>(c))) t += c;
    if (t.size() % 2 != 0)
        throw std::invalid_argument("card list must have an even number of characters: '" + s + "'");
    std::vector<Card> out;
    for (size_t i = 0; i < t.size(); i += 2)
        out.push_back(parse_card(t.substr(i, 2)));
    return out;
}

std::string commas(uint64_t x) {
    std::string s = std::to_string(x), out;
    int c = 0;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        out += s[i];
        if (++c % 3 == 0 && i > 0) out += ',';
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::string pct(double frac) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(2) << frac * 100.0;
    return o.str();
}

void print_usage() {
    std::cout <<
        "Texas Hold'em Odds Calculator\n\n"
        "Usage:\n"
        "  poker [--players AsKs QhQd ...] [--board 2c7d9h] [--trials N] [--seed S] [--mc] [--json]\n\n"
        "  --players  each token is one player's two cards (e.g. AsKs). 2-9 players.\n"
        "  --board    community cards: 0 (preflop), 3 (flop), 4 (turn) or 5 (river).\n"
        "  --trials   Monte Carlo trial count (default 100000).\n"
        "  --seed     RNG seed for reproducible Monte Carlo (default 1).\n"
        "  --mc       also run Monte Carlo when the board is already dealt.\n"
        "  --json     emit results as JSON (added in a later step).\n\n"
        "With no --players, the program prompts interactively.\n"
        "Cards: rank in 2-9,T,J,Q,K,A ; suit in s,h,d,c.\n";
}

// Returns true if --players was supplied (otherwise caller prompts interactively).
bool parse_args(int argc, char** argv, Config& cfg) {
    bool have_players = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need_value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(a + " needs a value");
            return argv[++i];
        };
        if (a == "--players" || a == "-p") {
            have_players = true;
            while (i + 1 < argc && argv[i + 1][0] != '-') {
                std::string tok = argv[++i];
                auto cards = parse_card_list(tok);
                if (cards.size() != 2)
                    throw std::invalid_argument("each player needs exactly 2 cards: '" + tok + "'");
                cfg.players.push_back({cards[0], cards[1]});
            }
        } else if (a == "--board" || a == "-b") {
            if (i + 1 < argc && argv[i + 1][0] != '-')
                cfg.board = parse_card_list(argv[++i]);
        } else if (a == "--trials" || a == "-t") {
            cfg.trials = std::stoull(need_value());
        } else if (a == "--seed" || a == "-s") {
            cfg.seed = std::stoull(need_value());
        } else if (a == "--json") {
            cfg.json = true;
        } else if (a == "--mc") {
            cfg.force_mc = true;
        } else if (a == "--help" || a == "-h") {
            print_usage();
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown argument: " + a);
        }
    }
    return have_players;
}

std::string prompt_line(const std::string& msg) {
    std::cout << msg;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

int prompt_int(const std::string& msg, int lo, int hi) {
    while (true) {
        std::string line = prompt_line(msg);
        try {
            int v = std::stoi(line);
            if (v < lo || v > hi)
                throw std::out_of_range("value must be between " + std::to_string(lo) +
                                        " and " + std::to_string(hi));
            return v;
        } catch (const std::exception& e) {
            std::cout << "  invalid: " << e.what() << "; try again.\n";
        }
    }
}

void interactive_input(Config& cfg) {
    std::cout << "Texas Hold'em Odds Calculator\n";
    std::cout << "Cards: rank(2-9,T,J,Q,K,A) + suit(s,h,d,c), e.g. As Td 9c\n\n";
    int n = prompt_int("Number of players (2-9): ", MIN_PLAYERS, MAX_PLAYERS);
    for (int i = 0; i < n; ++i) {
        while (true) {
            std::string line = prompt_line("Player " + std::to_string(i + 1) +
                                           " hole cards (e.g. As Ks): ");
            try {
                auto c = parse_card_list(line);
                if (c.size() != 2) throw std::invalid_argument("enter exactly 2 cards");
                cfg.players.push_back({c[0], c[1]});
                break;
            } catch (const std::exception& e) {
                std::cout << "  " << e.what() << "; try again.\n";
            }
        }
    }
    while (true) {
        std::string line = prompt_line("Board (blank=preflop, or 3/4/5 cards, e.g. 2c 7d 9h): ");
        try {
            auto c = parse_card_list(line);
            size_t s = c.size();
            if (!(s == 0 || s == 3 || s == 4 || s == 5))
                throw std::invalid_argument("board must be 0, 3, 4 or 5 cards");
            cfg.board = c;
            break;
        } catch (const std::exception& e) {
            std::cout << "  " << e.what() << "; try again.\n";
        }
    }
}

void validate(const Config& cfg) {
    if (cfg.players.size() < MIN_PLAYERS || cfg.players.size() > MAX_PLAYERS)
        throw std::invalid_argument("need between 2 and 9 players");
    size_t bs = cfg.board.size();
    if (!(bs == 0 || bs == 3 || bs == 4 || bs == 5))
        throw std::invalid_argument("board must be 0, 3, 4 or 5 cards");
    bool seen[NUM_CARDS] = {false};
    auto mark = [&](Card c) {
        if (seen[c]) throw std::invalid_argument("duplicate card: " + card_to_string(c));
        seen[c] = true;
    };
    for (const auto& p : cfg.players) { mark(p.a); mark(p.b); }
    for (Card c : cfg.board) mark(c);
}

void print_header(const Config& cfg) {
    std::cout << "\n=== Texas Hold'em Odds ===\n";
    std::cout << "Players: " << cfg.players.size() << "   Board: ";
    if (cfg.board.empty()) {
        std::cout << "preflop";
    } else {
        for (size_t i = 0; i < cfg.board.size(); ++i) {
            if (i) std::cout << ' ';
            std::cout << card_to_string(cfg.board[i]);
        }
        std::cout << " (" << street_name(cfg.board.size()) << ")";
    }
    std::cout << "\n";
}

void print_result(const Result& r, const Config& cfg, const char* label) {
    std::cout << "\nMethod: " << label << " (" << commas(r.outcomes)
              << (r.method == Method::Enumeration ? " boards" : " trials")
              << ", " << std::fixed << std::setprecision(3) << r.seconds << "s)\n";

    std::cout << "  " << std::left << std::setw(4) << "#" << std::setw(9) << "Hole"
              << std::right << std::setw(9) << "Win%" << std::setw(9) << "Tie%"
              << std::setw(10) << "Equity%";
    if (r.method == Method::MonteCarlo) std::cout << std::setw(12) << "+/-95%CI";
    std::cout << "\n";

    for (size_t i = 0; i < r.players.size(); ++i) {
        const auto& ps = r.players[i];
        std::string hole = card_to_string(cfg.players[i].a) + " " + card_to_string(cfg.players[i].b);
        std::cout << "  " << std::left << std::setw(4) << ("P" + std::to_string(i + 1))
                  << std::setw(9) << hole
                  << std::right << std::setw(9) << pct(ps.win)
                  << std::setw(9) << pct(ps.tie)
                  << std::setw(10) << pct(ps.equity);
        if (r.method == Method::MonteCarlo)
            std::cout << std::setw(12) << ("+/-" + pct(ps.equity_ci));
        std::cout << "\n";
    }
}

void print_comparison(const Result& ex, const Result& mc) {
    double maxerr = 0.0;
    for (size_t i = 0; i < ex.players.size(); ++i)
        maxerr = std::max(maxerr, std::fabs(ex.players[i].equity - mc.players[i].equity));
    std::cout << "\nMC vs exact: max |equity error| = " << pct(maxerr) << " pts";
    double speedup = (mc.seconds > 0) ? ex.seconds / mc.seconds : 0.0;
    std::cout << "   |   enum " << std::fixed << std::setprecision(3) << ex.seconds
              << "s vs mc " << mc.seconds << "s";
    if (speedup > 0) std::cout << " (" << std::setprecision(1) << speedup << "x)";
    std::cout << "\n";
}

void print_categories(const Result& r) {
    std::cout << "\nHand-category probabilities (final made hand, exact):\n";
    const double N = static_cast<double>(r.outcomes);
    for (size_t i = 0; i < r.players.size(); ++i) {
        std::cout << "  P" << (i + 1);
        for (int c = 0; c < NUM_CATEGORIES; ++c)
            std::cout << "  " << SHORT_CAT[c] << " " << pct(r.players[i].category_counts[c] / N);
        std::cout << "\n";
    }
}

void run(const Config& cfg) {
    const bool also_mc = cfg.board.empty() || cfg.force_mc;
    Result ex = enumerate(cfg.players, cfg.board);
    Result mc;
    if (also_mc) mc = monte_carlo(cfg.players, cfg.board, cfg.trials, cfg.seed);

    if (cfg.json) {
        std::cout << results_to_json(cfg.players, cfg.board, ex, also_mc ? &mc : nullptr) << "\n";
        return;
    }

    print_header(cfg);
    print_result(ex, cfg, "exact enumeration");
    if (also_mc) {
        print_result(mc, cfg, "monte carlo");
        print_comparison(ex, mc);
    }
    print_categories(ex);
}

} // namespace

int main(int argc, char** argv) {
    try {
        Config cfg;
        bool have_players = parse_args(argc, argv, cfg);
        if (!have_players) interactive_input(cfg);
        validate(cfg);
        run(cfg);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
