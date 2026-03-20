#ifndef HEADERS_H
#define HEADERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Window & logical sizes */
#define WINDOW_W 640
#define WINDOW_H 360
#define LOGICAL_W 320
#define LOGICAL_H 180

/* Snow / UI */
#define MAX_SNOW 80
#define MAX_BUTTONS 5

/* Sprite constants (adjust display sizes & speeds here) */
#define KID_FRAMES 36
#define KID_COLUMNS 6
#define KID_DISPLAY_W 15
#define KID_DISPLAY_H 25
#define KID_SPEED 23.5f

#define DOG_FRAMES 36
#define DOG_COLUMNS 6
#define DOG_DISPLAY_W 20
#define DOG_DISPLAY_H 25
#define DOG_SPEED 22.0f

#define THIEF_FRAMES 36
#define THIEF_COLUMNS 6
#define THIEF_DISPLAY_W 15
#define THIEF_DISPLAY_H 25
#define THIEF1_SPEED  18.5f
#define THIEF2_SPEED  20.0f

/* =================== Types =================== */

typedef struct {
    float x, y;
    float speed;
    float drift;
    float angle;
    float rotationSpeed;
    int size;
} Snowflake;

typedef struct {
    SDL_Texture* background;
    SDL_Texture* snow_tex;
    Snowflake snow[MAX_SNOW];
} Background;

typedef struct {
    SDL_Rect rect;
    char label[32];
    int selected;
    float bounceOffset;
    float scale;           // current scale (1.0 = normal)
    float targetScale;     // target scale for animation
    SDL_Texture* texture;  // button PNG texture
} Button;

/* Animated sprite with separate frame and display sizes */
typedef struct {
    SDL_Texture* texture;
    float x, y;
    int frames;        /* total frames in sheet */
    int columns;       /* columns in sheet */
    int frameW, frameH;/* size of one frame in texture */
    int displayW, displayH; /* size drawn on screen (scaled) */
    int currentFrame;
    float frameTimer;
    float frameDelay;
    float speed;       /* horizontal speed in logical pixels/sec */
} AnimatedSprite;

/* =================== Prototypes =================== */

/* SDL init & helper */
int init_SDL(SDL_Window** window, SDL_Renderer** renderer);
SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer);

/* Background */
void init_background(Background* bg, SDL_Renderer* renderer);
void update_background(Background* bg, float dt);
void render_background(Background* bg, SDL_Renderer* renderer);
void destroy_background(Background* bg);

/* Buttons */
void init_buttons(Button buttons[], int count, SDL_Renderer* renderer);
void update_buttons(Button buttons[], int count, int* selected, float dt);
void render_buttons(Button buttons[], int count, SDL_Renderer* renderer);
void destroy_buttons(Button buttons[], int count);

/* Sprites */
int init_sprite(AnimatedSprite* s, SDL_Renderer* renderer,
                const char* path,
                float y,
                float speed,
                int displayW, int displayH,
                int frames, int columns,
                float frameDelay);
void update_sprite(AnimatedSprite* s, float dt);
void render_sprite(AnimatedSprite* s, SDL_Renderer* renderer);
void destroy_sprite(AnimatedSprite* s);

#endif /* HEADERS_H */
