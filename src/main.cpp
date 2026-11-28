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
#include <atomic>
#include <deque>

namespace {
float get_card_scale() { return 8.0f; }

struct AnimatedCard {
    Card card;
    Vector2 position{0.f, 0.f};
    bool face_up{false};
};

struct Animation {
    int card;
    bool is_face_up{false};
    Vector2 target_position;
};

// should just be the raw action object itself
std::deque<Animation> command_queue;
std::optional<Animation> current_animation;

constexpr float CARD_WIDTH = 11.0f;
constexpr float CARD_HEIGHT = 16.0f;

const float card_stacking_offset = 0.01f * get_card_scale();
const float card_tableau_offset = 5.f * get_card_scale();

std::array<AnimatedCard, 52> animated_cards;

struct LayoutContext {
    CanvasLayer *canvas_layer;
    LayoutNode *stock_bounder;
    LayoutNode *waste_bounder;
    std::vector<LayoutNode *> tableau_bounders;
    std::vector<LayoutNode *> foundation_bounders;
};

LayoutContext layout_context;

Vector2 get_target_position(LayoutNode *node) {
    const frameflow::Rect rect = node->get_rect();
    return {rect.origin.x + rect.size.x / 2.f, rect.origin.y + rect.size.y / 2.f};
}

void init_game_card_positions(Solitaire &game) {
    // Initialize animated cards with their initial positions and face-up states
    for (const auto &card : game.get_stock().cards) {
        animated_cards[card.id()] =
            AnimatedCard{.card = card, .position = get_target_position(layout_context.stock_bounder), .face_up = false};
    }

    for (const auto &card : game.get_waste().cards) {
        animated_cards[card.id()] =
            AnimatedCard{.card = card, .position = get_target_position(layout_context.waste_bounder), .face_up = true};
    }
    for (int i = 0; i < 4; ++i) {
        const auto &foundation = game.get_foundation(i);
        for (const auto &card : foundation.cards) {
            animated_cards[card.id()] = AnimatedCard{
                .card = card, .position = get_target_position(layout_context.foundation_bounders[i]), .face_up = true};
        }
    }
    for (int i = 0; i < 7; ++i) {
        const auto &tableau = game.get_tableau(i);
        int k = 0;
        for (const auto &card : tableau.cards) {
            auto animated_card = AnimatedCard{
                .card = card, .position = get_target_position(layout_context.tableau_bounders[i]), .face_up = true};
            animated_card.position.y += k * card_tableau_offset;
            animated_cards[card.id()] = animated_card;
            k++;
        }
    }
}

void new_game(PersistentContext &ctx) {
    command_queue.clear();
    current_animation.reset();

    GameRuleset &ruleset = ctx.ruleset;
    ctx.game = Solitaire{};
    ctx.game->reset(ruleset);
    auto &game = *ctx.game;
    init_game_card_positions(game);

    spdlog::info("Started a new game with ruleset: stock_to_waste_offset={}, allow_waste_to_recycle={}, "
                 "allow_any_card_to_empty_tableau={}, allow_undo={}, undo_plies={}, auto_play_to_foundation={}",
                 ruleset.stock_to_waste_offset, ruleset.allow_waste_to_recycle, ruleset.allow_any_card_to_empty_tableau,
                 ruleset.allow_undo, ruleset.undo_plies, ruleset.auto_play_to_foundation);
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
    int fuzzy_threshold = 2.0f * get_card_scale(); // Adjust this value to make the hitbox larger or smaller
    return (px >= rect.origin.x - fuzzy_threshold && px <= rect.origin.x + rect.size.x + fuzzy_threshold &&
            py >= rect.origin.y - fuzzy_threshold && py <= rect.origin.y + rect.size.y + fuzzy_threshold);
}

void ruleset_editor(GameRuleset &ruleset, bool readonly) {
    ImGui::SeparatorText("Game Ruleset");
    if (readonly) {
        ImGui::BeginDisabled();
    }
    ImGui::BeginDisabled();
    static int disabled_default_value_tableau = 7;
    ImGui::InputInt("Number of tableau piles", &disabled_default_value_tableau);
    ImGui::EndDisabled();
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
                ruleset_editor(ctx.ruleset, false);
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

Generic *init_canvas_layer(CanvasLayer *canvas_layer) {
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
    stock_deck_node->anchors_fill()->minimum_size(200.f, CARD_HEIGHT * get_card_scale() + 50);

    auto *stock_stack_node = new Center();
    left_vbox->add_child(stock_stack_node);
    stock_stack_node->anchors_fill()->minimum_size(200.f, CARD_HEIGHT * get_card_scale() + 50);

    auto *stock_bounder_node = new Generic();
    stock_deck_node->add_child(stock_bounder_node);
    stock_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale(), CARD_HEIGHT * get_card_scale());

    auto *waste_bounder_node = new Generic();
    stock_stack_node->add_child(waste_bounder_node);
    waste_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale(), CARD_HEIGHT * get_card_scale());

