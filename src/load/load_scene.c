#include "load_scene.h"
#include "mainmenu_headers.h"
#include "asset_paths.h"
#include "game_progress.h"
#include "ui_shared.h"

typedef enum {
    LOAD_FADE_NONE = 0,
    LOAD_FADE_IN,
    LOAD_FADE_OUT
} LoadFadeState;

typedef enum {
    LOAD_STAGE_MODE = 0,
    LOAD_STAGE_START,
    LOAD_STAGE_SAVE
} LoadStage;

static SDL_Renderer* g_renderer = NULL;
static Background g_bg = {0};
static AnimatedSprite g_plane = {0};
static AnimatedSprite g_thief = {0};
static AnimatedSprite g_dog = {0};
static AnimatedSprite g_sleeper = {0};
static SDL_Texture* g_hoop = NULL;
static TTF_Font* g_menu_font = NULL;
static TTF_Font* g_menu_font_hi = NULL;
static Mix_Chunk* g_menu_music = NULL;
static Mix_Chunk* g_button_click = NULL;
static int g_menu_music_channel = -1;
static int g_initialized = 0;
static int g_active = 0;

static Button g_buttons[MAX_BUTTONS];
static int g_active_buttons = 0;
static LoadStage g_stage = LOAD_STAGE_MODE;
static int g_mode_choice = 0;
static int g_start_choice = 0;
static int g_save_choice = -1;
static int g_pending_stage = -1;
static int g_selected = 0;
static int g_pending_action = -1;
static float g_fade_alpha = 255.0f;
static const float g_fade_speed = 420.0f;
static LoadFadeState g_fade_state = LOAD_FADE_IN;
static int g_return_requested = 0;
static int g_choose_requested = 0;
static int g_online_requested = 0;
static int g_choose_mode = 1;
static int g_choose_start = 1;
static int g_choose_save = 0;
static char g_notice_text[96] = {0};
static float g_notice_timer = 0.0f;

static int logical_to_window_x(int logical_x)
{
    return (logical_x * WINDOW_W) / LOGICAL_W;
}

static int logical_to_window_y(int logical_y)
{
    return (logical_y * WINDOW_H) / LOGICAL_H;
}

static int open_menu_font_size(TTF_Font** out_font, int point_size)
{
    if (!out_font || point_size <= 0) return 0;

    *out_font = ui_open_arial_font(point_size, 0);
    if (*out_font) return 1;

    const char* candidates[] = {
        ASSET_LOAD_FONT_TEXT_LOAD,
        ASSET_LOAD_FONT_OPTIONS,
        ASSET_LOAD_FONT_TEXT
    };
    const int count = (int)(sizeof(candidates) / sizeof(candidates[0]));

    *out_font = ui_open_font_from_candidates(candidates, count, point_size, 0);

    ui_apply_font_quality(*out_font);
    return (*out_font != NULL);
}

static int open_menu_font(TTF_Font** out_font)
{
    return open_menu_font_size(out_font, 12);
}

static void render_centered_text(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                                 int y, SDL_Color color)
{
    ui_draw_text_center(renderer, font, text, LOGICAL_W / 2, y, color);
}

static void render_centered_text_hi_res(TTF_Font* font,
                                        const char* text,
                                        int logical_y,
                                        SDL_Color color)
{
    if (!g_renderer || !font || !text || !text[0]) return;
    ui_draw_text_center(g_renderer,
                        font,
                        text,
                        WINDOW_W / 2,
                        logical_to_window_y(logical_y),
                        color);
}

