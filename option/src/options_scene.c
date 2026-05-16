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

static void options_adjust_selected_slider(int delta_val)
{
    OptionsAdjustSliderByDelta(g_selected, delta_val, 1);
}

static void options_activate_selected_item(OptionsSceneResult* result)
{
    if (g_selected == 4) {
        OptionsToggleFullscreen(1);
        return;
    }

    if (g_selected == 5) {
        OptionsShowCredits(1);
        return;
    }

    PlayClick();
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

    g_initialized = 1;
    return 1;
}

void options_scene_enter(void)
{
    if (!g_initialized) return;

    state = STATE_OPTIONS;
    g_selected = 0;

    if (!g_audio_started) {
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
    if (result) result->return_to_main = 0;
    if (!g_initialized || !e) return;

    if (state == STATE_OPTIONS &&
        (e->type == SDL_MOUSEMOTION ||
         e->type == SDL_MOUSEBUTTONDOWN ||
         e->type == SDL_MOUSEBUTTONUP)) {
        int keep_open = 1;
        HandleOptionsMouseEvent(e, &g_selected, &keep_open);
        if (!keep_open && result) {
            result->return_to_main = 1;
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

        if (e->key.keysym.sym == SDLK_RETURN) {
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

    if (state == STATE_OPTIONS) UpdateShooters(delta);
    else UpdateCredits(delta);

    UpdateSnow(delta);
}

void options_scene_render(void)
{
    if (!g_initialized) return;

    RenderUpSprite();

    if (state == STATE_OPTIONS) {
        RenderShooters();
        RenderOptionsMenu(g_selected);
    } else {
        RenderCredits();
    }

    RenderBrightnessOverlay();
    RenderSnow();
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

    g_initialized = 0;
}

int options_scene_get_brightness(void)
{
    if (!g_initialized) return 10;
    return clamp_int(settings.brightness, 0, 10);
}

int options_scene_get_music_volume_sdl(void)
{
    if (!g_initialized) return MIX_MAX_VOLUME;
    int final_music = (settings.music * settings.master) / 10;
    return (final_music * MIX_MAX_VOLUME) / 10;
}
