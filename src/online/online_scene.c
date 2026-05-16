#include "online_scene.h"

#include "asset_paths.h"
#include "mainmenu_headers.h"
#include "online_client.h"
#include "options_scene.h"
#include "ui_shared.h"
#include "../../lvls/shared/arcade_input.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ONLINE_JOIN_CODE_MAX 6

typedef enum {
    ONLINE_STATE_MENU = 0,
    ONLINE_STATE_JOIN,
    ONLINE_STATE_HOST_LOBBY,
    ONLINE_STATE_CLIENT_LOBBY,
    ONLINE_STATE_REMOTE_PLAY
} OnlineSceneState;

typedef struct {
    int id;
    SDL_Rect rect;
    char label[32];
} OnlineButton;

typedef struct {
    SDL_Renderer* renderer;
    SDL_Texture* background;
    SDL_Texture* panel_texture;
    TTF_Font* title_font;
    TTF_Font* body_font;
    TTF_Font* small_font;
    TTF_Font* button_font;
    SDL_Texture* remote_frame_texture;
    int remote_texture_w;
    int remote_texture_h;
    uint32_t remote_frame_sequence;
    int initialized;
    int active;
    int return_requested;
    int host_start_requested;
    int hovered_button;
    int selected_button;
    OnlineSceneState state;
    char local_name[24];
    char server_host[64];
    int server_port;
    char join_code_input[ONLINE_JOIN_CODE_MAX + 1];
    int local_skin;
    int local_control;
    int remote_pause_active;
} OnlineSceneContext;

enum {
    ONLINE_BUTTON_HOST = 1,
    ONLINE_BUTTON_JOIN,
    ONLINE_BUTTON_BACK,
    ONLINE_BUTTON_CONNECT,
    ONLINE_BUTTON_SKIN,
    ONLINE_BUTTON_CONTROL,
    ONLINE_BUTTON_START,
    ONLINE_BUTTON_KICK
};

static OnlineSceneContext g_online = {0};

