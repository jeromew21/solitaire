#include "draw.h"
#include "assets.h"
#include "solitaire.h"

#include <fmt/format.h>
#include <raylib.h>

#include <array>
#include <thread>
#include <memory>

namespace {

struct AssetJob {
    std::string path{};
    Image image;
    Texture2D texture;

    bool is_card{true};
    int id{};
};

std::vector<AssetJob> asset_jobs;

std::array<Texture2D, 52> card_texture_cache_v1_;

std::array<Texture2D, 8> card_back_texture_cache_v1_;
} // namespace

Image load_image_from_disk(const char *path) {
    const auto bytes = physfs_read_file(path);
    if (bytes.empty())
        return {};

    Image image = LoadImageFromMemory(".png", bytes.data(), static_cast<int>(bytes.size()));
    ImageResize(&image, image.width * 4, image.height * 4); // double the size of the image
    return image;
}

Texture2D load_texture_from_disk(const char *path) {
    Image image = load_image_from_disk(path);

    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    GenTextureMipmaps(&texture);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_16X);
    return texture;
}

const char *card_to_path_v1(const Card &card) {
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

Texture2D card_to_texture_v1(const Card &card) { return card_texture_cache_v1_[card.suit * 13 + (card.rank - 1)]; }

void init_card_images_v1(std::atomic<bool> &is_loading, std::atomic<int> &progress) {
    asset_jobs.clear();
    asset_jobs.reserve(60); // Prevents reallocations while populating

    for (int suit = 0; suit < 4; ++suit) {
        for (int rank = 1; rank <= 13; ++rank) {
            Card card{rank, suit};
            const char *path = card_to_path_v1(card);
            asset_jobs.push_back({.path = path, .image = {}, .texture = {}, .is_card = true, .id = suit * 13 + (rank - 1)});
        }
    }

    for (int i = 1; i <= 8; ++i) {
        std::string back_path = fmt::format("v1/Casino/Cards/back0{}.png", i);
        asset_jobs.push_back({.path = back_path, .image = {}, .texture = {}, .is_card = false, .id = i - 1});
    }

    unsigned int coreCount = std::thread::hardware_concurrency();
    unsigned int numWorkers = std::min(16u, std::max(1u, coreCount > 1 ? coreCount - 1 : 1u));
    fmt::print("Detected {} CPU cores, using {} threads for loading images.\n", coreCount, numWorkers);

    // Heap-allocated so it outlives init_card_images_v1() while detached threads run
    auto nextJobIndex = std::make_shared<std::atomic<size_t>>(0);

    for (unsigned int w = 0; w < numWorkers; ++w) {
        std::thread([nextJobIndex, &progress]() {
            while (true) {
                size_t idx = nextJobIndex->fetch_add(1, std::memory_order_relaxed);
                if (idx >= asset_jobs.size())
                    break;

                Image img = load_image_from_disk(asset_jobs[idx].path.c_str());
                if (img.data != nullptr) {
                    ImageResize(&img, img.width * 2, img.height * 2);
                }

                asset_jobs[idx].image = img;

                // Release semantics ensure asset_jobs[idx].image is visible to the main thread
                progress.fetch_add(1, std::memory_order_release);
            }
        }).detach();
    }
}

void init_card_textures_v1() {
    for (auto &job : asset_jobs) {
        job.texture = LoadTextureFromImage(job.image);
        UnloadImage(job.image);
        GenTextureMipmaps(&job.texture);
        SetTextureFilter(job.texture, TEXTURE_FILTER_ANISOTROPIC_16X);

        if (job.is_card) {
            card_texture_cache_v1_[job.id] = job.texture;
        } else {
            card_back_texture_cache_v1_[job.id] = job.texture;
        }
    }
}

void destroy_card_texture_cache_v1() {
    for (auto &texture : card_texture_cache_v1_) {
        UnloadTexture(texture);
    }
}

void draw_card_to_screen(const Card &card, const CardDrawParams &params) {
    Texture2D texture = card_to_texture_v1(card);
    if (params.face_up) {
        DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)},
                       {params.x, params.y, params.width, params.height}, {params.width / 2.0f, params.height / 2.0f},
                       params.rotation, WHITE);
    } else {
        // Draw the back of the card
        DrawTexturePro(card_back_texture_cache_v1_[0],
                       {0, 0, static_cast<float>(card_back_texture_cache_v1_[0].width),
                        static_cast<float>(card_back_texture_cache_v1_[0].height)},
                       {params.x, params.y, params.width, params.height}, {params.width / 2.0f, params.height / 2.0f},
                       params.rotation, WHITE);
    }
}