static void render_button_labels_hi_res(TTF_Font* font)
{
    const SDL_Color text_normal = {0xF7, 0xF2, 0xE8, 0xFF};
    const SDL_Color text_hover = {0xFF, 0xFA, 0xE2, 0xFF};
    const SDL_Color text_pressed = {0xF0, 0xE6, 0xD0, 0xFF};

    if (!g_renderer || !font) return;

    for (int i = 0; i < g_active_buttons; ++i) {
        SDL_Rect logical_dest = get_button_dest(&g_buttons[i]);
        SDL_Rect win_dest = {
            logical_to_window_x(logical_dest.x),
            logical_to_window_y(logical_dest.y),
            logical_to_window_x(logical_dest.w),
            logical_to_window_y(logical_dest.h)
        };
        SDL_Color color = text_normal;
        int pressed = (g_buttons[i].pressedTimer > 0.0f);
        int pressed_offset = pressed ? logical_to_window_y(1) : 0;
        int text_h = TTF_FontHeight(font);
        int text_y = 0;
        int center_x = 0;

        if (g_buttons[i].selected) {
            color = text_hover;
        }
        if (pressed) {
            color = text_pressed;
        }

        center_x = win_dest.x + win_dest.w / 2;
        text_y = win_dest.y + (win_dest.h - text_h) / 2 + pressed_offset;
        ui_draw_text_center(g_renderer, font, g_buttons[i].label, center_x, text_y, color);
    }
}

static int setup_buttons_for_stage(Button buttons[], LoadStage stage)
{
    if (!buttons) return 0;

    const char* mode_labels[] = {"SOLO", "DUO", "ONLINE MODE", "RETURN"};
    const char* start_labels[] = {"LOAD GAME", "NEW GAME", "RETURN"};
    const char* save_labels[] = {"YES", "NO", "RETURN"};

    const char** labels = NULL;
    int count = 0;

    if (stage == LOAD_STAGE_MODE) {
        labels = mode_labels;
        count = 4;
    } else if (stage == LOAD_STAGE_START) {
        labels = start_labels;
        count = 3;
    } else {
        labels = save_labels;
        count = 3;
    }

    for (int i = 0; i < MAX_BUTTONS; ++i) {
        prototype_button_init(&buttons[i], 0, 0, 0, 0, "");
    }

    for (int i = 0; i < count; ++i) {
        prototype_button_init(&buttons[i], 0, 0, 0, 0, labels[i]);
    }

    if (stage == LOAD_STAGE_MODE) {
        const int button_w = 146;
        const int button_h = 18;
        const int spacing = 20;
        const int button_x = (LOGICAL_W - button_w) / 2;
        const int y_start = 56;

        for (int i = 0; i < count; ++i) {
            buttons[i].rect = (SDL_Rect){button_x, y_start + i * spacing, button_w, button_h};
        }
    } else if (stage == LOAD_STAGE_START) {
        const int button_w = 136;
        const int button_h = 22;
        const int spacing = 24;
        const int button_x = (LOGICAL_W - button_w) / 2;
        const int y_start = 72;

        for (int i = 0; i < count; ++i) {
            buttons[i].rect = (SDL_Rect){button_x, y_start + i * spacing, button_w, button_h};
        }
    } else {
        const int yn_w = 86;
        const int yn_h = 20;
        const int gap = 14;
        const int total_w = yn_w * 2 + gap;
        const int start_x = (LOGICAL_W - total_w) / 2;
        const int y = 92;

        buttons[0].rect = (SDL_Rect){start_x, y, yn_w, yn_h};
        buttons[1].rect = (SDL_Rect){start_x + yn_w + gap, y, yn_w, yn_h};
        buttons[2].rect = (SDL_Rect){(LOGICAL_W - 112) / 2, 122, 112, 20};
    }

    return count;
}

static void stage_text(LoadStage stage, int mode_choice, int start_choice, int save_choice,
                       char* title, size_t title_size,
                       char* subtitle, size_t subtitle_size)
{
    (void)mode_choice;
    (void)start_choice;
    (void)save_choice;
    if (subtitle && subtitle_size > 0) {
        subtitle[0] = '\0';
        if (g_notice_text[0] != '\0') {
            snprintf(subtitle, subtitle_size, "%s", g_notice_text);
        }
    }

    if (stage == LOAD_STAGE_MODE) {
        snprintf(title, title_size, "CHOOSE MODE");
        return;
    }

    if (stage == LOAD_STAGE_START) {
        snprintf(title, title_size, "CHOOSE START");
        return;
    }

    snprintf(title, title_size, "ENABLE SAVE?");
}

