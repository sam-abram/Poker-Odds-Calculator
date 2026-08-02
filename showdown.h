#pragma once
#include <array>
#include "card.h"
#include "hand_evaluator.h"

namespace poker {

constexpr int MIN_PLAYERS = 2;
constexpr int MAX_PLAYERS = 9;

// A player's two known hole cards.
struct PlayerHole {
    Card a;
    Card b;
};

// The outcome of one complete showdown (all five board cards known).
struct Showdown {
    int n;                              // number of players
    uint32_t score[MAX_PLAYERS];        // each player's best-hand score
    Category category[MAX_PLAYERS];     // each player's made-hand category
    uint32_t best;                      // winning score
    int winners[MAX_PLAYERS];           // indices sharing the best score
    int num_winners;                    // 1 => sole winner, >1 => split pot
};

// Evaluate every player's 7 cards (2 hole + the shared 5-card board) and fill
// `out`. `out` is caller-owned and reused across outcomes to avoid allocation in
// the hot enumeration/simulation loops.
inline void run_showdown(const PlayerHole* holes, int n,
                         const std::array<Card, 5>& board, Showdown& out) {
    out.n = n;
    uint32_t best = 0;
    for (int i = 0; i < n; ++i) {
        std::array<Card, 7> seven = {holes[i].a, holes[i].b,
                                     board[0], board[1], board[2], board[3], board[4]};
        HandValue hv = evaluate7(seven);
        out.score[i] = hv.score;
        out.category[i] = hv.category;
        if (hv.score > best) best = hv.score;
    }
    out.best = best;

    int nw = 0;
    for (int i = 0; i < n; ++i)
        if (out.score[i] == best) out.winners[nw++] = i;
    out.num_winners = nw;
}

} // namespace poker