    auto foundation_hflow = new HFlow();
    right_vbox->add_child(foundation_hflow);
    foundation_hflow->anchors_fill()->expand(1.f, 0.f)->minimum_size(0.f, CARD_HEIGHT * get_card_scale() + 50);

    std::vector<LayoutNode *> foundation_bounders;
    for (int i = 0; i < 4; ++i) {
        auto *foundation_node = new Center();
        foundation_hflow->add_child(foundation_node);
        foundation_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale() + 50,
                                                      CARD_HEIGHT * get_card_scale() + 50);

        auto *foundation_bounder_node = new Generic();
        foundation_node->add_child(foundation_bounder_node);
        foundation_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale(),
                                                              CARD_HEIGHT * get_card_scale());
        foundation_bounders.push_back(foundation_bounder_node);
    }

    auto *tableau_hflow = new HFlow();
    right_vbox->add_child(tableau_hflow);
    tableau_hflow->anchors_fill()->expand(1.f, 1.f)->minimum_size(0.f, 256.f);

    std::vector<LayoutNode *> tableau_bounders;
    for (int i = 0; i < 7; ++i) {
        auto *tableau_node = new Center();
        tableau_hflow->add_child(tableau_node);
        tableau_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale() + 50,
                                                   CARD_HEIGHT * get_card_scale() + 50);

        auto *tableau_bounder_node = new Generic();
        tableau_node->add_child(tableau_bounder_node);
        tableau_bounder_node->anchors_fill()->minimum_size(CARD_WIDTH * get_card_scale(),
                                                           CARD_HEIGHT * get_card_scale());
        tableau_bounders.push_back(tableau_bounder_node);
    }

    layout_context.canvas_layer = canvas_layer;
    layout_context.stock_bounder = stock_bounder_node;
    layout_context.waste_bounder = waste_bounder_node;
    layout_context.tableau_bounders = tableau_bounders;
    layout_context.foundation_bounders = foundation_bounders;
    return root_node;
}

void update_animated_cards() {
    if (!current_animation && !command_queue.empty()) {
        current_animation = command_queue.front();
        command_queue.pop_front();

        AnimatedCard &animated_card = animated_cards[current_animation->card];
        animated_card.face_up = current_animation->is_face_up;
        TweenManager::get().create_tween(&animated_card.position.y, current_animation->target_position.y, 0.2f,
                                         Easing::EaseInOutQuad);
        TweenManager::get()
            .create_tween(&animated_card.position.x, current_animation->target_position.x, 0.2f, Easing::EaseInOutQuad)
            ->on_complete([&]() {
                animated_card.position = current_animation->target_position;
                current_animation.reset();
            });
    }
}

