#ifndef ENIGMA_SCENE_H
#define ENIGMA_SCENE_H

#include <SDL2/SDL.h>

int enigma_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer);
void enigma_scene_enter_random(void);
void enigma_scene_leave(void);
void enigma_scene_set_music_volume(int sdl_volume);
void enigma_scene_handle_event(const SDL_Event* e);
void enigma_scene_update(float dt);
void enigma_scene_render(void);
int enigma_scene_consume_return_request(void);
void enigma_scene_cleanup(void);

#endif /* ENIGMA_SCENE_H */
