#include "arcade_input.h"

#include <string.h>

enum {
    ARCADE_BUTTON_ENTER = 6,
    ARCADE_BUTTON_A = 7,
    ARCADE_BUTTON_INTERACT = 8,
    ARCADE_BUTTON_ESC = 9,
    ARCADE_BUTTON_JUMP = 10,
    ARCADE_BUTTON_B = 11
};

enum {
    GAMEPAD_BUTTON_SOUTH = 0,
    GAMEPAD_BUTTON_EAST = 1,
    GAMEPAD_BUTTON_WEST = 2,
    GAMEPAD_BUTTON_START = 7
};

typedef enum {
    ARCADE_KEY_LEFT = 0,
    ARCADE_KEY_RIGHT,
    ARCADE_KEY_UP,
    ARCADE_KEY_DOWN,
    ARCADE_KEY_SPACE,
    ARCADE_KEY_0,
    ARCADE_KEY_A,
    ARCADE_KEY_B,
    ARCADE_KEY_RETURN,
    ARCADE_KEY_ESCAPE,
    ARCADE_KEY_COUNT
} ArcadeLogicalKey;

typedef struct {
    SDL_Keycode keycode;
    SDL_Scancode scancode;
} ArcadeKeyMap;

typedef struct {
    bool initialized;
    SDL_Joystick *joystick;
    SDL_JoystickID joystick_id;
    bool current[ARCADE_KEY_COUNT];
    bool pressed[ARCADE_KEY_COUNT];
    bool released[ARCADE_KEY_COUNT];
} ArcadeInputState;

static const Sint16 ARCADE_DEADZONE = 16000;

static const ArcadeKeyMap kArcadeKeyMap[ARCADE_KEY_COUNT] = {
    [ARCADE_KEY_LEFT] = { SDLK_LEFT, SDL_SCANCODE_LEFT },
    [ARCADE_KEY_RIGHT] = { SDLK_RIGHT, SDL_SCANCODE_RIGHT },
    [ARCADE_KEY_UP] = { SDLK_UP, SDL_SCANCODE_UP },
    [ARCADE_KEY_DOWN] = { SDLK_DOWN, SDL_SCANCODE_DOWN },
    [ARCADE_KEY_SPACE] = { SDLK_SPACE, SDL_SCANCODE_SPACE },
    [ARCADE_KEY_0] = { SDLK_0, SDL_SCANCODE_0 },
    [ARCADE_KEY_A] = { SDLK_a, SDL_SCANCODE_A },
    [ARCADE_KEY_B] = { SDLK_b, SDL_SCANCODE_B },
    [ARCADE_KEY_RETURN] = { SDLK_RETURN, SDL_SCANCODE_RETURN },
    [ARCADE_KEY_ESCAPE] = { SDLK_ESCAPE, SDL_SCANCODE_ESCAPE }
};

static ArcadeInputState g_arcade_input;

static void arcade_input_push_key_event(ArcadeLogicalKey key, bool down)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.type = event.type;
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.sym = kArcadeKeyMap[key].keycode;
    event.key.keysym.scancode = kArcadeKeyMap[key].scancode;
    event.key.keysym.mod = KMOD_NONE;
    SDL_PushEvent(&event);
}

static void arcade_input_close_joystick(void)
{
    if (!g_arcade_input.joystick) {
        g_arcade_input.joystick_id = -1;
        return;
    }

    SDL_JoystickClose(g_arcade_input.joystick);
    g_arcade_input.joystick = NULL;
    g_arcade_input.joystick_id = -1;
}

static void arcade_input_open_first_joystick(void)
{
    int joystick_count;
    int i;

    if (g_arcade_input.joystick) {
        return;
    }

    joystick_count = SDL_NumJoysticks();
    for (i = 0; i < joystick_count; ++i) {
        SDL_Joystick *joystick = SDL_JoystickOpen(i);
        if (!joystick) {
            continue;
        }

        g_arcade_input.joystick = joystick;
        g_arcade_input.joystick_id = SDL_JoystickInstanceID(joystick);
        SDL_Log("Opened arcade joystick: %s", SDL_JoystickName(joystick));
        return;
    }
}

static int arcade_input_index_for_keycode(SDL_Keycode key)
{
    int i;

    for (i = 0; i < ARCADE_KEY_COUNT; ++i) {
        if (kArcadeKeyMap[i].keycode == key) {
            return i;
        }
    }

    return -1;
}

static int arcade_input_index_for_scancode(SDL_Scancode scancode)
{
    int i;

    for (i = 0; i < ARCADE_KEY_COUNT; ++i) {
        if (kArcadeKeyMap[i].scancode == scancode) {
            return i;
        }
    }

    return -1;
}

static void arcade_input_set_state(ArcadeLogicalKey key, bool down)
{
    bool previous;

    previous = g_arcade_input.current[key];
    g_arcade_input.current[key] = down;
    if (down && !previous) {
        g_arcade_input.pressed[key] = true;
        arcade_input_push_key_event(key, true);
    } else if (!down && previous) {
        g_arcade_input.released[key] = true;
        arcade_input_push_key_event(key, false);
    }
}

static bool arcade_input_button_down(int button_count, int button)
{
    return button >= 0 &&
           button < button_count &&
           SDL_JoystickGetButton(g_arcade_input.joystick, button) != 0;
}