void draw_animated_cards(Solitaire &game) {
    std::vector<Card> cards_sorted;
    cards_sorted.reserve(52);

    cards_sorted.insert(cards_sorted.end(), game.get_stock().cards.begin(), game.get_stock().cards.end());
    cards_sorted.insert(cards_sorted.end(), game.get_waste().cards.begin(), game.get_waste().cards.end());
    for (int i = 0; i < 4; ++i) {
        const auto &foundation = game.get_foundation(i);
        cards_sorted.insert(cards_sorted.end(), foundation.cards.begin(), foundation.cards.end());
    }
    for (int i = 0; i < 7; ++i) {
        const auto &tableau = game.get_tableau(i);
        cards_sorted.insert(cards_sorted.end(), tableau.cards.begin(), tableau.cards.end());
    }

    for (const auto &card : cards_sorted) {
        if (current_animation && card.id() == current_animation->card) {
            continue; // Skip drawing cards that are currently being animated
        }
        if (game.lifted_cards) {
            bool skip = false;
            for (const auto &lifted_card : game.lifted_cards->cards) {
                if (card.id() == lifted_card.id()) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue; // Skip drawing lifted cards
        }
        auto animated_card = animated_cards[card.id()];
        auto params = CardDrawParams{.x = animated_card.position.x,
                                     .y = animated_card.position.y,
                                     .width = CARD_WIDTH * get_card_scale(),
                                     .height = CARD_HEIGHT * get_card_scale(),
                                     .rotation = 0.f,
                                     .face_up = card.is_face_up};
        draw_card_to_screen(card, params);
    }

    if (current_animation) {
        auto card_id = current_animation->card;
        auto card = animated_cards[card_id].card;
        auto animated_card = animated_cards[card_id];
        auto params = CardDrawParams{.x = animated_card.position.x,
                                     .y = animated_card.position.y,
                                     .width = CARD_WIDTH * get_card_scale(),
                                     .height = CARD_HEIGHT * get_card_scale(),
                                     .rotation = 0.f,
                                     .face_up = current_animation->is_face_up};
        draw_card_to_screen(card, params);
    }
    if (game.lifted_cards) {
        int i = 0;
        for (const auto &lifted_card : game.lifted_cards->cards) {
            auto params = CardDrawParams{.x = GetMousePosition().x - game.lifted_cards->x_offset,
                                         .y = GetMousePosition().y - game.lifted_cards->y_offset,
                                         .width = CARD_WIDTH * get_card_scale(),
                                         .height = CARD_HEIGHT * get_card_scale(),
                                         .rotation = 0.f,
                                         .face_up = lifted_card.is_face_up};
            params.y += i * card_tableau_offset; // Stack lifted cards with an offset
            draw_card_to_screen(lifted_card, params);
            i++;
        }
    }
}

void handle_game_mouse_input(PersistentContext &ctx) {
    if (current_animation)
        return; // Ignore mouse input during animations

    auto &game = *ctx.game;
    const Vector2 mouse_pos = GetMousePosition();
    auto check_lift_one_from_simple_pile = [&mouse_pos, &game](LayoutNode *bounder, Pile &pile,
                                                               int source_pile_index) -> bool {
        if (point_inside_rect(mouse_pos.x, mouse_pos.y, bounder->get_rect())) {
            const auto center_x = bounder->get_rect().origin.x + bounder->get_rect().size.x / 2.f;
            const auto center_y = bounder->get_rect().origin.y + bounder->get_rect().size.y / 2.f;
            game.lifted_cards = LiftedCards{.cards = {pile.cards.back()},
                                            .source_pile_index = source_pile_index,
                                            .x_offset = mouse_pos.x - center_x,
                                            .y_offset = mouse_pos.y - center_y};
            return true;
        }
        return false;
    };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !game.lifted_cards) {
        if (point_inside_rect(mouse_pos.x, mouse_pos.y, layout_context.stock_bounder->get_rect())) {
            auto cards = game.flip_stock_to_waste(ctx.ruleset);
            for (const auto &card_id : cards) {
                auto target = get_target_position(layout_context.waste_bounder);
                command_queue.push_back(Animation{.card = card_id, .is_face_up = true, .target_position = target});
            }
            return;
        }
        if (check_lift_one_from_simple_pile(layout_context.waste_bounder, game.get_waste(), game.waste_))
            return;
        for (int i = 0; i < 4; ++i) {
            if (check_lift_one_from_simple_pile(layout_context.foundation_bounders[i], game.get_foundation(i),
                                                game.foundations_[i]))
                return;
        }
        // now check tableau piles. we can lift multiple cards from tableau piles, so we need to check if the mouse is
        // over any of the face-up cards in the tableau, using the offset.
        for (int i = 0; i < 7; ++i) {
            const auto &tableau = game.get_tableau(i);
            if (tableau.cards.empty())
                continue;
            const auto &top_card = tableau.cards.back();
            if (!top_card.is_face_up)
                continue;

            auto lift_rect = layout_context.tableau_bounders[i]->get_rect();
            lift_rect.size.y += card_tableau_offset * (tableau.cards.size() - 1);
            if (!point_inside_rect(mouse_pos.x, mouse_pos.y, lift_rect))
                continue;

            int card_index = static_cast<int>((mouse_pos.y - lift_rect.origin.y) / card_tableau_offset);
            if (card_index >= static_cast<int>(tableau.cards.size())) {
                card_index = static_cast<int>(tableau.cards.size()) - 1;
            }

            if (!tableau.cards[card_index].is_face_up)
                return;

            auto card_rect = layout_context.tableau_bounders[i]->get_rect();
            card_rect.origin.y += card_index * card_tableau_offset;

            const auto center_x = card_rect.origin.x + card_rect.size.x / 2.f;
            const auto center_y = card_rect.origin.y + card_rect.size.y / 2.f;

            std::vector<Card> lifted_cards(tableau.cards.begin() + card_index, tableau.cards.end());
            game.lifted_cards = LiftedCards{.cards = lifted_cards,
                                            .source_pile_index = game.tableaus_[i],
                                            .x_offset = mouse_pos.x - center_x,
                                            .y_offset = mouse_pos.y - center_y};

            return;
        }
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && game.lifted_cards) {
        if (game.lifted_cards->cards.size() == 1) {
            for (int i = 0; i < 4; ++i) {
                if (point_inside_rect(mouse_pos.x, mouse_pos.y, layout_context.foundation_bounders[i]->get_rect())) {
                    auto card = game.lifted_cards->cards[0];
                    if (game.drop_into_foundation(card, i, game.lifted_cards->source_pile_index)) {
                        animated_cards[card.id()].face_up = true;
                        animated_cards[card.id()].position = get_target_position(layout_context.foundation_bounders[i]);
                    }
                    game.lifted_cards.reset();
                    return;
                }
            }
        }
        for (int i = 0; i < 7; ++i) {
            auto rect = layout_context.tableau_bounders[i]->get_rect();
            rect.size.y += (game.get_tableau(i).cards.size()) * card_tableau_offset;
            if (point_inside_rect(mouse_pos.x, mouse_pos.y, rect)) {
                if (game.drop_into_tableau(game.lifted_cards->cards, i, game.lifted_cards->source_pile_index)) {
                    // Update positions of lifted cards to the target tableau pile
                    for (size_t j = 0; j < game.lifted_cards->cards.size(); ++j) {
                        auto &card = game.lifted_cards->cards[j];
                        animated_cards[card.id()].position = get_target_position(layout_context.tableau_bounders[i]);
                        animated_cards[card.id()].position.y +=
                            (game.get_tableau(i).cards.size() - 1) * card_tableau_offset;
                    }
                    game.lifted_cards.reset();
                    return;
                }
            }
        }
        game.lifted_cards.reset();
    }
}