static int touch_to_logical(float nx, float ny, int* lx, int* ly)
{
    if (!lx || !ly) return 0;

    if (nx < 0.0f || ny < 0.0f || nx > 1.0f || ny > 1.0f) {
        return 0;
    }

    int x = (int)floorf(nx * (float)LOGICAL_W);
    int y = (int)floorf(ny * (float)LOGICAL_H);

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= LOGICAL_W) x = LOGICAL_W - 1;
    if (y >= LOGICAL_H) y = LOGICAL_H - 1;

    *lx = x;
    *ly = y;
    return 1;
}

static int button_from_logical_point(Button buttons[], int count, int lx, int ly)
{
    if (lx < 0 || ly < 0 || lx >= LOGICAL_W || ly >= LOGICAL_H) {
        return -1;
    }

    return button_at_point(buttons, count, lx, ly);
}

static void activate_button(int index, int button_count, int* pending_action, LoadFadeState* fade_state,
                            Button buttons[])
{
    if (index < 0 || index >= button_count || !pending_action || !fade_state) return;
    if (*fade_state != LOAD_FADE_NONE) return;

    if (g_button_click) {
        Mix_PlayChannel(-1, g_button_click, 0);
    }
    buttons[index].pressedTimer = 0.12f;
    *pending_action = index;
    *fade_state = LOAD_FADE_OUT;
}

static int init_load_background(void)
{
    g_bg.background = load_texture(ASSET_LOAD_BG, g_renderer);
    g_bg.snow_tex = load_texture(ASSET_LOAD_SNOW, g_renderer);
    if (!g_bg.background || !g_bg.snow_tex) {
        return 0;
    }

    for (int i = 0; i < MAX_SNOW; ++i) {
        g_bg.snow[i].x = (float)(rand() % LOGICAL_W);
        g_bg.snow[i].y = (float)(rand() % LOGICAL_H);
        g_bg.snow[i].speed = 6.0f + (rand() % 12);
        g_bg.snow[i].drift = -6.0f + (rand() % 12);
        g_bg.snow[i].angle = (float)(rand() % 360);
        g_bg.snow[i].rotationSpeed = -20.0f + (rand() % 40);
        g_bg.snow[i].size = 2 + (rand() % 3);
    }

    return 1;
}

static void update_load_background(float dt)
{
    for (int i = 0; i < MAX_SNOW; ++i) {
        g_bg.snow[i].y += g_bg.snow[i].speed * dt;
        g_bg.snow[i].x += g_bg.snow[i].drift * dt;
        g_bg.snow[i].angle += g_bg.snow[i].rotationSpeed * dt;

        if (g_bg.snow[i].y > LOGICAL_H) {
            g_bg.snow[i].y = -4.0f;
            g_bg.snow[i].x = (float)(rand() % LOGICAL_W);
        }
        if (g_bg.snow[i].x < -8.0f) g_bg.snow[i].x = (float)LOGICAL_W;
        if (g_bg.snow[i].x > LOGICAL_W + 8.0f) g_bg.snow[i].x = 0.0f;
    }
}

static void render_load_background(void)
{
    if (g_bg.background) {
        SDL_Rect dst = {0, 0, LOGICAL_W, LOGICAL_H};
        SDL_RenderCopy(g_renderer, g_bg.background, NULL, &dst);
    }

    if (g_bg.snow_tex) {
        for (int i = 0; i < MAX_SNOW; ++i) {
            SDL_Rect r = {(int)g_bg.snow[i].x, (int)g_bg.snow[i].y, g_bg.snow[i].size, g_bg.snow[i].size};
            SDL_RenderCopyEx(g_renderer, g_bg.snow_tex, NULL, &r, g_bg.snow[i].angle, NULL, SDL_FLIP_NONE);
        }
    }
}

