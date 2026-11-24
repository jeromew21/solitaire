#pragma once

#include <atomic>
#include <thread>
#include <vector>

struct Card;

struct CardDrawParams {
    float x;
    float y;
    float width;
    float height;
    float rotation;
    bool face_up;
};

void draw_card_to_screen(const Card &card, const CardDrawParams &params);

void init_card_images_v1(std::atomic<bool> &is_loading, std::atomic<int> &progress);

void init_card_textures_v1();

void destroy_card_texture_cache_v1();