void handle_game_keyboard_input(PersistentContext &ctx, bool &want_quit) {
    if (IsKeyPressed(KEY_SPACE)) {
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        ctx.show_main_menu = !ctx.show_main_menu;
    }
    // TODO: delete this in prod
    if (IsKeyPressed(KEY_Q)) {
        want_quit = true;
    }

    // Ctrl-N for new game
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_N)) {
        new_game(ctx);
    }

    if (!ctx.game || current_animation)
        return;
    auto &game = *ctx.game;

    // handle game-specific keyboard input

    // Ctrl-Z for undo
    bool control_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (control_down && IsKeyPressed(KEY_Z)) {
        auto action_opt = game.undo_last_action();
        if (action_opt) {
            const auto action = *action_opt;
            for (const auto &unit : action.units) {
                Vector2 target;
                bool face_up = true;
                if (unit.source_pile_index == game.stock_) {
                    target = get_target_position(layout_context.stock_bounder);
                    face_up = false;
                } else if (unit.source_pile_index == game.waste_) {
                    target = get_target_position(layout_context.waste_bounder);
                } else {
                    for (int i = 0; i < 4; ++i) {
                        if (unit.source_pile_index == game.foundations_[i]) {
                            target = get_target_position(layout_context.foundation_bounders[i]);
                            break;
                        }
                    }
                    for (int i = 0; i < 7; ++i) {
                        if (unit.source_pile_index == game.tableaus_[i]) {
                            target = get_target_position(layout_context.tableau_bounders[i]);
                            break;
                        }
                    }
                }
                for (int i = 0; i < unit.card_count; ++i) {
                    auto card_id = game.piles_[unit.source_pile_index]
                                       .cards[game.piles_[unit.source_pile_index].cards.size() - 1 - i]
                                       .id();
                    command_queue.push_back(
                        Animation{.card = card_id, .is_face_up = face_up, .target_position = target});
                }
            }
        }
    }
}

