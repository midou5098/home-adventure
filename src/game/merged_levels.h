#ifndef MERGED_LEVELS_H
#define MERGED_LEVELS_H

#include <SDL2/SDL.h>
#include "game_session.h"

int merged_levels_run(SDL_Window* window, SDL_Renderer* renderer, const GameSelection* selection);
int merged_levels_run_from_level(SDL_Window* window,
                                 SDL_Renderer* renderer,
                                 const GameSelection* selection,
                                 int forced_start_level);

#endif /* MERGED_LEVELS_H */
