#include "headers.h"

int main()
{
    srand(time(NULL));
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (!init(&window, &renderer)) {
        printf("Failed to initialize!\n");
        return -1;
    }

    // Load textures with error checking
    SDL_Texture* background = loadTexture(renderer, "assets/background.png");
    if (!background) printf("Warning: Failed to load background.png\n");

    SDL_Texture* containerTex = loadTexture(renderer, "assets/ui/container.png");
    if (!containerTex) printf("Warning: Failed to load container.png\n");

    SDL_Texture* enterNameTex = loadTexture(renderer, "assets/ui/enter_name.png");
    if (!enterNameTex) printf("Warning: Failed to load enter_name.png\n");

    // Load sounds with error checking
    Mix_Chunk* clickSound = Mix_LoadWAV("assets/sounds/click.wav");
    if (!clickSound) printf("Warning: Failed to load click.wav - %s\n", Mix_GetError());

    Mix_Chunk* selectSound = Mix_LoadWAV("assets/sounds/select.wav");
    if (!selectSound) printf("Warning: Failed to load select.wav - %s\n", Mix_GetError());

    // Load background music
    Mix_Music* menuMusic = Mix_LoadMUS("assets/sounds/menu.wav");
    if (!menuMusic) {
        printf("Warning: Failed to load menu.wav - %s\n", Mix_GetError());
    } else {
        Mix_PlayMusic(menuMusic, -1);
        Mix_VolumeMusic(64);
    }

    // Load fonts with error checking
    TTF_Font* fontBig = TTF_OpenFont("assets/fonts/default.ttf", 28);
    if (!fontBig) printf("Warning: Failed to load font (28)\n");

    TTF_Font* fontSmall = TTF_OpenFont("assets/fonts/default.ttf", 20);
    if (!fontSmall) printf("Warning: Failed to load font (20)\n");

    Character chars[2];

    initAnimation(&chars[0].idle, renderer, "assets/characters/char1_idle.png", 0.04f, 1);
    initAnimation(&chars[0].select, renderer, "assets/characters/char1_select.png", 0.04f, 0);
    chars[0].name = "Aegon";
    chars[0].health = 5;
    chars[0].speed = 3;
    chars[0].desc1 = "Master of blades";
    chars[0].desc2 = "Fast and agile";
    chars[0].isSelected = 0;
    chars[0].containerRect = (SDL_Rect){SCREEN_WIDTH/2 - 350, 100, 350, 450};

    initAnimation(&chars[1].idle, renderer, "assets/characters/char2_idle.png", 0.055f, 1);
    initAnimation(&chars[1].select, renderer, "assets/characters/char2_select.png", 0.055f, 0);
    chars[1].name = "Nyra";
    chars[1].health = 3;
    chars[1].speed = 5;
    chars[1].desc1 = "Arcane wielder";
    chars[1].desc2 = "High burst damage";
    chars[1].isSelected = 0;
    chars[1].containerRect = (SDL_Rect){SCREEN_WIDTH/2 + 50, 100, 350, 450};

    Button confirmBtn;
    SDL_Texture *snow_tex = load_snow(renderer);
    initButton(&confirmBtn, renderer,
               "assets/ui/confirm.png",
               "assets/ui/confirm.png",
               (SDL_Rect){SCREEN_WIDTH/2 - 100, 600, 200, 80});

    if (snow_tex) {
        init_snow(snow_tex);
    } else {
        printf("Warning: Failed to load snow texture\n");
    }

    Button backBtn;
    initButton(&backBtn, renderer,
               "assets/ui/back.png",
               "assets/ui/back.png",
               (SDL_Rect){20, 100, 120, 60});

    Button multiBtn;
    initButton(&multiBtn, renderer,
               "assets/ui/multi_idle.png",
               "assets/ui/multi_pressed.png",
               (SDL_Rect){1000, SCREEN_HEIGHT - 100, 150, 70});

    GameState state = STATE_FADE_IN;
    int fadeAlpha = 255;

    char playerName[MAX_NAME_LENGTH+1] = "";
    int nameLength = 0;
    int backspaceCount = 0;
    int selectedChar = -1;

    Uint32 last = SDL_GetTicks();
    int running = 1;

    SDL_StartTextInput();

    while (running)
    {
        Uint32 currentTime = SDL_GetTicks();
        float delta = (currentTime - last) / 1000.0f;
        if (delta > 0.1f) delta = 0.1f;
        last = currentTime;

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = 0;

            if (state == STATE_SELECT)
            {
                for (int i=0;i<2;i++)
                {
                    if (e.type == SDL_MOUSEBUTTONDOWN)
                    {
                        int mx = e.button.x;
                        int my = e.button.y;

                        if (SDL_PointInRect(&(SDL_Point){mx,my}, &chars[i].containerRect))
                        {
                            if (selectedChar != i)
                            {
                                if (selectSound) Mix_PlayChannel(-1, selectSound, 0);
                                selectedChar = i;
                                chars[i].select.currentFrame = 0;
                                chars[i].select.finished = 0;
                            }
                        }
                    }
                }

                if (handleButtonEvent(&confirmBtn, &e) && selectedChar != -1)
                {
                    if (clickSound) Mix_PlayChannel(-1, clickSound, 0);
                    state = STATE_NAME_INPUT;
                }

                if (handleButtonEvent(&backBtn, &e))
                {
                    printf("Back pressed\n");
                }

                if (handleButtonEvent(&multiBtn, &e))
                {
                    printf("Multiplayer pressed\n");
                }
            }
            else if (state == STATE_NAME_INPUT)
            {
                if (e.type == SDL_TEXTINPUT && nameLength < MAX_NAME_LENGTH)
                {
                    strcat(playerName, e.text.text);
                    nameLength++;
                }

                if (e.type == SDL_KEYDOWN)
                {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && nameLength > 0 && backspaceCount < 2)
                    {
                        playerName[nameLength-1] = '\0';
                        nameLength--;
                        backspaceCount++;
                    }

                    if (e.key.keysym.sym == SDLK_RETURN && selectedChar != -1)
                    {
                        savePlayer(playerName, selectedChar+1);
                        state = STATE_FADE_OUT;
                    }
                }
            }
        }

        if (state == STATE_FADE_IN)
        {
            fadeAlpha -= 300 * delta;
            if (fadeAlpha <= 0)
            {
                fadeAlpha = 0;
                state = STATE_SELECT;
            }
        }

        if (state == STATE_FADE_OUT)
        {
            fadeAlpha += 300 * delta;
            if (fadeAlpha >= 255)
            {
                state = STATE_EXIT;
                running = 0;
            }
        }

        if (snow_tex) update_snow(delta);

        for (int i=0;i<2;i++)
        {
            if (selectedChar == i && !chars[i].select.finished)
                updateAnimation(&chars[i].select, delta);
            else
                updateAnimation(&chars[i].idle, delta);
        }

        SDL_RenderClear(renderer);

        if (background) SDL_RenderCopy(renderer, background, NULL, NULL);
        if (snow_tex) render_snow(renderer);

        for (int i=0;i<2;i++)
        {
            if (containerTex) SDL_RenderCopy(renderer, containerTex, NULL, &chars[i].containerRect);

            if (selectedChar == i && !chars[i].select.finished)
                renderAnimation(renderer, &chars[i].select,
                                chars[i].containerRect.x + 112,
                                chars[i].containerRect.y + 133,
                                125,125);
            else
                renderAnimation(renderer, &chars[i].idle,
                                chars[i].containerRect.x + 128,
                                chars[i].containerRect.y + 137,
                                90,120);

            SDL_Color white = {255,255,255};
            if (fontBig) {
                renderText(renderer, fontBig, chars[i].name, white,
                           chars[i].containerRect.x + 140,
                           chars[i].containerRect.y + 40);
            }

            char stats[64];
            sprintf(stats,"Health: %d  Speed: %d",chars[i].health,chars[i].speed);
            if (fontSmall) {
                renderText(renderer, fontSmall, stats, white,
                           chars[i].containerRect.x + 100,
                           chars[i].containerRect.y + 330);

                renderText(renderer, fontSmall, chars[i].desc1, white,
                           chars[i].containerRect.x + 100,
                           chars[i].containerRect.y + 365);
            }
        }

        renderButton(renderer, &confirmBtn, delta);
        renderButton(renderer, &backBtn, delta);
        renderButton(renderer, &multiBtn, delta);

        if (state == STATE_NAME_INPUT)
        {
            if (enterNameTex) {
                SDL_Rect dst = {SCREEN_WIDTH/2 - 90, 560, 300, 160};
                SDL_RenderCopy(renderer, enterNameTex, NULL, &dst);
            }

            SDL_Color white = {255,255,255};
            if (fontBig) {
                renderText(renderer, fontBig, playerName, white,
                           SCREEN_WIDTH/2 - 50, 632);
            }
        }

        renderFade(renderer, fadeAlpha);

        SDL_RenderPresent(renderer);
    }

    // Cleanup
    if (snow_tex) destroy_snow(snow_tex);
    if (menuMusic) Mix_FreeMusic(menuMusic);
    if (clickSound) Mix_FreeChunk(clickSound);
    if (selectSound) Mix_FreeChunk(selectSound);
    if (fontBig) TTF_CloseFont(fontBig);
    if (fontSmall) TTF_CloseFont(fontSmall);

    closeAll(window, renderer);
    return 0;
}
