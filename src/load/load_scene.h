#ifndef LOAD_SCENE_H
#define LOAD_SCENE_H

#include <SDL2/SDL.h>

typedef struct {
    int return_to_main;
} LoadSceneResult;

int load_scene_init(SDL_Renderer* shared_renderer);
void load_scene_enter(void);
void load_scene_leave(void);
void load_scene_set_music_volume(int sdl_volume);
void load_scene_handle_event(const SDL_Event* ev, LoadSceneResult* result);
void load_scene_update(float dt);
void load_scene_render(void);
int load_scene_consume_return_request(void);
int load_scene_consume_choose_request(int* selected_mode, int* start_choice, int* save_enabled);
int load_scene_consume_online_request(void);
void load_scene_cleanup(void);

#endif /* LOAD_SCENE_H */
