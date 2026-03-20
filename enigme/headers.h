#ifndef HEADERS_H
#define HEADERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define FRAME_COLS 6
#define FRAME_ROWS 6
#define TOTAL_FRAMES 36

typedef struct
{
    SDL_Texture* texture;
    SDL_Rect srcRect;
    SDL_Rect destRect;
    int frameWidth;
    int frameHeight;
    int currentFrame;
    int frameDelay;
    Uint32 lastUpdate;
    int loop;
} Sprite;

typedef struct
{
    SDL_Texture* texture;
    SDL_Rect rect;
    char text[128];
    int visible;
} Button;

typedef enum
{
    STATE_FADE_IN,
    STATE_DIALOGUE,
    STATE_CHOICES,
    STATE_RESULT_ANIM,
    STATE_FADE_OUT,
    STATE_EXIT
} EnigmaState;

/* ===== GLOBALS (defined in main.c) ===== */
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern TTF_Font* font;
extern Mix_Music* music;

extern SDL_Texture* background;
extern Sprite character;
extern Button buttons[4];

extern EnigmaState state;
extern int fadeAlpha;
extern int correctAnswer;
extern int resultFinished;

/* ---- Core ---- */
int InitSDL(void);
void CleanupSDL(void);

/* ---- Sprite ---- */
void InitSprite(Sprite* s, const char* path,
                int x,int y,int w,int h,int delay,int loop);
void UpdateSprite(Sprite* s);
void RenderSprite(Sprite* s);
void DestroySprite(Sprite* s);

/* ---- Button ---- */
void InitButton(Button* b, const char* path,
                const char* text,int x,int y,int w,int h);
void RenderButton(Button* b);
int ButtonClicked(Button* b,int mx,int my);

/* ---- Text ---- */
void RenderText(const char* text,int x,int y);

/* ---- Fade ---- */
void RenderFade(int alpha);

#endif
