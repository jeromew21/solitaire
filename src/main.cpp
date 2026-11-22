#include "assets.h"
#include "deck.h"
#include "draw.h"

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

int main(int argc, char *argv[]) {
    using namespace frameflow;
    (void)argc;
    physfs_init(argv[0]);

    SetTraceLogLevel(LOG_NONE);
    InitWindow(800, 600, "Solitaire");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    SetWindowMinSize(400, 300);
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

    System layout_system;
    NodeId root_node_id = add_generic(&layout_system, NullNode);

    auto root_node = get_node(&layout_system, root_node_id);
    root_node->bounds = {{0, 0}, {(float)GetScreenWidth(), (float)GetScreenHeight()}};

    NodeId box_node = add_box(&layout_system, root_node_id, BoxData{Direction::Horizontal, Align::Start});
    get_node(&layout_system, box_node)->anchors = Anchors{0.f, 0.f, 1.f, 1.f};

    NodeId left_node_id = add_box(&layout_system, box_node, BoxData{Direction::Vertical, Align::Start});
    auto left_node = get_node(&layout_system, left_node_id);
    left_node->minimum_size = {200.f, 0.f};
    left_node->expand = {0.f, 1.f};
    left_node->anchors = Anchors{0.f, 0.f, 1.f, 1.f};

    NodeId right_node_id = add_box(&layout_system, box_node, BoxData{Direction::Vertical, Align::Start});
    auto right_node = get_node(&layout_system, right_node_id);
    right_node->expand = {1.f, 1.f};
    right_node->anchors = Anchors{0.0f, 0.f, 1.f, 1.f};

    NodeId stock_container_node_id = add_center(&layout_system, left_node_id);
    auto stock_container_node = get_node(&layout_system, stock_container_node_id);
    stock_container_node->minimum_size = {200.f, 200.f};
    stock_container_node->expand = {1.f, 0.f};

    NodeId stock_node_id = add_generic(&layout_system, stock_container_node_id);
    auto stock_node = get_node(&layout_system, stock_node_id);
    stock_node->minimum_size = {11.f, 16.f};

    NodeId foundation_container_node_id = add_flow(&layout_system, right_node_id, FlowData{Direction::Horizontal, Align::Start});
    auto foundation_container_node = get_node(&layout_system, foundation_container_node_id);
    foundation_container_node->minimum_size = {200.f, 200.f};
    foundation_container_node->expand = {1.f, 0.f};
    foundation_container_node->anchors = Anchors{0.0f, 0.f, 1.f, 1.f};

    NodeId foundation_node_id[4];
    Node *foundation_node[4];
    for (int i = 0; i < 4; ++i) {
        foundation_node_id[i] = add_generic(&layout_system, foundation_container_node_id);
        foundation_node[i] = get_node(&layout_system, foundation_node_id[i]);
        foundation_node[i]->minimum_size = {11.f, 16.f};
    }


    compute_layout(&layout_system, root_node_id);

    while (!WindowShouldClose()) {
        rlImGuiBegin();
        BeginDrawing();
        ClearBackground(RAYWHITE);

        bool recompute_layout = true;
        if (recompute_layout) {
            auto root_node = get_node(&layout_system, root_node_id);
            root_node->bounds = {{0, 0}, {(float)GetScreenWidth(), (float)GetScreenHeight()}};
            compute_layout(&layout_system, root_node_id);
        }

        auto debug_draw_node = [&](NodeId id, Color color) {
            auto node = get_node(&layout_system, id);
            DrawRectangleLinesEx(
                {node->bounds.origin.x, node->bounds.origin.y, node->bounds.size.x, node->bounds.size.y}, 1, color);
        };

        debug_draw_node(left_node_id, BLUE);
        debug_draw_node(stock_container_node_id, GREEN);
        debug_draw_node(stock_node_id, GREEN);

        debug_draw_node(right_node_id, RED);
        debug_draw_node(foundation_container_node_id, PURPLE);
        for (int i = 0; i < 4; ++i) {
            debug_draw_node(foundation_node_id[i], ORANGE);
        }

        /*
        int y = 20 + 16 * 3.0f + 10;
        int x = 20 + 11 * 5.0f + 10;
        for (const auto &card : game.stock) {
            CardDrawParams params = {
                .x = (float)x, .y = (float)y, .width = 11 * 5.0f, .height = 16 * 5.0f, .rotation = 0, .face_up = false};
            draw_card(card, params);
            x += 11 * 5.0f + 10;
            if (x + 100 > GetScreenWidth()) {
                x = 20 + 11 * 5.0f + 10;
                y += 16 * 5.0f + 10;
            }
        }
        */

        ImGui::Begin("Solitaire Game");
        ImGui::Text("Stock size: %i", (int)game.stock.size());
        ImGui::End();

        rlImGuiEnd();
        EndDrawing();
    }

    destroy_card_texture_cache_v1();
    rlImGuiShutdown();
    CloseWindow();

    return 0;
}
