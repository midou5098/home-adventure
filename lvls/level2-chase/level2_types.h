#ifndef LEVEL2_TYPES_H
#define LEVEL2_TYPES_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct { float x, y; float angle; int active; } SkyBomb;

typedef enum {
    GS_KEY_SELECT,
    GS_INTRO_FADE_IN,
    GS_INTRO_TITLE,
    GS_INTRO_FADE_OUT,
    GS_DIALOGUE,
    GS_CHASE_RUN,
    GS_END_ROOM,
    GS_MINI_TRANSITION_IN,
    GS_MINI_LEVEL,
    GS_MINI_TRANSITION_OUT,
    GS_END_FADE,
    GS_GAME_OVER,
} GameState;

typedef enum {
    PHASE_OBJECTS,
    PHASE_SLAM,
    PHASE_PLATFORMS,
    PHASE_PATTERN_PLATFORMS,
    PHASE_RETURN_GROUND,
    PHASE_MICROWAVE,
    PHASE_KEVIN_ESCAPE,
    PHASE_SLOWDOWN,
} ChasePhase;

typedef enum { CHAR_HARRY = 0, CHAR_MARV = 1 } DlgCharacter;

typedef struct {
    float x, y, vx, vy;
    int   onGround;
    int   facingRight;
    int   animFrame;
    Uint32 animLastTick;
    int   lives;
    int   invincible;
    Uint32 invincStart;
    int   keepGoingShow;
    Uint32 keepGoingStart;
    int   dead;
    Uint32 deadSince;
    int   hasKey;
} Player;

typedef struct {
    float x, y, vx, vy;
    int   onGround;
    int   animFrame;
    Uint32 animLastTick;
    int   animFps;
    float targetX;
    int   reEntryDone;
    int   lives;
    int   invincible;
    Uint32 invincStart;
    int   keepGoingShow;
    Uint32 keepGoingStart;
    int   dead;
    Uint32 deadSince;
} Marv;

typedef struct {
    float x, y, vx, vy;
    int   active;
    int   big;
} Obj;

typedef struct {
    float x, y;
    int   w, h;
    int   active;
    int   trap;
    int   trapTriggered;
    float vy;
} Platform;

typedef struct {
    DlgCharacter speaker;
    const char  *text;
} DlgLine;

typedef struct {
    int used;
    TTF_Font *font;
    Uint8 r, g, b;
    char *text;
    SDL_Texture *texture;
    int w, h;
    Uint32 lastUsed;
} TextCacheEntry;

typedef struct { float x, vx; int active; int animFrame; Uint32 animLastTick; } Rat;
typedef struct { int x, y, w, h; } StaticPlat;
typedef struct { float x, y; int w, h; } MiniPlat;

/* Defaults for static platform layouts. */
static StaticPlat endPlats[3] = {
    {900,  GROUND_Y - 130, 150, 16},
    {1050, GROUND_Y - 260, 130, 16},
    {1150, GROUND_Y - 390, 120, 16},
};

static MiniPlat miniPlats[7] = {
    {160.0f, 555.0f, 110, 16},
    {360.0f, 430.0f, 100, 16},
    {550.0f, 315.0f,  80, 16},
    {730.0f, 210.0f,  60, 16},
    {890.0f, 460.0f, 105, 16},
    {1060.0f,340.0f,  75, 16},
    {1160.0f,195.0f,  80, 16},
};

#endif
