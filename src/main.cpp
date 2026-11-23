#include "assets.h"
#include "draw.h"
#include "layout.h"
#include "solitaire.h"

#include <anvil2/tween/tween.hpp>
#include <frameflow/layout.hpp>

#include <fmt/format.h>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>
#include <physfs.h>
#include <raylib.h>
#include <rlImGui.h>
#include <spdlog/spdlog.h>

#include <array>

namespace {
constexpr float CARD_WIDTH = 11.0f;
constexpr float CARD_HEIGHT = 16.0f;
constexpr float CARD_SCALE = 8.0f;

const float card_stacking_offset = 0.1f;
const float card_tableau_offset = 5.f * CARD_SCALE;

struct AnimatedCard {
    Card card;
    bool active{false};
    float x;
    float y;
    // float target_x;
    // float target_y;
    bool face_up;
};

std::array<AnimatedCard, 52> animated_cards;

struct LayoutContext {
    CanvasLayer *canvas_layer;
    LayoutNode *stock_bounder;
    LayoutNode *waste_bounder;
    std::vector<LayoutNode *> tableau_bounders;
    std::vector<LayoutNode *> foundation_bounders;
};

LayoutContext layout_context;

void new_game(PersistentContext &ctx) {
    GameRuleset &ruleset = ctx.ruleset;
    Solitaire game;
    game.ruleset = ruleset; // Use the current ruleset from the context
    game.stock = create_deck();
    game.tableau.resize(ruleset.num_tableau_piles);
    game.tableau_face_down.resize(ruleset.num_tableau_piles);
    game.foundation.resize(4); // 4 piles for foundation

    // set up tableau piles
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j <= i; ++j) {
            game.tableau[i].push_back(game.stock.back());
            game.tableau_face_down[i] = j; // all cards are face down except the last one
            game.stock.pop_back();
        }
    }

    ctx.game = game;
}

ImFont *load_imgui_font_from_disk(const char *path, float size, ImFontConfig *config = nullptr) {
    const auto bytes = physfs_read_file(path);
    if (bytes.empty())
        return {};

    // ImGui takes ownership of this allocation.
    auto *data = IM_ALLOC(bytes.size());
    if (!data)
        return nullptr;

    std::memcpy(data, bytes.data(), bytes.size());

    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, static_cast<int>(bytes.size()), size, config);
}

bool point_inside_rect(float px, float py, const frameflow::Rect &rect) {
    int fuzzy_threshold = 16.0f; // Adjust this value to make the hitbox larger or smaller
    return (px >= rect.origin.x - fuzzy_threshold && px <= rect.origin.x + rect.size.x + fuzzy_threshold &&
            py >= rect.origin.y - fuzzy_threshold && py <= rect.origin.y + rect.size.y + fuzzy_threshold);
}

CardDrawParams params_from_bounder(const frameflow::Rect &bounder, bool face_up) {
    return CardDrawParams{.x = bounder.origin.x + bounder.size.x / 2.f,
                          .y = bounder.origin.y + bounder.size.y / 2.f,
                          .width = bounder.size.x,
                          .height = bounder.size.y,
                          .rotation = 0.f,
                          .face_up = face_up};
}