static void destroy_load_background(void)
{
    if (g_bg.background) {
        SDL_DestroyTexture(g_bg.background);
        g_bg.background = NULL;
    }
    if (g_bg.snow_tex) {
        SDL_DestroyTexture(g_bg.snow_tex);
        g_bg.snow_tex = NULL;
    }
}

static void reset_scene_state(void)
{
    g_stage = LOAD_STAGE_MODE;
    g_mode_choice = 0;
    g_start_choice = 0;
    g_save_choice = -1;
    g_pending_stage = -1;
    g_pending_action = -1;
    g_selected = 0;
    g_fade_alpha = 255.0f;
    g_fade_state = LOAD_FADE_IN;
    g_return_requested = 0;
    g_choose_requested = 0;
    g_online_requested = 0;
    g_choose_mode = 1;
    g_choose_start = 1;
    g_choose_save = 0;
    g_notice_text[0] = '\0';
    g_notice_timer = 0.0f;
    g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);

    g_plane.x = (float)LOGICAL_W;
    g_thief.x = 269.0f;
    g_dog.x = 295.0f;
    g_sleeper.x = 35.0f;
}

int load_scene_init(SDL_Renderer* shared_renderer)
{
    if (g_initialized) return 1;
    if (!shared_renderer) return 0;
    g_renderer = shared_renderer;

    if (!init_load_background()) return 0;

    g_hoop = load_texture(ASSET_LOAD_HOOP, g_renderer);
    const float shared_anim_delay = 0.08f;

    if (!init_sprite(&g_plane, g_renderer,
                     ASSET_LOAD_PLANE,
                     10.0f, 0.0f,
                     47, 26,
                     36, 6, shared_anim_delay)) return 0;

    if (!init_sprite(&g_thief, g_renderer,
                     ASSET_LOAD_THIEF,
                     154.0f, 0.0f,
                     17, 17,
                     36, 6, shared_anim_delay)) return 0;

    if (!init_sprite(&g_dog, g_renderer,
                     ASSET_LOAD_DOG,
                     154.0f, 0.0f,
                     21, 17,
                     36, 6, shared_anim_delay)) return 0;

    if (!init_sprite(&g_sleeper, g_renderer,
                     ASSET_LOAD_SLEEP,
                     146.0f, 0.0f,
                     23, 29,
                     36, 6, shared_anim_delay)) return 0;

    g_menu_music = ui_load_wav(ASSET_LOAD_SOUND);
    g_button_click = ui_load_wav(ASSET_OPTIONS_CLICK);
    open_menu_font(&g_menu_font);
    open_menu_font_size(&g_menu_font_hi, 48);

    g_initialized = 1;
    reset_scene_state();
    return 1;
}

void load_scene_enter(void)
{
    if (!g_initialized) return;
    g_active = 1;
    reset_scene_state();

    if (g_menu_music) {
        g_menu_music_channel = Mix_PlayChannel(-1, g_menu_music, -1);
    }
}

void load_scene_leave(void)
{
    if (!g_initialized) return;
    g_active = 0;
    if (g_menu_music_channel >= 0) {
        Mix_HaltChannel(g_menu_music_channel);
        g_menu_music_channel = -1;
    }
}

void load_scene_set_music_volume(int sdl_volume)
{
    if (sdl_volume < 0) sdl_volume = 0;
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
    if (g_menu_music_channel >= 0) {
        Mix_Volume(g_menu_music_channel, sdl_volume);
    }
}

