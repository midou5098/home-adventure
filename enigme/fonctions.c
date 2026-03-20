#include "headers.h"

int InitSDL(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 0;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        return 0;
    }

    if (TTF_Init() < 0)
    {
        printf("TTF_Init Error: %s\n", TTF_GetError());
        return 0;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        printf("Mix_OpenAudio Error: %s\n", Mix_GetError());
        return 0;
    }

    window = SDL_CreateWindow(
        "Enigma",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );

    if (!window)
    {
        printf("Window Error: %s\n", SDL_GetError());
        return 0;
    }

    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer)
    {
        printf("Renderer Error: %s\n", SDL_GetError());
        return 0;
    }

    font = TTF_OpenFont("assets/font.ttf", 28);
    if (!font)
    {
        printf("Font Error: %s\n", TTF_GetError());
        return 0;
    }

    music = Mix_LoadMUS("assets/music/menu.wav");
    if (!music)
        printf("Music warning: %s\n", Mix_GetError());
    else
        Mix_PlayMusic(music, -1);

    return 1;
}


void CleanupSDL(void)
{
    if (music) Mix_FreeMusic(music);
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    Mix_CloseAudio();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

/* ---------- SPRITE ---------- */
void InitSprite(Sprite* s,const char* path,
                int x,int y,int w,int h,int delay,int loop)
{
    SDL_Surface* surf = IMG_Load(path);
    s->texture = SDL_CreateTextureFromSurface(renderer,surf);
    SDL_FreeSurface(surf);

    int tw,th;
    SDL_QueryTexture(s->texture,NULL,NULL,&tw,&th);

    s->frameWidth = tw/FRAME_COLS;
    s->frameHeight = th/FRAME_ROWS;

    s->srcRect = (SDL_Rect){0,0,s->frameWidth,s->frameHeight};
    s->destRect = (SDL_Rect){x,y,w,h};
    s->currentFrame = 0;
    s->frameDelay = delay;
    s->lastUpdate = SDL_GetTicks();
    s->loop = loop;
}

void UpdateSprite(Sprite* s)
{
    Uint32 now = SDL_GetTicks();
    if (now - s->lastUpdate > (Uint32)s->frameDelay)
    {
        s->currentFrame++;

        if (s->currentFrame >= TOTAL_FRAMES)
            s->currentFrame = s->loop ? 0 : TOTAL_FRAMES-1;

        int row = s->currentFrame/FRAME_COLS;
        int col = s->currentFrame%FRAME_COLS;

        s->srcRect.x = col*s->frameWidth;
        s->srcRect.y = row*s->frameHeight;

        s->lastUpdate = now;
    }
}

void RenderSprite(Sprite* s)
{
    SDL_RenderCopy(renderer,s->texture,&s->srcRect,&s->destRect);
}

void DestroySprite(Sprite* s)
{
    if (s->texture)
    {
        SDL_DestroyTexture(s->texture);
        s->texture = NULL;
    }
}

/* ---------- BUTTON ---------- */
void InitButton(Button* b,const char* path,
                const char* text,int x,int y,int w,int h)
{
    SDL_Surface* surf = IMG_Load(path);
    if (!surf)
    {
        printf("Button image error (%s): %s\n", path, IMG_GetError());
        b->texture = NULL;
        return;
    }

    b->texture = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    b->rect = (SDL_Rect){x,y,w,h};
    strcpy(b->text,text);
    b->visible = 1;
}


void RenderButton(Button* b)
{
    if (!b->texture) return;


    SDL_RenderCopy(renderer,b->texture,NULL,&b->rect);

    SDL_Color white = {255,255,255,255};
    SDL_Surface* surf = TTF_RenderText_Blended(font,b->text,white);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer,surf);

    SDL_Rect t = {
        b->rect.x + (b->rect.w - surf->w)/2,
        b->rect.y + (b->rect.h - surf->h)/2,
        surf->w,surf->h
    };

    SDL_RenderCopy(renderer,tex,NULL,&t);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

int ButtonClicked(Button* b,int mx,int my)
{
    if (!b->visible) return 0;

    return (mx>=b->rect.x &&
            mx<=b->rect.x+b->rect.w &&
            my>=b->rect.y &&
            my<=b->rect.y+b->rect.h);
}

/* ---------- TEXT ---------- */
void RenderText(const char* text,int x,int y)
{
    SDL_Color white={255,255,255,255};
    SDL_Surface* surf = TTF_RenderText_Blended(font,text,white);
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer,surf);

    SDL_Rect r={x,y,surf->w,surf->h};
    SDL_RenderCopy(renderer,tex,NULL,&r);

    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

/* ---------- FADE ---------- */
void RenderFade(int alpha)
{
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0,0,0,alpha);
    SDL_Rect r={0,0,SCREEN_WIDTH,SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer,&r);
}
