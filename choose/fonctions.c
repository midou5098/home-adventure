#include "headers.h"
Snow snowflakes[SNOW_MAX];

SDL_Texture* load_snow(SDL_Renderer *renderer)
{
    SDL_Surface *surface = IMG_Load("assets/ui/snow.png");
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
        snowflakes[i].x = rand() % 1280;
        snowflakes[i].y = rand() % 720;
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

        if (snowflakes[i].y > 720) {
            snowflakes[i].y = -snowflakes[i].size;
            snowflakes[i].x = rand() % 1280;
        }

        if (snowflakes[i].x > 1280)
            snowflakes[i].x = 0;
        if (snowflakes[i].x < 0)
            snowflakes[i].x = 1280;
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
    if (tex) SDL_DestroyTexture(tex);
}

int init(SDL_Window** window, SDL_Renderer** renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL Init failed: %s\n", SDL_GetError());
        return 0;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG Init failed: %s\n", IMG_GetError());
        return 0;
    }
    if (TTF_Init() == -1) {
        printf("TTF Init failed: %s\n", TTF_GetError());
        return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("Mix Init failed: %s\n", Mix_GetError());
        return 0;
    }

    *window = SDL_CreateWindow("Character Select",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                SCREEN_WIDTH,
                                SCREEN_HEIGHT,
                                0);
    if (!*window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        return 0;
    }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    if (!*renderer) {
        printf("Renderer creation failed: %s\n", SDL_GetError());
        return 0;
    }

    return 1;
}

