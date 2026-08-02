// Independent cross-check of the engine's equities. Uses a DIFFERENT evaluator
// (sort-based 5-card ranking, best of all C(7,5)=21 subsets) so a matching
// result validates the fast bitmask evaluator + enumerator. Depends only on
// card.h / deck.h, never on hand_evaluator.cpp / equity.cpp.
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include "card.h"
#include "deck.h"

using namespace poker;

// Rank a 5-card hand; higher is better. Independent of the project evaluator.
static long long eval5(int a, int b, int c, int d, int e) {
    int rk[5] = {rank_of(a), rank_of(b), rank_of(c), rank_of(d), rank_of(e)};
    int su[5] = {suit_of(a), suit_of(b), suit_of(c), suit_of(d), suit_of(e)};
    std::sort(rk, rk + 5, std::greater<int>());

    bool flush = (su[0] == su[1] && su[1] == su[2] && su[2] == su[3] && su[3] == su[4]);

    bool straight = false;
    int high = -1;
    if (rk[0] - 1 == rk[1] && rk[1] - 1 == rk[2] && rk[2] - 1 == rk[3] && rk[3] - 1 == rk[4]) {
        straight = true;
        high = rk[0];
    }
    if (rk[0] == 12 && rk[1] == 3 && rk[2] == 2 && rk[3] == 1 && rk[4] == 0) {
        straight = true;  // wheel A-2-3-4-5
        high = 3;
    }

    int cnt[13] = {0};
    for (int i = 0; i < 5; ++i) ++cnt[rk[i]];
    // Groups (count, rank) sorted by count desc then rank desc.
    std::vector<std::pair<int, int>> g;
    for (int r = 12; r >= 0; --r)
        if (cnt[r]) g.push_back({cnt[r], r});
    std::stable_sort(g.begin(), g.end(), [](auto& x, auto& y) {
        if (x.first != y.first) return x.first > y.first;
        return x.second > y.second;
    });

    int cat;
    if (straight && flush) cat = 8;
    else if (g[0].first == 4) cat = 7;
    else if (g[0].first == 3 && g.size() > 1 && g[1].first == 2) cat = 6;
    else if (flush) cat = 5;
    else if (straight) cat = 4;
    else if (g[0].first == 3) cat = 3;
    else if (g[0].first == 2 && g.size() > 1 && g[1].first == 2) cat = 2;
    else if (g[0].first == 2) cat = 1;
    else cat = 0;

    std::vector<int> tb;
    if (cat == 8 || cat == 4) tb = {high};
    else for (auto& pr : g) tb.push_back(pr.second);
    while (tb.size() < 5) tb.push_back(0);

    long long v = cat;
    for (int i = 0; i < 5; ++i) v = v * 16 + tb[i];
    return v;
}

static long long eval7(const std::array<int, 7>& h) {
    long long best = -1;
    for (int i = 0; i < 7; ++i)
        for (int j = i + 1; j < 7; ++j)
            for (int k = j + 1; k < 7; ++k)
                for (int l = k + 1; l < 7; ++l)
                    for (int m = l + 1; m < 7; ++m)
                        best = std::max(best, eval5(h[i], h[j], h[k], h[l], h[m]));
    return best;
}

static void equity_of(const std::vector<std::array<Card, 2>>& holes,
                      const std::vector<Card>& known_board) {
    int n = (int)holes.size();
    int b = (int)known_board.size();
    int k = 5 - b;

    std::vector<Card> used;
    for (auto& h : holes) { used.push_back(h[0]); used.push_back(h[1]); }
    for (Card c : known_board) used.push_back(c);
    std::vector<Card> pool = remaining_deck(used);
    int mm = (int)pool.size();

    std::vector<double> eq(n, 0.0), win(n, 0.0), tie(n, 0.0);
    uint64_t outcomes = 0;

    std::array<Card, 5> board{};
    for (int i = 0; i < b; ++i) board[i] = known_board[i];

    std::vector<int> idx(k);
    for (int i = 0; i < k; ++i) idx[i] = i;
    while (true) {
        for (int i = 0; i < k; ++i) board[b + i] = pool[idx[i]];
        long long best = -1;
        std::vector<long long> sc(n);
        for (int p = 0; p < n; ++p) {
            std::array<int, 7> seven = {holes[p][0], holes[p][1], board[0],
                                        board[1], board[2], board[3], board[4]};
            sc[p] = eval7(seven);
            best = std::max(best, sc[p]);
        }
        int nw = 0;
        for (int p = 0; p < n; ++p) if (sc[p] == best) ++nw;
        double share = 1.0 / nw;
        for (int p = 0; p < n; ++p) if (sc[p] == best) {
            eq[p] += share;
            if (nw == 1) win[p] += 1; else tie[p] += 1;
        }
        ++outcomes;

        int pos = k - 1;
        while (pos >= 0 && idx[pos] == mm - k + pos) --pos;
        if (pos < 0) break;
        ++idx[pos];
        for (int i = pos + 1; i < k; ++i) idx[i] = idx[i - 1] + 1;
    }

    std::cout << "  outcomes: " << outcomes << "\n";
    for (int p = 0; p < n; ++p)
        std::cout << "  P" << (p + 1) << "  win " << win[p] / outcomes * 100
                  << "%  tie " << tie[p] / outcomes * 100
                  << "%  equity " << eq[p] / outcomes * 100 << "%\n";
}

static std::array<Card, 2> hole(const char* s) {
    return {parse_card(std::string(s).substr(0, 2)), parse_card(std::string(s).substr(2, 2))};
}

int main() {
    std::cout.setf(std::ios::fixed);
    std::cout.precision(4);

    std::cout << "AA vs KK (preflop):\n";
    equity_of({hole("AhAs"), hole("KdKc")}, {});

    std::cout << "AK vs QQ on 2c 7d 9h (flop):\n";
    equity_of({hole("AhKh"), hole("QsQd")}, {parse_card("2c"), parse_card("7d"), parse_card("9h")});
    return 0;
}