void animate_flip_stock_to_waste(Solitaire &game, LayoutNode *stock_bounder_node, LayoutNode *waste_bounder_node) {
    static bool is_animating = false;

    if (is_animating) {
        return;
    }

    if (game.stock.empty()) {
        if (!game.ruleset.allow_waste_to_recycle) {
            return;
        }
        is_animating = true;
        auto waste_copy = game.waste; // make a copy of the waste to animate
        game.flip_stock_to_waste();
        // for (int i = (int)waste_copy.size() - 1; i >= 0; --i) {
        for (int i = 0; i < (int)waste_copy.size(); ++i) {
            Card card = waste_copy[i];
            auto &animated_card = animated_cards[card.suit * 13 + (card.rank - 1)];
            animated_card.active = true;
            animated_card.face_up = false;
            animated_card.x = waste_bounder_node->get_rect().origin.x + waste_bounder_node->get_rect().size.x / 2.f +
                              card_stacking_offset * i;
            animated_card.y = waste_bounder_node->get_rect().origin.y + waste_bounder_node->get_rect().size.y / 2.f +
                              card_stacking_offset * i;

            float target_x = stock_bounder_node->get_rect().origin.x + stock_bounder_node->get_rect().size.x / 2.f +
                             card_stacking_offset * i;
            float target_y = stock_bounder_node->get_rect().origin.y + stock_bounder_node->get_rect().size.y / 2.f +
                             card_stacking_offset * i;

            TweenManager::get()
                .create_tween(&animated_card.y, target_y, 0.2f, Easing::EaseInOutQuad)
                ->delay(i * 0.05)
                ->on_complete([&animated_card]() { animated_card.active = false; });
            TweenManager::get().create_tween(&animated_card.x, target_x, 0.5f, Easing::EaseInOutQuad);
        }
        TweenManager::get()
            .create_timer(0.2f + (waste_copy.size() - 1) * 0.05f)
            ->on_complete([]() { is_animating = false; });
        return;
    }

    is_animating = true;

    Card card = game.stock.back();
    auto &animated_card = animated_cards[card.suit * 13 + (card.rank - 1)];
    animated_card.active = true;
    animated_card.face_up = true;
    animated_card.x = stock_bounder_node->get_rect().origin.x + stock_bounder_node->get_rect().size.x / 2.f;
    animated_card.y = stock_bounder_node->get_rect().origin.y + stock_bounder_node->get_rect().size.y / 2.f;

    float target_x = waste_bounder_node->get_rect().origin.x + waste_bounder_node->get_rect().size.x / 2.f +
                     card_stacking_offset * (game.waste.size() + 1);
    float target_y = waste_bounder_node->get_rect().origin.y + waste_bounder_node->get_rect().size.y / 2.f +
                     card_stacking_offset * (game.waste.size() + 1);

    TweenManager::get().create_tween(&animated_card.y, target_y, 0.2f, Easing::EaseInOutQuad);
    TweenManager::get()
        .create_tween(&animated_card.x, target_x, 0.2f, Easing::EaseInOutQuad)
        ->on_complete([&game, &animated_card]() {
            game.flip_stock_to_waste();
            animated_card.active = false;
            is_animating = false;
        });
}

