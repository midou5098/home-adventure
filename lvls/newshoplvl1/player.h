#ifndef SDLVERSIONSHOP_PLAYER_H
#define SDLVERSIONSHOP_PLAYER_H

#include "game.h"

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    int w;
    int h;
    Facing facing;
    bool is_walking;

    int frame;
    float elapsed_ms;
    const SpriteSheet *active_sheet;
    float active_frame_ms;
} Player;

typedef struct {
    SpriteSheet idle_down;
    SpriteSheet idle_up;
    SpriteSheet idle_side;
    SpriteSheet walk_down;
    SpriteSheet walk_up;
    SpriteSheet walk_left;
} PlayerAssets;

void player_init(Player *player, float x, float y);
void player_update_facing(Player *player, int horizontal, int vertical);
void player_update_animation(Player *player, const PlayerAssets *assets, float dt_ms);
Rect player_rect(const Player *player);

#endif
