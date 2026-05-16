#include "options_scene.h"
#include "options_internal.h"

/* Globals required by options_fonctions.c */
SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
Settings settings;
AppState state = STATE_OPTIONS;
SDL_Texture* bgTexture = NULL;

Shooter leftShooter;
Shooter rightShooter;
Bullet bullets[MAX_BULLETS];
SDL_Texture* bulletTex = NULL;
SDL_Texture* brickTex = NULL;

TTF_Font* font = NULL;
SDL_Texture* barFullTex = NULL;
SDL_Texture* barEmptyTex = NULL;
SDL_Texture* volIcons[4] = {NULL, NULL, NULL, NULL};
SDL_Texture* sunIcons[3] = {NULL, NULL, NULL};

static int g_selected = 0;
static int g_audio_started = 0;
static int g_initialized = 0;
static int g_audio_enabled = 1;

static int options_scene_is_pause_overlay(void)
{
    return g_audio_enabled ? 0 : 1;
}

int options_scene_is_pause_overlay_mode(void)
{
    return options_scene_is_pause_overlay();
}

static void options_adjust_selected_slider(int delta_val)
{
    OptionsAdjustSliderByDelta(g_selected, delta_val, 1);
}

static void options_activate_selected_item(OptionsSceneResult* result)
{
    if (g_selected == 4) {
        OptionsPressActionButton(g_selected);
        OptionsToggleFullscreen(1);
        return;
    }

    if (g_selected == 5) {
        OptionsPressActionButton(g_selected);
        PlayClick();
        if (options_scene_is_pause_overlay()) {
            if (result) result->quit_to_menu = 1;
        } else {
            OptionsShowCredits(0);
        }
        return;
    }

    if (g_selected == 6) {
        OptionsPressActionButton(g_selected);
        PlayClick();
    }
    if (g_selected == 6 && result) {
        result->return_to_main = 1;
    }
}

int options_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer)
{
    if (!shared_window || !shared_renderer) return 0;

    window = shared_window;
    renderer = shared_renderer;
    state = STATE_OPTIONS;
    g_selected = 0;

    InitFont();
    InitBackground();
    InitUpSprite();
    LoadSettings();
    InitSnow();
    ApplySettings();
    InitShooters();
    LoadCreditsFromFile();
    OptionsResetButtonVisuals();

    g_initialized = 1;
    return 1;
}

void options_scene_set_audio_enabled(int enabled)
{
    g_audio_enabled = enabled ? 1 : 0;
    if (!g_audio_enabled && g_audio_started) {
        CleanupAudio();
        g_audio_started = 0;
    }
}

void options_scene_enter(void)
{
    if (!g_initialized) return;

    state = STATE_OPTIONS;
    g_selected = 0;
    OptionsResetButtonVisuals();

    if (g_audio_enabled && !g_audio_started) {
        InitAudio();
        g_audio_started = 1;
    }

    ApplySettings();
}

void options_scene_leave(void)
{
    if (!g_initialized) return;

    if (g_audio_started) {
        CleanupAudio();
        g_audio_started = 0;
    }

    state = STATE_OPTIONS;
}

void options_scene_handle_event(const SDL_Event* e, OptionsSceneResult* result)
{
    if (result) {
        result->return_to_main = 0;
        result->quit_to_menu = 0;
    }
    if (!g_initialized || !e) return;

    if (state == STATE_OPTIONS &&
        (e->type == SDL_MOUSEMOTION ||
         e->type == SDL_MOUSEBUTTONDOWN ||
         e->type == SDL_MOUSEBUTTONUP)) {
        int keep_open = 1;
        int quit_to_menu = 0;
        HandleOptionsMouseEvent(e, &g_selected, &keep_open, &quit_to_menu);
        if (!keep_open && result) {
            if (quit_to_menu) result->quit_to_menu = 1;
            else result->return_to_main = 1;
        }
    }

    if (e->type != SDL_KEYDOWN) return;

    if (state == STATE_OPTIONS) {
        if (e->key.keysym.sym == SDLK_UP || e->key.keysym.sym == SDLK_w) {
            g_selected = (g_selected - 1 + MENU_ITEMS) % MENU_ITEMS;
            PlayClick();
        }
        if (e->key.keysym.sym == SDLK_DOWN || e->key.keysym.sym == SDLK_s) {
            g_selected = (g_selected + 1) % MENU_ITEMS;
            PlayClick();
        }

        if (e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_RIGHT) {
            int delta_val = (e->key.keysym.sym == SDLK_RIGHT) ? 1 : -1;
            options_adjust_selected_slider(delta_val);
        }

        if (e->key.keysym.sym == SDLK_PLUS ||
            e->key.keysym.sym == SDLK_EQUALS ||
            e->key.keysym.sym == SDLK_KP_PLUS) {
            options_adjust_selected_slider(1);
        }

        if (e->key.keysym.sym == SDLK_MINUS ||
            e->key.keysym.sym == SDLK_KP_MINUS) {
            options_adjust_selected_slider(-1);
        }

        if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_SPACE) {
            options_activate_selected_item(result);
        }

        if (e->key.keysym.sym == SDLK_ESCAPE) {
            PlayClick();
            if (result) result->return_to_main = 1;
        }
    } else if (state == STATE_CREDITS) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            PlayClick();
            state = STATE_OPTIONS;
        }
    }
}

