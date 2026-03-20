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

#define MAX_BULLETS 64
#define MENU_ITEMS 6
#define MAX_SNOW 128

typedef struct {
    float x, y;
    float speed;
} Snowflake;

extern Snowflake snow[MAX_SNOW];

void InitSnow(void);
void UpdateSnow(float delta);
void RenderSnow(void);
void snow_quit(void);
// ================= STATE =================
typedef enum {
    STATE_OPTIONS,
    STATE_CREDITS
} AppState;

// ================= SETTINGS =================
typedef struct {
    int master;
    int music;
    int vfx;
    int brightness;
    int fullscreen;
} Settings;

// ================= SHOOTER =================
typedef struct {
    SDL_Texture* texture;
    float x, y;
    float animTimer;
    int frame;
    float shootTimer;
    float shootInterval;
    int firedThisCycle;
    int direction;
    int frameW;
    int frameH;
} Shooter;

// ================= BULLET =================
typedef struct {
    float x, y;
    float speed;
    int active;
} Bullet;

// ===== GLOBALS (defined in main.c) =====
extern SDL_Renderer* renderer;
extern SDL_Window* window;
extern Settings settings;
extern AppState state;
extern SDL_Texture* bgTexture;
extern SDL_Texture* upTex; // kid+dog looking up

extern Shooter leftShooter;
extern Shooter rightShooter;
extern Bullet bullets[MAX_BULLETS];
extern SDL_Texture* bulletTex;
extern SDL_Texture* brickTex;

extern TTF_Font* font;

/* UI icons/textures */
extern SDL_Texture* barFullTex;
extern SDL_Texture* barEmptyTex;
extern SDL_Texture* volIcons[4];
extern SDL_Texture* sunIcons[3];

/* ----------------- functions ----------------- */
void InitFont(void);
void CleanupFont(void);

void InitBackground(void);
void CleanupBackground(void);

void LoadSettings(void);
void SaveSettings(void);
void ApplySettings(void);

void InitAudio(void);
void CleanupAudio(void);
void PlayClick(void);
void PlayShootLeft(void);
void PlayShootRight(void);

void InitShooters(void);
void CleanupShooters(void);
void UpdateShooters(float delta);
void RenderShooters(void);

void LoadCreditsFromFile(void);
void InitCredits(void);
void UpdateCredits(float delta);
void RenderCredits(void);

void RenderOptionsMenu(int selected);
void RenderBrightnessOverlay(void);
void InitUpSprite(void);
void RenderUpSprite(void);
void CleanupUpSprite(void);

/* ---- inline utility ---- */
static inline int clamp_int(int v, int a, int b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

#endif
