#include "headers.h"
#include <stdio.h>
#include <math.h>

Snow snowflakes[SNOW_MAX];

// ================= BACKGROUND =================
SDL_Texture* init_bg(SDL_Renderer *renderer)
{
    SDL_Surface *surface = IMG_Load("assets/decorations/background.png");
    if (!surface) {
        printf("Error loading background: %s\n", IMG_GetError());
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void render_bg(SDL_Renderer *renderer, SDL_Texture *bg)
{
    SDL_RenderCopy(renderer, bg, NULL, NULL);
}

void destroy_bg(SDL_Texture *bg)
{
    SDL_DestroyTexture(bg);
}

// ================= SNOW =================
SDL_Texture* load_snow(SDL_Renderer *renderer)
{
    SDL_Surface *surface = IMG_Load("assets/decorations/snow.png");
    if (!surface) {
        printf("Error loading snow: %s\n", IMG_GetError());
        return NULL;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void init_snow(SDL_Texture *tex)
{
    for (int i = 0; i < SNOW_MAX; i++) {
        snowflakes[i].x = rand() % WIDTH;
        snowflakes[i].y = rand() % HEIGHT;
        snowflakes[i].speed = 40.0f + rand() % 80;
        snowflakes[i].drift = -30.0f + rand() % 60;
        snowflakes[i].size = 10 + rand() % 3;
        snowflakes[i].tex = tex;
    }
}

void update_snow(float delta)
{
    for (int i = 0; i < SNOW_MAX; i++) {
        snowflakes[i].y += snowflakes[i].speed * delta;
        snowflakes[i].x += snowflakes[i].drift * delta;
        if (snowflakes[i].y > HEIGHT) {
            snowflakes[i].y = -snowflakes[i].size;
            snowflakes[i].x = rand() % WIDTH;
        }
        if (snowflakes[i].x > WIDTH) snowflakes[i].x = 0;
        if (snowflakes[i].x < 0) snowflakes[i].x = WIDTH;
    }
}

void render_snow(SDL_Renderer *renderer)
{
    for (int i = 0; i < SNOW_MAX; i++) {
        SDL_Rect dst = {(int)snowflakes[i].x, (int)snowflakes[i].y, snowflakes[i].size+10, snowflakes[i].size+10};
        SDL_RenderCopy(renderer, snowflakes[i].tex, NULL, &dst);
    }
}

void destroy_snow(SDL_Texture *tex)
{
    SDL_DestroyTexture(tex);
}

// ================= BUTTON =================
bool point_in_rect(int x, int y, SDL_Rect *r)
{
    return (x >= r->x && x <= r->x + r->w &&
            y >= r->y && y <= r->y + r->h);
}

void init_button(Button *b, SDL_Renderer *renderer,
                 const char *path, int x, int y, int w, int h)
{
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        printf("IMG_Load Error: %s\n", IMG_GetError());
        return;
    }
    b->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    b->rect.x = x;
    b->rect.y = y;
    b->rect.w = w;
    b->rect.h = h;

    b->state = BTN_IDLE;
    b->scale = 1.0f;
    b->animTime = 0.0f;
    b->animDuration = 0.15f;
    b->clicked = false;
    b->selected = false;
}

void handle_button_event(Button *b, SDL_Event *event)
{
    if (event->type == SDL_MOUSEBUTTONDOWN)
    {
        int mx = event->button.x;
        int my = event->button.y;

        if (point_in_rect(mx, my, &b->rect))
        {
            b->state = BTN_PRESSED;
            b->animTime = 0.0f;
            b->clicked = false;
        }
    }
}

void update_button(Button *b, float deltaTime)
{
    if (b->state == BTN_PRESSED)
    {
        b->animTime += deltaTime;
        float t = b->animTime / b->animDuration;

        if (t >= 1.0f)
        {
            b->state = BTN_IDLE;
            b->scale = 1.0f;
            b->clicked = true;
        }
        else
        {
            float ease = sinf(t * M_PI);
            b->scale = 1.0f - 0.1f * ease;
        }
    }
}

void render_button(Button *b, SDL_Renderer *renderer)
{
    SDL_Rect dst = b->rect;

    int newW = (int)(dst.w * b->scale);
    int newH = (int)(dst.h * b->scale);

    dst.x += (dst.w - newW) / 2;
    dst.y += (dst.h - newH) / 2;
    dst.w = newW;
    dst.h = newH;

    SDL_RenderCopy(renderer, b->texture, NULL, &dst);
}

void destroy_button(Button *b)
{
    SDL_DestroyTexture(b->texture);
}

// ================= TEXT =================
SDL_Texture* render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color)
{
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

void render_text_on_button(SDL_Renderer *renderer, SDL_Texture *textTex, Button *btn)
{
    int texW, texH;
    SDL_QueryTexture(textTex, NULL, NULL, &texW, &texH);

    // compute current scaled rectangle
    int newW = (int)(btn->rect.w * btn->scale);
    int newH = (int)(btn->rect.h * btn->scale);
    int offsetX = (btn->rect.w - newW)/2-5;
    int offsetY = (btn->rect.h - newH)/2+5;

    SDL_Rect dst;
    dst.w = texW;
    dst.h = texH;
    dst.x = btn->rect.x + offsetX + (newW - texW)/2;
    dst.y = btn->rect.y + offsetY + (newH - texH)/2;

    SDL_RenderCopy(renderer, textTex, NULL, &dst);
}
SDL_Texture* load_texture(SDL_Renderer *renderer, const char *path)
{
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        printf("Error loading %s: %s\n", path, IMG_GetError());
        return NULL;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return tex;
}
void init_sprite(Sprite *sprite, SDL_Renderer *renderer,
                 const char *path,
                 int x, int y,
                 int frameW, int frameH,
                 float speed)
{
    sprite->texture = IMG_LoadTexture(renderer, path);

    sprite->frameWidth = frameW;
    sprite->frameHeight = frameH;

    sprite->columns = 6;   // because sheet is 6x6
    sprite->rows = 6;
    sprite->totalFrames = 36;

    sprite->currentFrame = 0;
    sprite->frameTime = 0.0f;
    sprite->frameSpeed = speed;

    sprite->srcRect.x = 0;
    sprite->srcRect.y = 0;
    sprite->srcRect.w = frameW;
    sprite->srcRect.h = frameH;

    sprite->destRect.x = x;
    sprite->destRect.y = y;
    sprite->destRect.w = frameW;
    sprite->destRect.h = frameH;
}

void update_sprite(Sprite *sprite, float deltaTime)
{
    sprite->frameTime += deltaTime;

    if (sprite->frameTime >= sprite->frameSpeed)
    {
        sprite->frameTime = 0.0f;

        sprite->currentFrame++;
        if (sprite->currentFrame >= sprite->totalFrames)
            sprite->currentFrame = 0;

        int column = sprite->currentFrame % sprite->columns;
        int row    = sprite->currentFrame / sprite->columns;

        sprite->srcRect.x = column * sprite->frameWidth;
        sprite->srcRect.y = row * sprite->frameHeight;
    }
}
void update_plane(Sprite *plane, float deltaTime, int screenWidth)
{
    // Animate frames normally
    update_sprite(plane, deltaTime);

    // Move from right to left
    plane->destRect.x -= (int)(300 * deltaTime); // speed = 300 px/sec

    // If fully off left side, reset to right
    if (plane->destRect.x + plane->destRect.w < 0)
    {
        plane->destRect.x = screenWidth;
    }
}
void render_sprite(Sprite *sprite, SDL_Renderer *renderer)
{
    SDL_RenderCopy(renderer,
                   sprite->texture,
                   &sprite->srcRect,
                   &sprite->destRect);
}

void destroy_sprite(Sprite *s)
{
    SDL_DestroyTexture(s->texture);
}

