#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "card.h"
#include "hand_evaluator.h"
#include "showdown.h"

namespace poker {

enum class Method { Enumeration, MonteCarlo };

struct PlayerStats {
    double win = 0.0;        // P(sole winner)
    double tie = 0.0;        // P(shares the best hand in a split)
    double equity = 0.0;     // mean pot share (win + fair split share)
    double equity_ci = 0.0;  // 95% CI half-width for equity (Monte Carlo; 0 if exact)
    // Outcomes ending in each category, indexed by Category's integer value.
    std::array<uint64_t, NUM_CATEGORIES> category_counts{};
};

struct Result {
    Method method = Method::Enumeration;
    std::vector<PlayerStats> players;
    uint64_t outcomes = 0;  // boards enumerated (exact) or trials run (Monte Carlo)
    double seconds = 0.0;   // wall-clock compute time
};

// Exact: enumerate every completion of the board from the remaining deck.
Result enumerate(const std::vector<PlayerHole>& players,
                 const std::vector<Card>& known_board);

// Estimate: `trials` random completions of the board, RNG seeded with `seed`.
Result monte_carlo(const std::vector<PlayerHole>& players,
                   const std::vector<Card>& known_board,
                   uint64_t trials, uint64_t seed);

} // namespace poker
