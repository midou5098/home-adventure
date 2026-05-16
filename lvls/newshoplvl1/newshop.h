#ifndef NEWSHOPLVL1_NEWSHOP_H
#define NEWSHOPLVL1_NEWSHOP_H

#include <SDL2/SDL.h>

#include "../shared/session.h"

#define NEWSHOP_CONTROL_SCHEME_ARROWS 1
#define NEWSHOP_CONTROL_SCHEME_WASD 2

typedef struct {
    int control_scheme;
    int lives;
    int extra_hearts;
    int duo_enabled;
    int online_hosted;
    InteractBind player_interact_bind[2];
    int player_skin_number[2];
    int player_lives[2];
    int player_extra_hearts[2];
    int keys_held;
    int active_player;
    int buy_count_jetpack;
    int buy_count_magnet;
    int buy_count_shoes;
    int player_buy_count_jetpack[2];
    int player_buy_count_magnet[2];
    int player_buy_count_shoes[2];

    /* Purchases made during the current shop visit. */
    int purchased_jetpack;
    int purchased_magnet;
    int purchased_shoes;
    int player_purchased_jetpack[2];
    int player_purchased_magnet[2];
    int player_purchased_shoes[2];
} NewShopState;

/* Runs newshoplvl1 inside an existing SDL window/renderer and returns when exited. */
int runNewShopLevel1(SDL_Window *shared_window, SDL_Renderer *shared_renderer, NewShopState *shop_state);

#endif
