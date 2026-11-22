#include "assets.h"
#include "deck.h"
#include "draw.h"

#include <fmt/format.h>
#include <raylib.h>

#include <array>

namespace {
    std::array<Texture2D, 52> card_texture_cache_v1_;

    std::array<Texture2D, 8> card_back_texture_cache_v1_;
}

Texture2D load_texture_from_disk(const char *path) {
    const auto bytes = physfs_read_file(path);
    if (bytes.empty())
        return {};

    Image image = LoadImageFromMemory(".png", bytes.data(),
                                      static_cast<int>(bytes.size()));
    if (!image.data)
        return {};

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_16X);
    return texture;
}

const char* card_to_path_v1(const Card &card) {
    static std::array<std::string, 52> cache;
    int index = card.suit * 13 + (card.rank - 1);

    if (!cache[index].empty()) {
        return cache[index].c_str();
    }

    static std::array<const char *, 4> suit_names = {"hearts", "clubs", "diamonds", "spades"};
    const char *suit_str = suit_names[card.suit];

    std::string rank_str;
    switch (card.rank) {
    case 1:
        rank_str = "ace";
        break;
    case 11:
        rank_str = "jack";
        break;
    case 12:
        rank_str = "queen";
        break;
    case 13:
        rank_str = "king";
        break;
    default:
        rank_str = std::to_string(card.rank);
        if (card.rank < 10) {
            rank_str = "0" + rank_str;
        }
    }

    cache[index] = fmt::format("v1/Casino/Cards/{}_{}.png", suit_str, rank_str);
    return cache[index].c_str();
}

Texture2D card_to_texture_v1(const Card &card) {
    return card_texture_cache_v1_[card.suit * 13 + (card.rank - 1)];
}

void init_card_texture_cache_v1() {
    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            Card card{rank, suit};
            const char *path = card_to_path_v1(card);
            card_texture_cache_v1_[suit * 13 + (rank - 1)] = load_texture_from_disk(path);
        }
    }

    for (int i = 0; i <= 8; ++i) {
        std::string back_path = fmt::format("v1/Casino/Cards/back0{}.png", i + 1);
        card_back_texture_cache_v1_[i] = load_texture_from_disk(back_path.c_str());
    }
}

void destroy_card_texture_cache_v1() {
    for (auto &texture : card_texture_cache_v1_) {
        UnloadTexture(texture);
    }
}

void draw_card(const Card &card, const CardDrawParams &params) {
    Texture2D texture = card_to_texture_v1(card);
    if (params.face_up) {
        DrawTexturePro(texture,
                       {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                       {params.x, params.y, params.width, params.height},
                       {params.width / 2.0f, params.height / 2.0f}, params.rotation, WHITE);
    } else {
        // Draw the back of the card
        DrawTexturePro(card_back_texture_cache_v1_[0],
                       {0, 0, static_cast<float>(card_back_texture_cache_v1_[0].width),
                        static_cast<float>(card_back_texture_cache_v1_[0].height)},
                       {params.x, params.y, params.width, params.height},
                       {params.width / 2.0f, params.height / 2.0f}, params.rotation, WHITE);
    }
}
