#include "solitaire.h"

#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/variant.hpp>
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

std::vector<int> Solitaire::flip_stock_to_waste(const GameRuleset &ruleset) {
    const int offset = ruleset.stock_to_waste_offset;
    if (offset <= 0) {
        return {};
    }
    auto &stock = get_stock();
    if (stock.cards.empty()) {
        // TODO
        return {};
    }

    std::vector<int> flipped_card_ids;

    int cards_remaining_to_flip = std::min(offset, static_cast<int>(stock.cards.size()));
    auto action = SolitaireAction{};
    while (cards_remaining_to_flip > 0) {
        auto action_unit =
            CardStackActionUnit{.source_pile_index = stock_, .dest_pile_index = waste_, .card_count = 1, .flips = true};
        action.units.push_back(action_unit);
        flipped_card_ids.push_back(stock.cards[stock.cards.size() - cards_remaining_to_flip].id());
        cards_remaining_to_flip--;
    }
    do_action(action);

    return flipped_card_ids;
}

void Solitaire::do_action(const SolitaireAction &action) {
    for (const auto &unit : action.units) {
        auto &source_pile = piles_[unit.source_pile_index];
        auto &dest_pile = piles_[unit.dest_pile_index];

        if (unit.card_count > 0 && unit.card_count <= static_cast<int>(source_pile.cards.size())) {
            dest_pile.cards.insert(dest_pile.cards.end(),
                                   std::make_move_iterator(source_pile.cards.end() - unit.card_count),
                                   std::make_move_iterator(source_pile.cards.end()));
            source_pile.cards.erase(source_pile.cards.end() - unit.card_count, source_pile.cards.end());
            if (unit.flips && !dest_pile.cards.empty()) {
                for (int i = 0; i < unit.card_count; ++i) {
                    dest_pile.cards[dest_pile.cards.size() - 1 - i].is_face_up =
                        !dest_pile.cards[dest_pile.cards.size() - 1 - i].is_face_up;
                }
            }
        }
    }
    action_history.push_back(action);
}

bool Solitaire::undo_last_action() {
    if (action_history.empty()) {
        return false;
    }

    const auto &last_action = action_history.back();
    for (auto it = last_action.units.rbegin(); it != last_action.units.rend(); ++it) {
        const auto &unit = *it;
        auto &source_pile = piles_[unit.source_pile_index];
        auto &dest_pile = piles_[unit.dest_pile_index];

        if (unit.card_count > 0 && unit.card_count <= static_cast<int>(dest_pile.cards.size())) {
            source_pile.cards.insert(source_pile.cards.end(),
                                     std::make_move_iterator(dest_pile.cards.end() - unit.card_count),
                                     std::make_move_iterator(dest_pile.cards.end()));
            dest_pile.cards.erase(dest_pile.cards.end() - unit.card_count, dest_pile.cards.end());
            if (unit.flips && !source_pile.cards.empty()) {
                for (int i = 0; i < unit.card_count; ++i) {
                    source_pile.cards[source_pile.cards.size() - 1 - i].is_face_up =
                        !source_pile.cards[source_pile.cards.size() - 1 - i].is_face_up;
                }
            }
        }
    }
    action_history.pop_back();
    return true;
}

void Solitaire::reset(const GameRuleset &ruleset, std::optional<int> seed) {
    (void)ruleset;
    action_history.clear();
    lifted_cards = {};
    piles_.clear();
    tableaus_.clear();
    foundations_.clear();

    std::vector<Card> deck = create_shuffled_deck(seed);

    // Initialize the piles based on the ruleset
    for (int i = 0; i < 7; ++i) {
        Pile tableau;
        for (int j = 0; j <= i; ++j) {
            tableau.cards.push_back(deck.back());
            deck.pop_back();
            if (j < i) {
                tableau.cards.back().is_face_up = false; // Face-down for all but the last card
            }
        }
        piles_.push_back(tableau);
        tableaus_.push_back(static_cast<int>(piles_.size()) - 1);
    }

    for (int i = 0; i < 4; ++i) {
        Pile foundation;
        piles_.push_back(foundation);
        foundations_.push_back(static_cast<int>(piles_.size()) - 1);
    }

    Pile stock;
    while (!deck.empty()) {
        deck.back().is_face_up = false; // All stock cards are face-down
        stock.cards.push_back(deck.back());
        deck.pop_back();
    }
    piles_.push_back(stock);
    stock_ = static_cast<int>(piles_.size()) - 1;

    Pile waste;
    piles_.push_back(waste);
    waste_ = static_cast<int>(piles_.size()) - 1;
}

const Pile &Solitaire::get_foundation(int i) const { return piles_[foundations_[i]]; }

const Pile &Solitaire::get_tableau(int i) const { return piles_[tableaus_[i]]; }

const Pile &Solitaire::get_stock() const { return piles_[stock_]; }

const Pile &Solitaire::get_waste() const { return piles_[waste_]; }

Pile &Solitaire::get_foundation(int i) { return piles_[foundations_[i]]; }

Pile &Solitaire::get_tableau(int i) { return piles_[tableaus_[i]]; }

Pile &Solitaire::get_stock() { return piles_[stock_]; }

Pile &Solitaire::get_waste() { return piles_[waste_]; }

bool Solitaire::is_winning() const {
    for (int i = 0; i < 4; ++i) {
        const Pile &foundation = get_foundation(i);
        if (foundation.cards.size() != 13) {
            return false;
        }
    }
    return true;
}

bool save_context(PersistentContext &context) {
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

bool load_context(PersistentContext &context) {
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

std::vector<Card> create_shuffled_deck(std::optional<int> seed_opt) {
    std::vector<Card> deck;
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            deck.push_back({rank, suit});
        }
    }

    int seed;
    if (seed_opt) {
        seed = seed_opt.value();
    } else {
        std::random_device rd;
        seed = rd();
    }

    std::mt19937 gen(seed);
    std::shuffle(deck.begin(), deck.end(), gen);
    return deck;
}