void load_scene_handle_event(const SDL_Event* ev, LoadSceneResult* result)
{
    if (result) result->return_to_main = 0;
    if (!g_initialized || !g_active || !ev) return;

    if (g_fade_state != LOAD_FADE_NONE) return;

    if (ev->type == SDL_KEYDOWN) {
        if (ev->key.keysym.sym == SDLK_UP || ev->key.keysym.sym == SDLK_w) {
            g_selected--;
            if (g_selected < 0) g_selected = g_active_buttons - 1;
        }

        if (ev->key.keysym.sym == SDLK_DOWN || ev->key.keysym.sym == SDLK_s) {
            g_selected++;
            if (g_selected >= g_active_buttons) g_selected = 0;
        }

        if (ev->key.keysym.sym == SDLK_LEFT || ev->key.keysym.sym == SDLK_a) {
            g_selected--;
            if (g_selected < 0) g_selected = g_active_buttons - 1;
        }

        if (ev->key.keysym.sym == SDLK_RIGHT || ev->key.keysym.sym == SDLK_d) {
            g_selected++;
            if (g_selected >= g_active_buttons) g_selected = 0;
        }

        if (ev->key.keysym.sym == SDLK_RETURN || ev->key.keysym.sym == SDLK_SPACE) {
            activate_button(g_selected, g_active_buttons, &g_pending_action, &g_fade_state, g_buttons);
        }

        if (ev->key.keysym.sym == SDLK_ESCAPE) {
            if (g_stage == LOAD_STAGE_SAVE) {
                g_pending_stage = LOAD_STAGE_START;
                g_pending_action = -1;
                g_fade_state = LOAD_FADE_OUT;
            } else if (g_stage == LOAD_STAGE_START) {
                g_pending_stage = LOAD_STAGE_MODE;
                g_pending_action = -1;
                g_fade_state = LOAD_FADE_OUT;
            } else {
                g_return_requested = 1;
                if (result) result->return_to_main = 1;
            }
        }
    }

    if (ev->type == SDL_MOUSEMOTION) {
        int hovered = button_from_logical_point(g_buttons, g_active_buttons, ev->motion.x, ev->motion.y);
        if (hovered >= 0) {
            g_selected = hovered;
        }
    }

    if (ev->type == SDL_FINGERMOTION) {
        int lx = 0;
        int ly = 0;
        if (touch_to_logical(ev->tfinger.x, ev->tfinger.y, &lx, &ly)) {
            int hovered = button_from_logical_point(g_buttons, g_active_buttons, lx, ly);
            if (hovered >= 0) {
                g_selected = hovered;
            }
        }
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        int hovered = button_from_logical_point(g_buttons, g_active_buttons, ev->button.x, ev->button.y);
        if (hovered >= 0) {
            g_selected = hovered;
            activate_button(hovered, g_active_buttons, &g_pending_action, &g_fade_state, g_buttons);
        }
    }

    if (ev->type == SDL_FINGERDOWN) {
        int lx = 0;
        int ly = 0;
        if (touch_to_logical(ev->tfinger.x, ev->tfinger.y, &lx, &ly)) {
            int hovered = button_from_logical_point(g_buttons, g_active_buttons, lx, ly);
            if (hovered >= 0) {
                g_selected = hovered;
                activate_button(hovered, g_active_buttons, &g_pending_action, &g_fade_state, g_buttons);
            }
        }
    }
}

