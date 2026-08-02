#include "hand_evaluator.h"
#include <algorithm>

namespace poker {

namespace {

// Pack a category plus up to five rank "tiebreakers" (most significant first)
// into a single comparable score. Each rank uses 4 bits (ranks are 0..12), the
// category uses the top bits. Within a category the tiebreaker layout is fixed,
// so plain integer comparison of two scores yields the correct poker ordering.
inline uint32_t pack(Category cat, int t0 = 0, int t1 = 0, int t2 = 0, int t3 = 0, int t4 = 0) {
    return (static_cast<uint32_t>(cat) << 20) |
           (static_cast<uint32_t>(t0) << 16) |
           (static_cast<uint32_t>(t1) << 12) |
           (static_cast<uint32_t>(t2) << 8) |
           (static_cast<uint32_t>(t3) << 4) |
           (static_cast<uint32_t>(t4));
}

// Given a 13-bit mask of present ranks, return the high card rank (0..12) of the
// best straight, or -1 if there is none. The wheel A-2-3-4-5 is a 5-high
// straight, reported as rank index 3 (the '5').
inline int straight_high(uint32_t mask) {
    for (int high = 12; high >= 4; --high) {
        uint32_t need = 0;
        for (int r = high; r > high - 5; --r) need |= (1u << r);
        if ((mask & need) == need) return high;
    }
    const uint32_t wheel = (1u << 12) | (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
    if ((mask & wheel) == wheel) return 3;
    return -1;
}

// Collect up to maxn highest set ranks (highest first) from a 13-bit mask.
inline void top_ranks(uint32_t mask, int out[], int maxn) {
    int n = 0;
    for (int r = 12; r >= 0 && n < maxn; --r)
        if (mask & (1u << r)) out[n++] = r;
    while (n < maxn) out[n++] = 0;
}

} // namespace

HandValue evaluate_n(const Card* cards, int n) {
    int rank_count[NUM_RANKS] = {0};
    int suit_count[NUM_SUITS] = {0};
    uint32_t suit_mask[NUM_SUITS] = {0, 0, 0, 0};
    uint32_t rank_mask = 0;

    for (int i = 0; i < n; ++i) {
        Card c = cards[i];
        int r = rank_of(c), s = suit_of(c);
        ++rank_count[r];
        ++suit_count[s];
        suit_mask[s] |= (1u << r);
        rank_mask |= (1u << r);
    }

    // At most one suit can hold 5+ of seven cards.
    int flush_suit = -1;
    for (int s = 0; s < NUM_SUITS; ++s)
        if (suit_count[s] >= 5) { flush_suit = s; break; }

    // Straight flush (includes royal flush) beats everything.
    if (flush_suit >= 0) {
        int sfh = straight_high(suit_mask[flush_suit]);
        if (sfh >= 0)
            return {pack(Category::StraightFlush, sfh), Category::StraightFlush};
    }

    // Scan high -> low so the first of each kind found is the highest.
    int quad = -1, trips = -1, trips2 = -1, pair = -1, pair2 = -1;
    for (int r = NUM_RANKS - 1; r >= 0; --r) {
        switch (rank_count[r]) {
            case 4: if (quad < 0) quad = r; break;
            case 3: if (trips < 0) trips = r; else if (trips2 < 0) trips2 = r; break;
            case 2: if (pair < 0) pair = r; else if (pair2 < 0) pair2 = r; break;
            default: break;
        }
    }

    // Four of a kind + best kicker.
    if (quad >= 0) {
        int kick = 0;
        for (int r = 12; r >= 0; --r)
            if (r != quad && rank_count[r] > 0) { kick = r; break; }
        return {pack(Category::Quads, quad, kick), Category::Quads};
    }

    // Full house: highest trips over the highest remaining pair (a second set of
    // trips can serve as the pair).
    if (trips >= 0) {
        int pair_part = -1;
        if (trips2 >= 0) pair_part = trips2;
        if (pair >= 0 && pair > pair_part) pair_part = pair;
        if (pair_part >= 0)
            return {pack(Category::FullHouse, trips, pair_part), Category::FullHouse};
    }

    // Flush (not a straight flush): top five cards of the flush suit.
    if (flush_suit >= 0) {
        int top[5];
        top_ranks(suit_mask[flush_suit], top, 5);
        return {pack(Category::Flush, top[0], top[1], top[2], top[3], top[4]), Category::Flush};
    }

    // Straight of mixed suits.
    int sh = straight_high(rank_mask);
    if (sh >= 0)
        return {pack(Category::Straight, sh), Category::Straight};

    // Three of a kind + two kickers.
    if (trips >= 0) {
        int k[2] = {0, 0}, n = 0;
        for (int r = 12; r >= 0 && n < 2; --r)
            if (r != trips && rank_count[r] > 0) k[n++] = r;
        return {pack(Category::Trips, trips, k[0], k[1]), Category::Trips};
    }

    // Two pair + best kicker (a third pair, if any, is just the kicker source).
    if (pair >= 0 && pair2 >= 0) {
        int kick = 0;
        for (int r = 12; r >= 0; --r)
            if (r != pair && r != pair2 && rank_count[r] > 0) { kick = r; break; }
        return {pack(Category::TwoPair, pair, pair2, kick), Category::TwoPair};
    }

    // One pair + three kickers.
    if (pair >= 0) {
        int k[3] = {0, 0, 0}, n = 0;
        for (int r = 12; r >= 0 && n < 3; --r)
            if (r != pair && rank_count[r] > 0) k[n++] = r;
        return {pack(Category::Pair, pair, k[0], k[1], k[2]), Category::Pair};
    }

    // High card: top five ranks.
    int top[5];
    top_ranks(rank_mask, top, 5);
    return {pack(Category::HighCard, top[0], top[1], top[2], top[3], top[4]), Category::HighCard};
}

HandValue evaluate7(const std::array<Card, 7>& cards) {
    return evaluate_n(cards.data(), 7);
}

HandValue evaluate_best(const std::vector<Card>& cards) {
    return evaluate_n(cards.data(), static_cast<int>(cards.size()));
}

const char* category_name(Category c) {
    switch (c) {
        case Category::HighCard:      return "high card";
        case Category::Pair:          return "pair";
        case Category::TwoPair:       return "two pair";
        case Category::Trips:         return "three of a kind";
        case Category::Straight:      return "straight";
        case Category::Flush:         return "flush";
        case Category::FullHouse:     return "full house";
        case Category::Quads:         return "four of a kind";
        case Category::StraightFlush: return "straight flush";
    }
    return "unknown";
}

} // namespace poker