static void online_copy_text(char* dest, size_t dest_size, const char* src)
{
    size_t i = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    while (src[i] != '\0' && i + 1 < dest_size) {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

static TTF_Font* online_open_font(int size)
{
    const char* candidates[] = {
        ASSET_BUTTON_FONT,
        ASSET_MAIN_MENU_FONT_TEXT,
        ASSET_MAIN_MENU_FONT_OPTIONS,
        ASSET_CHOOSE_FONT
    };

    TTF_Font* font = ui_open_arial_font(size, 0);
    if (font) return font;
    return ui_open_font_from_candidates(candidates,
                                        (int)(sizeof(candidates) / sizeof(candidates[0])),
                                        size,
                                        0);
}

static void online_draw_text_center(TTF_Font* font, const char* text, int x, int y, SDL_Color color)
{
    if (!g_online.renderer || !font || !text || !text[0]) return;
    ui_draw_text_center(g_online.renderer, font, text, x, y, color);
}

static void online_draw_text_left(TTF_Font* font, const char* text, int x, int y, SDL_Color color)
{
    if (!g_online.renderer || !font || !text || !text[0]) return;
    ui_draw_text_left(g_online.renderer, font, text, x, y, color);
}

static void online_draw_panel(const SDL_Rect* rect)
{
    SDL_Color fill = {18, 36, 58, 214};
    SDL_Color border = {246, 211, 119, 255};
    SDL_Color inner = {41, 68, 100, 120};

    if (!g_online.renderer || !rect) return;

    if (g_online.panel_texture) {
        SDL_RenderCopy(g_online.renderer, g_online.panel_texture, NULL, rect);
    } else {
        ui_fill_rect(g_online.renderer, rect, fill);
    }
    ui_draw_rect(g_online.renderer, rect, border);

    {
        SDL_Rect inner_rect = {rect->x + 8, rect->y + 8, rect->w - 16, rect->h - 16};
        ui_fill_rect(g_online.renderer, &inner_rect, inner);
    }
}

static int online_point_in_rect(const SDL_Rect* rect, int x, int y)
{
    if (!rect) return 0;
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static int online_add_button(OnlineButton* buttons,
                             int max_buttons,
                             int count,
                             int id,
                             SDL_Rect rect,
                             const char* label)
{
    if (!buttons || count < 0 || count >= max_buttons) return count;
    buttons[count].id = id;
    buttons[count].rect = rect;
    online_copy_text(buttons[count].label, sizeof(buttons[count].label), label);
    return count + 1;
}

static int online_build_buttons(OnlineButton* buttons, int max_buttons, OnlineLobbyState* lobby)
{
    int count = 0;

    if (!buttons || max_buttons <= 0) return 0;
    if (lobby) online_client_copy_lobby_state(lobby);

    if (g_online.state == ONLINE_STATE_MENU) {
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_HOST,
                                  (SDL_Rect){460, 250, 360, 58}, "HOST GAME");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_JOIN,
                                  (SDL_Rect){460, 328, 360, 58}, "JOIN GAME");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_BACK,
                                  (SDL_Rect){460, 406, 360, 58}, "BACK");
    } else if (g_online.state == ONLINE_STATE_JOIN) {
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_CONNECT,
                                  (SDL_Rect){430, 438, 190, 54}, "CONNECT");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_BACK,
                                  (SDL_Rect){660, 438, 190, 54}, "BACK");
    } else if (g_online.state == ONLINE_STATE_HOST_LOBBY) {
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_SKIN,
                                  (SDL_Rect){176, 500, 250, 54}, "CHANGE SKIN");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_CONTROL,
                                  (SDL_Rect){176, 570, 250, 54}, "CHANGE CONTROLS");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_START,
                                  (SDL_Rect){478, 570, 324, 60}, "START GAME");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_KICK,
                                  (SDL_Rect){856, 500, 250, 54}, "KICK PLAYER");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_BACK,
                                  (SDL_Rect){856, 570, 250, 54}, "BACK");
    } else if (g_online.state == ONLINE_STATE_CLIENT_LOBBY) {
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_SKIN,
                                  (SDL_Rect){176, 500, 250, 54}, "CHANGE SKIN");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_CONTROL,
                                  (SDL_Rect){176, 570, 250, 54}, "CHANGE CONTROLS");
        count = online_add_button(buttons, max_buttons, count, ONLINE_BUTTON_BACK,
                                  (SDL_Rect){856, 570, 250, 54}, "BACK");
    }

    return count;
}

static int online_button_at_point(const OnlineButton* buttons, int count, int x, int y)
{
    if (!buttons) return -1;
    for (int i = 0; i < count; ++i) {
        if (online_point_in_rect(&buttons[i].rect, x, y)) return i;
    }
    return -1;
}

static void online_start_join_text_input(void)
{
    SDL_StartTextInput();
}

static void online_stop_text_input(void)
{
    SDL_StopTextInput();
}

static void online_set_state(OnlineSceneState state)
{
    g_online.state = state;
    g_online.hovered_button = -1;
    g_online.selected_button = 0;

    if (state == ONLINE_STATE_JOIN) {
        online_start_join_text_input();
    } else {
        online_stop_text_input();
    }
}

static void online_open_remote_pause(void)
{
    if (g_online.remote_pause_active) return;
    online_client_send_input_mask(0);
    options_scene_set_audio_enabled(0);
    options_scene_enter();
    g_online.remote_pause_active = 1;
}

static void online_close_remote_pause(void)
{
    if (!g_online.remote_pause_active) return;
    options_scene_leave();
    options_scene_set_audio_enabled(1);
    g_online.remote_pause_active = 0;
}

static void online_request_remote_pause(int paused)
{
    if (paused) {
        online_open_remote_pause();
    }
    online_client_send_pause_state(paused ? 1 : 0);
}

static int online_other_skin(int skin)
{
    return (skin == 1) ? 2 : 1;
}

static void online_sync_local_skin_from_lobby(const OnlineLobbyState* lobby)
{
    if (!lobby) return;
    if (!online_client_is_connected()) return;

    if (online_client_is_host()) {
        if (lobby->host_skin >= 1 && lobby->host_skin <= 2) {
            g_online.local_skin = lobby->host_skin;
        }
    } else if (lobby->client_skin >= 1 && lobby->client_skin <= 2) {
        g_online.local_skin = lobby->client_skin;
    }
}