void closeAll(SDL_Window* window, SDL_Renderer* renderer)
{
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path)
{
    SDL_Surface* surf = IMG_Load(path);
    if (!surf) {
        printf("Failed to load texture: %s - %s\n", path, IMG_GetError());
        return NULL;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    if (!tex) {
        printf("Failed to create texture: %s\n", SDL_GetError());
    }
    return tex;
}

void initAnimation(Animation* anim, SDL_Renderer* renderer, const char* path, float delay, int loop)
{
    if (!anim || !renderer || !path) {
        printf("initAnimation: NULL parameter\n");
        return;
    }

    anim->texture = loadTexture(renderer, path);
    if (!anim->texture) {
        printf("Failed to load animation texture: %s\n", path);
        return;
    }

    int w, h;
    SDL_QueryTexture(anim->texture, NULL, NULL, &w, &h);

    anim->frameWidth = w / FRAME_COLS;
    anim->frameHeight = h / FRAME_ROWS;
    anim->currentFrame = 0;
    anim->frameTime = 0.f;
    anim->frameDelay = delay;
    anim->loop = loop;
    anim->finished = 0;
}

void updateAnimation(Animation* anim, float delta)
{
    if (!anim) return;
    if (anim->finished) return;

    anim->frameTime += delta;

    if (anim->frameTime >= anim->frameDelay)
    {
        anim->frameTime = 0.0f;
        anim->currentFrame++;

        if (anim->currentFrame >= TOTAL_FRAMES)
        {
            if (anim->loop)
                anim->currentFrame = 0;
            else
            {
                anim->currentFrame = TOTAL_FRAMES - 1;
                anim->finished = 1;
            }
        }
    }
}

void renderAnimation(SDL_Renderer* renderer, Animation* anim, int x, int y, int w, int h)
{
    if (!anim || !anim->texture) return;

    SDL_Rect src;
    src.x = (anim->currentFrame % FRAME_COLS) * anim->frameWidth;
    src.y = (anim->currentFrame / FRAME_COLS) * anim->frameHeight;
    src.w = anim->frameWidth;
    src.h = anim->frameHeight;

    SDL_Rect dst = { x, y, w, h };
    SDL_RenderCopy(renderer, anim->texture, &src, &dst);
}

void initButton(Button* btn, SDL_Renderer* renderer, const char* idlePath, const char* pressedPath, SDL_Rect rect)
{
    if (!btn || !renderer || !idlePath) {
        printf("initButton: NULL parameter\n");
        return;
    }

    // Determine button type based on path
    if (strstr(idlePath, "multi") != NULL) {
        // Multi button is animated (6x6 sprite sheet)
        btn->isStatic = 0;
        initAnimation(&btn->idle, renderer, idlePath, 0.03f, 1);
        initAnimation(&btn->pressed, renderer, pressedPath, 0.03f, 0);
    } else {
        // Confirm and Back buttons are static PNGs
        btn->isStatic = 1;

        // Load static textures
        btn->idle.texture = loadTexture(renderer, idlePath);
        btn->pressed.texture = loadTexture(renderer, pressedPath);

        if (!btn->idle.texture) {
            printf("Failed to load button texture: %s\n", idlePath);
        }
        if (!btn->pressed.texture) {
            printf("Failed to load button texture: %s\n", pressedPath);
        }

        // Set dummy animation values
        btn->idle.frameWidth = 0;
        btn->idle.frameHeight = 0;
        btn->pressed.frameWidth = 0;
        btn->pressed.frameHeight = 0;
    }

    btn->rect = rect;
    btn->isPressed = 0;
    btn->scale = 1.0f;
    btn->targetScale = 1.0f;
    btn->pressTimer = 0.0f;
}

void renderButton(SDL_Renderer* renderer, Button* btn, float delta)
{
    if (!btn) return;

    // Smooth scale interpolation
    if (btn->scale < btn->targetScale) {
        btn->scale += 5.0f * delta;
        if (btn->scale > btn->targetScale) btn->scale = btn->targetScale;
    } else if (btn->scale > btn->targetScale) {
        btn->scale -= 5.0f * delta;
        if (btn->scale < btn->targetScale) btn->scale = btn->targetScale;
    }

    // Calculate scaled rectangle (centered)
    SDL_Rect scaledRect = {
        btn->rect.x + (btn->rect.w * (1 - btn->scale)) / 2,
        btn->rect.y + (btn->rect.h * (1 - btn->scale)) / 2,
        btn->rect.w * btn->scale,
        btn->rect.h * btn->scale
    };

    if (btn->isPressed)
    {
        if (btn->isStatic) {
            // Static button pressed state
            if (btn->pressed.texture) {
                SDL_RenderCopy(renderer, btn->pressed.texture, NULL, &scaledRect);
            }
            btn->pressTimer += delta;
            if (btn->pressTimer > 0.1f) {
                btn->isPressed = 0;
                btn->targetScale = 1.0f;
                btn->pressTimer = 0.0f;
            }
        } else {
            // Animated button pressed state
            updateAnimation(&btn->pressed, delta);
            if (btn->pressed.texture) {
                renderAnimation(renderer, &btn->pressed, scaledRect.x, scaledRect.y, scaledRect.w, scaledRect.h);
            }
            if (btn->pressed.finished) {
                btn->isPressed = 0;
                btn->targetScale = 1.0f;
            }
        }
    }
    else
    {
        if (btn->isStatic) {
            // Static button idle state
            if (btn->idle.texture) {
                SDL_RenderCopy(renderer, btn->idle.texture, NULL, &scaledRect);
            }
        } else {
            // Animated button idle state
            updateAnimation(&btn->idle, delta);
            if (btn->idle.texture) {
                renderAnimation(renderer, &btn->idle, scaledRect.x, scaledRect.y, scaledRect.w, scaledRect.h);
            }
        }
    }
}

int handleButtonEvent(Button* btn, SDL_Event* e)
{
    if (!btn || !e) return 0;

    if (e->type == SDL_MOUSEBUTTONDOWN)
    {
        int mx = e->button.x;
        int my = e->button.y;

        if (SDL_PointInRect(&(SDL_Point){mx,my}, &btn->rect))
        {
            btn->isPressed = 1;
            if (!btn->isStatic) {
                btn->pressed.currentFrame = 0;
                btn->pressed.finished = 0;
            }
            btn->targetScale = 0.8f;
            btn->pressTimer = 0.0f;
            return 1;
        }
    }
    return 0;
}

void renderFade(SDL_Renderer* renderer, int alpha)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
    SDL_Rect r = {0,0,SCREEN_WIDTH,SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &r);
}

void savePlayer(const char* name, int characterID)
{
    mkdir("saves", 0777);

    char path[256];
    sprintf(path, "saves/%s.txt", name);

    FILE* f = fopen(path, "w");
    if (!f) {
        printf("Failed to save player: %s\n", path);
        return;
    }

    fprintf(f, "%s,%d,0,1", name, characterID);
    fclose(f);
    printf("Player saved: %s\n", path);
}

void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color, int x, int y)
{
    if (!renderer || !font || !text) return;

    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = { x, y, surf->w, surf->h };

    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}
