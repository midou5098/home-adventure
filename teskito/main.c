#include "headers.h"

/**
 * @file main.c
 * @brief Program entry point and main SDL loop for the Teskito task.
 *
 * This file owns SDL initialization, the game loop, event handling, camera
 * calculation, mono/split-screen view selection, rendering order, and cleanup.
 */

/**
 * @brief Keep the horizontal camera inside the playable level limits.
 *
 * The camera follows the player from left to right, but it should not scroll
 * before the beginning of the level or too far after the last platform.
 *
 * @param camera_x Raw horizontal camera position.
 * @return Clamped horizontal camera position.
 */
static float clampCamera(float camera_x)
{
    if (camera_x < 0.0f) {
        return 0.0f;
    }
    if (camera_x > 2300.0f) {
        return 2300.0f;
    }
    return camera_x;
}

/**
 * @brief Calculate vertical camera scroll from the player jump/fall position.
 *
 * The camera moves upward when the player jumps and downward when the player
 * falls back to the ground. The viewport height is passed in because split
 * screen uses two shorter viewports, so each half needs its own vertical camera
 * calculation.
 *
 * @param player Player used as the camera target.
 * @param viewport_h Height of the current viewport.
 * @return Vertical camera offset.
 */
static float calculerCameraY(const Player *player, int viewport_h)
{
    float target_y;
    float min_y = -180.0f;
    float max_y = GROUND_Y - (viewport_h - 70.0f);

    if (!player) {
        return 0.0f;
    }

    target_y = player->y - ((float)viewport_h - PLAYER_H - 95.0f);
    if (target_y < min_y) {
        return min_y;
    }
    if (target_y > max_y) {
        return max_y;
    }
    return target_y;
}

/**
 * @brief Launch the SDL task and run the game loop.
 *
 * Main responsibilities:
 * - initialize SDL video and SDL_image;
 * - create the window and renderer;
 * - load the background and player animation textures;
 * - process input every frame;
 * - update physics, platforms, camera, and animation;
 * - draw either mono view or split-screen view;
 * - free every SDL resource before exiting.
 */
int main(int argc, char **argv)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    Background bg = {0};
    Player player = {0};
    Platform platforms[MAX_PLATFORMS];
    int running = 1;
    int split_screen = 0;
    float camera_x = 0.0f;
    Uint32 last_ticks;
    Uint32 start_ticks;

    (void)argc;
    (void)argv;

    /* SDL_INIT_TIMER is used by SDL_GetTicks() for delta time and animation. */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("Teskito - background task",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_W, SCREEN_H,
                              SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    /* Prefer hardware rendering with VSync, then fall back to software. */
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    initPlateformes(platforms, MAX_PLATFORMS);

    /* Assets are loaded once before the loop and destroyed once after it. */
    if (!initBackground(&bg, renderer) || !initPlayer(&player, renderer)) {
        fprintf(stderr, "Asset loading failed. Run from project root or from teskito/.\n");
        libererPlayer(&player);
        libererBackground(&bg);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    last_ticks = SDL_GetTicks();
    start_ticks = last_ticks;

    while (running) {
        SDL_Event event;
        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_ticks) / 1000.0f;
        float old_camera_x = camera_x;
        const Uint8 *keys;
        char title[128];

        /*
         * Clamp dt to avoid a huge physics jump if the window is dragged or the
         * process is paused for a moment.
         */
        if (dt > 0.05f) {
            dt = 0.05f;
        }
        last_ticks = now;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (event.key.keysym.sym == SDLK_m) {
                    split_screen = !split_screen;
                }
            }
        }

        /*
         * Input is handled before physics. The player sets vx/vy from current
         * keys, then updatePlayer applies gravity, collision, and animation.
         */
        keys = SDL_GetKeyboardState(NULL);
        gererInputPlayer(&player, keys);
        updatePlateformes(platforms, MAX_PLATFORMS, dt);
        updatePlayer(&player, platforms, MAX_PLATFORMS, dt);

        /*
         * Horizontal camera follows the player. The difference from last frame
         * feeds parallax scrolling so distant layers move slower than foreground.
         */
        camera_x = clampCamera(player.x - 360.0f);
        scrollingBackground(&bg, camera_x - old_camera_x);

        snprintf(title, sizeof(title),
                 "Teskito - temps: %02u:%02u | M: mono/multi | arrows/A-D run | Space jump",
                 (unsigned)((now - start_ticks) / 60000),
                 (unsigned)(((now - start_ticks) / 1000) % 60));
        SDL_SetWindowTitle(window, title);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (split_screen) {
            /*
             * Split-screen mode draws the same world twice with two independent
             * viewports. The second camera is shifted forward to demonstrate
             * the "multi" rendering path required by the task.
             */
            SDL_Rect top_view = {0, 0, SCREEN_W, SCREEN_H / 2 - 2};
            SDL_Rect bottom_view = {0, SCREEN_H / 2 + 2, SCREEN_W, SCREEN_H / 2 - 2};
            float second_camera = clampCamera(camera_x + 420.0f);
            float top_camera_y = calculerCameraY(&player, top_view.h);
            float bottom_camera_y = calculerCameraY(&player, bottom_view.h);

            afficherBackground(renderer, &bg, &top_view, camera_x, top_camera_y);
            afficherPlateformes(renderer, platforms, MAX_PLATFORMS, &top_view, camera_x, top_camera_y);
            afficherPlayer(renderer, &player, &top_view, camera_x, top_camera_y);

            afficherBackground(renderer, &bg, &bottom_view, second_camera, bottom_camera_y);
            afficherPlateformes(renderer, platforms, MAX_PLATFORMS, &bottom_view, second_camera, bottom_camera_y);
            afficherPlayer(renderer, &player, &bottom_view, second_camera, bottom_camera_y);

            SDL_RenderSetViewport(renderer, NULL);
            SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
            SDL_RenderDrawLine(renderer, 0, SCREEN_H / 2, SCREEN_W, SCREEN_H / 2);
        } else {
            /* Mono mode draws one full-window viewport. */
            SDL_Rect view = {0, 0, SCREEN_W, SCREEN_H};
            float camera_y = calculerCameraY(&player, view.h);

            afficherBackground(renderer, &bg, &view, camera_x, camera_y);
            afficherPlateformes(renderer, platforms, MAX_PLATFORMS, &view, camera_x, camera_y);
            afficherPlayer(renderer, &player, &view, camera_x, camera_y);
        }

        SDL_RenderPresent(renderer);
    }

    libererPlayer(&player);
    libererBackground(&bg);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
