#pragma once
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>

// A playing card is an integer in [0, 52).
//   rank = card >> 2   (0..12 for 2,3,...,T,J,Q,K,A)
//   suit = card & 3    (0..3 for s,h,d,c)
// Higher rank index = higher card. Suits are equal in value (only used for
// flush detection and display).

namespace poker {

using Card = int;

constexpr int NUM_CARDS = 52;
constexpr int NUM_RANKS = 13;
constexpr int NUM_SUITS = 4;

// Low -> high. Index 0 == '2', index 12 == 'A'.
constexpr char RANK_CHARS[NUM_RANKS + 1] = "23456789TJQKA";
constexpr char SUIT_CHARS[NUM_SUITS + 1] = "shdc";

inline int rank_of(Card c) { return c >> 2; }
inline int suit_of(Card c) { return c & 3; }
inline Card make_card(int rank, int suit) { return (rank << 2) | suit; }

inline int rank_from_char(char ch) {
    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    for (int r = 0; r < NUM_RANKS; ++r)
        if (RANK_CHARS[r] == ch) return r;
    return -1;
}

inline int suit_from_char(char ch) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (int s = 0; s < NUM_SUITS; ++s)
        if (SUIT_CHARS[s] == ch) return s;
    return -1;
}

// Parse a two-character card like "As", "Td", "9c". Throws on malformed input.
inline Card parse_card(const std::string& str) {
    if (str.size() != 2)
        throw std::invalid_argument("card must be 2 characters (e.g. As): '" + str + "'");
    int r = rank_from_char(str[0]);
    int s = suit_from_char(str[1]);
    if (r < 0)
        throw std::invalid_argument("invalid rank in card '" + str + "' (use 2-9,T,J,Q,K,A)");
    if (s < 0)
        throw std::invalid_argument("invalid suit in card '" + str + "' (use s,h,d,c)");
    return make_card(r, s);
}

inline std::string card_to_string(Card c) {
    std::string out;
    out += RANK_CHARS[rank_of(c)];
    out += SUIT_CHARS[suit_of(c)];
    return out;
}

} // namespace poker
