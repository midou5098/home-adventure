#include "headers.h"
#include <stdio.h>
#define SPRITE_BOTTOM_MARGIN 10
int main(void)
{

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG Init Error: %s\n", IMG_GetError());
        return 1;
    }

    srand(time(NULL));

    SDL_Window *window = SDL_CreateWindow(
        "Top Score",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH,
        HEIGHT,
        0
    );

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED
    );
    float kid_y = (float)(HEIGHT - KID_DISPLAY_H - SPRITE_BOTTOM_MARGIN);
    float car_y = kid_y +10;  // behind kid
    float heli_y = 20;
// Initialize kid sprite
    AnimatedSprite kid = {0};
    if (!init_sprite(&kid, renderer,
        "assets/decorations/kid.png",
        kid_y,
        KID_SPEED,
        KID_DISPLAY_W,
        KID_DISPLAY_H,
        KID_FRAMES,
        KID_COLUMNS,
        0.05f,140.0f)) {
    printf("Failed to load kid sprite!\n");
    return 1;}

    float thiefDelay = 0.5f;
    int thievesStarted = 0;
    int running = 1;
    int selected = 0;
    SDL_Event ev;

    Uint32 last = SDL_GetTicks();
    LeaderboardPanel leaderboard = init_leaderboard(renderer, 400, 70, 500, 600);
    Button button = init_button(renderer, 200);


    SDL_Texture *bg = init_bg(renderer);
    SDL_Texture *snow_tex = load_snow(renderer);

    if (!bg || !snow_tex) return 1;

    init_snow(snow_tex);

    init_car(renderer, car_y);
    init_helicopter(renderer, heli_y);

    SDL_Event event;

    Uint32 lastTime = SDL_GetTicks();

    while (running) {

        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }
        SDL_RenderClear(renderer);
        update_button(&button, &event);
        update_snow(deltaTime);
        update_leaderboard(&leaderboard);
        update_sprite(&kid, deltaTime);
        update_car(deltaTime);
        update_helicopter(deltaTime);
        render_bg(renderer, bg);
        render_snow(renderer);

        render_car(renderer);
        render_helicopter(renderer); // correct variable





        render_sprite(&kid, renderer);


        render_leaderboard(renderer, &leaderboard);
        render_button(renderer, &button);


        SDL_RenderPresent(renderer);

    }

    destroy_snow(snow_tex);
    destroy_bg(bg);

    SDL_DestroyRenderer(renderer);
    destroy_car();

    destroy_helicopter();
    SDL_DestroyWindow(window);
    destroy_leaderboard(&leaderboard);
    destroy_button(&button);



    IMG_Quit();
    SDL_Quit();

    return 0;
}
     // parallel on X axis


// in the loop


// at cleanup