static void online_cycle_local_skin(void)
{
    g_online.local_skin = (g_online.local_skin == 2) ? 1 : 2;
    online_client_set_profile(g_online.local_name, g_online.local_skin, g_online.local_control);
}

static void online_cycle_local_control(void)
{
    g_online.local_control = (g_online.local_control == 2) ? 1 : 2;
    online_client_set_profile(g_online.local_name, g_online.local_skin, g_online.local_control);
}

static const char* online_control_name(int control)
{
    return (control == 1) ? "WASD" : "ARROWS";
}

static void online_activate_button(int button_id)
{
    OnlineLobbyState lobby = {0};

    online_client_copy_lobby_state(&lobby);

    switch (button_id) {
        case ONLINE_BUTTON_HOST:
            g_online.local_skin = 1;
            g_online.local_control = 2;
            if (online_client_connect_host(g_online.server_host, g_online.server_port, g_online.local_name)) {
                online_client_set_profile(g_online.local_name, g_online.local_skin, g_online.local_control);
                online_set_state(ONLINE_STATE_HOST_LOBBY);
            }
            break;
        case ONLINE_BUTTON_JOIN:
            if (g_online.local_skin < 1 || g_online.local_skin > 2) g_online.local_skin = 2;
            if (g_online.local_control < 1 || g_online.local_control > 2) g_online.local_control = 1;
            g_online.join_code_input[0] = '\0';
            online_set_state(ONLINE_STATE_JOIN);
            break;
        case ONLINE_BUTTON_BACK:
            if (g_online.state == ONLINE_STATE_JOIN) {
                online_set_state(ONLINE_STATE_MENU);
            } else {
                online_client_disconnect();
                g_online.return_requested = 1;
            }
            break;
        case ONLINE_BUTTON_CONNECT:
            if (g_online.join_code_input[0] != '\0' &&
                online_client_connect_join(g_online.server_host, g_online.server_port, g_online.join_code_input, g_online.local_name)) {
                online_client_set_profile(g_online.local_name, g_online.local_skin, g_online.local_control);
                online_set_state(ONLINE_STATE_CLIENT_LOBBY);
            }
            break;
        case ONLINE_BUTTON_SKIN:
            online_cycle_local_skin();
            break;
        case ONLINE_BUTTON_CONTROL:
            online_cycle_local_control();
            break;
        case ONLINE_BUTTON_START:
            if (lobby.remote_connected) {
                online_client_start_game();
                g_online.host_start_requested = 1;
            }
            break;
        case ONLINE_BUTTON_KICK:
            if (lobby.remote_connected) {
                online_client_kick_client();
            }
            break;
        default:
            break;
    }
}

static void online_handle_join_text(const SDL_Event* e)
{
    if (!e) return;

    if (e->type == SDL_TEXTINPUT) {
        size_t current_len = strlen(g_online.join_code_input);
        for (size_t i = 0; e->text.text[i] != '\0' && current_len < ONLINE_JOIN_CODE_MAX; ++i) {
            unsigned char c = (unsigned char)e->text.text[i];
            if (!isalnum(c)) continue;
            g_online.join_code_input[current_len++] = (char)toupper(c);
            g_online.join_code_input[current_len] = '\0';
        }
    } else if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_BACKSPACE) {
        size_t len = strlen(g_online.join_code_input);
        if (len > 0) g_online.join_code_input[len - 1] = '\0';
    }
}

static void online_update_remote_texture(void)
{
    OnlineRemoteFrame frame = {0};

    if (!online_client_latest_frame(&frame) || !frame.available || !frame.pixels) return;
    if (frame.sequence == g_online.remote_frame_sequence) return;

    if (!g_online.remote_frame_texture ||
        g_online.remote_texture_w != frame.width ||
        g_online.remote_texture_h != frame.height) {
        if (g_online.remote_frame_texture) SDL_DestroyTexture(g_online.remote_frame_texture);
        g_online.remote_frame_texture = SDL_CreateTexture(g_online.renderer,
                                                          SDL_PIXELFORMAT_ARGB8888,
                                                          SDL_TEXTUREACCESS_STREAMING,
                                                          frame.width,
                                                          frame.height);
#if SDL_VERSION_ATLEAST(2,0,12)
        if (g_online.remote_frame_texture) {
            SDL_SetTextureScaleMode(g_online.remote_frame_texture, SDL_ScaleModeLinear);
        }
#endif
        g_online.remote_texture_w = frame.width;
        g_online.remote_texture_h = frame.height;
    }

    if (g_online.remote_frame_texture) {
        SDL_UpdateTexture(g_online.remote_frame_texture, NULL, frame.pixels, frame.pitch);
        g_online.remote_frame_sequence = frame.sequence;
    }
}

