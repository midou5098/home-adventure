#include "headers.h"
#include <stdio.h>
#include <time.h>
#include <SDL2/SDL_ttf.h>  // for text

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

    if (TTF_Init() == -1) {
        printf("TTF Init Error: %s\n", TTF_GetError());
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
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Mixer Init Error: %s\n", Mix_GetError());
        return 1;}
    Mix_Music *menuMusic = Mix_LoadMUS("assets/ui/main.wav");
    if (!menuMusic) {
        printf("Failed to load music: %s\n", Mix_GetError());
    }
    Mix_PlayMusic(menuMusic, -1);  // -1 = infinite loop



    SDL_Texture *bg = init_bg(renderer);
    SDL_Texture *snow_tex = load_snow(renderer);
    if (!bg || !snow_tex) return 1;
    init_snow(snow_tex);
    // ---- INIT SPRITES ----

    // Example frame size (adjust to your real frame size)
    int texW, texH;
    SDL_QueryTexture(IMG_LoadTexture(renderer, "assets/skin/dog.png"),
                     NULL, NULL, &texW, &texH);

    int frameW = texW / 6;
    int frameH = texH / 6;


    int texWt, texHt;
    SDL_QueryTexture(IMG_LoadTexture(renderer, "assets/skin/thief.png"),
                     NULL, NULL, &texWt, &texHt);

    int frameWt = texWt / 6;
    int frameHt = texHt / 6;

    // Positioning (bottom right area)
    int baseY = HEIGHT - frameH - 40;

    // Thief (in front of hoop)



    // Load font
    TTF_Font *font = TTF_OpenFont("assets/fonts/text.ttf", 36);


    if (!font) { printf("Failed to load font\n"); return 1; }

    // Buttons
    Button newGameBtn, continueBtn, levelsBtn, loadingBtn;
    // Hoop texture
    SDL_Texture *hoop = load_texture(renderer, "assets/skin/hoop.png");

    // Sprites
    Sprite thief;
    Sprite dog;
    Sprite plane;
    Sprite sleeper;   // sleeping thief


    init_button(&newGameBtn, renderer, "assets/ui/button.png", 500, 150, 350, 150);
    init_button(&continueBtn, renderer, "assets/ui/button.png", 500, 270, 350, 150);
    init_button(&levelsBtn, renderer, "assets/ui/button.png", 500, 390, 350, 150);
    init_button(&loadingBtn, renderer, "assets/ui/button.png", 500, 510, 350, 150);
    init_sprite(&thief, renderer,
                "assets/skin/thief.png",
                WIDTH - frameW - 120,
                baseY,
                frameWt, frameHt,
                0.08f);

    // Dog (in front of thief)
    init_sprite(&dog, renderer,
                "assets/skin/dog.png",
                WIDTH - 100,
                baseY,
                frameW, frameH,
                0.08f);
    // -------- PLANE --------
    int texWp, texHp;
    SDL_QueryTexture(IMG_LoadTexture(renderer, "assets/skin/plane.png"),
                     NULL, NULL, &texWp, &texHp);

    int planeFrameW = texWp / 6;
    int planeFrameH = texHp / 6;

    init_sprite(&plane, renderer,
                "assets/skin/plane.png",
                WIDTH,          // start at right edge
                40,             // near top
                planeFrameW,
                planeFrameH,
                0.06f);
    plane.destRect.w *= 0.7f;
    plane.destRect.h *= 0.7f;// fast animation



    // -------- SLEEPING THIEF --------
    int texWs, texHs;
    SDL_QueryTexture(IMG_LoadTexture(renderer, "assets/skin/sleep.png"),
                     NULL, NULL, &texWs, &texHs);

    int sleepFrameW = texWs / 6;
    int sleepFrameH = texHs / 6;

    init_sprite(&sleeper, renderer,
                "assets/skin/sleep.png",
                40,                               // near hoop pole
                HEIGHT - sleepFrameH - 40,
                sleepFrameW,
                sleepFrameH,
                0.15f);
        // Scale sleeper (example: smaller)
    sleeper.destRect.w *= 0.35f;
    sleeper.destRect.h *= 0.35f;
    sleeper.destRect.y += 230;
    sleeper.destRect.x += 100;

