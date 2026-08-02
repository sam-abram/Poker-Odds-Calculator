#include "equity.h"
#include <chrono>
#include <cmath>
#include "deck.h"

namespace poker {

namespace {

// Gathers per-player tallies across many showdowns, for either compute path.
struct Accumulator {
    int n;
    std::vector<uint64_t> sole_wins;
    std::vector<uint64_t> tie_events;
    std::vector<double> equity_sum;   // sum of pot shares
    std::vector<double> equity_sum2;  // sum of (pot share)^2, for the CI
    std::vector<std::array<uint64_t, NUM_CATEGORIES>> cat;
    uint64_t outcomes = 0;

    explicit Accumulator(int players)
        : n(players), sole_wins(players, 0), tie_events(players, 0),
          equity_sum(players, 0.0), equity_sum2(players, 0.0),
          cat(players) {
        for (auto& a : cat) a.fill(0);
    }

    inline void add(const Showdown& sd) {
        ++outcomes;
        const double share = 1.0 / sd.num_winners;
        for (int w = 0; w < sd.num_winners; ++w) {
            int i = sd.winners[w];
            if (sd.num_winners == 1) ++sole_wins[i];
            else                     ++tie_events[i];
            equity_sum[i] += share;
            equity_sum2[i] += share * share;
        }
        for (int i = 0; i < n; ++i)
            ++cat[i][static_cast<int>(sd.category[i])];
    }

    Result finalize(Method method, double seconds) const {
        Result r;
        r.method = method;
        r.outcomes = outcomes;
        r.seconds = seconds;
        r.players.resize(n);
        const double N = static_cast<double>(outcomes);
        for (int i = 0; i < n; ++i) {
            PlayerStats& ps = r.players[i];
            ps.win = sole_wins[i] / N;
            ps.tie = tie_events[i] / N;
            ps.equity = equity_sum[i] / N;
            if (method == Method::MonteCarlo) {
                double var = equity_sum2[i] / N - ps.equity * ps.equity;
                if (var < 0) var = 0;  // guard tiny negative from rounding
                ps.equity_ci = 1.96 * std::sqrt(var / N);
            }
            ps.category_counts = cat[i];
        }
        return r;
    }
};

std::vector<Card> collect_used(const std::vector<PlayerHole>& players,
                               const std::vector<Card>& known_board) {
    std::vector<Card> used;
    used.reserve(players.size() * 2 + known_board.size());
    for (const auto& p : players) { used.push_back(p.a); used.push_back(p.b); }
    for (Card c : known_board) used.push_back(c);
    return used;
}

} // namespace

Result enumerate(const std::vector<PlayerHole>& players,
                 const std::vector<Card>& known_board) {
    const auto t0 = std::chrono::steady_clock::now();

    const int n = static_cast<int>(players.size());
    const int b = static_cast<int>(known_board.size());
    const int k = 5 - b;  // board cards still to be dealt

    std::vector<Card> pool = remaining_deck(collect_used(players, known_board));
    const int m = static_cast<int>(pool.size());

    Accumulator acc(n);
    Showdown sd;
    std::array<Card, 5> board{};
    for (int i = 0; i < b; ++i) board[i] = known_board[i];

    // Odometer over all C(m, k) index combinations of `pool`. Handles k == 0
    // (river fully known) as a single outcome.
    std::vector<int> idx(k);
    for (int i = 0; i < k; ++i) idx[i] = i;
    while (true) {
        for (int i = 0; i < k; ++i) board[b + i] = pool[idx[i]];
        run_showdown(players.data(), n, board, sd);
        acc.add(sd);

        int pos = k - 1;
        while (pos >= 0 && idx[pos] == m - k + pos) --pos;
        if (pos < 0) break;
        ++idx[pos];
        for (int i = pos + 1; i < k; ++i) idx[i] = idx[i - 1] + 1;
    }

    const auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    return acc.finalize(Method::Enumeration, secs);
}

Result monte_carlo(const std::vector<PlayerHole>& players,
                   const std::vector<Card>& known_board,
                   uint64_t trials, uint64_t seed) {
    const auto t0 = std::chrono::steady_clock::now();

    const int n = static_cast<int>(players.size());
    const int b = static_cast<int>(known_board.size());
    const int k = 5 - b;

    std::vector<Card> pool = remaining_deck(collect_used(players, known_board));
    std::mt19937_64 rng(seed);

    Accumulator acc(n);
    Showdown sd;
    std::array<Card, 5> board{};
    for (int i = 0; i < b; ++i) board[i] = known_board[i];
    Card drawn[5];

    for (uint64_t t = 0; t < trials; ++t) {
        sample_without_replacement(pool, k, drawn, rng);
        for (int i = 0; i < k; ++i) board[b + i] = drawn[i];
        run_showdown(players.data(), n, board, sd);
        acc.add(sd);
    }

    const auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    return acc.finalize(Method::MonteCarlo, secs);
}

} // namespace poker
