#pragma once

#include <vector>

struct Card {
    int rank; // 1-13 for Ace to King
    int suit; // 0-3. 0 = Hearts, 1 = Clubs, 2 = Diamonds, 3 = Spades
};

struct Solitaire {
    std::vector<Card> stock;

    std::vector<std::vector<Card>> tableau; // default 7 piles

    std::vector<std::vector<Card>> foundation; // default 4 piles
};

std::vector<Card> create_deck();
