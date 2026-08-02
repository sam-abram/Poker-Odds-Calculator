// Self-tests for the rules-based hand evaluator. Compile and run standalone:
//   cl /std:c++17 /EHsc /nologo test_evaluator.cpp hand_evaluator.cpp
// Exits non-zero if any check fails.
#include <array>
#include <iostream>
#include <string>
#include "card.h"
#include "hand_evaluator.h"

using namespace poker;

static int g_failures = 0;
static int g_checks = 0;

static void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cout << "  FAIL: " << what << "\n";
    }
}

// Build a 7-card hand from card strings.
static std::array<Card, 7> H(const char* a, const char* b, const char* c,
                             const char* d, const char* e, const char* f, const char* g) {
    return {parse_card(a), parse_card(b), parse_card(c), parse_card(d),
            parse_card(e), parse_card(f), parse_card(g)};
}

static HandValue eval(const std::array<Card, 7>& h) { return evaluate7(h); }

int main() {
    // --- Category detection --------------------------------------------------
    check(eval(H("As","Ks","Qs","Js","Ts","2h","3d")).category == Category::StraightFlush,
          "royal flush is a straight flush");
    check(eval(H("9c","8c","7c","6c","5c","Ah","Kd")).category == Category::StraightFlush,
          "9-high straight flush");
    check(eval(H("As","Ah","Ad","Ac","Kd","Qs","2h")).category == Category::Quads,
          "four of a kind");
    check(eval(H("As","Ah","Ad","Ks","Kh","Qd","2c")).category == Category::FullHouse,
          "full house (trips + pair)");
    check(eval(H("As","Ah","Ad","Ks","Kh","Kd","2c")).category == Category::FullHouse,
          "full house from two trips");
    check(eval(H("As","Ks","9s","6s","3s","2h","2d")).category == Category::Flush,
          "flush");
    check(eval(H("9c","8d","7h","6s","5c","Ah","Kd")).category == Category::Straight,
          "straight");
    check(eval(H("Ac","2d","3h","4s","5c","Kh","Qd")).category == Category::Straight,
          "wheel A-2-3-4-5 is a straight");
    check(eval(H("As","Ah","Ad","Ks","Qh","9d","2c")).category == Category::Trips,
          "three of a kind");
    check(eval(H("As","Ah","Ks","Kh","Qd","9c","2d")).category == Category::TwoPair,
          "two pair");
    check(eval(H("As","Ah","Ks","Qh","9d","7c","2d")).category == Category::Pair,
          "one pair");
    check(eval(H("As","Ks","Qh","9d","7c","5s","2d")).category == Category::HighCard,
          "high card");

    // --- Ordering between categories ----------------------------------------
    auto sf = eval(H("9c","8c","7c","6c","5c","Ah","Kd"));
    auto fl = eval(H("As","Ks","9s","6s","3s","2h","4d"));
    auto st = eval(H("9c","8d","7h","6s","5c","Ah","Kd"));
    auto fh = eval(H("As","Ah","Ad","Ks","Kh","Qd","2c"));
    auto tr = eval(H("As","Ah","Ad","Ks","Qh","9d","2c"));
    check(sf.score > fh.score, "straight flush beats full house");
    check(fh.score > fl.score, "full house beats flush");
    check(fl.score > st.score, "flush beats straight");
    check(st.score > tr.score, "straight beats three of a kind");

    // A hand that is simultaneously a straight and a full house must score as a
    // full house (the higher category).
    check(eval(H("5s","5h","5d","6c","6h","7s","8d")).category == Category::FullHouse,
          "full house preferred over embedded straight cards");
    // A hand that is simultaneously a straight and trips must score as a straight.
    check(eval(H("5s","5h","5d","6c","7h","8s","9d")).category == Category::Straight,
          "straight preferred over three of a kind");

    // --- Tiebreakers ---------------------------------------------------------
    // Pair of aces, king kicker beats pair of aces, queen kicker.
    check(eval(H("As","Ah","Ks","8d","5c","3h","2d")).score >
          eval(H("Ac","Ad","Qs","8h","5s","3c","2h")).score,
          "kicker breaks a tie between equal pairs");
    // Higher full house wins (aces full > kings full).
    check(eval(H("As","Ah","Ad","Ks","Kh","2d","3c")).score >
          eval(H("Ks","Kh","Kd","As","Ah","2c","3d")).score,
          "aces full beats kings full");
    // Ace-high flush beats king-high flush.
    check(eval(H("As","Ks","9s","6s","3s","2h","4d")).score >
          eval(H("Ks","Qs","9s","6s","3s","2h","4d")).score,
          "higher flush wins");

    // --- Exact ties ----------------------------------------------------------
    // Both players make the identical straight from a shared board; scores equal.
    auto boardStraightA = eval(H("Ah","Kh","Ts","9h","8c","7d","6s")); // uses T9876
    auto boardStraightB = eval(H("As","Kd","Ts","9h","8c","7d","6s")); // uses T9876
    check(boardStraightA.score == boardStraightB.score,
          "identical best five-card hands tie exactly");
    // Suits do not affect score: same ranks, different suits => equal.
    check(eval(H("As","Ks","Qs","Js","9s","5h","2d")).score ==
          eval(H("Ah","Kh","Qh","Jh","9h","5c","2s")).score,
          "suits do not change hand strength");

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures) {
        std::cout << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "All evaluator self-tests passed.\n";
    return 0;
}
