#include "mainmenu_headers.h"
#include "options_scene.h"
#include "load_scene.h"
#include "choose_scene.h"
#include "online_scene.h"
#include "top_scores_scene.h"
#include "enigma_scene.h"
#include "asset_paths.h"
#include "debug_log.h"
#include "final_cutscene.h"
#include "game_progress.h"
#include "merged_levels.h"
#include "online_client.h"
#include "ui_shared.h"
#include "main_scene_helpers.h"
#include "../../lvls/shared/arcade_input.h"
#include "../../lvls/newshoplvl1/newshop.h"

#include <limits.h>
#include <unistd.h>

typedef enum {
    FADE_NONE = 0,
    FADE_IN,
    FADE_OUT
} FadeState;

typedef enum {
    BUTTON_PLAY = 0,
    BUTTON_OPTIONS,
    BUTTON_TOP_SCORES,
    BUTTON_STORY,
    BUTTON_EXIT
} MainMenuButton;

typedef enum {
    SCREEN_MAIN = 0,
    SCREEN_OPTIONS,
    SCREEN_TOP_SCORES,
    SCREEN_LOAD,
    SCREEN_CHOOSE,
    SCREEN_ONLINE,
    SCREEN_ENIGMA
} ScreenState;

static const char* screen_name(ScreenState screen)
{
    switch (screen) {
        case SCREEN_MAIN: return "MAIN";
        case SCREEN_OPTIONS: return "OPTIONS";
        case SCREEN_TOP_SCORES: return "TOP_SCORES";
        case SCREEN_LOAD: return "LOAD";
        case SCREEN_CHOOSE: return "CHOOSE";
        case SCREEN_ONLINE: return "ONLINE";
        case SCREEN_ENIGMA: return "ENIGMA";
        default: return "UNKNOWN";
    }
}

static void activate_button(int index,
                            int* pending_action,
                            FadeState* fade_state,
                            Button buttons[],
                            Mix_Chunk* click_sfx)
{
    if (index < 0 || index >= MAX_BUTTONS || !pending_action || !fade_state) return;
    if (*fade_state != FADE_NONE) return;

    if (click_sfx) {
        Mix_PlayChannel(-1, click_sfx, 0);
    }
    buttons[index].pressedTimer = 0.12f;

    *pending_action = index;
    *fade_state = FADE_OUT;
}

static int screen_uses_window_space(ScreenState screen)
{
    switch (screen) {
        case SCREEN_OPTIONS:
        case SCREEN_TOP_SCORES:
        case SCREEN_CHOOSE:
        case SCREEN_ONLINE:
        case SCREEN_ENIGMA:
            return 1;
        default:
            return 0;
    }
}

static void restart_menu_music(Mix_Chunk* menu_music, int* music_channel, int volume)
{
    if (!menu_music || !music_channel) return;

    *music_channel = Mix_PlayChannel(-1, menu_music, -1);
    if (*music_channel >= 0) {
        Mix_Volume(*music_channel, volume);
    }
}

static void stop_menu_music(int* music_channel)
{
    if (!music_channel) return;
    if (*music_channel >= 0) {
        Mix_HaltChannel(*music_channel);
        *music_channel = -1;
    }
}

static int run_main_menu_shop_shortcut(SDL_Window* window, SDL_Renderer* renderer)
{
    char previous_cwd[PATH_MAX];
    int have_cwd = 0;
    int changed_dir = 0;
    int rc = 1;
    NewShopState shop_state;

    if (!window || !renderer) return 1;

    memset(&shop_state, 0, sizeof(shop_state));
    shop_state.control_scheme = NEWSHOP_CONTROL_SCHEME_WASD;
    shop_state.lives = 9;
    shop_state.extra_hearts = 0;
    shop_state.duo_enabled = 0;
    shop_state.online_hosted = 0;
    shop_state.player_interact_bind[0] = INTERACT_BIND_F;
    shop_state.player_interact_bind[1] = INTERACT_BIND_F;
    shop_state.player_skin_number[0] = 1;
    shop_state.player_skin_number[1] = 2;
    shop_state.player_lives[0] = 9;
    shop_state.player_lives[1] = 0;
    shop_state.keys_held = 10;
    shop_state.active_player = 0;

    have_cwd = getcwd(previous_cwd, sizeof(previous_cwd)) != NULL;
    if (chdir("lvls/newshoplvl1") == 0) {
        changed_dir = 1;
    } else if (chdir("../lvls/newshoplvl1") == 0) {
        changed_dir = 1;
    }

    if (!changed_dir) {
        SDL_Log("Shift+K shop shortcut failed: could not enter lvls/newshoplvl1");
        return 1;
    }

    rc = runNewShopLevel1(window, renderer, &shop_state);

    if (have_cwd && chdir(previous_cwd) != 0) {
        SDL_Log("Shift+K shop shortcut failed to restore cwd: %s", previous_cwd);
    }

    return rc;
}

