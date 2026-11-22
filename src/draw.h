#pragma once

struct Card;

struct CardDrawParams {
    float x;
    float y;
    float width;
    float height;
    float rotation;
    bool face_up;
};

void draw_card(const Card &card, const CardDrawParams &params);

void init_card_texture_cache_v1();
void destroy_card_texture_cache_v1();
