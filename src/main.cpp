#include "assets.h"
#include "deck.h"
#include "draw.h"
#include "layout.h"

#include <frameflow/layout.hpp>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>
#include <physfs.h>
#include <raylib.h>
#include <rlImGui.h>

ImFont *load_imgui_font_from_disk(const char *path, float size) {
    const auto bytes = physfs_read_file(path);
    if (bytes.empty())
        return {};

    // ImGui takes ownership of this allocation.
    auto *data = IM_ALLOC(bytes.size());
    if (!data)
        return nullptr;

    std::memcpy(data, bytes.data(), bytes.size());

    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, static_cast<int>(bytes.size()), size);
}

bool point_inside_rect(float px, float py, const frameflow::Rect &rect) {
    return px >= rect.origin.x && px <= rect.origin.x + rect.size.x && py >= rect.origin.y &&
           py <= rect.origin.y + rect.size.y;
}

constexpr float CARD_WIDTH = 11.0f;
constexpr float CARD_HEIGHT = 16.0f;
constexpr float CARD_SCALE = 8.0f;

const float card_stacking_offset = 0.1f;
const float card_tableau_offset = 5.f * CARD_SCALE;

CardDrawParams params_from_bounder(const frameflow::Rect &bounder, bool face_up) {
    return CardDrawParams{.x = bounder.origin.x + bounder.size.x / 2.f,
                          .y = bounder.origin.y + bounder.size.y / 2.f,
                          .width = bounder.size.x,
                          .height = bounder.size.y,
                          .rotation = 0.f,
                          .face_up = face_up};
}

void handle_press(Solitaire &game, const Vector2 &mouse_pos, const std::vector<LayoutNode *> &foundation_bounders,
                  const std::vector<LayoutNode *> &tableau_bounders, LayoutNode *stock_card_bounder_node,
                  LayoutNode *stock_stack_bounder_node) {
    bool clicked_on_stock = point_inside_rect(mouse_pos.x, mouse_pos.y, stock_card_bounder_node->get_bounding_rect());
    bool clicked_on_waste = point_inside_rect(mouse_pos.x, mouse_pos.y, stock_stack_bounder_node->get_bounding_rect());
    if (clicked_on_stock && !game.stock.empty()) {
        game.flip_stock_to_waste();
        return;
    } else if (clicked_on_waste && !game.waste.empty()) {
        const float center_x = stock_stack_bounder_node->get_bounding_rect().origin.x +
                               stock_stack_bounder_node->get_bounding_rect().size.x / 2.f;
        const float center_y = stock_stack_bounder_node->get_bounding_rect().origin.y +
                               stock_stack_bounder_node->get_bounding_rect().size.y / 2.f;
        game.lifted_cards.cards.push_back(game.waste.back());
        game.lifted_cards.is_waste = true;
        game.lifted_cards.x_offset = mouse_pos.x - center_x;
        game.lifted_cards.y_offset = mouse_pos.y - center_y;
        game.waste.pop_back();
        return;
    }

    // Check if the mouse is over any tableau pile
    for (int i = 0; i < (int)game.tableau.size(); ++i) {
        auto rect = tableau_bounders[i]->get_bounding_rect();
        rect.size.y += card_tableau_offset * (game.tableau[i].size() - 1);
        bool mouse_over_tableau = point_inside_rect(mouse_pos.x, mouse_pos.y, rect);
        if (mouse_over_tableau && !game.tableau[i].empty()) {
            // Determine which card in the tableau pile was clicked
            int card_index = (int)((mouse_pos.y - rect.origin.y) / card_tableau_offset);
            if (card_index >= (int)game.tableau[i].size()) {
                card_index = (int)game.tableau[i].size() - 1;
            }
            if (!game.can_pick_up_stack_from_tableau(game.tableau[i], card_index)) {
                break; // Can't pick up this stack, so don't lift any cards
            }
            // Lift the cards from the clicked index to the top of the pile
            auto card_rect = tableau_bounders[i]->get_bounding_rect();
            card_rect.origin.y += card_tableau_offset * card_index;
            const float center_x = card_rect.origin.x + card_rect.size.x / 2.f;
            const float center_y = card_rect.origin.y + card_rect.size.y / 2.f;
            game.lifted_cards.cards.insert(game.lifted_cards.cards.end(), game.tableau[i].begin() + card_index,
                                           game.tableau[i].end());
            game.lifted_cards.is_waste = false;
            game.lifted_cards.is_tableau = true;
            game.lifted_cards.tableau_index = i;
            game.lifted_cards.x_offset = mouse_pos.x - center_x;
            game.lifted_cards.y_offset = mouse_pos.y - center_y;
            game.tableau[i].erase(game.tableau[i].begin() + card_index, game.tableau[i].end());
            return;
        }
    }
}