static void step_fade_in(float* fade_alpha, FadeState* fade_state, float fade_speed, float dt)
{
    if (!fade_alpha || !fade_state) return;
    if (*fade_state != FADE_IN) return;

    *fade_alpha -= fade_speed * dt;
    if (*fade_alpha <= 0.0f) {
        *fade_alpha = 0.0f;
        *fade_state = FADE_NONE;
    }
}

static void transition_to_main(SDL_Renderer* renderer,
                               Mix_Chunk* menu_music,
                               int* menu_music_channel,
                               int selected_button,
                               ScreenState* screen,
                               int* selected_main,
                               float* fade_alpha,
                               FadeState* fade_state)
{
    if (!renderer || !menu_music_channel || !screen || !selected_main || !fade_alpha || !fade_state) return;

    main_scene_set_menu_render_mode(renderer);
    restart_menu_music(menu_music, menu_music_channel, options_scene_get_music_volume_sdl());
    *screen = SCREEN_MAIN;
    *selected_main = selected_button;
    *fade_alpha = 255.0f;
    *fade_state = FADE_IN;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    int exit_code = 1;

    Background bg = {0};
    AnimatedSprite kid = {0};
    AnimatedSprite dog = {0};
    AnimatedSprite thief1 = {0};
    AnimatedSprite thief2 = {0};
    Button buttons[MAX_BUTTONS];

    TTF_Font* title_font = NULL;
    TTF_Font* title_font_hi = NULL;
    TTF_Font* button_font_hi = NULL;
    SDL_Texture* title_texture = NULL;
    SDL_Rect title_rect = {0, 0, 0, 0};

    Mix_Chunk* menu_music = NULL;
    Mix_Chunk* button_click = NULL;
    int menu_music_channel = -1;

    int options_scene_ready = 0;
    int load_scene_ready = 0;
    int choose_scene_ready = 0;
    int online_scene_ready = 0;
    int top_scores_scene_ready = 0;
    int enigma_scene_ready = 0;

    float sprite_bottom_margin = 6.0f;
    float kid_y = (float)(LOGICAL_H - KID_DISPLAY_H - sprite_bottom_margin);
    float dog_y = (float)(LOGICAL_H - DOG_DISPLAY_H - sprite_bottom_margin);
    float thief_y = (float)(LOGICAL_H - THIEF_DISPLAY_H - sprite_bottom_margin);

    float thief_delay = 0.5f;
    int thieves_started = 0;

    int running = 1;
    int selected_main = BUTTON_PLAY;
    int pending_action = -1;
    float fade_alpha = 255.0f;
    const float fade_speed = 420.0f;
    FadeState fade_state = FADE_IN;
    ScreenState screen = SCREEN_MAIN;
    SDL_Event ev;

    const int autotest_flow = (getenv("MENU_AUTOTEST_FLOW") != NULL);
    int autotest_step = 0;
    float autotest_choose_timer = 0.0f;
    int dev_final_requested = 0;
    int dev_final_cutscene_requested = 0;
    int dev_level2_requested = 0;
    int dev_shop_requested = 0;
    float heartbeat_timer = 0.0f;

    Uint32 last = 0;

    debug_log_init("/tmp/menu_offline_runtime.log");
    debug_log_install_signal_handlers();
    debug_logf("main_offline: process started");
    srand((unsigned int)time(NULL));

    if (!init_SDL(&window, &renderer)) {
        debug_logf("main_offline: init_SDL failed");
        goto cleanup;
    }
    arcade_input_init();

    init_background(&bg, renderer);

    if (!init_sprite(&kid, renderer,
                     ASSET_MAIN_MENU_KID,
                     kid_y, KID_SPEED,
                     KID_DISPLAY_W, KID_DISPLAY_H,
                     KID_FRAMES, KID_COLUMNS, 0.06f)) {
        debug_logf("main_offline: init kid sprite failed");
        goto cleanup;
    }

    if (!init_sprite(&dog, renderer,
                     ASSET_MAIN_MENU_DOG,
                     dog_y, DOG_SPEED,
                     DOG_DISPLAY_W, DOG_DISPLAY_H,
                     DOG_FRAMES, DOG_COLUMNS, 0.06f)) {
        debug_logf("main_offline: init dog sprite failed");
        goto cleanup;
    }

    if (!init_sprite(&thief1, renderer,
                     ASSET_MAIN_MENU_THIEF_1,
                     thief_y, THIEF1_SPEED,
                     THIEF_DISPLAY_W, THIEF_DISPLAY_H,
                     THIEF_FRAMES, THIEF_COLUMNS, 0.06f)) {
        debug_logf("main_offline: init thief1 sprite failed");
        goto cleanup;
    }

    if (!init_sprite(&thief2, renderer,
                     ASSET_MAIN_MENU_THIEF_2,
                     thief_y, THIEF2_SPEED,
                     THIEF_DISPLAY_W, THIEF_DISPLAY_H,
                     THIEF_FRAMES, THIEF_COLUMNS, 0.06f)) {
        debug_logf("main_offline: init thief2 sprite failed");
        goto cleanup;
    }

    init_buttons(buttons, MAX_BUTTONS, renderer);

    {
        const char* title_candidates[] = {
            ASSET_MAIN_MENU_FONT_TITLE,
            ASSET_MAIN_MENU_FONT_OPTIONS,
            ASSET_MAIN_MENU_FONT_TEXT
        };

        title_font = ui_open_arial_font(20, 0);
        if (!title_font) {
            title_font = ui_open_font_from_candidates(title_candidates,
                                                      (int)(sizeof(title_candidates) / sizeof(title_candidates[0])),
                                                      20,
                                                      0);
        }

        title_font_hi = ui_open_arial_font(80, 0);
        if (!title_font_hi) {
            title_font_hi = ui_open_font_from_candidates(title_candidates,
                                                         (int)(sizeof(title_candidates) / sizeof(title_candidates[0])),
                                                         80,
                                                         0);
        }
    }

    {
        const char* button_candidates[] = {
            ASSET_BUTTON_FONT,
            ASSET_MAIN_MENU_FONT_OPTIONS,
            ASSET_MAIN_MENU_FONT_TEXT,
            ASSET_MAIN_MENU_FONT_TITLE
        };

        button_font_hi = ui_open_arial_font(48, 0);
        if (!button_font_hi) {
            button_font_hi = ui_open_font_from_candidates(button_candidates,
                                                          (int)(sizeof(button_candidates) / sizeof(button_candidates[0])),
                                                          48,
                                                          0);
        }
    }

    title_texture = main_scene_create_title_texture(renderer, title_font, &title_rect);

    options_scene_ready = options_scene_init(window, renderer);
    if (!options_scene_ready) {
        debug_logf("main_offline: options_scene_init failed");
        goto cleanup;
    }

    menu_music = ui_load_wav(ASSET_MAIN_MENU_SOUND);
    button_click = ui_load_wav(ASSET_OPTIONS_CLICK);
    restart_menu_music(menu_music, &menu_music_channel, options_scene_get_music_volume_sdl());

    load_scene_ready = load_scene_init(renderer);
    choose_scene_ready = choose_scene_init(renderer);
    online_scene_ready = online_scene_init(renderer);
    top_scores_scene_ready = top_scores_scene_init(window, renderer);
    enigma_scene_ready = enigma_scene_init(window, renderer);
    debug_logf("main_offline: preload status load=%d choose=%d online=%d top=%d enigma=%d",
               load_scene_ready,
               choose_scene_ready,
               online_scene_ready,
               top_scores_scene_ready,
               enigma_scene_ready);

    main_scene_set_menu_render_mode(renderer);
    last = SDL_GetTicks();

    while (running)
    {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.05f) dt = 0.05f;

        heartbeat_timer += dt;
        if (heartbeat_timer >= 1.0f) {
            heartbeat_timer = 0.0f;
            debug_logf("main_offline: heartbeat screen=%s fade_state=%d fade_alpha=%.1f",
                       screen_name(screen), (int)fade_state, fade_alpha);
        }

        if (autotest_flow) {
            if (autotest_step == 0) {
                if (!choose_scene_ready) {
                    choose_scene_ready = choose_scene_init(renderer);
                }
                if (choose_scene_ready) {
                    main_scene_set_window_render_mode(renderer);
                    choose_scene_enter(1, 1, 1);
                    choose_scene_set_music_volume(options_scene_get_music_volume_sdl());
                    screen = SCREEN_CHOOSE;
                    fade_alpha = 0.0f;
                    fade_state = FADE_NONE;
                    autotest_step = 1;
                    debug_logf("autotest_offline: entered CHOOSE scene");
                } else {
                    debug_logf("autotest_offline: failed to init choose scene");
                    running = 0;
                }
            } else if (autotest_step == 1 && screen == SCREEN_CHOOSE) {
                autotest_choose_timer += dt;
                if (autotest_choose_timer > 0.25f) {
                    choose_scene_debug_force_start("bot");
                    autotest_step = 2;
                    debug_logf("autotest_offline: forced start request from CHOOSE");
                }
            }
        }

        arcade_input_begin_frame();
        while (SDL_PollEvent(&ev)) {
            arcade_input_handle_event(&ev);
            if (ev.type == SDL_QUIT) {
                debug_logf("main_offline: SDL_QUIT while screen=%s", screen_name(screen));
                running = 0;
                break;
            }
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keymod mod = SDL_GetModState();
                if ((ev.key.keysym.sym == SDLK_2 || ev.key.keysym.sym == SDLK_KP_2) &&
                    (mod & KMOD_ALT) &&
                    (mod & KMOD_SHIFT) &&
                    !(mod & KMOD_CTRL)) {
                    dev_level2_requested = 1;
                    continue;
                }
                if (ev.key.keysym.sym == SDLK_k &&
                    (mod & KMOD_SHIFT) &&
                    !(mod & KMOD_ALT) &&
                    !(mod & KMOD_CTRL)) {
                    dev_shop_requested = 1;
                    continue;
                }
                if (ev.key.keysym.sym == SDLK_p) {
                    if ((mod & KMOD_ALT) && (mod & KMOD_SHIFT) && !(mod & KMOD_CTRL)) {
                        dev_final_cutscene_requested = 1;
                        continue;
                    }
                    if ((mod & KMOD_CTRL) && ((mod & KMOD_ALT) || (mod & KMOD_SHIFT))) {
                        dev_final_requested = 1;
                        continue;
                    }
                }
            }

            switch (screen) {
                case SCREEN_MAIN: {
                    if (fade_state != FADE_NONE) break;

                    if (ev.type == SDL_KEYDOWN) {
                        if (ev.key.keysym.sym == SDLK_ESCAPE) {
                            selected_main = BUTTON_EXIT;
                            activate_button(BUTTON_EXIT, &pending_action, &fade_state, buttons, button_click);
                        }

                        if (ev.key.keysym.sym == SDLK_UP || ev.key.keysym.sym == SDLK_w) {
                            selected_main--;
                            if (selected_main < 0) selected_main = MAX_BUTTONS - 1;
                        }

                        if (ev.key.keysym.sym == SDLK_DOWN || ev.key.keysym.sym == SDLK_s) {
                            selected_main++;
                            if (selected_main >= MAX_BUTTONS) selected_main = 0;
                        }

                        if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_SPACE) {
                            activate_button(selected_main, &pending_action, &fade_state, buttons, button_click);
                        }

                        if (ev.key.keysym.sym == SDLK_e) {
                            selected_main = BUTTON_STORY;
                            activate_button(BUTTON_STORY, &pending_action, &fade_state, buttons, button_click);
                        }
                    }

                    if (ev.type == SDL_MOUSEMOTION) {
                        int hovered = main_scene_button_from_logical_point(buttons, MAX_BUTTONS, ev.motion.x, ev.motion.y);
                        if (hovered >= 0) {
                            selected_main = hovered;
                        }
                    }

                    if (ev.type == SDL_FINGERMOTION) {
                        int lx = 0;
                        int ly = 0;
                        if (main_scene_touch_to_logical(ev.tfinger.x, ev.tfinger.y, &lx, &ly)) {
                            int hovered = main_scene_button_from_logical_point(buttons, MAX_BUTTONS, lx, ly);
                            if (hovered >= 0) {
                                selected_main = hovered;
                            }
                        }
                    }

                    if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                        int hovered = main_scene_button_from_logical_point(buttons, MAX_BUTTONS, ev.button.x, ev.button.y);
                        if (hovered >= 0) {
                            selected_main = hovered;
                            activate_button(hovered, &pending_action, &fade_state, buttons, button_click);
                        }
                    }

                    if (ev.type == SDL_FINGERDOWN) {
                        int lx = 0;
                        int ly = 0;
                        if (main_scene_touch_to_logical(ev.tfinger.x, ev.tfinger.y, &lx, &ly)) {
                            int hovered = main_scene_button_from_logical_point(buttons, MAX_BUTTONS, lx, ly);
                            if (hovered >= 0) {
                                selected_main = hovered;
                                activate_button(hovered, &pending_action, &fade_state, buttons, button_click);
                            }
                        }
                    }
                    break;
                }
                case SCREEN_OPTIONS: {
                    OptionsSceneResult result = {0};
                    options_scene_handle_event(&ev, &result);
                    if (result.return_to_main) {
                        options_scene_leave();
                        transition_to_main(renderer,
                                           menu_music,
                                           &menu_music_channel,
                                           BUTTON_OPTIONS,
                                           &screen,
                                           &selected_main,
                                           &fade_alpha,
                                           &fade_state);
                    }
                    break;
                }
                case SCREEN_LOAD: {
                    LoadSceneResult result = {0};
                    load_scene_handle_event(&ev, &result);
                    if (result.return_to_main) {
                        load_scene_leave();
                        transition_to_main(renderer,
                                           menu_music,
                                           &menu_music_channel,
                                           BUTTON_PLAY,
                                           &screen,
                                           &selected_main,
                                           &fade_alpha,
                                           &fade_state);
                    }
                    break;
                }
                case SCREEN_TOP_SCORES:
                    top_scores_scene_handle_event(&ev);
                    break;
                case SCREEN_CHOOSE:
                    choose_scene_handle_event(&ev, NULL);
                    break;
                case SCREEN_ONLINE:
                    online_scene_handle_event(&ev, NULL);
                    break;
                case SCREEN_ENIGMA:
                    enigma_scene_handle_event(&ev);
                    break;
                default:
                    break;
            }
        }

        if (dev_level2_requested) {
            GameSelection dev_selection = {0};

            dev_level2_requested = 0;
            dev_selection.duo_mode = 0;
            dev_selection.player_count = 1;
            dev_selection.selected_skin[0] = 1;
            dev_selection.selected_skin[1] = 0;
            dev_selection.control_scheme[0] = 1;
            dev_selection.control_scheme[1] = 0;
            dev_selection.resume_from_save = 0;
            dev_selection.save_enabled = 0;

            stop_menu_music(&menu_music_channel);
            if (screen == SCREEN_CHOOSE) choose_scene_leave();
            if (screen == SCREEN_LOAD) load_scene_leave();
            if (screen == SCREEN_OPTIONS) options_scene_leave();
            if (screen == SCREEN_ONLINE) online_scene_leave();
            if (screen == SCREEN_ENIGMA) enigma_scene_leave();

            debug_logf("main_offline: Alt+Shift+2 dev shortcut -> level 2");
            merged_levels_run_from_level(window, renderer, &dev_selection, 2);
            transition_to_main(renderer,
                               menu_music,
                               &menu_music_channel,
                               BUTTON_PLAY,
                               &screen,
                               &selected_main,
                               &fade_alpha,
                               &fade_state);
            continue;
        }

        if (dev_shop_requested) {
            dev_shop_requested = 0;

            stop_menu_music(&menu_music_channel);
            if (screen == SCREEN_CHOOSE) choose_scene_leave();
            if (screen == SCREEN_LOAD) load_scene_leave();
            if (screen == SCREEN_OPTIONS) options_scene_leave();
            if (screen == SCREEN_ONLINE) online_scene_leave();
            if (screen == SCREEN_ENIGMA) enigma_scene_leave();

            debug_logf("main_offline: Shift+K dev shortcut -> shop");
            run_main_menu_shop_shortcut(window, renderer);
            transition_to_main(renderer,
                               menu_music,
                               &menu_music_channel,
                               BUTTON_PLAY,
                               &screen,
                               &selected_main,
                               &fade_alpha,
                               &fade_state);
            continue;
        }

        if (dev_final_requested) {
            GameSelection dev_selection = {0};

            dev_final_requested = 0;
            dev_selection.duo_mode = 0;
            dev_selection.player_count = 1;
            dev_selection.selected_skin[0] = 1;
            dev_selection.selected_skin[1] = 0;
            dev_selection.control_scheme[0] = 1;
            dev_selection.control_scheme[1] = 0;
            dev_selection.resume_from_save = 0;
            dev_selection.save_enabled = 0;

            stop_menu_music(&menu_music_channel);
            if (screen == SCREEN_CHOOSE) choose_scene_leave();
            if (screen == SCREEN_LOAD) load_scene_leave();
            if (screen == SCREEN_OPTIONS) options_scene_leave();
            if (screen == SCREEN_ONLINE) online_scene_leave();
            if (screen == SCREEN_ENIGMA) enigma_scene_leave();

            debug_logf("main_offline: Ctrl+Alt+P dev shortcut -> level 4");
            merged_levels_run_from_level(window, renderer, &dev_selection, 4);
            transition_to_main(renderer,
                               menu_music,
                               &menu_music_channel,
                               BUTTON_PLAY,
                               &screen,
                               &selected_main,
                               &fade_alpha,
                               &fade_state);
            continue;
        }

        if (dev_final_cutscene_requested) {
            dev_final_cutscene_requested = 0;
            stop_menu_music(&menu_music_channel);
            if (screen == SCREEN_CHOOSE) choose_scene_leave();
            if (screen == SCREEN_LOAD) load_scene_leave();
            if (screen == SCREEN_OPTIONS) options_scene_leave();
            if (screen == SCREEN_ONLINE) online_scene_leave();
            if (screen == SCREEN_ENIGMA) enigma_scene_leave();

            debug_logf("main_offline: Alt+Shift+P dev shortcut -> final cutscene chase");
            final_cutscene_run_from_chase(window, renderer);
            transition_to_main(renderer,
                               menu_music,
                               &menu_music_channel,
                               BUTTON_PLAY,
                               &screen,
                               &selected_main,
                               &fade_alpha,
                               &fade_state);
            continue;
        }

        switch (screen) {
            case SCREEN_MAIN: {
                update_background(&bg, dt);
                update_buttons(buttons, MAX_BUTTONS, &selected_main, dt);

                if (fade_state == FADE_IN) {
                    step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                } else if (fade_state == FADE_OUT) {
                    fade_alpha += fade_speed * dt;
                    if (fade_alpha >= 255.0f) {
                        fade_alpha = 255.0f;

                        if (pending_action == BUTTON_EXIT) {
                            debug_logf("main_offline: exit requested from main menu");
                            running = 0;
                        } else if (pending_action == BUTTON_PLAY) {
                            if (!load_scene_ready) {
                                load_scene_ready = load_scene_init(renderer);
                            }

                            if (load_scene_ready) {
                                stop_menu_music(&menu_music_channel);
                                main_scene_set_menu_render_mode(renderer);
                                load_scene_enter();
                                load_scene_set_music_volume(options_scene_get_music_volume_sdl());
                                screen = SCREEN_LOAD;
                                debug_logf("main_offline: transition -> %s", screen_name(screen));
                                pending_action = -1;
                                fade_state = FADE_IN;
                            } else {
                                SDL_Log("Load scene init failed; staying on main menu.");
                                pending_action = -1;
                                fade_state = FADE_IN;
                            }
                        } else if (pending_action == BUTTON_OPTIONS) {
                            stop_menu_music(&menu_music_channel);
                            main_scene_set_window_render_mode(renderer);
                            options_scene_set_audio_enabled(1);
                            options_scene_enter();
                            screen = SCREEN_OPTIONS;
                            debug_logf("main_offline: transition -> %s", screen_name(screen));
                            pending_action = -1;
                            fade_state = FADE_IN;
                        } else if (pending_action == BUTTON_TOP_SCORES) {
                            if (!top_scores_scene_ready) {
                                top_scores_scene_ready = top_scores_scene_init(window, renderer);
                            }

                            if (top_scores_scene_ready) {
                                stop_menu_music(&menu_music_channel);
                                main_scene_set_window_render_mode(renderer);
                                top_scores_scene_enter();
                                top_scores_scene_set_music_volume(options_scene_get_music_volume_sdl());
                                screen = SCREEN_TOP_SCORES;
                                debug_logf("main_offline: transition -> %s", screen_name(screen));
                                pending_action = -1;
                                fade_state = FADE_IN;
                            } else {
                                SDL_Log("Top Scores scene init failed; staying on main menu.");
                                pending_action = -1;
                                fade_state = FADE_IN;
                            }
                        } else if (pending_action == BUTTON_STORY) {
                            if (!enigma_scene_ready) {
                                enigma_scene_ready = enigma_scene_init(window, renderer);
                            }

                            if (enigma_scene_ready) {
                                stop_menu_music(&menu_music_channel);
                                main_scene_set_window_render_mode(renderer);
                                enigma_scene_enter_random();
                                enigma_scene_set_music_volume(options_scene_get_music_volume_sdl());
                                screen = SCREEN_ENIGMA;
                                debug_logf("main_offline: transition -> %s", screen_name(screen));
                                pending_action = -1;
                                fade_state = FADE_IN;
                            } else {
                                SDL_Log("Enigma scene init failed; staying on main menu.");
                                pending_action = -1;
                                fade_state = FADE_IN;
                            }
                        } else {
                            pending_action = -1;
                            fade_state = FADE_IN;
                        }
                    }
                }

                if (!thieves_started) {
                    thief_delay -= dt;
                    if (thief_delay <= 0.0f) {
                        thieves_started = 1;
                    }
                }

                update_sprite(&kid, dt);
                update_sprite(&dog, dt);
                if (thieves_started) {
                    update_sprite(&thief1, dt);
                    update_sprite(&thief2, dt);
                }
                break;
            }
            case SCREEN_OPTIONS:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                options_scene_update(dt);
                break;
            case SCREEN_LOAD:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                load_scene_update(dt);

                {
                    int choose_mode = 1;
                    int choose_start = 1;
                    int choose_save_enabled = 0;
                    if (load_scene_consume_choose_request(&choose_mode, &choose_start, &choose_save_enabled)) {
                        if (!choose_scene_ready) {
                            choose_scene_ready = choose_scene_init(renderer);
                        }

                        if (choose_scene_ready) {
                            load_scene_leave();
                            main_scene_set_window_render_mode(renderer);
                            choose_scene_enter(choose_mode, choose_start, choose_save_enabled);
                            choose_scene_set_music_volume(options_scene_get_music_volume_sdl());
                            screen = SCREEN_CHOOSE;
                            debug_logf("main_offline: transition -> %s (mode=%d start=%d save_enabled=%d)",
                                       screen_name(screen), choose_mode, choose_start, choose_save_enabled);
                            fade_alpha = 0.0f;
                            fade_state = FADE_NONE;
                        } else {
                            SDL_Log("Choose scene init failed; returning to main menu.");
                            load_scene_leave();
                            transition_to_main(renderer,
                                               menu_music,
                                               &menu_music_channel,
                                               BUTTON_PLAY,
                                               &screen,
                                               &selected_main,
                                               &fade_alpha,
                                               &fade_state);
                        }
                    }

                    if (load_scene_consume_online_request()) {
                        if (!online_scene_ready) {
                            online_scene_ready = online_scene_init(renderer);
                        }

                        if (online_scene_ready) {
                            load_scene_leave();
                            main_scene_set_window_render_mode(renderer);
                            online_scene_enter();
                            screen = SCREEN_ONLINE;
                            debug_logf("main_offline: transition -> %s", screen_name(screen));
                            fade_alpha = 0.0f;
                            fade_state = FADE_NONE;
                        } else {
                            SDL_Log("Online scene init failed; returning to main menu.");
                            load_scene_leave();
                            transition_to_main(renderer,
                                               menu_music,
                                               &menu_music_channel,
                                               BUTTON_PLAY,
                                               &screen,
                                               &selected_main,
                                               &fade_alpha,
                                               &fade_state);
                        }
                    }
                }

                if (load_scene_consume_return_request()) {
                    load_scene_leave();
                    transition_to_main(renderer,
                                       menu_music,
                                       &menu_music_channel,
                                       BUTTON_PLAY,
                                       &screen,
                                       &selected_main,
                                       &fade_alpha,
                                       &fade_state);
                }
                break;
            case SCREEN_TOP_SCORES:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                top_scores_scene_update(dt);
                if (top_scores_scene_consume_return_request()) {
                    top_scores_scene_leave();
                    transition_to_main(renderer,
                                       menu_music,
                                       &menu_music_channel,
                                       BUTTON_TOP_SCORES,
                                       &screen,
                                       &selected_main,
                                       &fade_alpha,
                                       &fade_state);
                }
                break;
            case SCREEN_CHOOSE:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                choose_scene_update(dt);

                {
                    GameSelection start_selection = {0};
                    if (choose_scene_consume_start_game_request(&start_selection)) {
                        debug_logf("main_offline: choose start requested duo=%d players=%d skin1=%d skin2=%d ctrl1=%d ctrl2=%d",
                                   start_selection.duo_mode,
                                   start_selection.player_count,
                                   start_selection.selected_skin[0],
                                   start_selection.selected_skin[1],
                                   start_selection.control_scheme[0],
                                   start_selection.control_scheme[1]);

                        if (autotest_flow) {
                            choose_scene_leave();
                            transition_to_main(renderer,
                                               menu_music,
                                               &menu_music_channel,
                                               BUTTON_PLAY,
                                               &screen,
                                               &selected_main,
                                               &fade_alpha,
                                               &fade_state);
                            debug_logf("autotest_offline: reached CHOOSE start successfully");
                            running = 0;
                        } else {
                            choose_scene_leave();
                            stop_menu_music(&menu_music_channel);
                            merged_levels_run(window, renderer, &start_selection);
                            transition_to_main(renderer,
                                               menu_music,
                                               &menu_music_channel,
                                               BUTTON_PLAY,
                                               &screen,
                                               &selected_main,
                                               &fade_alpha,
                                               &fade_state);
                        }
                    } else if (choose_scene_consume_return_request()) {
                        debug_logf("main_offline: choose requested return to main");
                        choose_scene_leave();
                        transition_to_main(renderer,
                                           menu_music,
                                           &menu_music_channel,
                                           BUTTON_PLAY,
                                           &screen,
                                           &selected_main,
                                           &fade_alpha,
                                           &fade_state);
                    }
                }
                break;
            case SCREEN_ONLINE:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                online_scene_update(dt);

                {
                    GameSelection start_selection = {0};
                    if (online_scene_consume_host_start_request(&start_selection)) {
                        game_progress_set_player_name(online_scene_local_player_name());
                        game_progress_set_selection(&start_selection);
                        online_client_begin_host_gameplay();
                        merged_levels_run(window, renderer, &start_selection);
                        online_client_end_host_gameplay("HOST_SESSION_ENDED");
                        online_scene_leave();
                        transition_to_main(renderer,
                                           menu_music,
                                           &menu_music_channel,
                                           BUTTON_PLAY,
                                           &screen,
                                           &selected_main,
                                           &fade_alpha,
                                           &fade_state);
                    } else if (online_scene_consume_return_request()) {
                        online_scene_leave();
                        transition_to_main(renderer,
                                           menu_music,
                                           &menu_music_channel,
                                           BUTTON_PLAY,
                                           &screen,
                                           &selected_main,
                                           &fade_alpha,
                                           &fade_state);
                    }
                }
                break;
            case SCREEN_ENIGMA:
                step_fade_in(&fade_alpha, &fade_state, fade_speed, dt);
                enigma_scene_update(dt);
                if (enigma_scene_consume_return_request()) {
                    enigma_scene_leave();
                    transition_to_main(renderer,
                                       menu_music,
                                       &menu_music_channel,
                                       BUTTON_STORY,
                                       &screen,
                                       &selected_main,
                                       &fade_alpha,
                                       &fade_state);
                }
                break;
            default:
                break;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        switch (screen) {
            case SCREEN_MAIN:
                main_scene_render(renderer,
                                  &bg, &kid, &dog, &thief1, &thief2,
                                  thieves_started,
                                  title_font_hi, button_font_hi,
                                  title_texture, &title_rect,
                                  buttons,
                                  MAX_BUTTONS,
                                  options_scene_get_brightness());
                break;
            case SCREEN_OPTIONS:
                options_scene_render();
                break;
            case SCREEN_LOAD:
                load_scene_render();
                main_scene_render_brightness_overlay(renderer, options_scene_get_brightness());
                break;
            case SCREEN_TOP_SCORES:
                top_scores_scene_render();
                main_scene_render_brightness_overlay(renderer, options_scene_get_brightness());
                break;
            case SCREEN_CHOOSE:
                choose_scene_render();
                break;
            case SCREEN_ONLINE:
                online_scene_render();
                break;
            case SCREEN_ENIGMA:
                enigma_scene_render();
                break;
            default:
                break;
        }

        main_scene_render_fade_overlay(renderer, fade_alpha, screen_uses_window_space(screen));
        SDL_RenderPresent(renderer);
    }

    exit_code = 0;

cleanup:
    if (load_scene_ready) load_scene_cleanup();
    if (choose_scene_ready) choose_scene_cleanup();
    if (online_scene_ready) online_scene_cleanup();
    if (top_scores_scene_ready) top_scores_scene_cleanup();
    if (enigma_scene_ready) enigma_scene_cleanup();
    if (options_scene_ready) options_scene_cleanup();

    destroy_background(&bg);
    destroy_sprite(&kid);
    destroy_sprite(&dog);
    destroy_sprite(&thief1);
    destroy_sprite(&thief2);
    destroy_buttons(buttons, MAX_BUTTONS);

    if (title_texture) SDL_DestroyTexture(title_texture);
    if (title_font) TTF_CloseFont(title_font);
    if (title_font_hi) TTF_CloseFont(title_font_hi);
    if (button_font_hi) TTF_CloseFont(button_font_hi);
    if (menu_music) Mix_FreeChunk(menu_music);
    if (button_click) Mix_FreeChunk(button_click);

    arcade_input_shutdown();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    debug_logf("main_offline: shutdown exit_code=%d", exit_code);
    debug_log_close();
    return exit_code;
}