void handle_game_input(PersistentContext &ctx, bool &want_quit) {
    if (!ImGui::GetIO().WantCaptureMouse && ctx.game) {
        handle_game_mouse_input(ctx);
    }
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        handle_game_keyboard_input(ctx, want_quit);
    }
}

} // namespace

int main(int argc, char *argv[]) {
    (void)argc;

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "Solitaire: House Rules");
    SetWindowMinSize(800, 600);
    SetTargetFPS(60);

    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();

    physfs_init(argv[0]);

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
            animated_cards[i * 13 + (j - 1)] = AnimatedCard{.card = Card{j, i}};
        }
    }

    std::atomic<bool> is_loading_images = true;
    std::atomic<int> loading_progress = 0;
    init_card_images_v1(is_loading_images, loading_progress);

    auto *canvas_layer = new CanvasLayer();
    auto *root_node = init_canvas_layer(canvas_layer);
    canvas_layer->compute_layout();

    PersistentContext ctx;
    bool load_result = load_context(ctx);
    if (load_result) {
        spdlog::info("Setting window size to {}x{}, maximized={}", ctx.window.width, ctx.window.height,
                     ctx.window.is_maximized);
        SetWindowSize(ctx.window.width, ctx.window.height);
        if (ctx.window.is_maximized) {
            MaximizeWindow();
        }
        canvas_layer->compute_layout();
        init_game_card_positions(*ctx.game);
    } else {
        SetWindowSize(1280, 720);
        new_game(ctx);
    }

    const Color bg_color = {0x1F, 0x3F, 0x5F, 0xFF};

    bool want_quit = false;
    bool is_loading = true;
    bool is_first_frame = true;

    while (!WindowShouldClose() && !want_quit) {
        // show loading screen
        if (is_loading) {
            if (is_loading_images) {
                BeginDrawing();

                std::string wins_text = fmt::format("Loaded images: {}/60", loading_progress.load());
                auto text_size = MeasureTextEx(hud_font, wins_text.c_str(), (float)hud_logical_font_size, 0.0f);
                Vector2 pos = {floorf(GetScreenWidth() - text_size.x - 24.0f), floorf(24.0f)};
                DrawTextEx(hud_font, wins_text.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, (float)hud_logical_font_size,
                           0.0f, Fade(BLACK, 0.7f));
                DrawTextEx(hud_font, wins_text.c_str(), pos, (float)hud_logical_font_size, 0.0f, WHITE);

                ClearBackground(BLACK);
                EndDrawing();
                continue;
            } else {
                init_card_textures_v1();
                SetExitKey(KEY_NULL); // disable default exit
                is_loading = false;
            }
        }

        TweenManager::get().update(GetFrameTime());

        rlImGuiBegin();
        BeginDrawing();
        ClearBackground(bg_color);

        bool recompute_layout = IsWindowResized() || is_first_frame;
        is_first_frame = false;
        if (recompute_layout) {
            root_node->bounds(0.f, 0.f, (float)GetScreenWidth(), (float)GetScreenHeight());
            canvas_layer->compute_layout();
            ctx.window.width = GetScreenWidth();
            ctx.window.height = GetScreenHeight();
            ctx.window.is_maximized = IsWindowMaximized();
        }

        auto debug_draw_node = [&](LayoutNode *node, Color color) {
            frameflow::Rect rect = node->get_rect();
            float x = rect.origin.x;
            float y = rect.origin.y;
            float w = rect.size.x;
            float h = rect.size.y;
            DrawRectangleLinesEx({x, y, w, h}, 2.0f, color);
        };

        auto debug_draw_node_recursive = [&](LayoutNode *node, int depth, auto &self) -> void {
            // set color based on depth
            Color color;
            switch (depth % 6) {
            case 0:
                color = RED;
                break;
            case 1:
                color = GREEN;
                break;
            case 2:
                color = BLUE;
                break;
            case 3:
                color = YELLOW;
                break;
            case 4:
                color = MAGENTA;
                break;
            case 5:
                color = PINK;
                break;
            }

            debug_draw_node(node, color);
            for (auto *child : node->children) {
                self(child, depth + 1, self);
            }
        };

        debug_draw_node_recursive(root_node, 0, debug_draw_node_recursive);

        update_animated_cards();

        handle_game_input(ctx, want_quit);
        render_main_menu(&ctx.show_main_menu, &want_quit, ctx);

        if (ctx.game) {
            auto &game = *ctx.game;
            if (game.is_winning()) {
                ctx.wins++;
                // Probably spend some time animating the win, then start a new game
                // new_game(ctx);
            }
            draw_animated_cards(game);
        }

        {
            ImGui::Begin("Game State Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.1f", (float)GetFPS());
            if (ctx.game) {
                ImGui::Text("Current animation: %s", current_animation ? "Yes" : "No");
                ImGui::Text("Game state:");
                ImGui::Text("Stock: %d cards", (int)ctx.game->get_stock().cards.size());
                ImGui::Text("Waste: %d cards", (int)ctx.game->get_waste().cards.size());
                for (int i = 0; i < 4; ++i) {
                    ImGui::Text("Foundation %d: %d cards", i + 1, (int)ctx.game->get_foundation(i).cards.size());
                }
                for (int i = 0; i < 7; ++i) {
                    ImGui::Text("Tableau %d: %d cards", i + 1, (int)ctx.game->get_tableau(i).cards.size());
                }
            } else {
                ImGui::Text("No game in progress");
            }
            ImGui::End();
        }

        [&]() {
            std::string wins_text = fmt::format("Wins: {}", ctx.wins);
            auto text_size = MeasureTextEx(hud_font, wins_text.c_str(), (float)hud_logical_font_size, 0.0f);
            Vector2 pos = {floorf(GetScreenWidth() - text_size.x - 24.0f), floorf(24.0f)};
            DrawTextEx(hud_font, wins_text.c_str(), {pos.x + 1.0f, pos.y + 1.0f}, (float)hud_logical_font_size, 0.0f,
                       Fade(BLACK, 0.7f));
            DrawTextEx(hud_font, wins_text.c_str(), pos, (float)hud_logical_font_size, 0.0f, WHITE);
        }();

        rlImGuiEnd();
        EndDrawing();
    }

    UnloadFont(hud_font);
    destroy_card_texture_cache_v1();
    rlImGuiShutdown();
    CloseWindow();

    save_context(ctx);

    delete canvas_layer;
    spdlog::info("Exiting Solitaire House Rules");

    return 0;
}
