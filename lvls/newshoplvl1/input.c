#include "input.h"

#include <string.h>

enum {
    INPUT_SOURCE_KEYBOARD   = 1 << 0,
    INPUT_SOURCE_CONTROLLER = 1 << 1,
    INPUT_SOURCE_REMOTE     = 1 << 2
};

#define INPUT_REMOTE_WINDOW_ID 0xFFFFFFFFu

enum {
    ARCADE_BUTTON_A = 7,
    ARCADE_BUTTON_B = 11,
    ARCADE_BUTTON_INTERACT = 8,
    ARCADE_BUTTON_ESC = 9,
    ARCADE_BUTTON_JUMP = 10,
    ARCADE_BUTTON_ENTER = 6
};

static const Sint16 INPUT_JOYSTICK_DEADZONE = 16000;

static bool input_is_valid_scancode(SDL_Scancode scancode) {
    return scancode >= 0 && scancode < SDL_NUM_SCANCODES;
}

static void input_set_key_source(
    InputState *input,
    SDL_Scancode scancode,
    uint8_t source,
    bool down
) {
    if (!input_is_valid_scancode(scancode)) {
        return;
    }

    const uint8_t previous_sources = input->key_sources[scancode];
    uint8_t next_sources = previous_sources;

    if (down) {
        next_sources |= source;
    } else {
        next_sources &= (uint8_t)~source;
    }

    input->key_sources[scancode] = next_sources;
    input->keys[scancode] = next_sources != 0;
    if (!input->keys[scancode] || previous_sources != 0 || !down) {
        return;
    }

    input->just_pressed[scancode] = true;
}

static void input_open_first_joystick(InputState *input) {
    const int joystick_count = SDL_NumJoysticks();
    for (int i = 0; i < joystick_count; ++i) {
        SDL_Joystick *joystick = SDL_JoystickOpen(i);
        if (!joystick) {
            continue;
        }
        input->joystick = joystick;
        input->joystick_id = SDL_JoystickInstanceID(joystick);
        input->joystick_available = true;
        SDL_Log("Opened joystick: %s", SDL_JoystickName(joystick));
        return;
    }
}

static void input_close_joystick(InputState *input) {
    if (!input->joystick) {
        input->joystick_available = false;
        input->joystick_id = -1;
        return;
    }

    SDL_JoystickClose(input->joystick);
    input->joystick = NULL;
    input->joystick_available = false;
    input->joystick_id = -1;
}

static void input_update_arcade_controller(InputState *input) {
    if (!input->joystick_available || !input->joystick) {
        input->controller_interact_down = false;
        return;
    }

    SDL_JoystickUpdate();

    const Sint16 axis_x = SDL_JoystickNumAxes(input->joystick) > 0
        ? SDL_JoystickGetAxis(input->joystick, 0)
        : 0;
    const Sint16 axis_y = SDL_JoystickNumAxes(input->joystick) > 1
        ? SDL_JoystickGetAxis(input->joystick, 1)
        : 0;
    const Uint8 hat = SDL_JoystickNumHats(input->joystick) > 0
        ? SDL_JoystickGetHat(input->joystick, 0)
        : SDL_HAT_CENTERED;

    const bool left_down = axis_x <= -INPUT_JOYSTICK_DEADZONE || (hat & SDL_HAT_LEFT) != 0;
    const bool right_down = axis_x >= INPUT_JOYSTICK_DEADZONE || (hat & SDL_HAT_RIGHT) != 0;
    const bool up_down = axis_y <= -INPUT_JOYSTICK_DEADZONE || (hat & SDL_HAT_UP) != 0;
    const bool down_down = axis_y >= INPUT_JOYSTICK_DEADZONE || (hat & SDL_HAT_DOWN) != 0;

    input_set_key_source(input, SDL_SCANCODE_LEFT, INPUT_SOURCE_CONTROLLER, left_down);
    input_set_key_source(input, SDL_SCANCODE_RIGHT, INPUT_SOURCE_CONTROLLER, right_down);
    input_set_key_source(input, SDL_SCANCODE_UP, INPUT_SOURCE_CONTROLLER, up_down);
    input_set_key_source(input, SDL_SCANCODE_DOWN, INPUT_SOURCE_CONTROLLER, down_down);

    const int button_count = SDL_JoystickNumButtons(input->joystick);
    const bool jump_down = ARCADE_BUTTON_JUMP < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_JUMP) != 0;
    const bool interact_down = ARCADE_BUTTON_INTERACT < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_INTERACT) != 0;
    const bool a_down = ARCADE_BUTTON_A < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_A) != 0;
    const bool b_down = ARCADE_BUTTON_B < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_B) != 0;
    const bool enter_down = ARCADE_BUTTON_ENTER < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_ENTER) != 0;
    const bool esc_down = ARCADE_BUTTON_ESC < button_count &&
        SDL_JoystickGetButton(input->joystick, ARCADE_BUTTON_ESC) != 0;

    input_set_key_source(input, SDL_SCANCODE_SPACE, INPUT_SOURCE_CONTROLLER, jump_down);

    input->controller_interact_pressed = interact_down && !input->controller_interact_down;
    input->controller_interact_down = interact_down;

    input_set_key_source(input, SDL_SCANCODE_A, INPUT_SOURCE_CONTROLLER, a_down);
    input_set_key_source(input, SDL_SCANCODE_B, INPUT_SOURCE_CONTROLLER, b_down);
    input_set_key_source(input, SDL_SCANCODE_RETURN, INPUT_SOURCE_CONTROLLER, enter_down || interact_down || a_down);
    input_set_key_source(input, SDL_SCANCODE_ESCAPE, INPUT_SOURCE_CONTROLLER, esc_down || b_down);
}

