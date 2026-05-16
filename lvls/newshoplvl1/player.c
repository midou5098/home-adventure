#include "player.h"

#include <math.h>
#include <stdlib.h>

void player_init(Player *player, float x, float y) {
    player->x = x;
    player->y = y;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->w = PLAYER_W;
    player->h = PLAYER_H;
    player->facing = FACING_DOWN;
    player->is_walking = false;
    player->frame = 0;
    player->elapsed_ms = 0.0f;
    player->active_sheet = NULL;
    player->active_frame_ms = 0.0f;
}

void player_update_facing(Player *player, int horizontal, int vertical) {
    if (horizontal == 0 && vertical == 0) {
        return;
    }

    if (abs(horizontal) >= abs(vertical) && horizontal != 0) {
        player->facing = horizontal > 0 ? FACING_RIGHT : FACING_LEFT;
        return;
    }

    if (vertical != 0) {
        player->facing = vertical > 0 ? FACING_DOWN : FACING_UP;
    }
}

static void pick_animation(
    const Player *player,
    const PlayerAssets *assets,
    const SpriteSheet **out_sheet,
    float *out_frame_ms
) {
    if (player->is_walking) {
        switch (player->facing) {
            case FACING_DOWN:
                *out_sheet = &assets->walk_down;
                *out_frame_ms = 70.0f;
                return;
            case FACING_UP:
                *out_sheet = &assets->walk_up;
                *out_frame_ms = 70.0f;
                return;
            case FACING_LEFT:
            case FACING_RIGHT:
                *out_sheet = &assets->walk_left;
                *out_frame_ms = 70.0f;
                return;
            default:
                break;
        }
    }

    switch (player->facing) {
        case FACING_DOWN:
            *out_sheet = &assets->idle_down;
            *out_frame_ms = 120.0f;
            return;
        case FACING_UP:
            *out_sheet = &assets->idle_up;
            *out_frame_ms = 120.0f;
            return;
        case FACING_LEFT:
        case FACING_RIGHT:
        default:
            *out_sheet = &assets->idle_side;
            *out_frame_ms = 120.0f;
            return;
    }
}

void player_update_animation(Player *player, const PlayerAssets *assets, float dt_ms) {
    const SpriteSheet *next_sheet = NULL;
    float frame_ms = 120.0f;
    pick_animation(player, assets, &next_sheet, &frame_ms);

    if (!next_sheet || !next_sheet->base.loaded || next_sheet->frame_count <= 0) {
        player->active_sheet = NULL;
        player->frame = 0;
        player->elapsed_ms = 0.0f;
        return;
    }

    if (player->active_sheet != next_sheet || fabsf(player->active_frame_ms - frame_ms) > 0.01f) {
        player->active_sheet = next_sheet;
        player->active_frame_ms = frame_ms;
        player->frame = 0;
        player->elapsed_ms = 0.0f;
    }

    player->elapsed_ms += dt_ms;
    while (player->elapsed_ms >= frame_ms) {
        player->elapsed_ms -= frame_ms;
        player->frame = (player->frame + 1) % next_sheet->frame_count;
    }
}

Rect player_rect(const Player *player) {
    return (Rect){
        .x = player->x,
        .y = player->y,
        .w = (float)player->w,
        .h = (float)player->h
    };
}
