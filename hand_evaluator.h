#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "card.h"

namespace poker {

// Poker hand categories, ordered weakest (0) to strongest (8). The integer
// values are used directly as the high bits of a hand's packed score, so their
// ordering must match real poker rankings.
enum class Category : int {
    HighCard = 0,
    Pair = 1,
    TwoPair = 2,
    Trips = 3,
    Straight = 4,
    Flush = 5,
    FullHouse = 6,
    Quads = 7,
    StraightFlush = 8,
};
constexpr int NUM_CATEGORIES = 9;

// The strength of a 5-card hand chosen from 7 cards.
//   - score: higher is a better hand; equal scores are an exact tie (split).
//   - category: the hand class, for reporting hand-type frequencies.
struct HandValue {
    uint32_t score;
    Category category;
};

// Evaluate the best 5-card poker hand contained in the given 7 cards.
HandValue evaluate7(const std::array<Card, 7>& cards);

// General form: best 5-card hand among n cards (n >= 5). Used to report a
// player's current made hand from fewer than 7 known cards (flop/turn).
HandValue evaluate_n(const Card* cards, int n);
HandValue evaluate_best(const std::vector<Card>& cards);

// Human-readable category name (e.g. "two pair", "straight flush").
const char* category_name(Category c);

} // namespace poker
