#include "headers.h"

/* Define globals **once** in main.c */
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
SDL_Texture* volIcons[4] = {NULL,NULL,NULL,NULL};
SDL_Texture* sunIcons[3] = {NULL,NULL,NULL};

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Options",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (!window) { SDL_Log("Window create failed: %s", SDL_GetError()); return 1; }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) { SDL_Log("Renderer failed: %s", SDL_GetError()); return 1; }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    }

    InitFont();
    InitBackground();
    InitUpSprite();

    LoadSettings();
    InitAudio();
    InitSnow();
    ApplySettings();
    InitShooters();
    LoadCreditsFromFile();

    int running = 1;
    int selected = 0;
    Uint32 last = SDL_GetTicks();

    while (running) {
        Uint32 now = SDL_GetTicks();
        float delta = (now - last) / 1000.0f;
        last = now;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = 0; break; }

            if (e.type == SDL_KEYDOWN) {
                if (state == STATE_OPTIONS) {
                    if (e.key.keysym.sym == SDLK_UP) selected = (selected - 1 + MENU_ITEMS) % MENU_ITEMS;
                    if (e.key.keysym.sym == SDLK_DOWN) selected = (selected + 1) % MENU_ITEMS;

                    if (e.key.keysym.sym == SDLK_LEFT || e.key.keysym.sym == SDLK_RIGHT) {
                        int deltaVal = (e.key.keysym.sym == SDLK_RIGHT) ? 1 : -1;
                        if (selected == 0) settings.master = clamp_int(settings.master + deltaVal, 0, 10);
                        if (selected == 1) settings.music  = clamp_int(settings.music  + deltaVal, 0, 10);
                        if (selected == 2) settings.vfx    = clamp_int(settings.vfx    + deltaVal, 0, 10);
                        if (selected == 3) settings.brightness = clamp_int(settings.brightness + deltaVal, 0, 10);
                        ApplySettings();
                        SaveSettings();
                        PlayClick();
                    }

                    if (e.key.keysym.sym == SDLK_RETURN) {
                        PlayClick();
                        if (selected == 4) {
                            settings.fullscreen = !settings.fullscreen;
                            ApplySettings();
                            SaveSettings();
                        } else if (selected == 5) {
                            state = STATE_CREDITS;
                            InitCredits();
                        }
                    }
                } else if (state == STATE_CREDITS) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        state = STATE_OPTIONS;
                    }
                }
            }
        }

        if (state == STATE_OPTIONS) UpdateShooters(delta);
        else UpdateCredits(delta);

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        if (bgTexture) SDL_RenderCopy(renderer, bgTexture, NULL, NULL);
        RenderUpSprite();
        if (state == STATE_OPTIONS) {
            RenderShooters();
            RenderOptionsMenu(selected);
        } else {
            RenderCredits();
        }

        RenderBrightnessOverlay();
        RenderSnow();
        UpdateSnow(delta);

        SDL_RenderPresent(renderer);
    }

    CleanupAudio();
    CleanupShooters();
    CleanupBackground();
    CleanupFont();

    Mix_CloseAudio();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    CleanupUpSprite();

    snow_quit();
    SDL_Quit();


    return 0;
}
