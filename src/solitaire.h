#pragma once

#include <cassert>
#include <optional>
#include <variant>
#include <vector>

// Standard boilerplate to combine multiple lambdas into one visitor
template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};

// Deduction guide (needed in C++17, automatic in C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct Solitaire;

struct Card {
    int rank; // 1-13 for Ace to King
    int suit; // 0-3. 0 = Hearts, 1 = Clubs, 2 = Diamonds, 3 = Spades
    bool is_face_up = true;

    int id() const {
        return suit * 13 + (rank - 1); // Unique ID for each card, 0-51
    }

    template <typename T> void serialize(T &archive) { archive(rank, suit, is_face_up); }
};

enum PileKind { Stock, Waste, Tableau, Foundation };

struct Pile {
    std::vector<Card> cards;

    PileKind kind;
    int index{-1};

    template <typename T> void serialize(T &archive) { archive(cards, kind, index); }
};

struct LiftedCards {
    std::vector<Card> cards;
    int source_pile_index = -1; // index of the pile from which the cards were lifted

    float x_offset = 0.f; // x offset for dragging
    float y_offset = 0.f; // y offset for dragging

    template <typename T> void serialize(T &archive) { archive(cards, source_pile_index, x_offset, y_offset); }
};

struct GameRuleset {
    int stock_to_waste_offset = 1;               // offset for flipping stock to waste
    bool allow_waste_to_recycle = true;          // allow recycling waste back to stock
    bool allow_any_card_to_empty_tableau = true; // allow any card to be placed on empty tableau piles
    bool allow_undo = true;                      // allow undoing moves
    int undo_plies = 0;                          // number of undos allowed, 0 for unlimited
    bool auto_play_to_foundation = true;         // automatically play cards to foundation when possible

    template <typename T> void serialize(T &archive) {
        archive(stock_to_waste_offset, allow_waste_to_recycle, allow_any_card_to_empty_tableau, allow_undo, undo_plies,
                auto_play_to_foundation);
    }
};

struct CardStackActionUnit {
    int source_pile_index;
    int dest_pile_index;
    int card_count;

    bool flips{false};

    template <typename T> void serialize(T &archive) { archive(source_pile_index, dest_pile_index, card_count); }
};

struct SolitaireAction {
    std::vector<CardStackActionUnit> units;

    void backwards(Solitaire &game) const;

    void forwards(Solitaire &game) const;

    template <typename T> void serialize(T &archive) { archive(units); }
};

struct Solitaire {
    std::optional<LiftedCards> lifted_cards;

    std::vector<SolitaireAction> action_history; // for undo functionality

    std::vector<Pile> piles_;
    std::vector<int> tableaus_;
    std::vector<int> foundations_;
    int stock_;
    int waste_;

    void reset(const GameRuleset &ruleset, std::optional<int> seed = std::nullopt);
    bool is_winning() const;

    std::vector<int> flip_stock_to_waste(const GameRuleset &ruleset);

    void do_action(const SolitaireAction &action);
    std::optional<SolitaireAction> undo_last_action();

    const Pile &get_foundation(int i) const;
    const Pile &get_tableau(int i) const;
    const Pile &get_stock() const;
    const Pile &get_waste() const;

    Pile &get_foundation(int i);
    Pile &get_tableau(int i);
    Pile &get_stock();
    Pile &get_waste();

    const Pile &get_pile(int i) const;
    Pile &get_pile(int i);


    // need a mapping pile id -> screen location for drawing cards and for hit detection

    template <typename T> void serialize(T &archive) {
        archive(lifted_cards, action_history, piles_, tableaus_, foundations_, stock_, waste_);
    }
};

struct PersistentContext {
    struct Window {
        int width{1280};          // window width
        int height{720};          // window height
        bool is_maximized{false}; // whether the window is maximized

        template <typename T> void serialize(T &archive) { archive(width, height, is_maximized); }
    };

    std::optional<Solitaire> game;
    bool show_main_menu = false;
    int wins = 0; // number of wins
    Window window;
    GameRuleset ruleset;

    template <typename T> void serialize(T &archive) { archive(game, wins, window, ruleset); }
};

bool save_context(PersistentContext &context);

bool load_context(PersistentContext &context);

std::vector<Card> create_shuffled_deck(std::optional<int> seed = std::nullopt);
