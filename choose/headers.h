#ifndef HEADERS_H
#define HEADERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define FRAME_COLS 6
#define FRAME_ROWS 6
#define TOTAL_FRAMES 36
#define SNOW_MAX 100
typedef struct {
    float x, y;
    float speed;
    float drift;
    int size;
    SDL_Texture *tex;
} Snow;
extern Snow snowflakes[SNOW_MAX];

#define MAX_NAME_LENGTH 10

typedef enum {
    STATE_FADE_IN,
    STATE_SELECT,
    STATE_NAME_INPUT,
    STATE_FADE_OUT,
    STATE_EXIT
} GameState;

typedef struct {
    SDL_Texture* texture;
    int frameWidth;
    int frameHeight;
    int currentFrame;
    float frameTime;
    float frameDelay;
    int loop;
    int finished;
} Animation;

typedef struct {
    Animation idle;
    Animation select;
    int isSelected;
    SDL_Rect containerRect;
    const char* name;
    int health;
    int speed;
    const char* desc1;
    const char* desc2;
} Character;

typedef struct {
    Animation idle;
    Animation pressed;
    int isPressed;
    SDL_Rect rect;
    float scale;
    float targetScale;
    int isStatic;
    float pressTimer;
} Button;

SDL_Texture* load_snow(SDL_Renderer *renderer);
void init_snow(SDL_Texture *tex);
void update_snow(float delta);
void render_snow(SDL_Renderer *renderer);
void destroy_snow(SDL_Texture *tex);

/* Core */
int init(SDL_Window** window, SDL_Renderer** renderer);
void closeAll(SDL_Window* window, SDL_Renderer* renderer);

/* Animation */
void initAnimation(Animation* anim, SDL_Renderer* renderer, const char* path, float delay, int loop);
void updateAnimation(Animation* anim, float delta);
void renderAnimation(SDL_Renderer* renderer, Animation* anim, int x, int y, int w, int h);

/* Button */
void initButton(Button* btn, SDL_Renderer* renderer, const char* idlePath, const char* pressedPath, SDL_Rect rect);
void renderButton(SDL_Renderer* renderer, Button* btn, float delta);
int handleButtonEvent(Button* btn, SDL_Event* e);

/* Fade */
void renderFade(SDL_Renderer* renderer, int alpha);

/* File */
void savePlayer(const char* name, int characterID);

/* Utility */
SDL_Texture* loadTexture(SDL_Renderer* renderer, const char* path);
void renderText(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color, int x, int y);

#endif
