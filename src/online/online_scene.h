#ifndef ONLINE_SCENE_H
#define ONLINE_SCENE_H

#include <SDL2/SDL.h>

#include "game_session.h"

typedef struct {
    int return_to_main;
} OnlineSceneResult;

int online_scene_init(SDL_Renderer* renderer);
void online_scene_enter(void);
void online_scene_leave(void);
void online_scene_handle_event(const SDL_Event* e, OnlineSceneResult* result);
void online_scene_update(float delta);
void online_scene_render(void);
int online_scene_consume_return_request(void);
int online_scene_consume_host_start_request(GameSelection* out_selection);
const char* online_scene_local_player_name(void);
void online_scene_cleanup(void);

#endif /* ONLINE_SCENE_H */