void load_scene_update(float dt)
{
    if (!g_initialized || !g_active) return;

    update_load_background(dt);
    update_buttons(g_buttons, g_active_buttons, &g_selected, dt);
    if (g_notice_timer > 0.0f) {
        g_notice_timer -= dt;
        if (g_notice_timer <= 0.0f) {
            g_notice_timer = 0.0f;
            g_notice_text[0] = '\0';
        }
    }

    if (g_fade_state == LOAD_FADE_IN) {
        g_fade_alpha -= g_fade_speed * dt;
        if (g_fade_alpha <= 0.0f) {
            g_fade_alpha = 0.0f;
            g_fade_state = LOAD_FADE_NONE;
        }
    } else if (g_fade_state == LOAD_FADE_OUT) {
        g_fade_alpha += g_fade_speed * dt;
        if (g_fade_alpha >= 255.0f) {
            g_fade_alpha = 255.0f;
            if (g_pending_stage >= 0) {
                g_stage = (LoadStage)g_pending_stage;
                g_pending_stage = -1;
                g_selected = 0;
                if (g_stage != LOAD_STAGE_SAVE) g_save_choice = -1;
                g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);
                g_fade_state = LOAD_FADE_IN;
            } else if (g_pending_action >= 0) {
                if (g_stage == LOAD_STAGE_MODE) {
                if (g_pending_action == 0 || g_pending_action == 1) {
                    g_mode_choice = g_pending_action;
                    g_stage = LOAD_STAGE_START;
                        g_selected = 0;
                        g_save_choice = -1;
                        g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);
                } else if (g_pending_action == 2) {
                    g_online_requested = 1;
                } else if (g_pending_action == 3) {
                    g_return_requested = 1;
                }
                } else if (g_stage == LOAD_STAGE_START) {
                    if (g_pending_action == 0 || g_pending_action == 1) {
                        char save_path[256];
                        int selected_mode = (g_mode_choice == 1) ? 2 : 1;
                        game_progress_copy_last_save_path_for_mode(selected_mode, save_path, sizeof(save_path));
                        if (g_pending_action == 0 && !game_progress_has_resumable_save(save_path)) {
                            snprintf(g_notice_text, sizeof(g_notice_text), "NO SAVE FOUND");
                            g_notice_timer = 2.5f;
                        } else if (g_pending_action == 0) {
                            g_choose_mode = (g_mode_choice == 1) ? 2 : 1;
                            g_choose_start = 0;
                            g_choose_save = 1;
                            g_choose_requested = 1;
                        } else {
                            g_start_choice = g_pending_action;
                            g_stage = LOAD_STAGE_SAVE;
                            g_selected = 0;
                            g_save_choice = -1;
                            g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);
                        }
                    } else if (g_pending_action == 2) {
                        g_stage = LOAD_STAGE_MODE;
                        g_selected = 0;
                        g_save_choice = -1;
                        g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);
                    }
                } else {
                    if (g_pending_action == 0 || g_pending_action == 1) {
                        g_save_choice = (g_pending_action == 0) ? 1 : 0;
                        g_choose_mode = (g_mode_choice == 1) ? 2 : 1;
                        g_choose_start = 1;
                        g_choose_save = g_save_choice;
                        g_choose_requested = 1;
                    } else if (g_pending_action == 2) {
                        g_stage = LOAD_STAGE_START;
                        g_selected = 0;
                        g_save_choice = -1;
                        g_active_buttons = setup_buttons_for_stage(g_buttons, g_stage);
                    }
                }
                g_pending_action = -1;
                if (!g_return_requested) {
                    g_fade_state = LOAD_FADE_IN;
                }
            } else {
                g_fade_state = LOAD_FADE_IN;
            }
        }
    }

    update_sprite(&g_plane, dt);
    update_sprite(&g_thief, dt);
    update_sprite(&g_dog, dt);
    update_sprite(&g_sleeper, dt);

    g_plane.x -= 45.0f * dt;
    if (g_plane.x + g_plane.displayW < 0.0f) {
        g_plane.x = (float)LOGICAL_W;
    }
}

