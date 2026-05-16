#include "options_scene.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

static void set_working_directory_to_executable(void)
{
    char* base_path = SDL_GetBasePath();

    if (!base_path) return;
    if (chdir(base_path) != 0) {
        SDL_Log("Unable to change directory to executable path: %s", base_path);
    }
    SDL_free(base_path);
}

int main(void)
{
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;
    Uint64 last_counter = 0;
    int running = 1;
    int exit_code = 1;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    set_working_directory_to_executable();

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        goto cleanup;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        goto cleanup;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        SDL_Log("Mix_OpenAudio warning: %s", Mix_GetError());
    }

    window = SDL_CreateWindow("Option Menu",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              1280,
                              720,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    renderer = SDL_CreateRenderer(window,
                                  -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer, 1280, 720);

    if (!options_scene_init(window, renderer)) {
        fprintf(stderr, "options_scene_init failed\n");
        goto cleanup;
    }
    options_scene_enter();

    last_counter = SDL_GetPerformanceCounter();
    while (running) {
        SDL_Event ev;
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - last_counter) / (float)SDL_GetPerformanceFrequency();
        last_counter = now;

        while (SDL_PollEvent(&ev)) {
            OptionsSceneResult result = {0};

            if (ev.type == SDL_QUIT) {
                running = 0;
                break;
            }

            options_scene_handle_event(&ev, &result);
            if (result.return_to_main) {
                running = 0;
                break;
            }
        }

        if (!running) break;

        options_scene_update(dt);

        SDL_SetRenderDrawColor(renderer, 18, 28, 42, 255);
        SDL_RenderClear(renderer);
        options_scene_render();
        SDL_RenderPresent(renderer);
    }

    exit_code = 0;

cleanup:
    options_scene_cleanup();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return exit_code;
}
