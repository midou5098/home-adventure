#include "headers.h"

/* ---------------- SDL init & texture ---------------- */
int init_SDL(SDL_Window** window, SDL_Renderer** renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 0;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init: %s", IMG_GetError());
        return 0;
    }
    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init: %s", TTF_GetError());
        return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio: %s", Mix_GetError());
        return 0;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    *window = SDL_CreateWindow("Home Adventure",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_W, WINDOW_H, 0);
    if (!*window) { SDL_Log("CreateWindow: %s", SDL_GetError()); return 0; }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) { SDL_Log("CreateRenderer: %s", SDL_GetError()); return 0; }

    SDL_RenderSetLogicalSize(*renderer, LOGICAL_W, LOGICAL_H);
    SDL_RenderSetIntegerScale(*renderer, SDL_TRUE);

    return 1;
}
SDL_Rect get_button_dest(Button* b)
{
    int scaled_w = (int)(b->rect.w * b->scale);
    int scaled_h = (int)(b->rect.h * b->scale);
    int scaled_x = b->rect.x - (scaled_w - b->rect.w) / 2;
    int scaled_y = b->rect.y - (scaled_h - b->rect.h) / 2;

    SDL_Rect dest = {scaled_x, scaled_y, scaled_w, scaled_h};
    dest.y += (int)b->bounceOffset;

    return dest;
}
SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer)
{
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        SDL_Log("IMG_Load(%s): %s", path, IMG_GetError());
        return NULL;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) SDL_Log("CreateTextureFromSurface(%s): %s", path, SDL_GetError());
    return tex;
}

/* ---------------- Background ---------------- */
void init_background(Background* bg, SDL_Renderer* renderer)
{
    bg->background = load_texture("assets/background/background.png", renderer);
    bg->snow_tex   = load_texture("assets/background/snow.png", renderer);

    srand((unsigned)time(NULL));
    for (int i = 0; i < MAX_SNOW; ++i) {
        bg->snow[i].x = (float)(rand() % LOGICAL_W);
        bg->snow[i].y = (float)(rand() % LOGICAL_H);
        bg->snow[i].speed = 6.0f + (rand() % 12);
        bg->snow[i].drift = -6.0f + (rand() % 12);
        bg->snow[i].angle = (float)(rand() % 360);
        bg->snow[i].rotationSpeed = -20.0f + (rand() % 40);
        bg->snow[i].size = 2 + (rand() % 3);
    }
}

void update_background(Background* bg, float dt)
{
    for (int i = 0; i < MAX_SNOW; ++i) {
        bg->snow[i].y += bg->snow[i].speed * dt;
        bg->snow[i].x += bg->snow[i].drift * dt;
        bg->snow[i].angle += bg->snow[i].rotationSpeed * dt;

        if (bg->snow[i].y > LOGICAL_H) {
            bg->snow[i].y = -4.0f;
            bg->snow[i].x = (float)(rand() % LOGICAL_W);
        }
        if (bg->snow[i].x < -8.0f) bg->snow[i].x = (float)LOGICAL_W;
        if (bg->snow[i].x > LOGICAL_W + 8.0f) bg->snow[i].x = 0.0f;
    }
}

void render_background(Background* bg, SDL_Renderer* renderer)
{
    if (bg->background) {
        SDL_Rect dst = {0, 0, LOGICAL_W, LOGICAL_H};
        SDL_RenderCopy(renderer, bg->background, NULL, &dst);
    }

    if (bg->snow_tex) {
        for (int i = 0; i < MAX_SNOW; ++i) {
            SDL_Rect r = {(int)bg->snow[i].x, (int)bg->snow[i].y, bg->snow[i].size, bg->snow[i].size};
            SDL_RenderCopyEx(renderer, bg->snow_tex, NULL, &r, bg->snow[i].angle, NULL, SDL_FLIP_NONE);
        }
    }
}

void destroy_background(Background* bg)
{
    if (bg->background) SDL_DestroyTexture(bg->background);
    if (bg->snow_tex) SDL_DestroyTexture(bg->snow_tex);
}

