#ifndef CHOOSE_SCENE_H
#define CHOOSE_SCENE_H

#include <SDL2/SDL.h>
#include "game_session.h"

typedef struct {
    int return_to_main;
} ChooseSceneResult;

int choose_scene_init(SDL_Renderer* shared_renderer);
void choose_scene_enter(int mode, int start_choice, int save_enabled);
void choose_scene_leave(void);
void choose_scene_set_music_volume(int sdl_volume);
void choose_scene_handle_event(const SDL_Event* e, ChooseSceneResult* result);
void choose_scene_update(float delta);
void choose_scene_render(void);
int choose_scene_consume_return_request(void);
int choose_scene_consume_start_game_request(GameSelection* out_selection);
void choose_scene_debug_force_start(const char* name);
void choose_scene_cleanup(void);

#endif /* CHOOSE_SCENE_H */