static uint32_t online_collect_remote_input_mask(void)
{
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    uint32_t mask = 0;

    if (!keys) return 0;

    if (g_online.local_control == 2) {
        if (keys[SDL_SCANCODE_LEFT] || arcade_input_scancode_down(SDL_SCANCODE_LEFT)) mask |= ONLINE_INPUT_A;
        if (keys[SDL_SCANCODE_RIGHT] || arcade_input_scancode_down(SDL_SCANCODE_RIGHT)) mask |= ONLINE_INPUT_D;
        if (keys[SDL_SCANCODE_UP] || arcade_input_scancode_down(SDL_SCANCODE_UP)) mask |= ONLINE_INPUT_W;
        if (keys[SDL_SCANCODE_DOWN] || arcade_input_scancode_down(SDL_SCANCODE_DOWN)) mask |= ONLINE_INPUT_S;
        if (keys[SDL_SCANCODE_0] || keys[SDL_SCANCODE_KP_0] || arcade_input_scancode_down(SDL_SCANCODE_0)) mask |= ONLINE_INPUT_0;
        if (keys[SDL_SCANCODE_SPACE] || arcade_input_scancode_down(SDL_SCANCODE_SPACE)) mask |= ONLINE_INPUT_SPACE;
    } else {
        if (keys[SDL_SCANCODE_A] || arcade_input_scancode_down(SDL_SCANCODE_LEFT)) mask |= ONLINE_INPUT_A;
        if (keys[SDL_SCANCODE_D] || arcade_input_scancode_down(SDL_SCANCODE_RIGHT)) mask |= ONLINE_INPUT_D;
        if (keys[SDL_SCANCODE_W] || arcade_input_scancode_down(SDL_SCANCODE_UP)) mask |= ONLINE_INPUT_W;
        if (keys[SDL_SCANCODE_S] || arcade_input_scancode_down(SDL_SCANCODE_DOWN)) mask |= ONLINE_INPUT_S;
        if (keys[SDL_SCANCODE_SPACE] || arcade_input_scancode_down(SDL_SCANCODE_SPACE)) mask |= ONLINE_INPUT_SPACE;
        if (keys[SDL_SCANCODE_F] || arcade_input_scancode_down(SDL_SCANCODE_0)) mask |= ONLINE_INPUT_F;
    }

    return mask;
}

int online_scene_init(SDL_Renderer* renderer)
{
    if (!renderer) return 0;
    if (g_online.initialized) return 1;

    memset(&g_online, 0, sizeof(g_online));
    g_online.renderer = renderer;
    g_online.background = ui_load_texture(renderer, ASSET_CHOOSE_BACKGROUND);
    if (!g_online.background) {
        g_online.background = ui_load_texture(renderer, ASSET_LOAD_BG);
    }
    g_online.panel_texture = ui_load_texture(renderer, ASSET_CHOOSE_UI_CONTAINER);
    g_online.title_font = online_open_font(42);
    g_online.body_font = online_open_font(26);
    g_online.small_font = online_open_font(20);
    g_online.button_font = online_open_font(26);
    online_client_default_server(g_online.server_host, sizeof(g_online.server_host), &g_online.server_port);
    online_client_default_player_name(g_online.local_name, sizeof(g_online.local_name));
    g_online.local_skin = 1;
    g_online.local_control = 2;
    g_online.initialized = 1;
    return 1;
}

