#include "headers.h"

int main(void)
{
    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (!init_SDL(&window, &renderer)) return 1;

    /* Background */
    Background bg;
    init_background(&bg, renderer);

    /* Sprite positioning */
    float sprite_bottom_margin = 6.0f;

    float kid_y   = (float)(LOGICAL_H - KID_DISPLAY_H - sprite_bottom_margin);
    float dog_y   = (float)(LOGICAL_H - DOG_DISPLAY_H - sprite_bottom_margin);
    float thief_y = (float)(LOGICAL_H - THIEF_DISPLAY_H - sprite_bottom_margin);

    AnimatedSprite kid   = {0};
    AnimatedSprite dog   = {0};
    AnimatedSprite thief1 = {0};
    AnimatedSprite thief2 = {0};

    if (!init_sprite(&kid, renderer,
        "assets/background/kid.png",
        kid_y, KID_SPEED,
        KID_DISPLAY_W, KID_DISPLAY_H,
        KID_FRAMES, KID_COLUMNS, 0.06f)) return 1;

    if (!init_sprite(&dog, renderer,
        "assets/background/dog.png",
        dog_y, DOG_SPEED,
        DOG_DISPLAY_W, DOG_DISPLAY_H,
        DOG_FRAMES, DOG_COLUMNS, 0.06f)) return 1;

    if (!init_sprite(&thief1, renderer,
        "assets/background/thief1.png",
        thief_y, THIEF1_SPEED,
        THIEF_DISPLAY_W, THIEF_DISPLAY_H,
        THIEF_FRAMES, THIEF_COLUMNS, 0.06f)) return 1;

    if (!init_sprite(&thief2, renderer,
        "assets/background/thief2.png",
        thief_y, THIEF2_SPEED,
        THIEF_DISPLAY_W, THIEF_DISPLAY_H,
        THIEF_FRAMES, THIEF_COLUMNS, 0.06f)) return 1;

    /* Buttons - now with PNG textures */
    Button buttons[MAX_BUTTONS];
    init_buttons(buttons, MAX_BUTTONS, renderer);

    /* Fonts - still needed for title maybe */
    TTF_Font* titleFont  = TTF_OpenFont("assets/fonts/title.ttf", 20);

    /* Music */
    Mix_Chunk* menuMusic = Mix_LoadWAV("assets/sound/main.wav");
    if (menuMusic)
        Mix_PlayChannel(-1, menuMusic, -1);

    /* Thief delay system */
    float thiefDelay = 0.5f;
    int thievesStarted = 0;

    /* Main loop */
    int running = 1;
    int selected = 0;
    SDL_Event ev;

    Uint32 last = SDL_GetTicks();

    while (running)
    {
        Uint32 now = SDL_GetTicks();
        float dt = (now - last) / 1000.0f;
        last = now;

        /* -------- Events -------- */
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                running = 0;

            if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_UP) {
                    selected--;
                    if (selected < 0) selected = MAX_BUTTONS - 1;
                }

                if (ev.key.keysym.sym == SDLK_DOWN) {
                    selected++;
                    if (selected >= MAX_BUTTONS) selected = 0;
                }

                if (ev.key.keysym.sym == SDLK_RETURN) {
                    if (selected == 4)
                        running = 0;
                }
            }

            if (ev.type == SDL_MOUSEMOTION)
                {
                    float lx, ly;
                    SDL_RenderWindowToLogical(renderer,
                                              ev.motion.x,
                                              ev.motion.y,
                                              &lx, &ly);

                    int mx = (int)lx;
                    int my = (int)ly;

                    for (int i = 0; i < MAX_BUTTONS; ++i)
                    {
                        Button* b = &buttons[i];

                        int scaled_w = (int)(b->rect.w * b->scale);
                        int scaled_h = (int)(b->rect.h * b->scale);
                        int scaled_x = b->rect.x - (scaled_w - b->rect.w) / 2;
                        int scaled_y = b->rect.y - (scaled_h - b->rect.h) / 2;

                        // Add bounce offset
                        scaled_y += (int)b->bounceOffset;

                        SDL_Rect scaled_r = {scaled_x, scaled_y, scaled_w, scaled_h};

                        if (mx >= scaled_r.x-300 && mx <= scaled_r.x + scaled_r.w &&
                            my >= scaled_r.y-200 && my <= scaled_r.y + scaled_r.h)
                        {
                            selected = i;
                            break;
                        }
                    }
                }

            if (ev.type == SDL_MOUSEBUTTONDOWN &&
    ev.button.button == SDL_BUTTON_LEFT)
            {
                // Recalculate which button is under the mouse at click time
                float lx, ly;
                SDL_RenderWindowToLogical(renderer,
                                          ev.button.x,
                                          ev.button.y,
                                          &lx, &ly);

                int mx = (int)lx;
                int my = (int)ly;

                for (int i = 0; i < MAX_BUTTONS; ++i)
                {
                    Button* b = &buttons[i];

                    int scaled_w = (int)(b->rect.w * b->scale);
                    int scaled_h = (int)(b->rect.h * b->scale);
                    int scaled_x = b->rect.x - (scaled_w - b->rect.w) / 2;
                    int scaled_y = b->rect.y - (scaled_h - b->rect.h) / 2;
                    scaled_y += (int)b->bounceOffset;

                    SDL_Rect scaled_r = {scaled_x, scaled_y, scaled_w, scaled_h};

                    if (mx >= scaled_r.x && mx <= scaled_r.x + scaled_r.w &&
                        my >= scaled_r.y && my <= scaled_r.y + scaled_r.h)
                    {
                        if (i == 4)  // Quit button
                            running = 0;
                        break;
                    }
                }
            }
        }

        /* -------- Update -------- */

        update_background(&bg, dt);
        update_buttons(buttons, MAX_BUTTONS, &selected, dt);

        /* Thief delay logic */
        if (!thievesStarted) {
            thiefDelay -= dt;
            if (thiefDelay <= 0.0f)
                thievesStarted = 1;
        }

        update_sprite(&kid, dt);
        update_sprite(&dog, dt);

        if (thievesStarted) {
            update_sprite(&thief1, dt);
            update_sprite(&thief2, dt);
        }

        /* -------- Render -------- */

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        render_background(&bg, renderer);

        render_sprite(&kid, renderer);
        render_sprite(&dog, renderer);
        render_sprite(&thief1, renderer);
        render_sprite(&thief2, renderer);

        // Draw title if font exists
        if (titleFont) {
            SDL_Color white = {255,255,255,255};
            SDL_Surface* surf = TTF_RenderText_Solid(titleFont, "Home Adventure", white);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect r = {(LOGICAL_W - surf->w)/2, 8, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, NULL, &r);
                SDL_FreeSurface(surf);
                if (tex) SDL_DestroyTexture(tex);
            }
        }

        render_buttons(buttons, MAX_BUTTONS, renderer);

        SDL_RenderPresent(renderer);
    }

    /* -------- Cleanup -------- */

    destroy_background(&bg);
    destroy_sprite(&kid);
    destroy_sprite(&dog);
    destroy_sprite(&thief1);
    destroy_sprite(&thief2);
    destroy_buttons(buttons, MAX_BUTTONS);

    if (titleFont)  TTF_CloseFont(titleFont);
    if (menuMusic)  Mix_FreeChunk(menuMusic);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