/* ---------------- Buttons with PNG textures ---------------- */
void init_buttons(Button buttons[], int count, SDL_Renderer* renderer)
{
    const char* filenames[MAX_BUTTONS] = {
        "assets/buttons/start.png",
        "assets/buttons/options.png",
        "assets/buttons/topscores.png",
        "assets/buttons/story.png",
        "assets/buttons/quit.png"
    };

    int yStart = 40;
    int spacing = 30;
    int button_w = 60;  // keep consistent
    int button_h = 20;  // keep consistent

    for (int i = 0; i < count; ++i) {
        buttons[i].texture = load_texture(filenames[i], renderer);

        buttons[i].rect.x = 130;
        buttons[i].rect.y = yStart + i * spacing;
        buttons[i].rect.w = 60;  // use 60, not 20
        buttons[i].rect.h = 20;  // use 20, not 10

        buttons[i].selected = 0;
        buttons[i].bounceOffset = 0.0f;
        buttons[i].scale = 1.0f;
        buttons[i].targetScale = 1.0f;
    }
}

void update_buttons(Button buttons[], int count, int* selected, float dt)
{
    for (int i = 0; i < count; ++i) {
        buttons[i].selected = (i == *selected);

        if (buttons[i].selected) {
            buttons[i].targetScale = 1.2f;
        } else {
            buttons[i].targetScale = 1.0f;
        }

        float diff = buttons[i].targetScale - buttons[i].scale;
        buttons[i].scale += diff * 10.0f * dt;

        if (buttons[i].bounceOffset < 0.0f) {
            buttons[i].bounceOffset += 60.0f * dt;
            if (buttons[i].bounceOffset > 0.0f) buttons[i].bounceOffset = 0.0f;
        }
    }
}

void render_buttons(Button buttons[], int count, SDL_Renderer* renderer)
{
    for (int i = 0; i < count; ++i) {
        Button* b = &buttons[i];

        if (b->texture) {

            SDL_Rect dest = get_button_dest(b);

            if (b->selected) {
                SDL_SetTextureColorMod(b->texture, 255, 220, 100);
            } else {
                SDL_SetTextureColorMod(b->texture, 255, 255, 255);
            }

            SDL_RenderCopy(renderer, b->texture, NULL, &dest);
        }
    }
}

void destroy_buttons(Button buttons[], int count)
{
    for (int i = 0; i < count; ++i) {
        if (buttons[i].texture) {
            SDL_DestroyTexture(buttons[i].texture);
            buttons[i].texture = NULL;
        }
    }
}

/* ---------------- Sprites ---------------- */
int init_sprite(AnimatedSprite* s, SDL_Renderer* renderer,
                const char* path, float y, float speed,
                int displayW, int displayH, int frames, int columns, float frameDelay)
{
    if (!s || !renderer || !path) return 0;

    s->texture = load_texture(path, renderer);
    if (!s->texture) {
        SDL_Log("init_sprite: failed to load %s", path);
        return 0;
    }

    int texW = 0, texH = 0;
    SDL_QueryTexture(s->texture, NULL, NULL, &texW, &texH);

    s->frames = frames;
    s->columns = columns > 0 ? columns : 1;
    int rows = (frames + s->columns - 1) / s->columns;

    s->frameW = texW / s->columns;
    s->frameH = texH / (rows > 0 ? rows : 1);
    s->displayW = displayW;
    s->displayH = displayH;
    s->currentFrame = 0;
    s->frameTimer = 0.0f;
    s->frameDelay = (frameDelay > 0.0f) ? frameDelay : 0.05f;
    s->speed = speed;
    s->x = - (float)s->displayW;
    s->y = y;

    return 1;
}

void update_sprite(AnimatedSprite* s, float dt)
{
    if (!s || !s->texture) return;

    s->x += s->speed * dt;
    if (s->x > LOGICAL_W) s->x = - (float)s->displayW;

    s->frameTimer += dt;
    if (s->frameTimer >= s->frameDelay) {
        s->frameTimer -= s->frameDelay;
        s->currentFrame = (s->currentFrame + 1) % s->frames;
    }
}

void render_sprite(AnimatedSprite* s, SDL_Renderer* renderer)
{
    if (!s || !s->texture) return;

    int col = s->currentFrame % s->columns;
    int row = s->currentFrame / s->columns;

    SDL_Rect src = { col * s->frameW, row * s->frameH, s->frameW, s->frameH };
    SDL_Rect dst = { (int)s->x, (int)s->y, s->displayW, s->displayH };
    SDL_RenderCopy(renderer, s->texture, &src, &dst);
}

void destroy_sprite(AnimatedSprite* s)
{
    if (s && s->texture) {
        SDL_DestroyTexture(s->texture);
        s->texture = NULL;
    }
}