void options_scene_update(float delta)
{
    if (!g_initialized) return;

    UpdateOptionsMenuButtons(g_selected, delta);

    if (state == STATE_OPTIONS) UpdateShooters(delta);
    else UpdateCredits(delta);

    UpdateSnow(delta);
}

void options_scene_render(void)
{
    SDL_Rect shade = {0, 0, 0, 0};

    if (!g_initialized) return;

    if (options_scene_is_pause_overlay()) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderGetLogicalSize(renderer, &shade.w, &shade.h);
        if (shade.w <= 0 || shade.h <= 0) {
            SDL_GetRendererOutputSize(renderer, &shade.w, &shade.h);
        }
        if (shade.w <= 0) shade.w = SCREEN_WIDTH;
        if (shade.h <= 0) shade.h = SCREEN_HEIGHT;
        SDL_SetRenderDrawColor(renderer, 8, 16, 28, 92);
        SDL_RenderFillRect(renderer, &shade);
    } else {
        if (bgTexture) SDL_RenderCopy(renderer, bgTexture, NULL, NULL);
        RenderUpSprite();
    }

    if (state == STATE_OPTIONS) {
        if (!options_scene_is_pause_overlay()) {
            RenderShooters();
        }
        RenderOptionsMenu(g_selected);
    } else {
        RenderCredits();
    }

    RenderBrightnessOverlay();
    if (!options_scene_is_pause_overlay()) {
        RenderSnow();
    }
}

void options_scene_cleanup(void)
{
    if (!g_initialized) return;

    options_scene_leave();
    CleanupShooters();
    CleanupBackground();
    CleanupFont();
    CleanupUpSprite();
    CleanupSnow();
    CleanupCredits();
    OptionsResetButtonVisuals();

    g_initialized = 0;
}

int options_scene_get_brightness(void)
{
    if (!g_initialized) {
        Settings loaded = {0};
        if (options_settings_load_global(&loaded)) {
            return clamp_int(loaded.brightness, 0, 10);
        }
        return 10;
    }
    return clamp_int(settings.brightness, 0, 10);
}

int options_scene_get_music_volume_sdl(void)
{
    if (!g_initialized) {
        Settings loaded = {0};
        if (options_settings_load_global(&loaded)) {
            int final_music = (loaded.music * loaded.master) / 10;
            return (final_music * MIX_MAX_VOLUME) / 10;
        }
        return MIX_MAX_VOLUME;
    }
    int final_music = (settings.music * settings.master) / 10;
    return (final_music * MIX_MAX_VOLUME) / 10;
}

void options_scene_render_global_brightness_overlay(SDL_Renderer* target_renderer)
{
    int render_w = SCREEN_WIDTH;
    int render_h = SCREEN_HEIGHT;
    int brightness = options_scene_get_brightness();
    int alpha = 0;

    if (!target_renderer) return;

    SDL_RenderGetLogicalSize(target_renderer, &render_w, &render_h);
    if (render_w <= 0 || render_h <= 0) {
        SDL_GetRendererOutputSize(target_renderer, &render_w, &render_h);
    }
    if (render_w <= 0 || render_h <= 0) return;

    alpha = (10 - clamp_int(brightness, 0, 10)) * 18;
    if (alpha <= 0) return;

    SDL_SetRenderDrawBlendMode(target_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(target_renderer, 0, 0, 0, (Uint8)alpha);
    SDL_Rect overlay = {0, 0, render_w, render_h};
    SDL_RenderFillRect(target_renderer, &overlay);
}
