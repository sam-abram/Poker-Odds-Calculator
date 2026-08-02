#pragma once
#include <string>
#include <vector>
#include "card.h"
#include "hand_evaluator.h"
#include "showdown.h"
#include "equity.h"

namespace poker {

// Short category labels indexed by Category's integer value (high card .. straight
// flush). Shared by the console output, the CLI JSON, and the HTTP server.
inline constexpr const char* SHORT_CAT[NUM_CATEGORIES] = {
    "high", "pair", "2pair", "trips", "straight", "flush", "full", "quads", "sflush"};

inline const char* street_name(size_t board_size) {
    switch (board_size) {
        case 0: return "preflop";
        case 3: return "flop";
        case 4: return "turn";
        case 5: return "river";
        default: return "board";
    }
}

// Serialize results to the JSON schema shared by the CLI (`--json`) and the HTTP
// server. `monte_carlo` may be null (post-flop without --mc). No trailing newline.
std::string results_to_json(const std::vector<PlayerHole>& players,
                            const std::vector<Card>& board,
                            const Result& enumeration,
                            const Result* monte_carlo);

} // namespace poker
