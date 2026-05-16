#ifndef LEVEL1_TYPES_H
#define LEVEL1_TYPES_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct { float worldY; int gapX; } Floor;

typedef struct {
    float x, y, vx, vy;
    int   onGround, facingRight;
    int   jetpackFloors;
    Uint32 magnetTimer;
    Uint32 shoesTimer;
    int   extraHearts;
    int   buyCountJetpack;
    int   buyCountMagnet;
    int   buyCountShoes;
} Player;

typedef struct { float x, y; int collected; } Key;
typedef struct { float x, y; int active, floorIdx; } House;

typedef enum {
    GS_INTRO_FADE_IN,
    GS_INTRO_TITLE,
    GS_INTRO_FADE_OUT,
    GS_DIALOGUE,
    GS_PLAYING,
    GS_SECOND_CHANCE,
    GS_COUNTDOWN,
    GS_TRANSITION,
    GS_GAME_OVER,
    GS_FINAL_CAMPAN,
    GS_FINAL_DIALOGUE,
    GS_CONSEQUENCE,
    GS_DOOR_OPENING,
    GS_HARRY_ENTER,
    GS_CREDITS
} GameState;

typedef struct {
    SDL_Texture *tex;
    TTF_Font    *font;
    int          w, h;
    Uint8        r, g, b;
    Uint32       lastUsed;
    char         text[512];
} TextCacheEntry;

typedef struct {
    SDL_Texture *tex;
    int          w, h;
} TextureSizeCacheEntry;

typedef struct {
    int       valid;
    TTF_Font *font;
    int       maxW;
    int       maxLines;
    int       lineCount;
    char      text[512];
    char      lines[8][256];
} WrapCache;

typedef struct {
    int moveLeftHeld;
    int moveRightHeld;
    int moveJumpHeld;
    int moveLeftHeldP2;
    int moveRightHeldP2;
    int moveJumpHeldP2;
    int jumpPressed;
    int jumpReleased;
    int jumpPressedP2;
    int jumpReleasedP2;
    int interactPressed;
    int interactReleased;
    int confirmPressed;
    int confirmReleased;
} InputSnapshot;

typedef struct {
    int    pending;
    Uint32 pressedAtMs;
} ActionBuffer;

typedef enum {
    ACT_JUMP = 0,
    ACT_INTERACT,
    ACT_CONFIRM,
    ACT_CHOICE_TOGGLE,
    ACT_COUNT
} BufferedAction;

typedef struct {
    float x;
    float speed;
    int   dir;
    int   frame;
    Uint32 lastTick;
    int   row;
} Rat;

typedef struct {
    float   x;
    float   floorY;
    int     floorIdx;
    int     dir;
    int     leftBound;
    int     rightBound;
    int     frame;
    Uint32  lastTick;
    int     active;
} Dog;

typedef enum { GAUGE_IDLE, GAUGE_AGITATE, GAUGE_FLOWING, GAUGE_DRAINING } GaugePhase;

typedef struct {
    float x, y;
    float vx;
    float vy;
    int   w, h;
    int   alive;
    float alpha;
} WaterDrop;

typedef struct {
    float      floorY;
    int        floorIdx;
    int        nozzleX;
    GaugePhase phase;
    Uint32     phaseStart;
    float      flowX;
    int        animFrame;
    Uint32     animTick;
    int        active;
    int        started;
    WaterDrop  drops[80];
    Uint32     lastSpawn;
} Gauge;

typedef struct {
    float  y;
    float  angle;
    float  angleSpd;
    int    texIdx;
    int    srcY;
    float  wobble;
    float  wobbleSpd;
} DebrisPiece;

typedef enum {
    DEB_IDLE,
    DEB_WARNING,
    DEB_FALLING,
} DebrisPhase;

typedef enum { PANIM_IDLE, PANIM_RUN, PANIM_JUMP } PlayerAnim;
typedef enum { CHAR_HARRY = 0, CHAR_MARV = 1 } DlgCharacter;

typedef struct {
    DlgCharacter speaker;
    const char  *text;
} DlgLine;

typedef enum {
    DLGSEQ_INTRO      = 0,
    DLGSEQ_SEWER      = 1,
    DLGSEQ_STREET     = 2,
    DLGSEQ_POSTFRIDGE = 3,
    DLGSEQ_MICROWAVE  = 4,
    DLGSEQ_POSTMICRO  = 5,
    DLGSEQ_FINAL_PRE  = 6,
    DLGSEQ_FINAL_GUN  = 7,
    DLGSEQ_FINAL_FORG = 8,
    DLGSEQ_MARV_REACT = 9,
} DlgSeqID;

typedef enum { FWALK_NONE, FWALK_IDLE, FWALK_RUNNING, FWALK_DONE } FinalWalkPhase;

typedef enum {
    DOOR_CLOSED,
    DOOR_OPENING,
    DOOR_OPEN,
    DOOR_CLOSING,
} DoorState;

typedef enum {
    FRIDGE_IDLE,
    FRIDGE_COUNTDOWN,
    FRIDGE_SHAKING,
    FRIDGE_DROPPING,
    FRIDGE_FADING,
} FridgePhase;

typedef enum {
    MW_IDLE,
    MW_WARNING,
    MW_DROPPING,
    MW_FADING,
} MwPhase;

#endif
