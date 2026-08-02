#include "serialize.h"
#include <iomanip>
#include <sstream>

namespace poker {

namespace {

const char* const RANK_SINGULAR[NUM_RANKS] = {
    "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine", "Ten",
    "Jack", "Queen", "King", "Ace"};
const char* const RANK_PLURAL[NUM_RANKS] = {
    "Twos", "Threes", "Fours", "Fives", "Sixes", "Sevens", "Eights", "Nines",
    "Tens", "Jacks", "Queens", "Kings", "Aces"};

// A short human label for a made hand, decoding the top ranks packed in the score.
std::string made_hand_label(HandValue hv) {
    int t0 = (hv.score >> 16) & 0xF;
    int t1 = (hv.score >> 12) & 0xF;
    switch (hv.category) {
        case Category::HighCard:      return std::string(RANK_SINGULAR[t0]) + " high";
        case Category::Pair:          return std::string("pair of ") + RANK_PLURAL[t0];
        case Category::TwoPair:       return std::string("two pair, ") + RANK_PLURAL[t0] + " & " + RANK_PLURAL[t1];
        case Category::Trips:         return std::string("three of a kind, ") + RANK_PLURAL[t0];
        case Category::Straight:      return "straight";
        case Category::Flush:         return "flush";
        case Category::FullHouse:     return std::string("full house, ") + RANK_PLURAL[t0] + " full of " + RANK_PLURAL[t1];
        case Category::Quads:         return std::string("four of a kind, ") + RANK_PLURAL[t0];
        case Category::StraightFlush: return "straight flush";
    }
    return "";
}

void emit_result(std::ostream& o, const std::vector<PlayerHole>& players,
                 const std::vector<Card>& board, const Result& r, const char* method) {
    o << "{\"method\":\"" << method << "\",\"outcomes\":" << r.outcomes
      << ",\"seconds\":" << r.seconds << ",\"players\":[";
    const double N = static_cast<double>(r.outcomes);
    for (size_t i = 0; i < r.players.size(); ++i) {
        const auto& ps = r.players[i];
        if (i) o << ",";
        o << "{\"hole\":[\"" << card_to_string(players[i].a) << "\",\""
          << card_to_string(players[i].b) << "\"],"
          << "\"win\":" << ps.win << ",\"tie\":" << ps.tie
          << ",\"equity\":" << ps.equity << ",\"equity_ci\":" << ps.equity_ci
          << ",\"categories\":{";
        for (int c = 0; c < NUM_CATEGORIES; ++c) {
            if (c) o << ",";
            o << "\"" << SHORT_CAT[c] << "\":" << ps.category_counts[c] / N;
        }
        o << "}";  // close categories

        // Player's current made hand, once at least 5 cards are known (post-flop).
        std::vector<Card> known = {players[i].a, players[i].b};
        known.insert(known.end(), board.begin(), board.end());
        if (known.size() >= 5) {
            HandValue hv = evaluate_best(known);
            o << ",\"current\":{\"category\":\"" << SHORT_CAT[static_cast<int>(hv.category)]
              << "\",\"label\":\"" << made_hand_label(hv) << "\"}";
        }
        o << "}";  // close player object
    }
    o << "]}";
}

} // namespace

std::string results_to_json(const std::vector<PlayerHole>& players,
                            const std::vector<Card>& board,
                            const Result& enumeration,
                            const Result* monte_carlo) {
    std::ostringstream o;
    o << std::fixed << std::setprecision(6);
    o << "{\"players\":" << players.size()
      << ",\"street\":\"" << street_name(board.size()) << "\",\"board\":[";
    for (size_t i = 0; i < board.size(); ++i) {
        if (i) o << ",";
        o << "\"" << card_to_string(board[i]) << "\"";
    }
    o << "],\"results\":[";
    emit_result(o, players, board, enumeration, "enumeration");
    if (monte_carlo) { o << ","; emit_result(o, players, board, *monte_carlo, "monte_carlo"); }
    o << "]}";
    return o.str();
}

} // namespace poker
