#include "deck.h"

#include <algorithm> // std::shuffle
#include <iostream>
#include <random> // std::random_device, std::mt19937

std::vector<Card> create_deck() {
    std::vector<Card> deck;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            deck.push_back({rank, suit});
        }
    }
    std::random_device rd;

    // 2. Seed the random number generator
    std::mt19937 gen(rd());

    // 3. Shuffle the vector in place
    std::shuffle(deck.begin(), deck.end(), gen);

    return deck;
}