void handle_press(Solitaire &game) {
    const Vector2 mouse_pos = GetMousePosition();
    auto stock_bounder = layout_context.stock_bounder;
    auto waste_bounder = layout_context.waste_bounder;
    auto &tableau_bounders = layout_context.tableau_bounders;

    bool clicked_on_stock = point_inside_rect(mouse_pos.x, mouse_pos.y, stock_bounder->get_rect());
    bool clicked_on_waste = point_inside_rect(mouse_pos.x, mouse_pos.y, waste_bounder->get_rect());
    if (clicked_on_stock) {
        animate_flip_stock_to_waste(game, stock_bounder, waste_bounder);
        return;
    } else if (clicked_on_waste && !game.waste.empty()) {
        const float center_x = waste_bounder->get_rect().origin.x + waste_bounder->get_rect().size.x / 2.f;
        const float center_y = waste_bounder->get_rect().origin.y + waste_bounder->get_rect().size.y / 2.f;
        game.lifted_cards.cards.push_back(game.waste.back());
        game.lifted_cards.is_waste = true;
        game.lifted_cards.x_offset = mouse_pos.x - center_x;
        game.lifted_cards.y_offset = mouse_pos.y - center_y;
        game.waste.pop_back();
        return;
    }

    // Check if the mouse is over any tableau pile
    for (int i = 0; i < (int)game.tableau.size(); ++i) {
        auto rect = tableau_bounders[i]->get_rect();
        rect.size.y += card_tableau_offset * (game.tableau[i].size() - 1);
        bool mouse_over_tableau = point_inside_rect(mouse_pos.x, mouse_pos.y, rect);
        if (mouse_over_tableau && !game.tableau[i].empty()) {
            // Determine which card in the tableau pile was clicked
            int card_index = (int)((mouse_pos.y - rect.origin.y) / card_tableau_offset);
            if (card_index >= (int)game.tableau[i].size()) {
                card_index = (int)game.tableau[i].size() - 1;
            }

            bool is_face_up = card_index >= game.tableau_face_down[i];
            if (!is_face_up) {
                break;
            }

            if (!game.can_pick_up_stack_from_tableau(game.tableau[i], card_index)) {
                break; // Can't pick up this stack, so don't lift any cards
            }

            // Lift the cards from the clicked index to the top of the pile
            auto card_rect = tableau_bounders[i]->get_rect();
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

void handle_drop(Solitaire &game) {
    auto mouse_pos = GetMousePosition();
    auto &foundation_bounders = layout_context.foundation_bounders;
    auto &tableau_bounders = layout_context.tableau_bounders;
    if (game.lifted_cards.cards.size() == 1) {
        bool mouse_over_foundation[4];
        for (int i = 0; i < 4; ++i) {
            mouse_over_foundation[i] =
                point_inside_rect(GetMousePosition().x, GetMousePosition().y, foundation_bounders[i]->get_rect());
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
        auto rect = tableau_bounders[i]->get_rect();
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

void reveal_tableau_cards_if_needed(Solitaire &game) {
    for (int i = 0; i < (int)game.tableau.size(); ++i) {
        if (!game.tableau[i].empty() && game.tableau_face_down[i] == (int)game.tableau[i].size()) {
            // Reveal the top card of the tableau pile
            game.tableau_face_down[i]--;
        }
    }
}

void play_cards_to_foundation_if_needed(Solitaire &game) {
    auto foundation_bounders = layout_context.foundation_bounders;
    auto waste_bounder = layout_context.waste_bounder;
    static bool is_animating = false;
    if (is_animating) {
        return;
    }
    if (!game.ruleset.auto_play_to_foundation) {
        return;
    }
    if (!game.waste.empty()) {
        Card top_waste_card = game.waste.back();
        for (int i = 0; i < 4; ++i) {
            if (game.can_drop_into_foundation(top_waste_card, i)) {
                auto card = game.waste.back();
                game.waste.pop_back();
                auto &animated_card = animated_cards[top_waste_card.suit * 13 + (top_waste_card.rank - 1)];
                animated_card.active = true;
                animated_card.face_up = true;
                animated_card.x = waste_bounder->get_rect().origin.x + waste_bounder->get_rect().size.x / 2.f;
                animated_card.y = waste_bounder->get_rect().origin.y + waste_bounder->get_rect().size.y / 2.f;
                float target_x =
                    foundation_bounders[i]->get_rect().origin.x + foundation_bounders[i]->get_rect().size.x / 2.f;
                float target_y =
                    foundation_bounders[i]->get_rect().origin.y + foundation_bounders[i]->get_rect().size.y / 2.f;
                is_animating = true;
                TweenManager::get().create_tween(&animated_card.y, target_y, 0.5f, Easing::EaseInOutQuad);
                TweenManager::get()
                    .create_tween(&animated_card.x, target_x, 0.5f, Easing::EaseInOutQuad)
                    ->on_complete([&game, i, card, &animated_card]() {
                        is_animating = false;
                        animated_card.active = false;
                        game.foundation[i].push_back(card);
                    });
                return;
            }
        }
    }
}

void ruleset_editor(GameRuleset &ruleset, bool readonly) {
    ImGui::SeparatorText("Game Ruleset");
    if (readonly) {
        ImGui::BeginDisabled();
    }
    ImGui::InputInt("Number of tableau piles", &ruleset.num_tableau_piles);
    ImGui::InputInt("Stock to waste offset", &ruleset.stock_to_waste_offset);
    ImGui::Checkbox("Allow waste to recycle", &ruleset.allow_waste_to_recycle);
    ImGui::Checkbox("Allow any card to empty tableau", &ruleset.allow_any_card_to_empty_tableau);
    ImGui::Checkbox("Auto-play to foundation", &ruleset.auto_play_to_foundation);
    ImGui::Checkbox("Allow undo", &ruleset.allow_undo);
    ImGui::InputInt("Undo plies (0 for unlimited)", &ruleset.undo_plies);
    if (readonly) {
        ImGui::EndDisabled();
    }
}

void render_main_menu(bool *show_menu, bool *want_quit, PersistentContext &ctx) {
    if (!*show_menu)
        return;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    // 1. Match the entire screen/window bounds
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    // 2. Control background opacity (0.0f = fully transparent, 0.75f = dimmed pause overlay)
    ImGui::SetNextWindowBgAlpha(0.95f);

    // 3. Remove rounded corners and outer border for flush edges
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | // Hides title bar, scrollbars, collapse, and resize grips
                             ImGuiWindowFlags_NoMove |       // Prevents dragging the menu
                             ImGuiWindowFlags_NoSavedSettings |      // Ignores imgui.ini layout persistence
                             ImGuiWindowFlags_NoBringToFrontOnFocus; // Keeps popups/modals clickable on top

    // Note: ImGui::End() must always be called regardless of Begin()'s return value
    if (ImGui::Begin("GameMainMenu", nullptr, flags)) {
        // Center a group of buttons on screen
        ImVec2 button_size(220.0f, 48.0f);
        ImGui::SetCursorPos(
            ImVec2((viewport->Size.x - button_size.x) * 0.5f, (viewport->Size.y - (button_size.y * 3 + 20.0f)) * 0.5f));

        ImGui::BeginGroup();

        // Automatically focus the first button when the menu opens
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }

        if (ImGui::Button("Resume Game", button_size)) {
            *show_menu = false;
        }
        if (ImGui::Button("New Game", button_size)) {
            new_game(ctx);
            *show_menu = false;
        }
        if (ImGui::Button("Settings", button_size)) {
            ImGui::OpenPopup("###SettingsModal");
        }
        if (ImGui::Button("Statistics", button_size)) {
            ImGui::OpenPopup("###StatisticsModal");
        }
        if (ImGui::Button("Quit", button_size)) {
            *want_quit = true;
        }

        ImGui::EndGroup();

        // Modals still render above the full-screen menu properly
        if (ImGui::BeginPopupModal("Settings###SettingsModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ctx.game) {
                auto &game = *ctx.game;
                ruleset_editor(ctx.ruleset, true);
                ImGui::SeparatorText("Debug");
                ImGui::Text("Stock : %zu", game.stock.size());
                ImGui::Text("Waste : %zu", game.waste.size());
                ImGui::Text("Lifted cards : %zu", game.lifted_cards.cards.size());
            }
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Statistics###StatisticsModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
} // namespace

int main(int argc, char *argv[]) {
    using namespace frameflow;
    (void)argc;
    physfs_init(argv[0]);

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Solitaire: House Rules");
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable default exit
    rlImGuiSetup(true);
    auto &io = ImGui::GetIO();
    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_NoHinting;
    const float dpi_scale = GetWindowScaleDPI().x;
    ImFontConfig font_config;
    ImFont *imgui_font = load_imgui_font_from_disk("Inter-Medium.ttf", 24, &font_config);
    io.FontDefault = imgui_font;
    int hud_logical_font_size = 24;
    auto font_buffer = physfs_read_file("Inter-Regular.ttf");
    Font hud_font = LoadFontFromMemory(".ttf", font_buffer.data(), static_cast<int>(font_buffer.size()),
                                       hud_logical_font_size * dpi_scale, nullptr, 0);
    SetTextureFilter(hud_font.texture, TEXTURE_FILTER_BILINEAR);

    for (int i = 0; i < 4; ++i) {
        for (int j = 1; j <= 13; ++j) {
            animated_cards[i * 13 + (j - 1)] = AnimatedCard{.card = Card{j, i},
                                                            .active = false,
                                                            .x = 0.f,
                                                            .y = 0.f,
                                                            // .target_x = 0.f,
                                                            // .target_y = 0.f,
                                                            .face_up = true};
        }
    }

    init_card_texture_cache_v1();

    PersistentContext ctx;
    if (!load_game(ctx)) {
        new_game(ctx);
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

    auto *stock_bounder_node = new Generic();
    stock_deck_node->add_child(stock_bounder_node);
    stock_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);

    auto *waste_bounder_node = new Generic();
    stock_stack_node->add_child(waste_bounder_node);
    waste_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);

    auto foundation_hflow = new HFlow();
    right_vbox->add_child(foundation_hflow);
    foundation_hflow->anchors_fill()->expand(1.f, 0.f)->minimum_size(0.f, CARD_HEIGHT * CARD_SCALE + 50);

    std::vector<LayoutNode *> foundation_bounders;
    for (int i = 0; i < (int)ctx.game->foundation.size(); ++i) {
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
    for (int i = 0; i < (int)ctx.game->tableau.size(); ++i) {
        auto *tableau_node = new Center();
        tableau_hflow->add_child(tableau_node);
        tableau_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE + 50, CARD_HEIGHT * CARD_SCALE + 50);

        auto *tableau_bounder_node = new Generic();
        tableau_node->add_child(tableau_bounder_node);
        tableau_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * CARD_SCALE, CARD_HEIGHT * CARD_SCALE);
        tableau_bounders.push_back(tableau_bounder_node);
    }

    layout_context.canvas_layer = canvas_layer;
    layout_context.stock_bounder = stock_bounder_node;
    layout_context.waste_bounder = waste_bounder_node;
    layout_context.tableau_bounders = tableau_bounders;
    layout_context.foundation_bounders = foundation_bounders;

    canvas_layer->compute_layout();

    const Color bg_color = {0x1F, 0x3F, 0x5F, 0xFF};

    bool want_quit = false;

    while (!WindowShouldClose() && !want_quit) {
        TweenManager::get().update(GetFrameTime());

        rlImGuiBegin();
        BeginDrawing();
        ClearBackground(bg_color);

        bool recompute_layout = true;
        if (recompute_layout) {
            root_node->bounds(0.f, 0.f, (float)GetScreenWidth(), (float)GetScreenHeight());
            canvas_layer->compute_layout();
        }

        auto debug_draw_node = [&](LayoutNode *node, Color color) {
            frameflow::Rect rect = node->get_rect();
            float x = rect.origin.x;
            float y = rect.origin.y;
            float w = rect.size.x;
            float h = rect.size.y;
            DrawRectangleLinesEx({x, y, w, h}, 2.0f, color);
        };

        /*
        debug_draw_node(left_vbox, RED);
        debug_draw_node(right_vbox, BLUE);
        debug_draw_node(stock_stack_node, GREEN);
        debug_draw_node(stock_bounder_node, GREEN);
        debug_draw_node(waste_bounder_node, GREEN);
        */

        if (ctx.game) {
            auto &game = *ctx.game;
            if (game.is_winning()) {
                ctx.wins++;
                // Probably spend some time animating the win, then start a new game
                new_game(ctx);
            }
        }

        {
            Vector2 pos = {floorf(24.0f), floorf(24.0f)};
            std::string wins_text = fmt::format("Wins: {}", ctx.wins);
            DrawTextEx(hud_font, wins_text.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, (float)hud_logical_font_size, 0.0f,
                       Fade(BLACK, 0.7f));
            DrawTextEx(hud_font, wins_text.c_str(), pos, (float)hud_logical_font_size, 0.0f, WHITE);
        }

        auto &game = *ctx.game;

        if (game.stock.empty()) {
            debug_draw_node(stock_bounder_node, GREEN);
        }

        // debug_draw_node(foundation_hflow, PURPLE);
        for (int i = 0; i < (int)game.foundation.size(); ++i) {
            if (game.foundation[i].empty()) {
                debug_draw_node(foundation_bounders[i], ORANGE);
            }
        }

        // draw empty tableau bounders
        for (int i = 0; i < (int)game.tableau.size(); ++i) {
            if (game.tableau[i].empty()) {
                debug_draw_node(tableau_bounders[i], ORANGE);
            }
        }

        if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !game.is_lifting()) {
                handle_press(game);
            } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game.is_lifting()) {
                handle_drop(game);
                reveal_tableau_cards_if_needed(game);
            }
        }
        play_cards_to_foundation_if_needed(game);

        if (!game.stock.empty()) {
            auto params = params_from_bounder(stock_bounder_node->get_rect(), false);
            for (int i = 0; i < (int)game.stock.size(); ++i) {
                params.y += card_stacking_offset;
                params.x += card_stacking_offset;
                draw_card_to_screen(game.stock[i], params);
            }
        }
        if (!game.waste.empty()) {
            auto params = params_from_bounder(waste_bounder_node->get_rect(), true);
            for (int i = 0; i < (int)game.waste.size() - 1; ++i) {
                params.y += card_stacking_offset;
                params.x += card_stacking_offset;
                draw_card_to_screen(game.waste[i], params);
            }
            draw_card_to_screen(game.waste.back(), params);
        }
        for (int i = 0; i < (int)game.foundation.size(); ++i) {
            if (!game.foundation[i].empty()) {
                auto params = params_from_bounder(foundation_bounders[i]->get_rect(), true);
                draw_card_to_screen(game.foundation[i].back(), params);
            }
        }
        for (int i = 0; i < (int)game.tableau.size(); ++i) {
            auto params = params_from_bounder(tableau_bounders[i]->get_rect(), true);
            for (int j = 0; j < (int)game.tableau[i].size(); ++j) {
                if (j < game.tableau_face_down[i]) {
                    params.face_up = false;
                } else {
                    params.face_up = true;
                }
                draw_card_to_screen(game.tableau[i][j], params);
                params.y += card_tableau_offset;
            }
        }
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

        for (const auto &animated_card : animated_cards) {
            if (animated_card.active) {
                auto params = CardDrawParams{.x = animated_card.x,
                                             .y = animated_card.y,
                                             .width = CARD_WIDTH * CARD_SCALE,
                                             .height = CARD_HEIGHT * CARD_SCALE,
                                             .rotation = 0.f,
                                             .face_up = animated_card.face_up};
                draw_card_to_screen(animated_card.card, params);
            }
        }

        render_main_menu(&ctx.show_main_menu, &want_quit, ctx);
        if (!ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(KEY_ESCAPE)) {
            ctx.show_main_menu = !ctx.show_main_menu;
        }
        // TODO: delete this in prod
        if (IsKeyPressed(KEY_Q)) {
            want_quit = true;
        }
        rlImGuiEnd();
        EndDrawing();
    }

    UnloadFont(hud_font);
    destroy_card_texture_cache_v1();
    rlImGuiShutdown();
    CloseWindow();

    save_game(ctx);

    delete canvas_layer;
    spdlog::info("Exiting Solitaire House Rules");

    return 0;
}