void online_scene_enter(void)
{
    if (!g_online.initialized) return;

    g_online.active = 1;
    g_online.return_requested = 0;
    g_online.host_start_requested = 0;
    g_online.hovered_button = -1;
    g_online.selected_button = 0;
    g_online.remote_frame_sequence = 0;
    g_online.remote_pause_active = 0;
    g_online.join_code_input[0] = '\0';
    if (g_online.remote_frame_texture) {
        SDL_DestroyTexture(g_online.remote_frame_texture);
        g_online.remote_frame_texture = NULL;
        g_online.remote_texture_w = 0;
        g_online.remote_texture_h = 0;
    }
    online_client_disconnect();
    online_set_state(ONLINE_STATE_MENU);
}

void online_scene_leave(void)
{
    if (!g_online.initialized) return;
    g_online.active = 0;
    g_online.host_start_requested = 0;
    online_close_remote_pause();
    online_stop_text_input();
    online_client_disconnect();
}

void online_scene_handle_event(const SDL_Event* e, OnlineSceneResult* result)
{
    OnlineButton buttons[8];
    int button_count = 0;
    int clicked = -1;

    if (result) result->return_to_main = 0;
    if (!g_online.initialized || !g_online.active || !e) return;

    button_count = online_build_buttons(buttons, 8, NULL);

    if (g_online.state == ONLINE_STATE_REMOTE_PLAY && g_online.remote_pause_active) {
        OptionsSceneResult pause_result = {0};
        options_scene_handle_event(e, &pause_result);
        if (pause_result.quit_to_menu) {
            online_close_remote_pause();
            online_client_disconnect();
            g_online.return_requested = 1;
            if (result) result->return_to_main = 1;
        } else if (pause_result.return_to_main) {
            online_request_remote_pause(0);
        }
        return;
    }

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        if (g_online.state == ONLINE_STATE_REMOTE_PLAY) {
            online_request_remote_pause(1);
            return;
        }
        if (g_online.state == ONLINE_STATE_JOIN) {
            online_set_state(ONLINE_STATE_MENU);
            return;
        }
        online_client_disconnect();
        g_online.return_requested = 1;
        if (result) result->return_to_main = 1;
        return;
    }

    if (g_online.state == ONLINE_STATE_JOIN) {
        online_handle_join_text(e);
    }

    if (e->type == SDL_MOUSEMOTION) {
        clicked = online_button_at_point(buttons, button_count, e->motion.x, e->motion.y);
        g_online.hovered_button = clicked;
        if (clicked >= 0) g_online.selected_button = clicked;
    } else if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        clicked = online_button_at_point(buttons, button_count, e->button.x, e->button.y);
        if (clicked >= 0) {
            g_online.selected_button = clicked;
            online_activate_button(buttons[clicked].id);
        }
    } else if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_UP || e->key.keysym.sym == SDLK_w) {
            if (button_count > 0) {
                g_online.selected_button = (g_online.selected_button + button_count - 1) % button_count;
            }
        } else if (e->key.keysym.sym == SDLK_DOWN || e->key.keysym.sym == SDLK_s) {
            if (button_count > 0) {
                g_online.selected_button = (g_online.selected_button + 1) % button_count;
            }
        } else if (e->key.keysym.sym == SDLK_LEFT) {
            if ((g_online.state == ONLINE_STATE_HOST_LOBBY || g_online.state == ONLINE_STATE_CLIENT_LOBBY) &&
                button_count > 0) {
                if (buttons[g_online.selected_button].id == ONLINE_BUTTON_SKIN) online_cycle_local_skin();
                if (buttons[g_online.selected_button].id == ONLINE_BUTTON_CONTROL) online_cycle_local_control();
            }
        } else if (e->key.keysym.sym == SDLK_RIGHT) {
            if ((g_online.state == ONLINE_STATE_HOST_LOBBY || g_online.state == ONLINE_STATE_CLIENT_LOBBY) &&
                button_count > 0) {
                if (buttons[g_online.selected_button].id == ONLINE_BUTTON_SKIN) online_cycle_local_skin();
                if (buttons[g_online.selected_button].id == ONLINE_BUTTON_CONTROL) online_cycle_local_control();
            }
        } else if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_SPACE) {
            if (button_count > 0) {
                online_activate_button(buttons[g_online.selected_button].id);
            }
        }
    }
}

