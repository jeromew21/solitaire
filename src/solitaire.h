#pragma once

#include <optional>
#include <vector>

struct Card {
    int rank; // 1-13 for Ace to King
    int suit; // 0-3. 0 = Hearts, 1 = Clubs, 2 = Diamonds, 3 = Spades

    template <typename T> void serialize(T &archive) { archive(rank, suit); }
};

struct LiftedCards {
    std::vector<Card> cards;
    bool is_waste = false;   // true if the lifted cards are from the stock to waste flip
    bool is_tableau = false; // true if the lifted cards are from a tableau pile

    int tableau_index = -1; // index of the tableau pile if lifted from tableau, -1 otherwise

    float x_offset = 0.f; // x offset for dragging
    float y_offset = 0.f; // y offset for dragging

    template <typename T> void serialize(T &archive) {
        archive(cards, is_waste, is_tableau, tableau_index, x_offset, y_offset);
    }
};

struct GameRuleset {
    int num_tableau_piles = 7;
    int stock_to_waste_offset = 1;                // offset for flipping stock to waste
    bool allow_waste_to_recycle = true;           // allow recycling waste back to stock
    bool allow_any_card_to_empty_tableau = false; // allow any card to be placed on empty tableau piles
    bool allow_undo = false;                      // allow undoing moves
    int undo_plies = 0;                           // number of undos allowed, 0 for unlimited
    bool auto_play_to_foundation = true;          // automatically play cards to foundation when possible

    template <typename T> void serialize(T &archive) {
        archive(num_tableau_piles, stock_to_waste_offset, allow_waste_to_recycle, allow_any_card_to_empty_tableau,
                allow_undo, undo_plies, auto_play_to_foundation);
    }
};

struct Solitaire {
    std::vector<Card> stock;

    std::vector<Card> waste;

    std::vector<std::vector<Card>> tableau; // default 7 piles
    std::vector<int> tableau_face_down;

    std::vector<std::vector<Card>> foundation; // default 4 piles

    LiftedCards lifted_cards;

    GameRuleset ruleset; // should not be changed

    template <typename T> void serialize(T &archive) {
        archive(stock, waste, tableau, tableau_face_down, foundation, lifted_cards, ruleset);
    }

    bool is_winning() const {
        for (const auto &pile : foundation) {
            if (pile.size() != 13) {
                return false;
            }
        }
        return true;
    }

    bool is_lifting() const { return !lifted_cards.cards.empty(); }

    void flip_stock_to_waste() {
        if (stock.empty() && ruleset.allow_waste_to_recycle) {
            std::reverse(waste.begin(), waste.end());
            stock = std::move(waste);
            waste.clear();
        } else {
            for (int i = 0; i < ruleset.stock_to_waste_offset && !stock.empty(); ++i) {
                Card card = stock.back();
                stock.pop_back();
                waste.push_back(card);
            }
        }
    }

    bool can_drop_into_foundation(const Card &card, int foundation_index) const {
        if (foundation_index < 0 || foundation_index >= (int)foundation.size()) {
            return false;
        }
        const auto &foundation_pile = foundation[foundation_index];
        if (foundation_pile.empty()) {
            return card.rank == 1; // Ace can be placed on empty foundation
        } else {
            const Card &top_card = foundation_pile.back();
            return card.suit == top_card.suit && card.rank == top_card.rank + 1;
        }
    }

    bool can_drop_stack_onto_tableau(const std::vector<Card> &stack, int tableau_index) const {
        if (tableau_index < 0 || tableau_index >= (int)tableau.size()) {
            return false;
        }
        const auto &tableau_pile = tableau[tableau_index];
        if (tableau_pile.empty()) {
            if (ruleset.allow_any_card_to_empty_tableau) {
                return true;
            }
            return stack.front().rank == 13; // King can be placed on empty tableau
        } else {
            const Card &top_card = tableau_pile.back();
            return stack.front().rank == top_card.rank - 1 && stack.front().suit % 2 != top_card.suit % 2;
        }
    }

    bool can_pick_up_stack_from_tableau(const std::vector<Card> &tableau_pile, int start_index) const {
        if (start_index < 0 || start_index >= (int)tableau_pile.size()) {
            return false;
        }
        for (int i = start_index; i < (int)tableau_pile.size() - 1; ++i) {
            const Card &current_card = tableau_pile[i];
            const Card &next_card = tableau_pile[i + 1];
            if (!(current_card.rank == next_card.rank + 1 && current_card.suit % 2 != next_card.suit % 2)) {
                return false;
            }
        }
        return true;
    }
};

struct PersistentContext {
    std::optional<Solitaire> game;
    bool show_main_menu = false;
    int wins = 0;           // number of wins
    int window_width{1280}; // window width
    int window_height{720}; // window height
    bool is_window_maximized{false}; // whether the window is maximized

    GameRuleset ruleset;

    // list of presets and statistics,
    // user settings, etc.

    template <typename T> void serialize(T &archive) { archive(game, wins, ruleset, window_width, window_height, is_window_maximized); }
};

bool save_game(PersistentContext &context);

bool load_game(PersistentContext &context);

std::vector<Card> create_deck(int seed);

std::vector<Card> create_deck();
