#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "../shared/session.h"

#define PLACEHOLDER_W 1280
#define PLACEHOLDER_H 720
#define PLACEHOLDER_DURATION_MS 1800

static TTF_Font *open_placeholder_font(int size)
{
    const char *paths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
        NULL
    };
    int i;

    for (i = 0; paths[i]; ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) return font;
    }
    return NULL;
}

static void draw_centered_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, int y, SDL_Color color)
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect dst;

    if (!renderer || !font || !text) return;

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        dst.x = (PLACEHOLDER_W - surface->w) / 2;
        dst.y = y;
        dst.w = surface->w;
        dst.h = surface->h;
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void render_placeholder(SDL_Renderer *renderer, TTF_Font *title_font, TTF_Font *body_font)
{
    SDL_Color title = {245, 235, 180, 255};
    SDL_Color body = {220, 232, 238, 255};
    SDL_Rect band = {0, 250, PLACEHOLDER_W, 220};
    SDL_Rect stripe = {0, 468, PLACEHOLDER_W, 6};

    SDL_SetRenderDrawColor(renderer, 22, 29, 35, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 38, 55, 64, 255);
    SDL_RenderFillRect(renderer, &band);
    SDL_SetRenderDrawColor(renderer, 218, 138, 55, 255);
    SDL_RenderFillRect(renderer, &stripe);

    if (title_font && body_font) {
        draw_centered_text(renderer, title_font, "LEVEL 3 PLACEHOLDER", 292, title);
        draw_centered_text(renderer, body_font, "This level is reserved for the next chapter.", 374, body);
        draw_centered_text(renderer, body_font, "Continuing to Level 4...", 414, body);
    }

    SDL_RenderPresent(renderer);
}

int runLevel3(GameSession *session, SDL_Window *window, SDL_Renderer *renderer)
{
    TTF_Font *title_font;
    TTF_Font *body_font;
    Uint32 start_ticks;
    int running = 1;

    if (!window || !renderer) {
        fprintf(stderr, "Level 3 placeholder requires a shared SDL window and renderer.\n");
        return 1;
    }

    SDL_SetWindowTitle(window, "Home Alone - Level 3 Placeholder");
    SDL_RenderSetLogicalSize(renderer, PLACEHOLDER_W, PLACEHOLDER_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    title_font = open_placeholder_font(46);
    body_font = open_placeholder_font(24);
    start_ticks = SDL_GetTicks();

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                if (session) session->quit_requested = 1;
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    if (session) session->quit_requested = 1;
                    running = 0;
                } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    running = 0;
                }
            }
        }

        render_placeholder(renderer, title_font, body_font);
        if (SDL_GetTicks() - start_ticks >= PLACEHOLDER_DURATION_MS) {
            running = 0;
        }
        SDL_Delay(16);
    }

    if (title_font) TTF_CloseFont(title_font);
    if (body_font) TTF_CloseFont(body_font);

    if (session && !session->quit_requested) {
        session->level3.completed = 1;
        session->level3.points = 0;
        session_calculate_total_points(session);
    }

    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_SetWindowTitle(window, "Home Alone - Merged");
    return 0;
}