void handle_drop(Solitaire &game, const Vector2 &mouse_pos, const std::vector<LayoutNode *> &foundation_bounders,
                 const std::vector<LayoutNode *> &tableau_bounders, LayoutNode *stock_card_bounder_node,
                 LayoutNode *stock_stack_bounder_node) {
    if (game.lifted_cards.cards.size() == 1) {
        bool mouse_over_foundation[4];
        for (int i = 0; i < 4; ++i) {
            mouse_over_foundation[i] = point_inside_rect(GetMousePosition().x, GetMousePosition().y,
                                                         foundation_bounders[i]->get_bounding_rect());
        }
        for (int i = 0; i < 4; ++i) {
            if (mouse_over_foundation[i] && game.can_drop_into_foundation(game.lifted_cards.cards.back(), i)) {
                game.foundation[i].push_back(game.lifted_cards.cards.back());
                game.lifted_cards.cards.clear();
                return;
            }
        }
    }

    // Check if the mouse is over any tableau pile
    for (int i = 0; i < (int)game.tableau.size(); ++i) {
        auto rect = tableau_bounders[i]->get_bounding_rect();
        rect.size.y += card_tableau_offset * (game.tableau[i].size() - 1);
        bool mouse_over_tableau = point_inside_rect(mouse_pos.x, mouse_pos.y, rect);
        if (mouse_over_tableau) {
            if (game.can_drop_stack_onto_tableau(game.lifted_cards.cards, i)) {
                game.tableau[i].insert(game.tableau[i].end(), game.lifted_cards.cards.begin(),
                                       game.lifted_cards.cards.end());
                game.lifted_cards.cards.clear();
                return;
            }
        }
    }

    if (game.lifted_cards.is_waste) {
        game.waste.insert(game.waste.end(), game.lifted_cards.cards.begin(), game.lifted_cards.cards.end());
    } else if (game.lifted_cards.is_tableau) {
        game.tableau[game.lifted_cards.tableau_index].insert(game.tableau[game.lifted_cards.tableau_index].end(),
                                                             game.lifted_cards.cards.begin(),
                                                             game.lifted_cards.cards.end());
    }
    game.lifted_cards.cards.clear();
}

