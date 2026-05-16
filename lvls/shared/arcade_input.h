#ifndef SHARED_ARCADE_INPUT_H
#define SHARED_ARCADE_INPUT_H

#include <SDL.h>
#include <stdbool.h>

void arcade_input_init(void);
void arcade_input_shutdown(void);
void arcade_input_begin_frame(void);
void arcade_input_handle_event(const SDL_Event *event);

bool arcade_input_key_down(SDL_Keycode key);
bool arcade_input_key_pressed(SDL_Keycode key);
bool arcade_input_key_released(SDL_Keycode key);
bool arcade_input_scancode_down(SDL_Scancode scancode);

#endif
