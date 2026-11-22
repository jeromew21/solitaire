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

namespace {

void new_game(PersistentContext &ctx);

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

void handle_press(Solitaire &game, const Vector2 &mouse_pos, const std::vector<LayoutNode *> &tableau_bounders,
                  LayoutNode *stock_card_bounder_node, LayoutNode *stock_stack_bounder_node) {
    bool clicked_on_stock = point_inside_rect(mouse_pos.x, mouse_pos.y, stock_card_bounder_node->get_bounding_rect());
    bool clicked_on_waste = point_inside_rect(mouse_pos.x, mouse_pos.y, stock_stack_bounder_node->get_bounding_rect());
    if (clicked_on_stock) {
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
                 const std::vector<LayoutNode *> &tableau_bounders) {
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

void ruleset_editor(GameRuleset &ruleset, bool readonly) {
    ImGui::SeparatorText("Game Ruleset");
    if (readonly) {
        ImGui::BeginDisabled();
    }
    ImGui::InputInt("Number of tableau piles", &ruleset.num_tableau_piles);
    ImGui::InputInt("Stock to waste offset", &ruleset.stock_to_waste_offset);
    ImGui::Checkbox("Allow waste to recycle", &ruleset.allow_waste_to_recycle);
    ImGui::Checkbox("Allow any card to empty tableau", &ruleset.allow_any_card_to_empty_tableau);
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
                ruleset_editor(game.ruleset, true);
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

void new_game(PersistentContext &ctx) {
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

    ctx.game = game;
}
} // namespace

int main(int argc, char *argv[]) {
    using namespace frameflow;
    (void)argc;
    physfs_init(argv[0]);

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Solitaire House Rules");
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // disable default exit
    rlImGuiSetup(true);
    auto &io = ImGui::GetIO();
    io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
    io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_NoHinting;
    ImFontConfig font_config;
    float dpi_scale = GetWindowScaleDPI().x;
    font_config.RasterizerDensity = dpi_scale;
    ImFont *imgui_font = load_imgui_font_from_disk("Inter-Medium.ttf", 16);
    io.FontDefault = imgui_font;

    int hud_logical_font_size = 24;
    auto font_buffer = physfs_read_file("Inter-Regular.ttf");
    Font hud_font = LoadFontFromMemory(".ttf", font_buffer.data(), static_cast<int>(font_buffer.size()), hud_logical_font_size, nullptr, 0);
    SetTextureFilter(hud_font.texture, TEXTURE_FILTER_BILINEAR);

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

    canvas_layer->compute_layout();

    const Color bg_color = {0x1F, 0x3F, 0x5F, 0xFF};

    bool want_quit = false;

    while (!WindowShouldClose() && !want_quit) {
        rlImGuiBegin();
        BeginDrawing();
        ClearBackground(bg_color);

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
        debug_draw_node(stock_stack_node, GREEN);
        debug_draw_node(stock_card_bounder_node, GREEN);
        debug_draw_node(stock_stack_bounder_node, GREEN);
        */

        if (ctx.game) {
            auto &game = *ctx.game;
            if (game.is_winning()) {
                ctx.wins++;
                // Probably spend some time animating the win, then start a new game
                new_game(ctx);
            }
        }
        Vector2 pos = {floorf(24.0f), floorf(24.0f)};
        std::string wins_text = fmt::format("Wins: {}", ctx.wins);
        DrawTextEx(hud_font, wins_text.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, (float)hud_logical_font_size, 0.0f,
                   Fade(BLACK, 0.7f));
        DrawTextEx(hud_font, wins_text.c_str(), pos, (float)hud_logical_font_size, 0.0f, WHITE);

        auto &game = *ctx.game;

        if (game.stock.empty()) {
            debug_draw_node(stock_card_bounder_node, GREEN);
        }

        // debug_draw_node(foundation_hflow, PURPLE);
        for (auto *foundation_bounder : foundation_bounders) {
            debug_draw_node(foundation_bounder, PURPLE);
        }

        // draw empty tableau bounders
        for (int i = 0; i < (int)game.tableau.size(); ++i) {
            if (game.tableau[i].empty()) {
                debug_draw_node(tableau_bounders[i], ORANGE);
            }
        }

        if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard) {
            const Vector2 mouse_pos = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !game.is_lifting()) {
                handle_press(game, mouse_pos, tableau_bounders, stock_card_bounder_node, stock_stack_bounder_node);
            } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game.is_lifting()) {
                handle_drop(game, mouse_pos, foundation_bounders, tableau_bounders);
            }
        }

        if (!game.stock.empty()) {
            auto params = params_from_bounder(stock_card_bounder_node->get_bounding_rect(), false);
            for (int i = 0; i < (int)game.stock.size(); ++i) {
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

        render_main_menu(&ctx.show_main_menu, &want_quit, ctx);
        if (!ImGui::GetIO().WantCaptureKeyboard && IsKeyPressed(KEY_ESCAPE)) {
            ctx.show_main_menu = !ctx.show_main_menu;
        }
        rlImGuiEnd();
        EndDrawing();
    }

    UnloadFont(hud_font);
    destroy_card_texture_cache_v1();
    rlImGuiShutdown();
    CloseWindow();

    save_game(ctx);
    fmt::print("Game saved to disk.\n");

    return 0;
}
