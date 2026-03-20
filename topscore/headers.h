#ifndef HEADERS_H
#define HEADERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 1280
#define HEIGHT 720
#define SNOW_MAX 100


#define KID_FRAMES 36
#define KID_COLUMNS 6
#define KID_DISPLAY_W 60
#define KID_DISPLAY_H 100
#define KID_SPEED 70.0f
// ================= CAR =================
#define CAR_FRAMES 36
#define CAR_COLUMNS 6
#define CAR_DISPLAY_W 180
#define CAR_DISPLAY_H 70
#define CAR_SPEED 60.0f   // pixels/sec

// ================= HELICOPTER =================
#define HELI_FRAMES 36
#define HELI_COLUMNS 6
#define HELI_DISPLAY_W 110
#define HELI_DISPLAY_H 80
#define HELI_SPEED 60.0f   // pixels/sec

// Declare new sprites



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


extern AnimatedSprite car;
extern AnimatedSprite helicopter;




// ================= BUTTON =================

typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect;
    int hovered;
    int clicked;
} Button;

Button init_button(SDL_Renderer *renderer, int desiredWidth);
void update_button(Button *button, SDL_Event *event);
void render_button(SDL_Renderer *renderer, Button *button);
void destroy_button(Button *button);

// ================= LEADERBOARD PANEL =================

typedef struct {
    SDL_Texture *texture;
    SDL_Rect rect;
} LeaderboardPanel;

LeaderboardPanel init_leaderboard(SDL_Renderer *renderer, int x, int y, int w, int h);
void update_leaderboard(LeaderboardPanel *panel);   // optional
void render_leaderboard(SDL_Renderer *renderer, LeaderboardPanel *panel);
void destroy_leaderboard(LeaderboardPanel *panel);

typedef struct {
    float x, y;
    float speed;
    float drift;
    int size;
    SDL_Texture *tex;
} Snow;

extern Snow snowflakes[SNOW_MAX];


// Background
SDL_Texture* init_bg(SDL_Renderer *renderer);
void render_bg(SDL_Renderer *renderer, SDL_Texture *bg);
void destroy_bg(SDL_Texture *bg);

// Snow
SDL_Texture* load_snow(SDL_Renderer *renderer);
void init_snow(SDL_Texture *tex);
void update_snow(float delta);
void render_snow(SDL_Renderer *renderer);
void destroy_snow(SDL_Texture *tex);








// CAR
int init_car(SDL_Renderer* renderer, float y);
void update_car(float dt);
void render_car(SDL_Renderer* renderer);
void destroy_car(void);

// HELICOPTER
int init_helicopter(SDL_Renderer* renderer, float y);
void update_helicopter(float dt);
void render_helicopter(SDL_Renderer* renderer);
void destroy_helicopter(void);









int init_sprite(AnimatedSprite* s, SDL_Renderer* renderer,const char* path, float y, float speed,int displayW, int displayH, int frames, int columns, float frameDelay,int stratx);
void update_sprite(AnimatedSprite* s, float dt);
void render_sprite(AnimatedSprite* s, SDL_Renderer* renderer);
void destroy_sprite(AnimatedSprite* s);








































#endif