int main(int argc, char *argv[]) {
    using namespace frameflow;
    (void)argc;
    physfs_init(argv[0]);

    SetTraceLogLevel(LOG_NONE);
    InitWindow(1280, 720, "Solitaire");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);
    rlImGuiSetup(true);
    auto &io = ImGui::GetIO();
    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    ImFont *imgui_font = load_imgui_font_from_disk("Courier Prime Code.ttf", 16);
    io.FontDefault = imgui_font;

    init_card_texture_cache_v1();

    Solitaire game;
    game.stock = create_deck();
    game.tableau.resize(7);    // 7 piles for tableau
    game.foundation.resize(4); // 4 piles for foundation

    // set up tableau piles
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j <= i; ++j) {
            game.tableau[i].push_back(game.stock.back());
            game.stock.pop_back();
        }
    }

    auto *canvas_layer = new CanvasLayer();
    auto *root_node = new Generic();
    canvas_layer->add_root_node(root_node);
    root_node->bounds(0.f, 0.f, (float)GetScreenWidth(), (float)GetScreenHeight());

    auto *hbox = new HBox();
    root_node->add_child(hbox);
    hbox->anchors_fill()->expand(1.f, 1.f);

    auto *left_vbox = new VBox();
    hbox->add_child(left_vbox);
    left_vbox->anchors_fill()->minimum_size(256.f, 0.f)->expand(0.f, 1.f);

    auto *right_vbox = new VBox();
    hbox->add_child(right_vbox);
    right_vbox->anchors_fill()->expand(1.f, 1.f);

    auto *stock_deck_node = new Center();
    left_vbox->add_child(stock_deck_node);
    stock_deck_node->anchors_fill()->minimum_size(200.f, 300.f);

    auto *stock_stack_node = new Center();
    left_vbox->add_child(stock_stack_node);
    stock_stack_node->anchors_fill()->minimum_size(200.f, 300.f);

    auto *stock_card_bounder_node = new Generic();
    stock_deck_node->add_child(stock_card_bounder_node);
    stock_card_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);

    auto *stock_stack_bounder_node = new Generic();
    stock_stack_node->add_child(stock_stack_bounder_node);
    stock_stack_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);

    auto foundation_hflow = new HFlow();
    right_vbox->add_child(foundation_hflow);
    foundation_hflow->anchors_fill()->expand(1.f, 0.f)->minimum_size(0.f, CARD_HEIGHT * CARD_SCALE + 50);

    std::vector<LayoutNode *> foundation_bounders;
    for (int i = 0; i < (int)game.foundation.size(); ++i) {
        auto *foundation_node = new Center();
        foundation_hflow->add_child(foundation_node);
        foundation_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE + 50, CARD_HEIGHT * CARD_SCALE + 50);

        auto *foundation_bounder_node = new Generic();
        foundation_node->add_child(foundation_bounder_node);
        foundation_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);
        foundation_bounders.push_back(foundation_bounder_node);
    }

    auto *tableau_hflow = new HFlow();
    right_vbox->add_child(tableau_hflow);
    tableau_hflow->anchors_fill()->expand(1.f, 1.f)->minimum_size(0.f, 256.f);

    std::vector<LayoutNode *> tableau_bounders;
    for (int i = 0; i < (int)game.tableau.size(); ++i) {
        auto *tableau_node = new Center();
        tableau_hflow->add_child(tableau_node);
        tableau_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE + 50, CARD_HEIGHT * CARD_SCALE + 50);

        auto *tableau_bounder_node = new Generic();
        tableau_node->add_child(tableau_bounder_node);
        tableau_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);
        tableau_bounders.push_back(tableau_bounder_node);
    }

    canvas_layer->compute_layout();

    while (!WindowShouldClose()) {
        rlImGuiBegin();
        BeginDrawing();
        ClearBackground(RAYWHITE);

        bool recompute_layout = true;
        if (recompute_layout) {
            root_node->bounds(0.f, 0.f, (float)GetScreenWidth(), (float)GetScreenHeight());
            canvas_layer->compute_layout();
        }

        auto debug_draw_node = [&](LayoutNode *node, Color color) {
            frameflow::Rect rect = node->get_bounding_rect();
            float x = rect.origin.x;
            float y = rect.origin.y;
            float w = rect.size.x;
            float h = rect.size.y;
            DrawRectangleLinesEx({x, y, w, h}, 2.0f, color);
        };

        /*
        debug_draw_node(left_vbox, RED);
        debug_draw_node(right_vbox, BLUE);
        debug_draw_node(stock_deck_node, GREEN);
        debug_draw_node(stock_stack_node, GREEN);
        debug_draw_node(stock_card_bounder_node, GREEN);
        debug_draw_node(stock_stack_bounder_node, GREEN);
        */

        // debug_draw_node(foundation_hflow, PURPLE);
        for (auto *foundation_bounder : foundation_bounders) {
            debug_draw_node(foundation_bounder, PURPLE);
        }

        /*
        debug_draw_node(tableau_hflow, ORANGE);
        for (auto *tableau_bounder : tableau_bounders) {
            debug_draw_node(tableau_bounder, ORANGE);
        }
        */

        const Vector2 mouse_pos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !game.is_lifting()) {
            handle_press(game, mouse_pos, foundation_bounders, tableau_bounders, stock_card_bounder_node,
                         stock_stack_bounder_node);
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game.is_lifting()) {
            handle_drop(game, mouse_pos, foundation_bounders, tableau_bounders, stock_card_bounder_node,
                        stock_stack_bounder_node);
        }

        if (!game.stock.empty()) {
            auto params = params_from_bounder(stock_card_bounder_node->get_bounding_rect(), false);
            for (int i = 0; i < (int)game.stock.size() - 1; ++i) {
                params.y += card_stacking_offset;
                params.x += card_stacking_offset;
                draw_card_to_screen(game.stock[i], params);
            }
        }
        if (!game.waste.empty()) {
            auto params = params_from_bounder(stock_stack_bounder_node->get_bounding_rect(), true);
            for (int i = 0; i < (int)game.waste.size() - 1; ++i) {
                params.y += card_stacking_offset;
                params.x += card_stacking_offset;
                draw_card_to_screen(game.waste[i], params);
            }
            draw_card_to_screen(game.waste.back(), params);
        }
        for (int i = 0; i < (int)game.foundation.size(); ++i) {
            if (!game.foundation[i].empty()) {
                auto params = params_from_bounder(foundation_bounders[i]->get_bounding_rect(), true);
                draw_card_to_screen(game.foundation[i].back(), params);
            }
        }
        for (int i = 0; i < (int)game.tableau.size(); ++i) {
            auto params = params_from_bounder(tableau_bounders[i]->get_bounding_rect(), true);
            for (int j = 0; j < (int)game.tableau[i].size(); ++j) {
                draw_card_to_screen(game.tableau[i][j], params);
                params.y += card_tableau_offset;
            }
        }

        ImGui::Begin("Solitaire Game");
        ImGui::Text("Stock size: %i", (int)game.stock.size());
        ImGui::End();

        rlImGuiEnd();
        if (game.is_lifting()) {
            auto params = CardDrawParams{.x = GetMousePosition().x - game.lifted_cards.x_offset,
                                         .y = GetMousePosition().y - game.lifted_cards.y_offset,
                                         .width = CARD_WIDTH * CARD_SCALE,
                                         .height = CARD_HEIGHT * CARD_SCALE,
                                         .rotation = 0.f,
                                         .face_up = true};
            for (int i = 0; i < (int)game.lifted_cards.cards.size(); ++i) {
                draw_card_to_screen(game.lifted_cards.cards[i], params);
                params.y += card_tableau_offset;
            }
        }
        EndDrawing();
    }

    destroy_card_texture_cache_v1();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