// slower sleepy animation






    // Button array & labels
    Button *buttons[4] = {&newGameBtn, &continueBtn, &levelsBtn, &loadingBtn};
    const char *labels[4] = {"New Game", "Continue", "Levels", "Loading"};
    SDL_Texture *texts[4];

    for (int i=0;i<4;i++){
        SDL_Color white = {255,255,255,255};
        texts[i] = render_text(renderer, font, labels[i], white);
    }

    int selectedIndex = 0;
    buttons[selectedIndex]->selected = true;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    int running = 1;
    SDL_Event event;
    Uint32 lastTime = SDL_GetTicks();

    while (running) {
        Uint32 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;

            for(int i=0;i<4;i++) handle_button_event(buttons[i], &event);

            if(event.type == SDL_KEYDOWN){
                buttons[selectedIndex]->selected = false;
                if(event.key.keysym.sym == SDLK_DOWN)
                    selectedIndex = (selectedIndex +1)%4;
                else if(event.key.keysym.sym == SDLK_UP)
                    selectedIndex = (selectedIndex -1 +4)%4;
                else if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)
                    buttons[selectedIndex]->state = BTN_PRESSED;
                buttons[selectedIndex]->selected = true;
            }
        }

        update_snow(deltaTime);
        update_sprite(&thief, deltaTime);
        update_sprite(&dog, deltaTime);
        update_plane(&plane, deltaTime, WIDTH);
        update_sprite(&sleeper, deltaTime);



        for(int i=0;i<4;i++) update_button(buttons[i], deltaTime);

        SDL_RenderClear(renderer);
        render_bg(renderer, bg);
        render_snow(renderer);
        render_sprite(&plane, renderer);

        // Render hoop bottom-right
        if (hoop) {
            int hoopW = 200;
            int hoopH = 225;

            SDL_Rect hoopRect = {
                WIDTH - hoopW - 20,
                HEIGHT - hoopH - 20,
                hoopW,
                hoopH
            };
            SDL_RenderCopy(renderer, hoop, NULL, &hoopRect);
        }
        render_sprite(&sleeper, renderer);


        // Render thief (in front of hoop)
        render_sprite(&thief, renderer);

        // Render dog (in front of thief)
        render_sprite(&dog, renderer);

        for(int i=0;i<4;i++){
            render_button(buttons[i], renderer);

            SDL_Color color = buttons[i]->selected ? (SDL_Color){255,200,50,255} : (SDL_Color){255,255,255,255};

            // recreate text only if selection changed (optimization)
            SDL_DestroyTexture(texts[i]);
            texts[i] = render_text(renderer, font, labels[i], color);

            render_text_on_button(renderer, texts[i], buttons[i]);

            if(buttons[i]->clicked){
                printf("%s pressed!\n", labels[i]);
                buttons[i]->clicked = false;
            }
        }
        // Render hoop first (background)
        SDL_Rect hoopRect = {40, HEIGHT - 270, 180, 250 };
        SDL_RenderCopy(renderer, hoop, NULL, &hoopRect);

        // Then thief
        render_sprite(&thief, renderer);

        // Then dog
        render_sprite(&dog, renderer);


        SDL_RenderPresent(renderer);
    }

    // Cleanup
    for(int i=0;i<4;i++){
        destroy_button(buttons[i]);
        SDL_DestroyTexture(texts[i]);
    }
    destroy_snow(snow_tex);
    destroy_bg(bg);
    TTF_CloseFont(font);
    destroy_sprite(&thief);
    destroy_sprite(&dog);
    SDL_DestroyTexture(hoop);
    destroy_sprite(&plane);
    destroy_sprite(&sleeper);
    Mix_FreeMusic(menuMusic);
    Mix_CloseAudio();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
