#ifndef SDLVERSIONSHOP_INPUT_H
#define SDLVERSIONSHOP_INPUT_H

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool keys[SDL_NUM_SCANCODES];
    bool just_pressed[SDL_NUM_SCANCODES];
    uint8_t key_sources[SDL_NUM_SCANCODES];
    bool controller_interact_down;
    bool controller_interact_pressed;
    bool mouse_clicked;
    int mouse_x;
    int mouse_y;
    bool quit_requested;
    SDL_Joystick *joystick;
    SDL_JoystickID joystick_id;
    bool joystick_available;
} InputState;

void input_init(InputState *input);
void input_begin_frame(InputState *input);
void input_process(InputState *input);
bool input_down(const InputState *input, SDL_Scancode scancode);
bool input_pressed(const InputState *input, SDL_Scancode scancode);
bool input_pressed_from_remote(const InputState *input, SDL_Scancode scancode);
bool input_pressed_without_remote(const InputState *input, SDL_Scancode scancode);
void input_copy_without_remote(InputState *dst, const InputState *src);
void input_clear_movement_keys(InputState *input);
void input_shutdown(InputState *input);

#endif