void online_scene_update(float delta)
{
    OnlineLobbyState lobby = {0};
    int synced_pause = 0;

    if (!g_online.initialized || !g_online.active) return;

    online_client_pump();
    online_client_copy_lobby_state(&lobby);
    online_sync_local_skin_from_lobby(&lobby);

    if (g_online.state == ONLINE_STATE_HOST_LOBBY) {
        if (!online_client_is_connected() || lobby.kicked) {
            g_online.return_requested = 1;
            return;
        }
    }

    if (g_online.state == ONLINE_STATE_CLIENT_LOBBY) {
        if (!online_client_is_connected() || lobby.kicked || lobby.connection_lost) {
            g_online.return_requested = 1;
            return;
        }
        if (lobby.game_started) {
            g_online.remote_pause_active = 0;
            online_set_state(ONLINE_STATE_REMOTE_PLAY);
        }
    } else if (g_online.state == ONLINE_STATE_REMOTE_PLAY) {
        while (online_client_consume_pause_state_change(&synced_pause)) {
            if (synced_pause) {
                online_open_remote_pause();
            } else {
                online_close_remote_pause();
            }
        }

        if (!online_client_is_connected() || lobby.kicked || lobby.connection_lost || lobby.end_reason[0] != '\0') {
            g_online.return_requested = 1;
            return;
        }

        if (!g_online.remote_pause_active) {
            uint32_t input_mask = online_collect_remote_input_mask();
            online_client_send_input_mask(input_mask);
            online_update_remote_texture();
        } else {
            online_update_remote_texture();
            options_scene_update(delta);
        }
    }
}

static void online_render_header(const char* title, const char* subtitle)
{
    online_draw_text_center(g_online.title_font,
                            title,
                            WINDOW_W / 2,
                            72,
                            (SDL_Color){248, 244, 232, 255});
    online_draw_text_center(g_online.small_font,
                            subtitle,
                            WINDOW_W / 2,
                            122,
                            (SDL_Color){210, 223, 236, 255});
}

static void online_render_buttons(const OnlineButton* buttons, int count)
{
    if (!buttons || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        int hovered = (i == g_online.hovered_button) || (i == g_online.selected_button);
        ui_render_styled_button(g_online.renderer,
                                g_online.button_font ? g_online.button_font : g_online.body_font,
                                &buttons[i].rect,
                                buttons[i].label,
                                hovered,
                                0,
                                1);
    }
}

static void online_render_mode_menu(void)
{
    OnlineButton buttons[8];
    int count = online_build_buttons(buttons, 8, NULL);
    char server_line[128];

    snprintf(server_line, sizeof(server_line), "Server %s:%d", g_online.server_host, g_online.server_port);
    online_render_header("ONLINE MODE", server_line);
    online_render_buttons(buttons, count);

    online_draw_text_center(g_online.small_font,
                            "Host runs the authoritative duo session.",
                            WINDOW_W / 2,
                            520,
                            (SDL_Color){239, 224, 163, 255});
    online_draw_text_center(g_online.small_font,
                            "Joiners connect with a code and play remotely.",
                            WINDOW_W / 2,
                            550,
                            (SDL_Color){210, 223, 236, 255});
}

static void online_render_join_menu(void)
{
    OnlineButton buttons[8];
    int count = online_build_buttons(buttons, 8, NULL);
    SDL_Rect panel = {336, 200, 608, 320};
    SDL_Rect input_rect = {434, 298, 412, 72};
    OnlineLobbyState lobby = {0};

    online_client_copy_lobby_state(&lobby);
    online_render_header("JOIN GAME", "Enter the host join code.");
    online_draw_panel(&panel);

    ui_fill_rect(g_online.renderer, &input_rect, (SDL_Color){10, 20, 36, 220});
    ui_draw_rect(g_online.renderer, &input_rect, (SDL_Color){246, 211, 119, 255});
    online_draw_text_center(g_online.body_font,
                            g_online.join_code_input[0] ? g_online.join_code_input : "______",
                            WINDOW_W / 2,
                            318,
                            (SDL_Color){248, 244, 232, 255});
    online_draw_text_center(g_online.small_font,
                            lobby.notice[0] ? lobby.notice : "Codes are 6 characters.",
                            WINDOW_W / 2,
                            388,
                            (SDL_Color){210, 223, 236, 255});
    online_render_buttons(buttons, count);
}

