#include "solitaire.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/vector.hpp>
#include <physfs.h>
#include <spdlog/spdlog.h>

#include <algorithm> // std::shuffle
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random> // std::random_device, std::mt19937
#include <string>

namespace {
std::string get_save_directory() {
    const char *dir = PHYSFS_getPrefDir("DongkeyGames", "SolitaireHouseRules");

    if (!dir) {
        // auto code = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
        std::exit(EXIT_FAILURE);
    }

    auto path = std::filesystem::path(dir) / "saves";
    std::filesystem::create_directories(path);
    return path.string();
}

std::string get_game_save_file_path() { return (std::filesystem::path{get_save_directory()} / "slot0.save").string(); }
} // namespace

bool save_game(PersistentContext &context) {
    try {
        std::ofstream out(get_game_save_file_path());
        if (!out.is_open()) {
            return false;
        }
        cereal::PortableBinaryOutputArchive archive(out);
        archive(context);
        out.flush();
        return out.good();
    } catch (const std::exception &e) {
        // Log e.what() here.
        return false;
    }
}

bool load_game(PersistentContext &context) {
    try {
        std::ifstream in(get_game_save_file_path());
        if (!in.is_open()) {
            return false;
        }

        cereal::PortableBinaryInputArchive archive(in);
        archive(context);
        if (!in.bad()) {
            return true;
        } else {
            return false;
        }
    } catch (const std::exception &e) {
        return false;
    }
}

std::vector<Card> create_deck(int seed) {
    std::vector<Card> deck;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            deck.push_back({rank, suit});
        }
    }

    // 2. Seed the random number generator
    std::mt19937 gen(seed);

    // 3. Shuffle the vector in place
    std::shuffle(deck.begin(), deck.end(), gen);

    return deck;
}

std::vector<Card> create_deck() {
    std::random_device rd;

    return create_deck(rd());
}
