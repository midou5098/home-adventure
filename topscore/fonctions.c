#include "headers.h"
#include <stdio.h>

Snow snowflakes[SNOW_MAX];
// ---------------- New Sprites ----------------
AnimatedSprite car;
AnimatedSprite helicopter;


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
        snowflakes[i].speed = 40.0f + rand() % 80;      // vertical speed
        snowflakes[i].drift = -30.0f + rand() % 60;     // horizontal drift
        snowflakes[i].size = 10 + rand() % 3;
        snowflakes[i].tex = tex;
    }
}
// ================= LEADERBOARD PANEL =================

LeaderboardPanel init_leaderboard(SDL_Renderer *renderer, int x, int y, int w, int h)
{
    LeaderboardPanel panel;

    SDL_Surface *surface = IMG_Load("assets/ui/border.png");
    if (!surface) {
        printf("Error loading border.png: %s\n", IMG_GetError());
        panel.texture = NULL;
        return panel;
    }

    panel.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    panel.rect.x = x;
    panel.rect.y = y;
    panel.rect.w = w;
    panel.rect.h = h;

    return panel;
}

void update_leaderboard(LeaderboardPanel *panel)
{
    // For now nothing to update.
    // You could animate glow, pulse, etc. later.
}

void render_leaderboard(SDL_Renderer *renderer, LeaderboardPanel *panel)
{
    if (panel->texture)
        SDL_RenderCopy(renderer, panel->texture, NULL, &panel->rect);
}

void destroy_leaderboard(LeaderboardPanel *panel)
{
    if (panel->texture)
        SDL_DestroyTexture(panel->texture);

    panel->texture = NULL;
}

void update_snow(float delta)
{
    for (int i = 0; i < SNOW_MAX; i++) {

        snowflakes[i].y += snowflakes[i].speed * delta;
        snowflakes[i].x += snowflakes[i].drift * delta;

        // reset vertically
        if (snowflakes[i].y > HEIGHT) {
            snowflakes[i].y = -snowflakes[i].size;
            snowflakes[i].x = rand() % WIDTH;
        }

        // wrap horizontally
        if (snowflakes[i].x > WIDTH)
            snowflakes[i].x = 0;

        if (snowflakes[i].x < 0)
            snowflakes[i].x = WIDTH;
    }
}

void render_snow(SDL_Renderer *renderer)
{
    for (int i = 0; i < SNOW_MAX; i++) {
        SDL_Rect dst = {
            (int)snowflakes[i].x,
            (int)snowflakes[i].y,
            snowflakes[i].size+10,
            snowflakes[i].size+10
        };

        SDL_RenderCopy(renderer, snowflakes[i].tex, NULL, &dst);
    }
}

void destroy_snow(SDL_Texture *tex)
{
    SDL_DestroyTexture(tex);
}
// ================= BUTTON =================

Button init_button(SDL_Renderer *renderer, int desiredWidth)
{
    Button button;

    SDL_Surface *surface = IMG_Load("assets/ui/button.png");
    if (!surface) {
        printf("Error loading button.png: %s\n", IMG_GetError());
        button.texture = NULL;
        return button;
    }
    button.texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    // Preserve aspect ratio

    button.rect.w = 180;
    button.rect.h = 120;

    button.rect.x = WIDTH - button.rect.w - 40;
    button.rect.y = (HEIGHT / 2) - (button.rect.h / 2);

    button.hovered = 0;
    button.clicked = 0;

    return button;
}


void update_button(Button *button, SDL_Event *event)
{
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    button->hovered =
        (mouseX >= button->rect.x &&
         mouseX <= button->rect.x + button->rect.w &&
         mouseY >= button->rect.y &&
         mouseY <= button->rect.y + button->rect.h);

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        button->hovered)
    {
        button->clicked = 1;
        printf("Button clicked!\n");
    }
}
SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer) {
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        SDL_Log("IMG_Load failed: %s", IMG_GetError());
        return NULL;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

void render_button(SDL_Renderer *renderer, Button *button)
{
    if (!button->texture) return;

    SDL_RenderCopy(renderer, button->texture, NULL, &button->rect);

    // Optional: visual feedback on hover
    //if (button->hovered) {
       // SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      //  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
     //   SDL_RenderFillRect(renderer, &button->rect);
   // }
}

void destroy_button(Button *button)
{
    if (button->texture)
        SDL_DestroyTexture(button->texture);

    button->texture = NULL;
}









int init_sprite(AnimatedSprite* s, SDL_Renderer* renderer,
                const char* path, float y, float speed,
                int displayW, int displayH, int frames, int columns, float frameDelay,int startx)
{
    if (!s || !renderer || !path) return 0;
    s->texture = load_texture(path, renderer);
    if (!s->texture) {
        SDL_Log("init_sprite: failed to load %s", path);
        return 0;
    }

    int texW = 0, texH = 0;
    SDL_QueryTexture(s->texture, NULL, NULL, &texW, &texH);
    if (texW <= 0 || texH <= 0) {
        SDL_Log("init_sprite: invalid texture size %s (%d,%d)", path, texW, texH);
        return 0;
    }

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

    s->x = startx; /* start off left */
    s->y = y; /* top position already computed by caller */

    return 1;
}

void update_sprite(AnimatedSprite* s, float dt)
{
    if (!s || !s->texture) return;
    s->x += s->speed * dt;
    if (s->x > WIDTH) s->x = - (float)s->displayW;

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
// ---------------- CAR ----------------
int init_car(SDL_Renderer* renderer, float y) {
    return init_sprite(&car, renderer,
                       "assets/decorations/thief_car.png",
                       y,
                       CAR_SPEED,
                       CAR_DISPLAY_W,
                       CAR_DISPLAY_H,
                       CAR_FRAMES,
                       CAR_COLUMNS,
                       0.05f,-10.0f);
}

void update_car(float dt) {
    update_sprite(&car, dt);
}

void render_car(SDL_Renderer* renderer) {
    render_sprite(&car, renderer);
}

void destroy_car() {
    destroy_sprite(&car);
}

// ---------------- HELICOPTER ----------------
int init_helicopter(SDL_Renderer* renderer, float y) {
    return init_sprite(&helicopter, renderer,
                       "assets/decorations/helicopter.png",
                       y,
                       HELI_SPEED,
                       HELI_DISPLAY_W,
                       HELI_DISPLAY_H,
                       HELI_FRAMES,
                       HELI_COLUMNS,
                       0.05f,-10.0f);
}

void update_helicopter(float dt) {
    update_sprite(&helicopter, dt);
}

void render_helicopter(SDL_Renderer* renderer) {
    render_sprite(&helicopter, renderer);
}

void destroy_helicopter() {
    destroy_sprite(&helicopter);
}





