static void online_render_player_slot(const SDL_Rect* rect,
                                      const char* title,
                                      const char* name,
                                      int skin,
                                      int control,
                                      const char* status)
{
    char line[96];

    online_draw_panel(rect);
    online_draw_text_left(g_online.body_font, title, rect->x + 30, rect->y + 26, (SDL_Color){248, 244, 232, 255});
    snprintf(line, sizeof(line), "NAME  %s", name && name[0] ? name : "WAITING");
    online_draw_text_left(g_online.small_font, line, rect->x + 30, rect->y + 92, (SDL_Color){210, 223, 236, 255});
    snprintf(line, sizeof(line), "SKIN  %d", skin);
    online_draw_text_left(g_online.small_font, line, rect->x + 30, rect->y + 128, (SDL_Color){239, 224, 163, 255});
    snprintf(line, sizeof(line), "INPUT  %s", online_control_name(control));
    online_draw_text_left(g_online.small_font, line, rect->x + 30, rect->y + 164, (SDL_Color){239, 224, 163, 255});
    online_draw_text_left(g_online.small_font,
                          status,
                          rect->x + 30,
                          rect->y + rect->h - 42,
                          (SDL_Color){210, 223, 236, 255});
}

static void online_render_host_lobby(void)
{
    OnlineButton buttons[8];
    OnlineLobbyState lobby = {0};
    SDL_Rect host_rect = {124, 188, 462, 272};
    SDL_Rect client_rect = {694, 188, 462, 272};
    char title[96];
    int count = online_build_buttons(buttons, 8, &lobby);

    snprintf(title, sizeof(title), "JOIN CODE  %s", lobby.join_code[0] ? lobby.join_code : "------");
    online_render_header(title, lobby.notice[0] ? lobby.notice : "Waiting for a second player.");
    online_render_player_slot(&host_rect,
                              "HOST PLAYER",
                              lobby.host_name,
                              g_online.local_skin,
                              g_online.local_control,
                              "Host can change skin, controls, and start.");
    online_render_player_slot(&client_rect,
                              "CLIENT PLAYER",
                              lobby.remote_connected ? lobby.client_name : "WAITING",
                              lobby.client_skin,
                              lobby.client_control,
                              lobby.remote_connected ? "Connected and ready for host start." : "Share the code to invite a player.");
    online_render_buttons(buttons, count);
}

static void online_render_client_lobby(void)
{
    OnlineButton buttons[8];
    OnlineLobbyState lobby = {0};
    SDL_Rect host_rect = {124, 188, 462, 272};
    SDL_Rect client_rect = {694, 188, 462, 272};
    char title[96];
    int count = online_build_buttons(buttons, 8, &lobby);

    snprintf(title, sizeof(title), "CONNECTED  %s", lobby.join_code[0] ? lobby.join_code : "------");
    online_render_header(title, lobby.notice[0] ? lobby.notice : "Waiting for the host to start.");
    online_render_player_slot(&host_rect,
                              "HOST PLAYER",
                              lobby.host_name,
                              lobby.host_skin,
                              lobby.host_control,
                              "Host controls session settings and game start.");
    online_render_player_slot(&client_rect,
                              "CLIENT PLAYER",
                              g_online.local_name,
                              g_online.local_skin,
                              g_online.local_control,
                              "Client can change skin, controls, but cannot start.");
    online_render_buttons(buttons, count);
}