void input_init(InputState *input) {
    memset(input, 0, sizeof(*input));
    input->joystick_id = -1;

    if ((SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
            SDL_Log("SDL joystick init failed: %s", SDL_GetError());
            return;
        }
    }

    SDL_JoystickEventState(SDL_ENABLE);
    input_open_first_joystick(input);
}

void input_begin_frame(InputState *input) {
    memset(input->just_pressed, 0, sizeof(input->just_pressed));
    input->controller_interact_pressed = false;
    input->mouse_clicked = false;
}

void input_process(InputState *input) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                input->quit_requested = true;
                break;
            case SDL_KEYDOWN:
                if (input_is_valid_scancode(event.key.keysym.scancode)) {
                    const uint8_t source = (event.key.windowID == INPUT_REMOTE_WINDOW_ID)
                        ? INPUT_SOURCE_REMOTE
                        : INPUT_SOURCE_KEYBOARD;
                    input_set_key_source(
                        input,
                        event.key.keysym.scancode,
                        source,
                        true
                    );
                }
                break;
            case SDL_KEYUP:
                if (input_is_valid_scancode(event.key.keysym.scancode)) {
                    const uint8_t source = (event.key.windowID == INPUT_REMOTE_WINDOW_ID)
                        ? INPUT_SOURCE_REMOTE
                        : INPUT_SOURCE_KEYBOARD;
                    input_set_key_source(
                        input,
                        event.key.keysym.scancode,
                        source,
                        false
                    );
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input->mouse_clicked = true;
                    input->mouse_x = event.button.x;
                    input->mouse_y = event.button.y;
                }
                break;
            case SDL_JOYDEVICEADDED:
                if (!input->joystick_available) {
                    input_open_first_joystick(input);
                }
                break;
            case SDL_JOYDEVICEREMOVED:
                if (event.jdevice.which == input->joystick_id) {
                    input_close_joystick(input);
                    input_open_first_joystick(input);
                }
                break;
            default:
                break;
        }
    }

    input_update_arcade_controller(input);
}

bool input_down(const InputState *input, SDL_Scancode scancode) {
    if (!input_is_valid_scancode(scancode)) {
        return false;
    }
    return input->keys[scancode];
}

bool input_pressed(const InputState *input, SDL_Scancode scancode) {
    if (!input_is_valid_scancode(scancode)) {
        return false;
    }
    return input->just_pressed[scancode];
}

bool input_pressed_from_remote(const InputState *input, SDL_Scancode scancode) {
    if (!input_is_valid_scancode(scancode)) {
        return false;
    }
    return input->just_pressed[scancode] &&
        (input->key_sources[scancode] & INPUT_SOURCE_REMOTE) != 0;
}

bool input_pressed_without_remote(const InputState *input, SDL_Scancode scancode) {
    if (!input_is_valid_scancode(scancode)) {
        return false;
    }
    return input->just_pressed[scancode] &&
        (input->key_sources[scancode] & (INPUT_SOURCE_KEYBOARD | INPUT_SOURCE_CONTROLLER)) != 0;
}

void input_copy_without_remote(InputState *dst, const InputState *src) {
    if (!dst || !src) {
        return;
    }

    *dst = *src;
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i) {
        dst->key_sources[i] &= (uint8_t)~INPUT_SOURCE_REMOTE;
        dst->keys[i] = dst->key_sources[i] != 0;
        if (!dst->keys[i]) {
            dst->just_pressed[i] = false;
        }
    }
}

void input_clear_movement_keys(InputState *input) {
    const SDL_Scancode movement_keys[] = {
        SDL_SCANCODE_W,
        SDL_SCANCODE_A,
        SDL_SCANCODE_S,
        SDL_SCANCODE_D,
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT
    };

    const int count = (int)(sizeof(movement_keys) / sizeof(movement_keys[0]));
    for (int i = 0; i < count; ++i) {
        input->keys[movement_keys[i]] = false;
        input->key_sources[movement_keys[i]] = 0;
    }
}

void input_shutdown(InputState *input) {
    input_close_joystick(input);
}