void load_scene_render(void)
{
    int use_hi_res_text = 0;

    if (!g_initialized || !g_active) return;
    use_hi_res_text = (g_menu_font_hi != NULL);

    render_load_background();
    render_sprite(&g_plane, g_renderer);

    if (g_hoop) {
        SDL_Rect left_hoop_rect = {10, 113, 45, 63};
        SDL_Rect right_hoop_rect = {265, 119, 50, 56};
        SDL_RenderCopy(g_renderer, g_hoop, NULL, &left_hoop_rect);
        SDL_RenderCopy(g_renderer, g_hoop, NULL, &right_hoop_rect);
    }

    render_sprite(&g_sleeper, g_renderer);
    render_sprite(&g_thief, g_renderer);
    render_sprite(&g_dog, g_renderer);

    char title[96] = {0};
    char subtitle[160] = {0};
    stage_text(g_stage, g_mode_choice, g_start_choice, g_save_choice, title, sizeof(title), subtitle,
               sizeof(subtitle));

    if (!use_hi_res_text && g_menu_font) {
        render_centered_text(g_renderer, g_menu_font, title, 36, (SDL_Color){0xF8, 0xF6, 0xE8, 0xFF});
        render_centered_text(g_renderer, g_menu_font, subtitle, 52, (SDL_Color){0xD4, 0xDF, 0xEC, 0xFF});
    }

    if (use_hi_res_text) {
        render_buttons_no_text(g_buttons, g_active_buttons, g_renderer);
    } else {
        render_buttons(g_buttons, g_active_buttons, g_renderer);
    }

    if (use_hi_res_text) {
        SDL_RenderSetLogicalSize(g_renderer, WINDOW_W, WINDOW_H);
        SDL_RenderSetIntegerScale(g_renderer, SDL_FALSE);
        render_centered_text_hi_res(g_menu_font_hi, title, 36, (SDL_Color){0xF8, 0xF6, 0xE8, 0xFF});
        render_centered_text_hi_res(g_menu_font_hi, subtitle, 52, (SDL_Color){0xD4, 0xDF, 0xEC, 0xFF});
        render_button_labels_hi_res(g_menu_font_hi);
        SDL_RenderSetLogicalSize(g_renderer, LOGICAL_W, LOGICAL_H);
        SDL_RenderSetIntegerScale(g_renderer, SDL_FALSE);
    }

    if (g_fade_alpha > 0.0f) {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0x0D, 0x18, 0x28, (Uint8)g_fade_alpha);
        SDL_Rect fade_rect = {0, 0, LOGICAL_W, LOGICAL_H};
        SDL_RenderFillRect(g_renderer, &fade_rect);
    }
}

int load_scene_consume_return_request(void)
{
    int out = g_return_requested;
    g_return_requested = 0;
    return out;
}

int load_scene_consume_choose_request(int* selected_mode, int* start_choice, int* save_enabled)
{
    int out = g_choose_requested;
    if (out) {
        if (selected_mode) *selected_mode = g_choose_mode;
        if (start_choice) *start_choice = g_choose_start;
        if (save_enabled) *save_enabled = g_choose_save;
    }
    g_choose_requested = 0;
    return out;
}

int load_scene_consume_online_request(void)
{
    int out = g_online_requested;
    g_online_requested = 0;
    return out;
}

void load_scene_cleanup(void)
{
    if (!g_initialized) return;

    load_scene_leave();
    destroy_load_background();
    destroy_sprite(&g_plane);
    destroy_sprite(&g_dog);
    destroy_sprite(&g_thief);
    destroy_sprite(&g_sleeper);
    destroy_buttons(g_buttons, g_active_buttons);

    if (g_hoop) {
        SDL_DestroyTexture(g_hoop);
        g_hoop = NULL;
    }

    if (g_menu_font) {
        TTF_CloseFont(g_menu_font);
        g_menu_font = NULL;
    }
    if (g_menu_font_hi) {
        TTF_CloseFont(g_menu_font_hi);
        g_menu_font_hi = NULL;
    }

    if (g_menu_music) {
        Mix_FreeChunk(g_menu_music);
        g_menu_music = NULL;
    }
    if (g_button_click) {
        Mix_FreeChunk(g_button_click);
        g_button_click = NULL;
    }

    g_initialized = 0;
    g_active = 0;
    g_renderer = NULL;
}