static void online_render_remote_play(void)
{
    OnlineLobbyState lobby = {0};
    SDL_Rect frame_rect = {0, 0, WINDOW_W, WINDOW_H};
    char save_line[128];

    online_client_copy_lobby_state(&lobby);

    if (g_online.remote_frame_texture) {
        SDL_RenderCopy(g_online.renderer, g_online.remote_frame_texture, NULL, &frame_rect);
    } else {
        online_draw_panel(&frame_rect);
        online_draw_text_center(g_online.body_font,
                                "WAITING FOR HOST VIDEO",
                                WINDOW_W / 2,
                                WINDOW_H / 2 - 20,
                                (SDL_Color){248, 244, 232, 255});
    }

    if (lobby.last_save_level > 0) {
        snprintf(save_line, sizeof(save_line),
                 "Host save sync: level %d  score %d  lives %d/%d  %s",
                 lobby.last_save_level,
                 lobby.last_save_score,
                 lobby.last_save_lives[0],
                 lobby.last_save_lives[1],
                 lobby.last_save_note[0] ? lobby.last_save_note : "SYNCED");
        ui_fill_rect(g_online.renderer, &(SDL_Rect){24, WINDOW_H - 56, 980, 32}, (SDL_Color){12, 20, 32, 190});
        online_draw_text_left(g_online.small_font,
                              save_line,
                              40,
                              WINDOW_H - 49,
                              (SDL_Color){210, 223, 236, 255});
    }

    if (g_online.remote_pause_active) {
        options_scene_render();
    }
}

void online_scene_render(void)
{
    if (!g_online.initialized || !g_online.active || !g_online.renderer) return;

    if (g_online.background) {
        SDL_Rect dst = {0, 0, WINDOW_W, WINDOW_H};
        SDL_RenderCopy(g_online.renderer, g_online.background, NULL, &dst);
    } else {
        SDL_SetRenderDrawColor(g_online.renderer, 17, 31, 48, 255);
        SDL_RenderClear(g_online.renderer);
    }

    switch (g_online.state) {
        case ONLINE_STATE_MENU:
            online_render_mode_menu();
            break;
        case ONLINE_STATE_JOIN:
            online_render_join_menu();
            break;
        case ONLINE_STATE_HOST_LOBBY:
            online_render_host_lobby();
            break;
        case ONLINE_STATE_CLIENT_LOBBY:
            online_render_client_lobby();
            break;
        case ONLINE_STATE_REMOTE_PLAY:
            online_render_remote_play();
            break;
        default:
            break;
    }
}

int online_scene_consume_return_request(void)
{
    int out = g_online.return_requested;
    g_online.return_requested = 0;
    return out;
}

int online_scene_consume_host_start_request(GameSelection* out_selection)
{
    OnlineLobbyState lobby = {0};

    if (!g_online.host_start_requested) return 0;

    if (out_selection) {
        memset(out_selection, 0, sizeof(*out_selection));
        online_client_copy_lobby_state(&lobby);
        out_selection->duo_mode = 1;
        out_selection->player_count = 2;
        out_selection->selected_skin[0] = g_online.local_skin;
        out_selection->selected_skin[1] = (lobby.client_skin >= 1 && lobby.client_skin <= 2) ? lobby.client_skin : 2;
        if (out_selection->selected_skin[0] == out_selection->selected_skin[1]) {
            out_selection->selected_skin[1] = online_other_skin(out_selection->selected_skin[0]);
        }
        out_selection->control_scheme[0] = g_online.local_control;
        out_selection->control_scheme[1] = (lobby.client_control >= 1 && lobby.client_control <= 2) ? lobby.client_control : 1;
        out_selection->resume_from_save = 0;
        out_selection->save_enabled = 1;
    }

    g_online.host_start_requested = 0;
    return 1;
}

const char* online_scene_local_player_name(void)
{
    return g_online.local_name;
}

void online_scene_cleanup(void)
{
    if (!g_online.initialized) return;

    online_scene_leave();
    if (g_online.remote_frame_texture) SDL_DestroyTexture(g_online.remote_frame_texture);
    if (g_online.background) SDL_DestroyTexture(g_online.background);
    if (g_online.panel_texture) SDL_DestroyTexture(g_online.panel_texture);
    if (g_online.title_font) TTF_CloseFont(g_online.title_font);
    if (g_online.body_font) TTF_CloseFont(g_online.body_font);
    if (g_online.small_font) TTF_CloseFont(g_online.small_font);
    if (g_online.button_font) TTF_CloseFont(g_online.button_font);

    g_online.remote_frame_texture = NULL;
    g_online.background = NULL;
    g_online.panel_texture = NULL;
    g_online.title_font = NULL;
    g_online.body_font = NULL;
    g_online.small_font = NULL;
    g_online.button_font = NULL;
    g_online.initialized = 0;
}
