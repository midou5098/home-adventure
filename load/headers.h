#ifndef HEADERS_H
#define HEADERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#include <SDL2/SDL_ttf.h>  // added for text rendering
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define WIDTH 1280
#define HEIGHT 720
#define SNOW_MAX 100

typedef struct {
    float x, y;
    float speed;
    float drift;
    int size;
    SDL_Texture *tex;
} Snow;

extern Snow snowflakes[SNOW_MAX];

typedef enum {
    BTN_IDLE,
    BTN_HOVER,
    BTN_PRESSED
} ButtonState;

typedef struct {
    SDL_Rect rect;
    SDL_Texture *texture;

    ButtonState state;

    float scale;
    float animTime;
    float animDuration;

    bool clicked;   // becomes true when animation finishes
    bool selected;  // new field for keyboard selection
} Button;
/* ---------- SPRITE (6x6 SHEET) ---------- */
typedef struct {
    SDL_Texture *texture;
    SDL_Rect srcRect;
    SDL_Rect destRect;

    int frameWidth;
    int frameHeight;

    int columns;     // 6
    int rows;        // 6
    int totalFrames; // 36

    int currentFrame;
    float frameTime;
    float frameSpeed;
} Sprite;


/* Sprite functions */
SDL_Texture* load_texture(SDL_Renderer *renderer, const char *path);
void init_sprite(Sprite *s, SDL_Renderer *renderer, const char *path,
                 int x, int y, int w, int h, float frameTime);
void update_sprite(Sprite *s, float deltaTime);
void render_sprite(Sprite *s, SDL_Renderer *renderer);
void destroy_sprite(Sprite *s);


/* ---------- BUTTON ---------- */
bool point_in_rect(int x, int y, SDL_Rect *r);
void init_button(Button *b, SDL_Renderer *renderer,
                 const char *path, int x, int y, int w, int h);
void handle_button_event(Button *b, SDL_Event *event);
void update_button(Button *b, float deltaTime);
void render_button(Button *b, SDL_Renderer *renderer);
void destroy_button(Button *b);

/* ---------- BACKGROUND ---------- */
SDL_Texture* init_bg(SDL_Renderer *renderer);
void render_bg(SDL_Renderer *renderer, SDL_Texture *bg);
void destroy_bg(SDL_Texture *bg);

/* ---------- SNOW ---------- */
SDL_Texture* load_snow(SDL_Renderer *renderer);
void init_snow(SDL_Texture *tex);
void update_snow(float delta);
void render_snow(SDL_Renderer *renderer);
void destroy_snow(SDL_Texture *tex);

/* ---------- TEXT ---------- */
SDL_Texture* render_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color);
void render_text_on_button(SDL_Renderer *renderer, SDL_Texture *textTex, Button *btn);
void update_plane(Sprite *plane, float deltaTime, int screenWidth);

#endif