static void arcade_input_poll_joystick(void)
{
    int button_count;
    Sint16 axis_x;
    Sint16 axis_y;
    Uint8 hat;

    if (!g_arcade_input.joystick) {
        return;
    }

    SDL_JoystickUpdate();

    axis_x = SDL_JoystickNumAxes(g_arcade_input.joystick) > 0
        ? SDL_JoystickGetAxis(g_arcade_input.joystick, 0)
        : 0;
    axis_y = SDL_JoystickNumAxes(g_arcade_input.joystick) > 1
        ? SDL_JoystickGetAxis(g_arcade_input.joystick, 1)
        : 0;
    hat = SDL_JoystickNumHats(g_arcade_input.joystick) > 0
        ? SDL_JoystickGetHat(g_arcade_input.joystick, 0)
        : SDL_HAT_CENTERED;

    arcade_input_set_state(
        ARCADE_KEY_LEFT,
        axis_x <= -ARCADE_DEADZONE || (hat & SDL_HAT_LEFT) != 0
    );
    arcade_input_set_state(
        ARCADE_KEY_RIGHT,
        axis_x >= ARCADE_DEADZONE || (hat & SDL_HAT_RIGHT) != 0
    );
    arcade_input_set_state(
        ARCADE_KEY_UP,
        axis_y <= -ARCADE_DEADZONE || (hat & SDL_HAT_UP) != 0
    );
    arcade_input_set_state(
        ARCADE_KEY_DOWN,
        axis_y >= ARCADE_DEADZONE || (hat & SDL_HAT_DOWN) != 0
    );

    button_count = SDL_JoystickNumButtons(g_arcade_input.joystick);
    arcade_input_set_state(
        ARCADE_KEY_RETURN,
        arcade_input_button_down(button_count, ARCADE_BUTTON_ENTER) ||
        arcade_input_button_down(button_count, GAMEPAD_BUTTON_START)
    );
    arcade_input_set_state(
        ARCADE_KEY_ESCAPE,
        arcade_input_button_down(button_count, ARCADE_BUTTON_ESC) ||
        arcade_input_button_down(button_count, GAMEPAD_BUTTON_EAST)
    );
    arcade_input_set_state(
        ARCADE_KEY_SPACE,
        arcade_input_button_down(button_count, ARCADE_BUTTON_JUMP) ||
        arcade_input_button_down(button_count, GAMEPAD_BUTTON_SOUTH)
    );
    arcade_input_set_state(
        ARCADE_KEY_0,
        arcade_input_button_down(button_count, ARCADE_BUTTON_INTERACT) ||
        arcade_input_button_down(button_count, GAMEPAD_BUTTON_WEST)
    );
    arcade_input_set_state(
        ARCADE_KEY_A,
        arcade_input_button_down(button_count, ARCADE_BUTTON_A)
    );
    arcade_input_set_state(
        ARCADE_KEY_B,
        arcade_input_button_down(button_count, ARCADE_BUTTON_B)
    );
}

void arcade_input_init(void)
{
    if (g_arcade_input.initialized) {
        return;
    }

    memset(&g_arcade_input, 0, sizeof(g_arcade_input));
    g_arcade_input.joystick_id = -1;

    if ((SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
            SDL_Log("SDL joystick init failed: %s", SDL_GetError());
            return;
        }
    }

    SDL_JoystickEventState(SDL_ENABLE);
    arcade_input_open_first_joystick();
    g_arcade_input.initialized = true;
}

void arcade_input_shutdown(void)
{
    arcade_input_close_joystick();
    memset(&g_arcade_input, 0, sizeof(g_arcade_input));
    g_arcade_input.joystick_id = -1;
}

void arcade_input_begin_frame(void)
{
    memset(g_arcade_input.pressed, 0, sizeof(g_arcade_input.pressed));
    memset(g_arcade_input.released, 0, sizeof(g_arcade_input.released));

    if (!g_arcade_input.initialized) {
        arcade_input_init();
    }

    if (!g_arcade_input.joystick) {
        arcade_input_open_first_joystick();
    }

    arcade_input_poll_joystick();
}

void arcade_input_handle_event(const SDL_Event *event)
{
    if (!event) {
        return;
    }

    if (event->type == SDL_JOYDEVICEADDED) {
        arcade_input_open_first_joystick();
    } else if (event->type == SDL_JOYDEVICEREMOVED &&
               event->jdevice.which == g_arcade_input.joystick_id) {
        arcade_input_close_joystick();
        arcade_input_open_first_joystick();
    }
}

bool arcade_input_key_down(SDL_Keycode key)
{
    int index = arcade_input_index_for_keycode(key);
    return index >= 0 ? g_arcade_input.current[index] : false;
}

bool arcade_input_key_pressed(SDL_Keycode key)
{
    int index = arcade_input_index_for_keycode(key);
    return index >= 0 ? g_arcade_input.pressed[index] : false;
}

bool arcade_input_key_released(SDL_Keycode key)
{
    int index = arcade_input_index_for_keycode(key);
    return index >= 0 ? g_arcade_input.released[index] : false;
}

bool arcade_input_scancode_down(SDL_Scancode scancode)
{
    int index = arcade_input_index_for_scancode(scancode);
    return index >= 0 ? g_arcade_input.current[index] : false;
}
