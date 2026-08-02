#pragma once
#include <array>
#include <random>
#include <vector>
#include "card.h"

// Deck utilities: build the full 52-card deck, compute the cards still available
// once some are known ("dead"), and draw random cards for Monte Carlo.

namespace poker {

// The cards not present in `used`, in ascending order. `used` holds every known
// card (all hole cards plus any board cards already dealt).
inline std::vector<Card> remaining_deck(const std::vector<Card>& used) {
    bool taken[NUM_CARDS] = {false};
    for (Card c : used) taken[c] = true;
    std::vector<Card> out;
    out.reserve(NUM_CARDS);
    for (int c = 0; c < NUM_CARDS; ++c)
        if (!taken[c]) out.push_back(c);
    return out;
}

// Draw k distinct cards from `pool` into out[0..k) via a partial Fisher-Yates
// shuffle. `pool` is permuted in place but keeps the same contents, so the same
// pool vector can be reused across many Monte Carlo trials.
template <class RNG>
inline void sample_without_replacement(std::vector<Card>& pool, int k, Card* out, RNG& rng) {
    const int n = static_cast<int>(pool.size());
    for (int i = 0; i < k; ++i) {
        std::uniform_int_distribution<int> dist(i, n - 1);
        int j = dist(rng);
        std::swap(pool[i], pool[j]);
        out[i] = pool[i];
    }
}

} // namespace poker
