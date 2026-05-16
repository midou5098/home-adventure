#ifndef TOP_SCORES_SCENE_H
#define TOP_SCORES_SCENE_H

#include <SDL2/SDL.h>

int top_scores_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer);
void top_scores_scene_enter(void);
void top_scores_scene_leave(void);
void top_scores_scene_set_music_volume(int sdl_volume);
void top_scores_scene_handle_event(const SDL_Event* e);
void top_scores_scene_update(float dt);
void top_scores_scene_render(void);
int top_scores_scene_consume_return_request(void);
void top_scores_scene_cleanup(void);

#endif /* TOP_SCORES_SCENE_H */
