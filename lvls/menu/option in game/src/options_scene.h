#ifndef OPTIONS_SCENE_H
#define OPTIONS_SCENE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

typedef struct {
    int return_to_main;
    int quit_to_menu;
} OptionsSceneResult;

int options_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer);
void options_scene_set_audio_enabled(int enabled);
void options_scene_enter(void);
void options_scene_leave(void);
void options_scene_handle_event(const SDL_Event* e, OptionsSceneResult* result);
void options_scene_update(float delta);
void options_scene_render(void);
void options_scene_cleanup(void);

int options_scene_get_brightness(void);
int options_scene_get_music_volume_sdl(void);
void options_scene_render_global_brightness_overlay(SDL_Renderer* target_renderer);

#endif /* OPTIONS_SCENE_H */
