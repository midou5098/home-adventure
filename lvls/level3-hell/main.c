
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

#include "../shared/session.h"
#include "../shared/arcade_input.h"
#include "../../src/options/options_scene.h"
#include "online_client.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SCREEN_W        1280
#define SCREEN_H        720
#define PLAYER_W        32
#define PLAYER_H        48
#define PLAYER_SPEED    4.5f
#define GRAVITY         0.55f
#define JUMP_VY        -13.0f
#define LVL1_JUMP_VY   -15.0f
#define MAX_FALL        18.0f
#define PLAT_H          18
#define PLAT_W          110
#define MAX_LEVELS      6
#define PLAYER_SKIN_COUNT 2
#define BACKGROUND_PATH      "assets/background1.png"
#define BACKGROUND6_PATH     "assets/background2.png"

#define SPIKE1_W        50
#define TOOTH_H         18
#define TOOTH_COUNT     3

#define SLIDE_W         55
#define SLIDE_H         200
#define SLIDE_TEETH     6
#define SLIDE_SPD       4.5f
#define SLIDE_FALL_ROT_SPD 5.0f
#define SLIDE_DIST      100.0f
#define SLIDE_FALL_SHIFT_X (PLAYER_W * 2)

#define LVL2_PLAT_W     80
#define LVL2_PLAT_Y     400
#define LVL2_PLAT_H     18
#define LVL2_JUMP_VY   -15.0f
#define LVL4_JUMP_VY   -14.0f
#define LVL5_JUMP_VY   -10.0f
#define SAW_FRAME_SIZE  38
#define SAW_FRAME_COUNT 8
#define SAW_HITBOX      30
#define SAW_OFFSET_Y   -6
#define FIRE_FRAME_DELAY 90
#define FIRE_PAD_X       4
#define FIRE_RENDER_H   160
#define FIRE_SHEET_COLS  6
#define FIRE_SHEET_ROWS  6
#define LVL4_LAZER_SHEET_COLS 5
#define LVL4_LAZER_SHEET_ROWS 5
#define LVL4_LAZER_FRAME_DELAY 45
#define ELECTRISE_SHEET_COLS 5
#define ELECTRISE_SHEET_ROWS 5
#define ELECTRISE_FRAME_DELAY 18
#define BURN_SHEET_COLS 4
#define BURN_SHEET_ROWS 4
#define BURN_FRAME_DELAY 55
#define DUST_SHEET_COLS 5
#define DUST_SHEET_ROWS 5
#define DUST_FRAME_DELAY 55
#define TOUCH_SHEET_COLS 5
#define TOUCH_SHEET_ROWS 5
#define TOUCH_FRAME_DELAY 0
#define TOUCH_FRAME_STEP 25
#define TOUCH_FLOOR_OFFSET 10
#define ICE_FRAME_DELAY 80
#define LVL6_FREEZE_SLIDE_SPEED 4.8f
#define DEATH_SCREEN_DELAY_MS 600
#define AUTO_RESTART_DELAY_MS 1400
#define LEVEL_TRANSITION_FADE_MS 650
#define INTRO_FRAME_COUNT 121
#define INTRO_FRAME_DELAY_MS 42
#define INTRO_SCAN_MAX_FILES 256

#define IDLE_DW   PLAYER_W
#define IDLE_DH   PLAYER_H
#define RUN_DW    PLAYER_W
#define RUN_DH    PLAYER_H
#define JUMP_DW   40
#define JUMP_DH   60

#define MAZE_RENDER_W SCREEN_W
#define MAZE_RENDER_H SCREEN_H
#define MAZE_MAP_W 16
#define MAZE_MAP_H 16
#define MAZE_PLANE_SCALE 0.57735026919f
#define MAZE_MOVE_SPEED 3.0f
#define MAZE_TURN_SPEED 2.2f
#define MAZE_MAX_DT 0.05f
#define MAZE_PLAYER_RADIUS 0.18f
#define MAZE_BASE_ENGINE_VOLUME 34
#define MAZE_BOOST_ENGINE_VOLUME 92
#define MAZE_RADIO_VOLUME_IDLE 38
#define MAZE_RADIO_VOLUME_MOVING 20
#define MAZE_ENGINE_BLEND_SPEED 7.0f
#define MAZE_ENGINE_CHANNEL 14
#define MAZE_RADIO_CHANNEL 15
#define GAMEPLAY_CHANNEL_BALL 0
#define GAMEPLAY_CHANNEL_FIRE 1
#define GAMEPLAY_CHANNEL_EARTH 2
#define GAMEPLAY_CHANNEL_ICE 3
#define GAMEPLAY_CHANNEL_ROLL 4
#define GAMEPLAY_CHANNEL_STEP 5
#define RESERVED_GAMEPLAY_CHANNELS 6
#define MAZE_TILE_FINISH 2
#define MAZE_TILE_SPAWN 3
#define MAZE_TILE_FAN 4
#define MAZE_TILE_SWALL 5
#define MAZE_FINISH_TRIGGER_DISTANCE 0.30f
#define MAZE_CAR_FADE_MS 700
#define MAZE_FAN_FRAME_TIME_MS 110
#define MAZE_FAN_LAYOUT_MAX_DIVISOR 12
#define MAZE_FAN_OVERLAY_WIDTH_FRACTION 0.55f
#define MAZE_FAN_OVERLAY_HEIGHT_FRACTION 0.62f

static const int lvl6MazeMap[MAZE_MAP_H][MAZE_MAP_W] = {
    {1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 3, 0, 0, 1, 0, 0, 0, 1, 0, 0, 4, 0, 0, 2, 1},
    {1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 4, 1, 1, 1, 0, 1, 0, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 1, 4, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1},
    {1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 4, 1, 0, 1, 1, 1, 0, 1, 1},
    {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 5, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

typedef enum { USTATE_WAIT, USTATE_FALLING, USTATE_SMASHED } UtensilState;
typedef enum { UTYPE_POT, UTYPE_FORK, UTYPE_SPOON }          UtensilType;
typedef enum { RAIN_SPOON, RAIN_POT, RAIN_FORK, RAIN_MICROWAVE } RainType;

#define UTENSIL_COUNT   1
#define SMASH_FRAMES    35
#define UTENSIL_DELAY   25
#define UTENSIL_FALL_DURATION_MS 500
#define UTENSIL_GRAVITY 88.5f
#define UTENSIL_VY0    -6.0f

#define POT_HW   13
#define FORK_HW   3
#define SPOON_HW  5
#define PAN_HW    20
#define FORK_RAIN_HW 30
#define MICROWAVE_HW 16
#define MICROWAVE_W  40
#define MICROWAVE_H  28

typedef struct {
    float        x, y;
    float        vy;
    UtensilState state;
    UtensilType  type;
    int          smashTimer;
    float        smashY;
} Utensil;

static Utensil utensils[UTENSIL_COUNT];
static int     utensilTriggered = 0;
static int     utensilTimer     = 0;
static int     utensilNext      = 0;
static Uint32  utensilFallStartTick = 0;

#define LVL5_DROP_COUNT 12
#define LVL5_DROP_DELAY 16
#define LVL5_STEP_W     32.0f
#define LVL5_LAST_MICROWAVE_IDX 11
#define LVL5_HOLE_OFFSET_STEPS  3.0f
#define LVL5_HOLE_W             64.0f
#define LVL5_HOLE_H             92

typedef struct {
    float        x, y;
    float        vy;
    UtensilState state;
    RainType     type;
    int          smashTimer;
} RainDrop;

static RainDrop lvl5Drops[LVL5_DROP_COUNT];
static int      lvl5RainTriggered = 0;
static int      lvl5RainTimer     = 0;
static int      lvl5RainNext      = 0;
static int      lvl5HoleVisible   = 0;
static float    lvl5HoleX         = 0.0f;
static float    lvl5HoleW         = LVL5_HOLE_W;
static int      lvl5InHole        = 0;
static int      lvl5WallCrushPending = 0;
static int      lvl5WallTouchSoundPlayed = 0;
static float    lvl5CrushX        = 0.0f;
static float    lvl5CrushY        = 0.0f;
static int      lvl2WallCrushPending = 0;
static int      lvl2WallTouchSoundPlayed = 0;
static float    lvl2CrushX        = 0.0f;
static float    lvl2CrushY        = 0.0f;

typedef struct {
    float x, y;
    int   w, h;
    int   falling;
    float fallVy;
    int   triggered;
} Platform;

typedef struct {
    float x, y;
    float vx, vy;
    int   onGround;
    int   facingRight;
    int   dead;
    int   won;
    int   noJump;
} Player;

typedef struct {
    float x;
    float y;
    float angle;
} MazePlayer;

typedef struct RuntimeState RuntimeState;

typedef enum { DEATH_NONE, DEATH_GENERIC, DEATH_LASER, DEATH_CRUSH, DEATH_FIRE, DEATH_TOUCH } DeathType;

typedef struct {
    float x, y;
    int   visible;
    int   triggered;
    int   platIdx;
} Spike;

static Spike    lvl5HoleSpike;

typedef struct {
    float x, y;
    int   w, h;
    float angle;
    int   triggered;
    int   falling;
    int   landed;
    int   dustTimer;
    Uint32 triggerTick;
    Uint32 delayMs;
    SDL_Rect triggerZone;
} FallingWallTrap;

typedef struct {
    float  x, y;
    float  vy;
    float  slideDone;
    float  angle;
    int    triggered;
    int    sliding;
    int    falling;
    int    landed;
    int    dustTimer;
    float  triggerX;
    float  fallX;
} SlideSpike;

typedef struct {
    float x, y;
    float vx, vy;
    int   size;
    int   life;
    int   active;
} WallDebris;

typedef struct {
    float  x, y;
    int    w, h;
    int    frame;
    int    frameCount;
    Uint32 lastTick;
    Uint32 frameDelay;
    int    active;
} SawBlade;

typedef struct {
    float x, y;
    int   w, h;
    float vy;
    int   triggered;
    int   falling;
    int   landed;
    int   delayTimer;
    float triggerX, triggerY;
    float triggerW, triggerH;
} FallingWall;

typedef struct {
    SDL_Texture *idle;
    SDL_Texture *run;
    SDL_Texture *jump;
    SDL_Texture *electrise;
    SDL_Texture *burn;
    SDL_Texture *dust;
    SDL_Texture *touch;
    SDL_Texture *ice;
} PlayerSkinAssets;

static PlayerSkinAssets playerSkins[PLAYER_SKIN_COUNT] = {0};
static SDL_Texture *texSaw  = NULL;
static SDL_Texture *texFire = NULL;
static SDL_Texture *texLvl4Laser = NULL;
static SDL_Texture *texMicrowave = NULL;
static SDL_Texture *texPan = NULL;
static SDL_Texture *texForkRain = NULL;
static SDL_Texture *texBalloon = NULL;
static SDL_Texture *texLvl6Car = NULL;
static SDL_Texture *texMazeWall = NULL;
static SDL_Texture *texMazeFinish = NULL;
static SDL_Texture *texMazeSwall = NULL;
static SDL_Texture *texMazeFan = NULL;
static SDL_Texture *texMazeCar = NULL;
static SDL_Texture *texMazeBackground = NULL;
static SDL_Texture *texMazeFrame = NULL;
static SDL_Texture *soloFrame = NULL;
static SDL_Texture *introFrames[INTRO_FRAME_COUNT] = {NULL};
static Mix_Chunk   *sndTouch = NULL;
static Mix_Music   *sndTouchMusic = NULL;
static Mix_Music   *sndBreak = NULL;
static Mix_Music   *sndLaser = NULL;
static Mix_Music   *sndFall = NULL;
static Mix_Music   *sndIntroMusic = NULL;
static Mix_Chunk   *sndLaserFx = NULL;
static Mix_Chunk   *sndFireFx = NULL;
static Mix_Chunk   *sndEarthFx = NULL;
static Mix_Chunk   *sndIceFx = NULL;
static Mix_Chunk   *sndWallFx = NULL;
static Mix_Chunk   *sndTrapFx = NULL;
static Mix_Chunk   *sndStepFx = NULL;
static Mix_Chunk   *sndJumpFx = NULL;
static Mix_Music   *sndWallMusic = NULL;
static Mix_Chunk   *sndRollFx = NULL;
static Mix_Music   *sndRollMusic = NULL;
static Mix_Chunk   *sndPanFx = NULL;
static Mix_Music   *sndPanMusic = NULL;
static Mix_Chunk   *sndForkFx = NULL;
static Mix_Music   *sndForkMusic = NULL;
static Mix_Chunk   *sndBall = NULL;
static Mix_Chunk   *sndMazeEngine = NULL;
static Mix_Chunk   *sndMazeRadio = NULL;
static int          sndBallChannel = -1;
static int          sndFireChannel = -1;
static int          sndEarthChannel = -1;
static int          sndIceChannel = -1;
static int          sndRollChannel = -1;
static int          sndStepChannel = -1;
static int          sndFallPlayed = 0;
static int          fireFrame    = 0;
static int          fireFrameCount = 1;
static int          fireFrameW   = 0;
static int          fireFrameH   = 0;
static Uint32       fireLastTick = 0;
static int          lvl4LaserFrame = 0;
static int          lvl4LaserFrameCount = 1;
static int          lvl4LaserFrameW = 0;
static int          lvl4LaserFrameH = 0;
static Uint32       lvl4LaserLastTick = 0;
static int          electriseFrame = 0;
static int          electriseFrameCount = 1;
static int          electriseFrameW = 0;
static int          electriseFrameH = 0;
static Uint32       electriseLastTick = 0;
static int          burnFrame = 0;
static int          burnFrameCount = 1;
static int          burnFrameW = 0;
static int          burnFrameH = 0;
static Uint32       burnLastTick = 0;
static int          dustFrame = 0;
static int          dustFrameCount = 1;
static int          dustFrameW = 0;
static int          dustFrameH = 0;
static Uint32       dustLastTick = 0;
static int          dustPendingStart = 0;
static int          dustVisible = 0;
static int          touchFrame = 0;
static int          touchFrameCount = 1;
static int          touchFrameW = 0;
static int          touchFrameH = 0;
static Uint32       touchLastTick = 0;
static int          iceFrame = 0;
static int          iceFrameCount = 1;
static int          iceFrameCols = 1;
static int          iceFrameW = 0;
static int          iceFrameH = 0;
static Uint32       iceLastTick = 0;
static int          iceAnimFinished = 0;
static int          mazeWallW = 0;
static int          mazeWallH = 0;
static int          mazeFinishW = 0;
static int          mazeFinishH = 0;
static int          mazeSwallW = 0;
static int          mazeSwallH = 0;
static int          mazeFanW = 0;
static int          mazeFanH = 0;
static int          mazeFanCols = 1;
static int          mazeFanRows = 1;
static int          mazeFanFrameCount = 1;
static int          mazeCarW = 0;
static int          mazeCarH = 0;
static int          lvl6MazeAssetsReady = 0;
static int          lvl6MazeActive = 0;
static int          lvl6MazeRadioPlaying = 0;
static int          lvl6MazeEnginePlaying = 0;
static float        lvl6MazeEngineVolume = (float)MAZE_BASE_ENGINE_VOLUME;
static Uint32       lvl6MazePrevTick = 0;
static MazePlayer   lvl6MazePlayer = {0};
static int          animFrame    = 0;
static Uint32       animLastTick = 0;
typedef enum { PANIM_IDLE, PANIM_RUN, PANIM_JUMP } PlayerAnim;
static PlayerAnim   animState = PANIM_IDLE;
static int isBurnAnimFinished(void);
static void tickTouchAnim(void);
static void updateBall3Sound(void);
static void updateLvl4RollSound(void);
static void updateStepSound(void);
static void updateFallingFloor(Platform *p);
static void loadIntroCinematic(void);
static void updateIntroCinematic(void);
static void renderIntroCinematic(void);
static void skipIntroCinematic(void);
static void playIntroCinematicSound(void);
static void stopIntroCinematicSound(void);
static void loadLvl6MazeAssets(void);
static void freeLvl6MazeAssets(void);
static void stopLvl6MazeAudio(void);
static void beginLvl6MazeTransition(void);
static void startLvl6Maze(void);
static int updateLvl6MazeTransition(void);
static void renderLvl6MazeTransitionFade(void);
static void toggleLvl6MazeRadio(void);
static void updateLvl6MazeMode(void);
static void renderLvl6MazeMode(void);
static void startSoloGame(void);
static void startDuoGame(void);
static int updateDuoGame(void);
static void renderDuoGame(void);
static void renderStartMenu(void);
static void saveRuntimeState(RuntimeState *state);
static void loadRuntimeState(const RuntimeState *state);
static void startLevelTransition(int nextLevel);
static int updateLevelTransition(void);
static void renderLevelTransitionFade(void);
static int isLevelTransitionActive(void);
static int level3CurrentInputPlayer(void);
static int isPlatformLeftPressed(void);
static int isPlatformRightPressed(void);
static int isPlatformJumpPressed(void);
static int isMazeForwardPressed(void);
static int isMazeBackwardPressed(void);
static int isMazeTurnLeftPressed(void);
static int isMazeTurnRightPressed(void);
static void renderTextAt(TTF_Font *f,const char *txt,int x,int y,Uint8 r,Uint8 g,Uint8 b);
static void renderText(TTF_Font *f,const char *txt,int y,Uint8 r,Uint8 g,Uint8 b);

static SDL_Window   *win           = NULL;
static SDL_Renderer *ren           = NULL;
static SDL_Texture  *texBackground = NULL;
static SDL_Texture  *texBackground6 = NULL;
static TTF_Font     *font          = NULL;
static TTF_Font     *bigFont       = NULL;
static void drawLvl4Ball(void);

static int    currentLevel = 0;
static int    introActive = 0;
static int    introFrameCount = 0;
static int    introFrameIndex = 0;
static Uint32 introLastTick = 0;
static Uint32 winStartTick = 0;
static int    godMode      = 0;
static DeathType deathType = DEATH_NONE;
static Uint32 deathStartTick = 0;
static float  touchDeathX = 0.0f;
static float  touchDeathY = 0.0f;
static float  touchEffectX = 0.0f;
static float  touchEffectY = 0.0f;
static float  laserDeathVy = 0.0f;
static float  laserDeathVx = 0.0f;
static float  laserDeathTargetX = 0.0f;
static Player player;
static int    lvl6FreezeState = 0;
static int    lvl6MazeCompleted = 0;
typedef enum { MAZE_FADE_NONE, MAZE_FADE_OUT, MAZE_FADE_IN } MazeFadePhase;
static MazeFadePhase lvl6MazeFadePhase = MAZE_FADE_NONE;
static Uint32 lvl6MazeFadeStartTick = 0;

typedef enum { MODE_MENU, MODE_SOLO, MODE_DUO } HellGameMode;
static HellGameMode gameMode = MODE_MENU;
static GameSession *activeSession = NULL;
static int pauseMenuReady = 0;
static int pauseMenuActive = 0;
static int level3HellCompleted = 0;
static int transitionActive = 0;
static int transitionNextLevel = 0;
static Uint32 transitionStartTick = 0;
static int previousRemoteInteractDown = 0;

static void playTouchSound(void)
{
    if(sndTouch) {
        Mix_PlayChannel(-1, sndTouch, 0);
    } else if(sndTouchMusic) {
        Mix_HaltMusic();
        Mix_PlayMusic(sndTouchMusic, 0);
    }
}

static void playWallSound(void)
{
    if(sndWallFx) {
        Mix_PlayChannel(-1, sndWallFx, 0);
    } else if(sndWallMusic) {
        Mix_HaltMusic();
        Mix_PlayMusic(sndWallMusic, 0);
    }
}

static void playTrapSound(void)
{
    if(sndTrapFx) {
        Mix_PlayChannel(-1, sndTrapFx, 0);
    }
}

static void playJumpSound(void)
{
    if(sndJumpFx) {
        Mix_PlayChannel(-1, sndJumpFx, 0);
    }
}

static void updateStepSound(void)
{
    int shouldPlay = !introActive &&
                     !player.dead &&
                     !player.won &&
                     player.onGround &&
                     fabsf(player.vx) > 0.5f &&
                     !player.noJump;

    if(shouldPlay) {
        if(sndStepFx && (sndStepChannel == -1 || !Mix_Playing(sndStepChannel))) {
            sndStepChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_STEP, sndStepFx, -1);
        }
    } else if(sndStepChannel != -1) {
        Mix_HaltChannel(sndStepChannel);
        sndStepChannel = -1;
    }
}

static void playPanLandSound(void)
{
    if(sndPanFx) {
        Mix_PlayChannel(-1, sndPanFx, 0);
    } else if(sndPanMusic) {
        Mix_HaltMusic();
        Mix_PlayMusic(sndPanMusic, 0);
    }
}

static void playForkLandSound(void)
{
    if(sndForkFx) {
        Mix_PlayChannel(-1, sndForkFx, 0);
    } else if(sndForkMusic) {
        Mix_HaltMusic();
        Mix_PlayMusic(sndForkMusic, 0);
    }
}


static void stopFallDeathSound(void)
{
    if(sndFallPlayed) {
        Mix_HaltMusic();
        sndFallPlayed = 0;
    }
}

static void killPlayer(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        printf("defaite\n");
        player.dead = 1;
        deathType = DEATH_GENERIC;
        deathStartTick = SDL_GetTicks();
    }
}

static void killPlayerWithFall(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        printf("defaite\n");
        player.dead = 1;
        deathType = DEATH_GENERIC;
        deathStartTick = SDL_GetTicks();
    }
}

static void triggerFallSound(void)
{
    if(!godMode && !sndFallPlayed && sndFall) {
        Mix_HaltMusic();
        Mix_PlayMusic(sndFall, 0);
        sndFallPlayed = 1;
    }
}

static void killPlayerWithLaser(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        printf("defaite\n");
        player.dead = 1;
        deathType = DEATH_LASER;
        deathStartTick = SDL_GetTicks();
        electriseFrame = 0;
        electriseLastTick = SDL_GetTicks();
        laserDeathVy = -7.5f;
        laserDeathVx = 2.2f;
        player.vx = 0.0f;
    }
}

static void killPlayerWithFire(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        printf("defaite\n");
        player.dead = 1;
        deathType = DEATH_FIRE;
        deathStartTick = SDL_GetTicks();
        burnFrame = 0;
        burnLastTick = SDL_GetTicks();
        dustFrame = 0;
        dustLastTick = 0;
        dustPendingStart = 0;
        dustVisible = 0;
        player.vx = 0.0f;
        player.vy = 0.0f;
    }
}

static void killPlayerWithTouch(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        playTouchSound();
        printf("defaite\n");
        touchDeathX = player.x;
        touchDeathY = player.y;
        touchEffectX = player.x;
        touchEffectY = player.y;
        player.dead = 1;
        deathType = DEATH_TOUCH;
        deathStartTick = SDL_GetTicks();
        touchFrame = 0;
        touchLastTick = SDL_GetTicks();
        player.vx = 0.0f;
        player.vy = 0.0f;
    }
}

static void killPlayerWithTouchAt(float x, float y)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        playTouchSound();
        printf("defaite\n");
        player.x = x;
        player.y = y;
        touchDeathX = x;
        touchDeathY = y;
        touchEffectX = x;
        touchEffectY = y;
        player.dead = 1;
        deathType = DEATH_TOUCH;
        deathStartTick = SDL_GetTicks();
        touchFrame = 0;
        touchLastTick = SDL_GetTicks();
        player.vx = 0.0f;
        player.vy = 0.0f;
    }
}

static void killPlayerWithTouchEffectAt(float effectX, float effectY)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        playTouchSound();
        printf("defaite\n");
        touchDeathX = player.x;
        touchDeathY = player.y;
        touchEffectX = effectX;
        touchEffectY = effectY;
        player.dead = 1;
        deathType = DEATH_TOUCH;
        deathStartTick = SDL_GetTicks();
        touchFrame = 0;
        touchLastTick = SDL_GetTicks();
        player.vx = 0.0f;
        player.vy = 0.0f;
    }
}

static void crushPlayer(void)
{
    if(!godMode && !player.dead) {
        stopFallDeathSound();
        printf("defaite\n");
        player.dead = 1;
        deathType = DEATH_CRUSH;
        deathStartTick = SDL_GetTicks();
        player.vx = 0.0f;
        player.vy = 0.0f;
    }
}

/* ══════════════════════════════════════════════════════
   NIVEAU 1
══════════════════════════════════════════════════════ */
#define LVL1_COUNT 9
#define LVL1_PLAT_W 90
#define LVL1_GROUND_H 90
#define LVL1_GROUND_Y (SCREEN_H - LVL1_GROUND_H)
#define LVL1_GROUND_TOOTH_W 18
#define LVL1_LAST_FLOOR_DELAY 12
static Platform lvl1[LVL1_COUNT];
static Platform lvl1Ground;
static Spike    spike1;
static int      lvl1LastFloorDelayTimer = 0;

static void initLvl1(void)
{
    lvl1LastFloorDelayTimer = 0;
    lvl1Ground = (Platform){0, LVL1_GROUND_Y, SCREEN_W, LVL1_GROUND_H, 0, 0, 0};
    lvl1[0] = (Platform){  60, 530, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[1] = (Platform){ 240, 430, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[2] = (Platform){ 420, 330, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[3] = (Platform){ 530, 230, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[4] = (Platform){ 700, 230, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[5] = (Platform){ 870, 230, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[6] = (Platform){ 950, 330, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[7] = (Platform){1050, 200, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
    lvl1[8] = (Platform){1190, 100, LVL1_PLAT_W, PLAT_H, 0, 0, 0};
}

static void initSpike1(void)
{
    spike1.x       = lvl1[4].x + lvl1[4].w/2.0f - SPIKE1_W/2.0f;
    spike1.y       = lvl1[4].y - TOOTH_H;
    spike1.visible = 0; spike1.triggered = 0; spike1.platIdx = 4;
}

static void initUtensils(void)
{
    utensilTriggered = 0;
    utensilTimer     = 0;
    utensilNext      = 0;
    utensilFallStartTick = 0;

    float px = lvl1[3].x;
    float pw = (float)lvl1[3].w;
    utensils[0].x          = px + pw * 0.50f;
    utensils[0].y          = -100.0f;
    utensils[0].vy         = UTENSIL_VY0;
    utensils[0].state      = USTATE_WAIT;
    utensils[0].type       = UTYPE_POT;
    utensils[0].smashTimer = 0;
    utensils[0].smashY     = 0;
}

static void updateLvl1LastFloorTrap(void)
{
    if(lvl1[7].triggered && !lvl1[7].falling && lvl1LastFloorDelayTimer > 0) {
        lvl1LastFloorDelayTimer--;
        if(lvl1LastFloorDelayTimer <= 0) {
            lvl1[7].falling = 1;
        }
    }
    updateFallingFloor(&lvl1[7]);
}

/* ══════════════════════════════════════════════════════
   NIVEAU 2
══════════════════════════════════════════════════════ */
#define SOL_H           200
#define SOL_Y           (SCREEN_H - SOL_H)
#define SOL_G_X         0
#define SOL_G_W         220
#define SOL_D_X         980
#define SOL_D_W         300
#define SLIDE_TRIGGER_X (SOL_D_X + 55)

#define LVL2_COUNT 6
static Platform   lvl2[LVL2_COUNT];
static SlideSpike slideSpike;

static void initLvl2(void)
{
    lvl2[0] = (Platform){SOL_G_X, SOL_Y, SOL_G_W, SOL_H, 0, 0, 0};
    int gap = (SOL_D_X - SOL_G_W - 4*LVL2_PLAT_W) / 5;
    int x0  = SOL_G_W + gap;
    for (int i = 0; i < 4; i++) {
        lvl2[1+i] = (Platform){
            (float)(x0 + i*(LVL2_PLAT_W+gap)),
            LVL2_PLAT_Y, LVL2_PLAT_W, LVL2_PLAT_H, 0, 0, 0
        };
    }
    lvl2[5] = (Platform){SOL_D_X, SOL_Y, SOL_D_W, SOL_H, 0, 0, 0};
    float startX         = (float)(SOL_D_X + SOL_D_W - SLIDE_W);
    slideSpike.x         = startX;
    slideSpike.y         = (float)(SOL_Y - SLIDE_H);
    slideSpike.vy        = 0;
    slideSpike.slideDone = 0;
    slideSpike.angle     = 0;
    slideSpike.triggered = 0;
    slideSpike.sliding   = 0;
    slideSpike.falling   = 0;
    slideSpike.landed    = 0;
    slideSpike.dustTimer = 0;
    slideSpike.triggerX  = (float)SLIDE_TRIGGER_X;
    slideSpike.fallX     = startX + SLIDE_W;
    lvl2WallCrushPending = 0;
    lvl2WallTouchSoundPlayed = 0;
    lvl2CrushX           = 0.0f;
    lvl2CrushY           = 0.0f;
}

/* ══════════════════════════════════════════════════════
   NIVEAU 4
   - meme disposition generale que le niveau 2
   - 5 petites plateformes au milieu
   - un laser tombe du haut sur le premier petit floor
   - invisible au debut puis visible quand le joueur y atterrit
   - le 3e petit floor se casse quand le joueur y atterrit
   - le 5e petit floor declenche du feu
══════════════════════════════════════════════════════ */
#define LVL4_PLAT_W 58
#define LVL4_COUNT  7
#define LVL4_LASER_W 18
#define LVL4_LASER_TOP 10
#define LVL4_BALL_COUNT 2
#define LVL4_BALL_R   26
#define LVL4_BALL_SPD 5.5f
static Platform lvl4[LVL4_COUNT];
static int      lvl4LaserVisible = 0;
static int      lvl4FireVisible  = 0;
static SawBlade lvl4Saw;
static float    lvl4BallX[LVL4_BALL_COUNT];
static float    lvl4BallY[LVL4_BALL_COUNT];
static float    lvl4BallVy[LVL4_BALL_COUNT];
static float    lvl4BallAngle[LVL4_BALL_COUNT];
static int      lvl4BallFalling[LVL4_BALL_COUNT];
static int      lvl4BallActive[LVL4_BALL_COUNT];
static int      lvl4BallTriggered = 0;

static int isLvl4BallRolling(void)
{
    for(int i=0;i<LVL4_BALL_COUNT;i++) {
        if(lvl4BallActive[i]) return 1;
    }
    return 0;
}

/* ══════════════════════════════════════════════════════
   NIVEAU 6
   - meme disposition que le niveau 4
   - vibration constante comme le niveau 3
   - le floor de depart a gauche s effondre immediatement
══════════════════════════════════════════════════════ */
static Platform lvl6[5];
static Platform lvl6Chunks[7];
static int      lvl6CollapseTimer = 0;
static int      lvl6CollapseSoundPlayed = 0;
static int      lvl6IceSoundPlayed = 0;

#define LVL6_COUNT 5
#define LVL6_LEFT_W 340
#define LVL6_RIGHT_X 860
#define LVL6_RIGHT_W (SCREEN_W - LVL6_RIGHT_X)
#define LVL6_PLAT_W 72
#define LVL6_CHUNK_COUNT 7
#define LVL6_COLLAPSE_DELAY 24
#define LVL6_CAR_X          (SCREEN_W - 250)
#define LVL6_CAR_DRAW_W     172
#define LVL6_CAR_DRAW_H     66
#define LVL6_CAR_INTERACT_PAD 96
#define LVL6_EXIT_DOOR_W    62
#define LVL6_EXIT_DOOR_H    248
#define LVL6_EXIT_DOOR_X    (SCREEN_W - 92)
#define LVL6_EXIT_DOOR_Y    (SOL_Y - LVL6_EXIT_DOOR_H)

static void initLvl6(void)
{
    lvl6[0] = (Platform){SOL_G_X, SOL_Y, LVL6_LEFT_W, SOL_H, 0, 0.0f, 1};
    int gap = (LVL6_RIGHT_X - LVL6_LEFT_W - 3*LVL6_PLAT_W) / 4;
    int x0  = LVL6_LEFT_W + gap;
    for (int i = 0; i < 3; i++) {
        lvl6[1+i] = (Platform){
            (float)(x0 + i*(LVL6_PLAT_W+gap)),
            LVL2_PLAT_Y, LVL6_PLAT_W, LVL2_PLAT_H, 0, 0, 0
        };
    }
    lvl6[4] = (Platform){LVL6_RIGHT_X, SOL_Y, LVL6_RIGHT_W, SOL_H, 0, 0, 0};
    lvl6CollapseTimer = 0;
    lvl6CollapseSoundPlayed = 0;
    lvl6IceSoundPlayed = 0;
    lvl6MazeActive = 0;
    lvl6MazeCompleted = 0;
    lvl6MazeFadePhase = MAZE_FADE_NONE;
    lvl6MazeFadeStartTick = 0;
    stopLvl6MazeAudio();
    if(sndEarthChannel != -1) {
        Mix_HaltChannel(sndEarthChannel);
        sndEarthChannel = -1;
    }
    for(int i=0;i<LVL6_CHUNK_COUNT;i++) {
        int x = (i * LVL6_LEFT_W) / LVL6_CHUNK_COUNT;
        int nextX = ((i + 1) * LVL6_LEFT_W) / LVL6_CHUNK_COUNT;
        lvl6Chunks[i] = (Platform){(float)x, (float)SOL_Y, nextX - x, SOL_H, 0, 0.0f, 0};
    }
}

static void initLvl4(void)
{
    lvl4[0] = (Platform){SOL_G_X, SOL_Y, SOL_G_W, SOL_H, 0, 0, 0};
    int gap = (SOL_D_X - SOL_G_W - 5*LVL4_PLAT_W) / 6;
    int x0  = SOL_G_W + gap;
    for (int i = 0; i < 5; i++) {
        lvl4[1+i] = (Platform){
            (float)(x0 + i*(LVL4_PLAT_W+gap)),
            LVL2_PLAT_Y, LVL4_PLAT_W, LVL2_PLAT_H, 0, 0, 0
        };
    }
    lvl4[6] = (Platform){SOL_D_X, SOL_Y, SOL_D_W, SOL_H, 0, 0, 0};
    laserDeathTargetX = (lvl4[1].x + lvl4[1].w + lvl4[2].x) * 0.5f - PLAYER_W * 0.5f;
    lvl4LaserVisible = 0;
    lvl4LaserFrame   = 0;
    lvl4LaserLastTick = SDL_GetTicks();
    lvl4FireVisible  = 0;
    if(sndFireChannel != -1) {
        Mix_HaltChannel(sndFireChannel);
        sndFireChannel = -1;
    }
    fireFrame        = 0;
    fireLastTick     = SDL_GetTicks();
    burnFrame        = 0;
    burnLastTick     = SDL_GetTicks();
    dustFrame        = 0;
    dustLastTick     = 0;
    dustPendingStart = 0;
    dustVisible      = 0;
    for(int i=0;i<LVL4_BALL_COUNT;i++) {
        lvl4BallX[i] = (float)(SCREEN_W + LVL4_BALL_R + i * 92);
        lvl4BallY[i] = (float)lvl4[6].y - LVL4_BALL_R;
        lvl4BallVy[i] = 0.0f;
        lvl4BallAngle[i] = 0.0f;
        lvl4BallFalling[i] = 0;
        lvl4BallActive[i] = 0;
    }
    lvl4BallTriggered = 0;

    // SAW INIT
    Platform *p = &lvl4[1];
    lvl4Saw.x          = p->x + p->w/2.0f - SAW_FRAME_SIZE/2.0f;
    lvl4Saw.y          = p->y - SAW_FRAME_SIZE - SAW_OFFSET_Y;
    lvl4Saw.w          = SAW_FRAME_SIZE;
    lvl4Saw.h          = SAW_FRAME_SIZE;
    lvl4Saw.frame      = 0;
    lvl4Saw.frameCount = SAW_FRAME_COUNT;
    lvl4Saw.lastTick   = SDL_GetTicks();
    lvl4Saw.frameDelay = 70;
    lvl4Saw.active     = 0;
}

/* ══════════════════════════════════════════════════════
   NIVEAU 5
   - un seul floor continu
══════════════════════════════════════════════════════ */
#define LVL5_COUNT 1
#define LVL5_SPIKE_TRIGGER_DIST 64.0f
#define LVL5_FALL_WALL_W 58
#define LVL5_FALL_WALL_H 280
#define LVL5_FALL_WALL_X (SCREEN_W - 140)
#define LVL5_FALL_WALL_HIDDEN_Y (-LVL5_FALL_WALL_H - 40)
#define LVL5_FALL_WALL_TRIGGER_W 240
#define LVL5_FALL_WALL_TRIGGER_H 140
#define LVL5_FALL_WALL_DELAY_MS  260
static Platform lvl5[LVL5_COUNT];
static Spike    spike5;
static FallingWallTrap lvl5WallTrap;

static SDL_Rect getPlayerRect(void)
{
    SDL_Rect r={(int)player.x,(int)player.y,PLAYER_W,PLAYER_H};
    return r;
}

static SDL_Rect getFallingWallRect(const FallingWallTrap *wall)
{
    float pivotX = wall->x;
    float pivotY = wall->y + wall->h;
    float a = wall->angle * (float)M_PI / 180.0f;
    float c = cosf(a);
    float s = sinf(a);
    float pts[4][2] = {
        {0.0f, 0.0f},
        {(float)wall->w, 0.0f},
        {0.0f, (float)-wall->h},
        {(float)wall->w, (float)-wall->h}
    };
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;

    for(int i=0;i<4;i++) {
        float lx = pts[i][0];
        float ly = pts[i][1];
        float wx = pivotX + lx * c + ly * s;
        float wy = pivotY - lx * s + ly * c;
        if(wx < minX) minX = wx;
        if(wx > maxX) maxX = wx;
        if(wy < minY) minY = wy;
        if(wy > maxY) maxY = wy;
    }

    SDL_Rect r={(int)minX,(int)minY,(int)(maxX-minX),(int)(maxY-minY)};
    return r;
}

static void updateFallingWallTrap(FallingWallTrap *wall)
{
    if(wall->dustTimer > 0) wall->dustTimer--;
    if(!wall->triggered || wall->landed) return;

    if(!wall->falling) {
        /* Short delay after entering the trigger zone to add tension. */
        Uint32 elapsed = SDL_GetTicks() - wall->triggerTick;
        if(elapsed >= wall->delayMs) {
            wall->falling = 1;
        }
        else return;
    }

    wall->angle += 6.0f;
    if(wall->angle >= 90.0f) {
        wall->angle = 90.0f;
        wall->falling = 0;
        wall->landed = 1;
        wall->dustTimer = 18;
        playWallSound();
    }
}

static void initLvl5(void)
{
    lvl5[0] = (Platform){0, SOL_Y, SCREEN_W, SOL_H, 0, 0, 0};
    spike5.x       = 124.0f;
    spike5.y       = lvl5[0].y - TOOTH_H;
    spike5.visible = 0;
    spike5.triggered = 0;
    spike5.platIdx = 0;

    lvl5WallTrap.w = LVL5_FALL_WALL_W;
    lvl5WallTrap.h = LVL5_FALL_WALL_H;
    lvl5WallTrap.x = (float)(SCREEN_W - lvl5WallTrap.w);
    lvl5WallTrap.y = (float)(SOL_Y - lvl5WallTrap.h);
    lvl5WallTrap.angle = 0.0f;
    lvl5WallTrap.triggered = 0;
    lvl5WallTrap.falling = 0;
    lvl5WallTrap.landed = 0;
    lvl5WallTrap.dustTimer = 0;
    lvl5WallTrap.triggerTick = 0;
    lvl5WallTrap.delayMs = LVL5_FALL_WALL_DELAY_MS;
    lvl5WallTrap.triggerZone = (SDL_Rect){
        LVL5_FALL_WALL_X - LVL5_FALL_WALL_TRIGGER_W - 12,
        SOL_Y - LVL5_FALL_WALL_TRIGGER_H,
        LVL5_FALL_WALL_TRIGGER_W,
        LVL5_FALL_WALL_TRIGGER_H
    };

    lvl5RainTriggered = 0;
    lvl5RainTimer     = 0;
    lvl5RainNext      = 0;
    lvl5HoleVisible   = 0;
    lvl5HoleW         = LVL5_HOLE_W;
    lvl5InHole        = 0;
    lvl5WallCrushPending = 0;
    lvl5WallTouchSoundPlayed = 0;
    lvl5CrushX        = 0.0f;
    lvl5CrushY        = 0.0f;

    float startX = spike5.x + 3.0f * LVL5_STEP_W;
    for(int i=0;i<LVL5_DROP_COUNT;i++) {
        RainType type;
        switch(i % 4) {
            case 0:  type = RAIN_SPOON; break;
            case 1:  type = RAIN_FORK; break;
            case 2:  type = RAIN_POT; break;
            default: type = RAIN_MICROWAVE; break;
        }
        lvl5Drops[i].x = startX + (float)i * LVL5_STEP_W + LVL5_STEP_W/2.0f;
        lvl5Drops[i].y = -120.0f;
        lvl5Drops[i].vy = 150.0f;
        lvl5Drops[i].state = USTATE_WAIT;
        lvl5Drops[i].type = type;
        lvl5Drops[i].smashTimer = 0;
    }

    lvl5HoleX = lvl5Drops[LVL5_LAST_MICROWAVE_IDX].x + LVL5_HOLE_OFFSET_STEPS * LVL5_STEP_W - lvl5HoleW * 0.5f;
    if(lvl5HoleX < 160.0f) lvl5HoleX = 160.0f;
    if(lvl5HoleX + lvl5HoleW > SCREEN_W - 120.0f) lvl5HoleX = SCREEN_W - 120.0f - lvl5HoleW;

    lvl5HoleSpike.x = lvl5HoleX + lvl5HoleW * 0.5f - SPIKE1_W * 0.5f;
    lvl5HoleSpike.y = SOL_Y + LVL5_HOLE_H - TOOTH_H - 6;
    lvl5HoleSpike.visible = 0;
    lvl5HoleSpike.triggered = 0;
    lvl5HoleSpike.platIdx = 0;
}

/* ══════════════════════════════════════════════════════
   NIVEAU 3
   - sol gauche (depart) + trou + sol droit (arrivee)
   - grand ballon roule de droite vers gauche
   - il s arrete au bord gauche sans sortir de l ecran
   - si le joueur s en approche, il repart vers la droite
   - a droite, il casse le mur de sortie
   - le joueur doit se cacher dans le trou
══════════════════════════════════════════════════════ */
#define LVL3_CEIL_H    50
#define LVL3_FLOOR_Y   480
#define LVL3_FLOOR_H   240
#define LVL3_GAP_X     580
#define LVL3_GAP_W     46
#define LVL3_HOLE_Y    540
#define LVL3_HOLE_H    150

#define BALL3_R        160
#define BALL3_CY       320
#define BALL3_SPD      8.0f
#define LVL3_PLAYER_SPEED 12.0f
#define LVL3_JUMP_VY    -10.0f
#define BALL3_TRIGGER_X 430.0f
#define LVL3_WALL_W    58
#define LVL3_WALL_X    (SCREEN_W - LVL3_WALL_W)
#define LVL3_WALL_Y    LVL3_CEIL_H
#define LVL3_WALL_H    (LVL3_FLOOR_Y + LVL3_FLOOR_H - LVL3_WALL_Y)
#define LVL3_WALL_DEBRIS_COUNT 16
#define LVL3_WALL_DEBRIS_LIFE  55

#define LVL3_COUNT     3
static Platform lvl3[LVL3_COUNT];
static float    ball3X      = 0.0f;
static float    ball3Angle  = 0.0f;
static int      ball3Active = 0;
static int      ball3State  = 0;
static int      lvl3WallBroken = 0;
static WallDebris lvl3WallDebris[LVL3_WALL_DEBRIS_COUNT];

static int isBall3Rolling(void)
{
    return ball3Active && (ball3State == -1 || ball3State == 1 || ball3State == 2);
}

typedef struct RuntimeState {
    Utensil utensils[UTENSIL_COUNT];
    int utensilTriggered;
    int utensilTimer;
    int utensilNext;
    Uint32 utensilFallStartTick;

    RainDrop lvl5Drops[LVL5_DROP_COUNT];
    int lvl5RainTriggered;
    int lvl5RainTimer;
    int lvl5RainNext;
    int lvl5HoleVisible;
    float lvl5HoleX;
    float lvl5HoleW;
    int lvl5InHole;
    int lvl5WallCrushPending;
    int lvl5WallTouchSoundPlayed;
    float lvl5CrushX;
    float lvl5CrushY;
    int lvl2WallCrushPending;
    int lvl2WallTouchSoundPlayed;
    float lvl2CrushX;
    float lvl2CrushY;
    Spike lvl5HoleSpike;

    int sndBallChannel;
    int sndFireChannel;
    int sndEarthChannel;
    int sndIceChannel;
    int sndRollChannel;
    int sndStepChannel;
    int sndFallPlayed;

    int fireFrame;
    Uint32 fireLastTick;
    int lvl4LaserFrame;
    Uint32 lvl4LaserLastTick;
    int electriseFrame;
    Uint32 electriseLastTick;
    int burnFrame;
    Uint32 burnLastTick;
    int dustFrame;
    Uint32 dustLastTick;
    int dustPendingStart;
    int dustVisible;
    int touchFrame;
    Uint32 touchLastTick;
    int iceFrame;
    Uint32 iceLastTick;
    int iceAnimFinished;

    int lvl6MazeActive;
    int lvl6MazeRadioPlaying;
    int lvl6MazeEnginePlaying;
    float lvl6MazeEngineVolume;
    Uint32 lvl6MazePrevTick;
    MazePlayer lvl6MazePlayer;

    int animFrame;
    Uint32 animLastTick;
    PlayerAnim animState;

    int currentLevel;
    Uint32 winStartTick;
    int transitionActive;
    int transitionNextLevel;
    Uint32 transitionStartTick;
    DeathType deathType;
    Uint32 deathStartTick;
    float touchDeathX;
    float touchDeathY;
    float touchEffectX;
    float touchEffectY;
    float laserDeathVy;
    float laserDeathVx;
    float laserDeathTargetX;
    Player player;
    int lvl6FreezeState;
    int lvl6MazeCompleted;

    Platform lvl1[LVL1_COUNT];
    Platform lvl1Ground;
    Spike spike1;
    int lvl1LastFloorDelayTimer;

    Platform lvl2[LVL2_COUNT];
    SlideSpike slideSpike;

    Platform lvl4[LVL4_COUNT];
    int lvl4LaserVisible;
    int lvl4FireVisible;
    SawBlade lvl4Saw;
    float lvl4BallX[LVL4_BALL_COUNT];
    float lvl4BallY[LVL4_BALL_COUNT];
    float lvl4BallVy[LVL4_BALL_COUNT];
    float lvl4BallAngle[LVL4_BALL_COUNT];
    int lvl4BallFalling[LVL4_BALL_COUNT];
    int lvl4BallActive[LVL4_BALL_COUNT];
    int lvl4BallTriggered;

    Platform lvl6[LVL6_COUNT];
    Platform lvl6Chunks[LVL6_CHUNK_COUNT];
    int lvl6CollapseTimer;
    int lvl6CollapseSoundPlayed;
    int lvl6IceSoundPlayed;

    Platform lvl5[LVL5_COUNT];
    Spike spike5;
    FallingWallTrap lvl5WallTrap;

    Platform lvl3[LVL3_COUNT];
    float ball3X;
    float ball3Angle;
    int ball3Active;
    int ball3State;
    int lvl3WallBroken;
    WallDebris lvl3WallDebris[LVL3_WALL_DEBRIS_COUNT];
} RuntimeState;

typedef struct {
    RuntimeState runtime;
    int assignedLevels[3];
    int progress;
    int finished;
} DuoPlayerState;

static DuoPlayerState duoPlayers[2];
static SDL_Texture   *duoFrames[2] = {NULL, NULL};
static int            duoInputPlayer = 0;
static int            duoInteractPressed[2] = {0, 0};
static int            duoRadioPressed[2] = {0, 0};
static Uint32         duoAllDoneTick = 0;
static int            soloSkinIndex = 0;
static int            duoSkinIndex[2] = {0, 1};

static int getActivePlayerSkinIndex(void)
{
    if(gameMode == MODE_DUO) {
        if(duoInputPlayer < 0 || duoInputPlayer >= 2) return 0;
        return duoSkinIndex[duoInputPlayer];
    }
    return soloSkinIndex;
}

static PlayerSkinAssets *getCurrentPlayerSkin(void)
{
    int skinIndex = getActivePlayerSkinIndex();
    if(skinIndex < 0 || skinIndex >= PLAYER_SKIN_COUNT) skinIndex = 0;
    return &playerSkins[skinIndex];
}

static SDL_Texture *loadTextureWithBlend(const char *path)
{
    SDL_Surface *surface = IMG_Load(path);
    SDL_Texture *texture;

    if(!surface) return NULL;
    texture = SDL_CreateTextureFromSurface(ren, surface);
    SDL_FreeSurface(surface);
    if(texture) SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

static void setSheetFrameSize(SDL_Texture *texture, int cols, int rows, int *frameW, int *frameH)
{
    int texW = 0;
    int texH = 0;

    if(frameW) *frameW = 0;
    if(frameH) *frameH = 0;
    if(!texture || cols <= 0 || rows <= 0 || !frameW || !frameH) return;

    SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
    if(texW > 0 && texH > 0) {
        *frameW = texW / cols;
        *frameH = texH / rows;
    }
}

static SDL_Texture *getFirstAvailablePlayerSkinTexture(int textureKind)
{
    for(int i=0;i<PLAYER_SKIN_COUNT;i++) {
        switch(textureKind) {
            case 0: if(playerSkins[i].electrise) return playerSkins[i].electrise; break;
            case 1: if(playerSkins[i].burn) return playerSkins[i].burn; break;
            case 2: if(playerSkins[i].touch) return playerSkins[i].touch; break;
            case 3: if(playerSkins[i].dust) return playerSkins[i].dust; break;
            case 4: if(playerSkins[i].ice) return playerSkins[i].ice; break;
        }
    }
    return NULL;
}

static void loadPlayerSkinAssets(void)
{
    const char *skinDirs[PLAYER_SKIN_COUNT] = {"assets/skin1", "assets/skin2"};
    char path[PATH_MAX];
    SDL_Texture *referenceTexture;

    for(int i=0;i<PLAYER_SKIN_COUNT;i++) {
        snprintf(path, sizeof(path), "%s/idle.png", skinDirs[i]);
        playerSkins[i].idle = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/run.png", skinDirs[i]);
        playerSkins[i].run = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/jump.png", skinDirs[i]);
        playerSkins[i].jump = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/electrise.png", skinDirs[i]);
        playerSkins[i].electrise = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/brule.png", skinDirs[i]);
        playerSkins[i].burn = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/touche.png", skinDirs[i]);
        playerSkins[i].touch = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/poussiere.png", skinDirs[i]);
        playerSkins[i].dust = loadTextureWithBlend(path);
        snprintf(path, sizeof(path), "%s/glace.png", skinDirs[i]);
        playerSkins[i].ice = loadTextureWithBlend(path);
    }

    referenceTexture = getFirstAvailablePlayerSkinTexture(0);
    electriseFrameCount = ELECTRISE_SHEET_COLS * ELECTRISE_SHEET_ROWS;
    setSheetFrameSize(referenceTexture, ELECTRISE_SHEET_COLS, ELECTRISE_SHEET_ROWS, &electriseFrameW, &electriseFrameH);
    electriseFrame = 0;
    electriseLastTick = SDL_GetTicks();

    referenceTexture = getFirstAvailablePlayerSkinTexture(1);
    burnFrameCount = BURN_SHEET_COLS * BURN_SHEET_ROWS;
    setSheetFrameSize(referenceTexture, BURN_SHEET_COLS, BURN_SHEET_ROWS, &burnFrameW, &burnFrameH);
    burnFrame = 0;
    burnLastTick = SDL_GetTicks();

    referenceTexture = getFirstAvailablePlayerSkinTexture(2);
    touchFrameCount = TOUCH_SHEET_COLS * TOUCH_SHEET_ROWS;
    setSheetFrameSize(referenceTexture, TOUCH_SHEET_COLS, TOUCH_SHEET_ROWS, &touchFrameW, &touchFrameH);
    touchFrame = 0;
    touchLastTick = SDL_GetTicks();

    referenceTexture = getFirstAvailablePlayerSkinTexture(3);
    dustFrameCount = DUST_SHEET_COLS * DUST_SHEET_ROWS;
    setSheetFrameSize(referenceTexture, DUST_SHEET_COLS, DUST_SHEET_ROWS, &dustFrameW, &dustFrameH);
    dustFrame = 0;
    dustLastTick = 0;
    dustPendingStart = 0;
    dustVisible = 0;

    referenceTexture = getFirstAvailablePlayerSkinTexture(4);
    iceFrameCols = 6;
    iceFrameCount = 36;
    setSheetFrameSize(referenceTexture, iceFrameCols, 6, &iceFrameW, &iceFrameH);
    iceFrame = 0;
    iceLastTick = SDL_GetTicks();
}
static void updateBall3Sound(void)
{
    if(!sndBall) return;

    if(currentLevel == 2 && isBall3Rolling() && !player.dead && !player.won) {
        if(sndBallChannel == -1 || !Mix_Playing(sndBallChannel)) {
            sndBallChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_BALL, sndBall, -1);
        }
    } else if(sndBallChannel != -1) {
        Mix_HaltChannel(sndBallChannel);
        sndBallChannel = -1;
    }
}

static void updateLvl4RollSound(void)
{
    int shouldPlay = (currentLevel == 3) && isLvl4BallRolling() && !player.dead && !player.won;

    if(sndRollFx) {
        if(shouldPlay) {
            if(sndRollChannel == -1 || !Mix_Playing(sndRollChannel)) {
                sndRollChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_ROLL, sndRollFx, -1);
            }
        } else if(sndRollChannel != -1) {
            Mix_HaltChannel(sndRollChannel);
            sndRollChannel = -1;
        }
    } else if(sndRollMusic) {
        if(shouldPlay) {
            if(!Mix_PlayingMusic()) Mix_PlayMusic(sndRollMusic, -1);
        } else if(Mix_PlayingMusic()) {
            Mix_HaltMusic();
        }
    }
}

static int hasImageExtension(const char *path)
{
    const char *ext = strrchr(path, '.');
    if(!ext) return 0;
    return strcmp(ext, ".png")  == 0 || strcmp(ext, ".PNG")  == 0 ||
           strcmp(ext, ".jpg")  == 0 || strcmp(ext, ".JPG")  == 0 ||
           strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".JPEG") == 0;
}

static int comparePathStrings(const void *a, const void *b)
{
    const char *pa = (const char *)a;
    const char *pb = (const char *)b;
    const char *ba = strrchr(pa, '/');
    const char *bb = strrchr(pb, '/');
    int na = -1, nb = -1;

    ba = ba ? ba + 1 : pa;
    bb = bb ? bb + 1 : pb;

    for(const char *p = ba; *p; p++) {
        if(isdigit((unsigned char)*p)) {
            na = (int)strtol(p, NULL, 10);
            break;
        }
    }
    for(const char *p = bb; *p; p++) {
        if(isdigit((unsigned char)*p)) {
            nb = (int)strtol(p, NULL, 10);
            break;
        }
    }

    if(na >= 0 && nb >= 0 && na != nb) return na - nb;
    return strcmp(pa, pb);
}

static int collectIntroFramePaths(const char *dirPath, char paths[][PATH_MAX], int count, int maxCount)
{
    DIR *dir = opendir(dirPath);
    if(!dir) return count;

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if(count >= maxCount) break;

        char fullPath[PATH_MAX];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", dirPath, entry->d_name);

        struct stat st;
        if(stat(fullPath, &st) != 0) continue;

        if(S_ISDIR(st.st_mode)) {
            count = collectIntroFramePaths(fullPath, paths, count, maxCount);
        } else if(hasImageExtension(fullPath)) {
            snprintf(paths[count], PATH_MAX, "%s", fullPath);
            count++;
        }
    }

    closedir(dir);
    return count;
}

static void freeIntroCinematic(void)
{
    for(int i=0;i<INTRO_FRAME_COUNT;i++) {
        if(introFrames[i]) {
            SDL_DestroyTexture(introFrames[i]);
            introFrames[i] = NULL;
        }
    }
    introFrameCount = 0;
    introFrameIndex = 0;
    introLastTick = 0;
}

static void playIntroCinematicSound(void)
{
    if(!sndIntroMusic) return;
    Mix_HaltMusic();
    Mix_PlayMusic(sndIntroMusic, 0);
}

static void stopIntroCinematicSound(void)
{
    if(sndIntroMusic && Mix_PlayingMusic()) {
        Mix_HaltMusic();
    }
}

static void tryExtractIntroZip(void)
{
    FILE *zip = fopen("assets/cinematique.zip", "rb");
    int unzipResult = 0;

    if(!zip) return;
    fclose(zip);

    if(mkdir("/tmp/level_intro_frames", 0755) != 0 && errno != EEXIST) {
        SDL_Log("Unable to create intro frame cache directory: %s", strerror(errno));
        return;
    }

    unzipResult = system("unzip -oq assets/cinematique.zip -d /tmp/level_intro_frames >/dev/null 2>&1");
    if(unzipResult != 0) {
        SDL_Log("Unable to extract intro cinematic frames.");
    }
}

static void loadIntroCinematic(void)
{
    char paths[INTRO_SCAN_MAX_FILES][PATH_MAX];
    int pathCount = 0;

    freeIntroCinematic();
    tryExtractIntroZip();

    pathCount = collectIntroFramePaths("assets/cinematique", paths, pathCount, INTRO_SCAN_MAX_FILES);
    pathCount = collectIntroFramePaths("/tmp/level_intro_frames", paths, pathCount, INTRO_SCAN_MAX_FILES);

    if(pathCount > 1) {
        qsort(paths, (size_t)pathCount, sizeof(paths[0]), comparePathStrings);
    }

    for(int i=0;i<pathCount && introFrameCount < INTRO_FRAME_COUNT;i++) {
        SDL_Surface *s = IMG_Load(paths[i]);
        if(!s) continue;

        introFrames[introFrameCount] = SDL_CreateTextureFromSurface(ren, s);
        if(introFrames[introFrameCount]) {
            SDL_SetTextureBlendMode(introFrames[introFrameCount], SDL_BLENDMODE_BLEND);
            introFrameCount++;
        }
        SDL_FreeSurface(s);
    }

    introActive = (introFrameCount > 0);
    introFrameIndex = 0;
    introLastTick = SDL_GetTicks();
    if(introActive) playIntroCinematicSound();
}

static void skipIntroCinematic(void)
{
    stopIntroCinematicSound();
    introActive = 0;
    introFrameIndex = 0;
}

static void updateIntroCinematic(void)
{
    if(!introActive || introFrameCount <= 0) return;

    Uint32 now = SDL_GetTicks();
    while(now - introLastTick >= INTRO_FRAME_DELAY_MS) {
        introLastTick += INTRO_FRAME_DELAY_MS;
        introFrameIndex++;
        if(introFrameIndex >= introFrameCount) {
            skipIntroCinematic();
            break;
        }
    }
}

static void renderIntroCinematic(void)
{
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    if(introFrameCount > 0 && introFrames[introFrameIndex]) {
        int texW = SCREEN_W;
        int texH = SCREEN_H;
        SDL_QueryTexture(introFrames[introFrameIndex], NULL, NULL, &texW, &texH);

        SDL_Rect dst = {0, 0, SCREEN_W, SCREEN_H};
        if(texW > 0 && texH > 0) {
            float scaleX = (float)SCREEN_W / (float)texW;
            float scaleY = (float)SCREEN_H / (float)texH;
            float scale = (scaleX < scaleY) ? scaleX : scaleY;
            dst.w = (int)(texW * scale);
            dst.h = (int)(texH * scale);
            dst.x = (SCREEN_W - dst.w) / 2;
            dst.y = (SCREEN_H - dst.h) / 2;
        }

        SDL_RenderCopy(ren, introFrames[introFrameIndex], NULL, &dst);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 90);
    SDL_Rect overlay = {0, SCREEN_H - 70, SCREEN_W, 70};
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    renderText(font, "PRESS ENTER OR SPACE TO SKIP", SCREEN_H - 48, 255, 255, 255);
}

static void spawnLvl3WallDebris(void)
{
    for(int i=0;i<LVL3_WALL_DEBRIS_COUNT;i++) {
        WallDebris *d=&lvl3WallDebris[i];
        float tx=(float)(i%4)/3.0f;
        float ty=(float)(i/4)/3.0f;
        d->x=(float)LVL3_WALL_X + 8.0f + tx*(float)(LVL3_WALL_W-18);
        d->y=(float)LVL3_WALL_Y + 40.0f + ty*(float)(LVL3_WALL_H-90);
        d->vx=4.0f + (float)(rand()%70)/10.0f;
        d->vy=-8.0f - (float)(rand()%70)/10.0f;
        d->size=8 + rand()%9;
        d->life=LVL3_WALL_DEBRIS_LIFE + rand()%20;
        d->active=1;
    }
}

static void initLvl3(void)
{
    lvl3[0] = (Platform){0,
                          (float)LVL3_FLOOR_Y,
                          LVL3_GAP_X,
                          LVL3_FLOOR_H, 0, 0, 0};
    lvl3[1] = (Platform){LVL3_GAP_X + LVL3_GAP_W,
                          (float)LVL3_FLOOR_Y,
                          SCREEN_W - (LVL3_GAP_X + LVL3_GAP_W),
                          LVL3_FLOOR_H, 0, 0, 0};
    lvl3[2] = (Platform){LVL3_GAP_X,
                          (float)LVL3_HOLE_Y,
                          LVL3_GAP_W,
                          LVL3_HOLE_H, 0, 0, 0};

    /* ballon demarre a l extremite droite, entierement visible */
    ball3X      = (float)(SCREEN_W - BALL3_R);
    ball3Angle  = 0.0f;
    ball3Active = 1;
    ball3State  = -1;
    if(sndBallChannel != -1) {
        Mix_HaltChannel(sndBallChannel);
        sndBallChannel = -1;
    }
    lvl3WallBroken = 0;
    for(int i=0;i<LVL3_WALL_DEBRIS_COUNT;i++) lvl3WallDebris[i].active=0;
}

static float getFallDeathY(void)
{
    if(currentLevel == 0) return lvl1Ground.y;
    if(currentLevel == 2) return (float)LVL3_FLOOR_Y;
    return (float)SOL_Y;
}

static int shouldTriggerFallSound(void)
{
    if(currentLevel == 2) {
        float px1 = player.x;
        float px2 = player.x + PLAYER_W;
        float holeX1 = (float)LVL3_GAP_X;
        float holeX2 = (float)(LVL3_GAP_X + LVL3_GAP_W);
        if(px2 > holeX1 && px1 < holeX2) return 0;
    }
    return 1;
}

static int playerIsOnPlatformTop(const Platform *p)
{
    const float eps = 2.0f;
    if(!player.onGround) return 0;
    if(player.x + PLAYER_W <= p->x || player.x >= p->x + p->w) return 0;
    return fabsf((player.y + PLAYER_H) - p->y) <= eps;
}

static int lvl6CarInteractionAvailable(void)
{
    int carLeft = LVL6_CAR_X - LVL6_CAR_DRAW_W/2 - LVL6_CAR_INTERACT_PAD;
    int carRight = LVL6_CAR_X + LVL6_CAR_DRAW_W/2 + LVL6_CAR_INTERACT_PAD;

    return currentLevel == 5 &&
           !player.dead &&
           !player.won &&
           !lvl6MazeActive &&
           lvl6MazeFadePhase == MAZE_FADE_NONE &&
           !lvl6MazeCompleted &&
           playerIsOnPlatformTop(&lvl6[LVL6_COUNT-1]) &&
           player.x + PLAYER_W >= carLeft &&
           player.x <= carRight;
}

static int lvl6CarPromptVisible(void)
{
    return currentLevel == 5 &&
           !player.dead &&
           !player.won &&
           !lvl6MazeActive &&
           lvl6MazeFadePhase == MAZE_FADE_NONE &&
           !lvl6MazeCompleted &&
           playerIsOnPlatformTop(&lvl6[LVL6_COUNT-1]);
}

static int mazeIsTileSolid(int tileValue)
{
    return tileValue != 0 && tileValue != MAZE_TILE_SPAWN;
}

static int mazeIsPositionWalkable(float x, float y, float radius)
{
    int minX = (int)floorf(x - radius);
    int maxX = (int)floorf(x + radius);
    int minY = (int)floorf(y - radius);
    int maxY = (int)floorf(y + radius);

    for(int cellY=minY; cellY<=maxY; ++cellY) {
        for(int cellX=minX; cellX<=maxX; ++cellX) {
            if(cellX < 0 || cellX >= MAZE_MAP_W || cellY < 0 || cellY >= MAZE_MAP_H) {
                return 0;
            }
            if(!mazeIsTileSolid(lvl6MazeMap[cellY][cellX])) {
                continue;
            }

            float nearestX = fmaxf((float)cellX, fminf(x, (float)cellX + 1.0f));
            float nearestY = fmaxf((float)cellY, fminf(y, (float)cellY + 1.0f));
            float dx = x - nearestX;
            float dy = y - nearestY;
            if((dx*dx + dy*dy) < (radius*radius)) {
                return 0;
            }
        }
    }
    return 1;
}

static int mazeFindSpawnPoint(float *spawnX, float *spawnY)
{
    if(!spawnX || !spawnY) return 0;

    for(int cellY=0; cellY<MAZE_MAP_H; ++cellY) {
        for(int cellX=0; cellX<MAZE_MAP_W; ++cellX) {
            if(lvl6MazeMap[cellY][cellX] == MAZE_TILE_SPAWN) {
                *spawnX = (float)cellX + 0.5f;
                *spawnY = (float)cellY + 0.5f;
                return 1;
            }
        }
    }
    return 0;
}

static int mazeIsNearTileType(float x, float y, int tileType, float distance)
{
    float distanceSq = distance * distance;

    for(int cellY=0; cellY<MAZE_MAP_H; ++cellY) {
        for(int cellX=0; cellX<MAZE_MAP_W; ++cellX) {
            if(lvl6MazeMap[cellY][cellX] != tileType) continue;

            float nearestX = fmaxf((float)cellX, fminf(x, (float)cellX + 1.0f));
            float nearestY = fmaxf((float)cellY, fminf(y, (float)cellY + 1.0f));
            float dx = x - nearestX;
            float dy = y - nearestY;
            if((dx*dx + dy*dy) <= distanceSq) {
                return 1;
            }
        }
    }
    return 0;
}

static void mazeDetectFanSheetLayout(int width, int height, int *outCols, int *outRows)
{
    if(!outCols || !outRows) return;

    *outCols = 1;
    *outRows = 1;
    if(width <= 0 || height <= 0) return;

    if(width >= height * 2 && (width % height) == 0) {
        *outCols = width / height;
        return;
    }
    if(height >= width * 2 && (height % width) == 0) {
        *outRows = height / width;
        return;
    }

    if(((width > height) ? ((float)width / (float)height) : ((float)height / (float)width)) <= 1.35f) {
        int bestDivisor = 1;
        for(int divisor=2; divisor<=MAZE_FAN_LAYOUT_MAX_DIVISOR; ++divisor) {
            int frameW;
            int frameH;

            if((width % divisor) != 0 || (height % divisor) != 0) continue;
            frameW = width / divisor;
            frameH = height / divisor;
            if(frameW < 48 || frameH < 48) continue;
            bestDivisor = divisor;
        }
        if(bestDivisor > 1) {
            *outCols = bestDivisor;
            *outRows = bestDivisor;
            return;
        }
    }

    if((width % height) == 0) {
        *outCols = width / height;
        return;
    }
    if((height % width) == 0) {
        *outRows = height / width;
    }
}

static void mazeDrawDarkBackground(SDL_Renderer *renderer)
{
    SDL_Rect ceiling = {0, 0, MAZE_RENDER_W, MAZE_RENDER_H / 2};
    SDL_Rect floor = {0, MAZE_RENDER_H / 2, MAZE_RENDER_W, MAZE_RENDER_H / 2};
    SDL_SetRenderDrawColor(renderer,54,54,54,255);
    SDL_RenderFillRect(renderer,&ceiling);
    SDL_SetRenderDrawColor(renderer,34,34,34,255);
    SDL_RenderFillRect(renderer,&floor);
}

static SDL_Texture *buildMazeStaticBackground(SDL_Renderer *renderer)
{
    SDL_Texture *background = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        MAZE_RENDER_W,
        MAZE_RENDER_H
    );
    SDL_Texture *previousTarget;

    if(!background) return NULL;
    previousTarget = SDL_GetRenderTarget(renderer);
    if(SDL_SetRenderTarget(renderer, background) != 0) {
        SDL_DestroyTexture(background);
        return NULL;
    }

    mazeDrawDarkBackground(renderer);
    SDL_SetRenderTarget(renderer, previousTarget);
    return background;
}

static void renderMazeScene(SDL_Renderer *renderer, const MazePlayer *mazePlayer)
{
    if(texMazeBackground) SDL_RenderCopy(renderer, texMazeBackground, NULL, NULL);
    else                  mazeDrawDarkBackground(renderer);

    {
        float dirX = cosf(mazePlayer->angle);
        float dirY = sinf(mazePlayer->angle);
        float planeX = -dirY * MAZE_PLANE_SCALE;
        float planeY = dirX * MAZE_PLANE_SCALE;

        for(int x=0; x<MAZE_RENDER_W; ++x) {
            float cameraX = (2.0f * x / (float)MAZE_RENDER_W) - 1.0f;
            float rayDirX = dirX + planeX * cameraX;
            float rayDirY = dirY + planeY * cameraX;
            int mapX = (int)mazePlayer->x;
            int mapY = (int)mazePlayer->y;
            float deltaDistX = (fabsf(rayDirX) < 0.00001f) ? 1e30f : fabsf(1.0f / rayDirX);
            float deltaDistY = (fabsf(rayDirY) < 0.00001f) ? 1e30f : fabsf(1.0f / rayDirY);
            int stepX;
            int stepY;
            int side = 0;
            float sideDistX;
            float sideDistY;
            float perpWallDist;
            float wallX;
            float shade;
            int wallType = 1;
            SDL_Texture *columnTexture = texMazeWall;
            int texW = mazeWallW;
            int texHTotal = mazeWallH;
            int drawFanOverlay = 0;
            int lineHeight;
            int drawStartRaw;
            int drawEndRaw;
            int drawStart;
            int drawEnd;
            int drawHeight;
            float texStep;
            int texX;
            int texY;
            int texH;
            Uint8 mod;

            if(rayDirX < 0.0f) {
                stepX = -1;
                sideDistX = (mazePlayer->x - (float)mapX) * deltaDistX;
            } else {
                stepX = 1;
                sideDistX = ((float)mapX + 1.0f - mazePlayer->x) * deltaDistX;
            }

            if(rayDirY < 0.0f) {
                stepY = -1;
                sideDistY = (mazePlayer->y - (float)mapY) * deltaDistY;
            } else {
                stepY = 1;
                sideDistY = ((float)mapY + 1.0f - mazePlayer->y) * deltaDistY;
            }

            while(1) {
                if(sideDistX < sideDistY) {
                    sideDistX += deltaDistX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sideDistY += deltaDistY;
                    mapY += stepY;
                    side = 1;
                }

                if(mapX < 0 || mapX >= MAZE_MAP_W || mapY < 0 || mapY >= MAZE_MAP_H) break;
                if(mazeIsTileSolid(lvl6MazeMap[mapY][mapX])) break;
            }

            if(mapX >= 0 && mapX < MAZE_MAP_W && mapY >= 0 && mapY < MAZE_MAP_H) {
                wallType = lvl6MazeMap[mapY][mapX];
            }

            if(wallType == MAZE_TILE_FINISH && texMazeFinish && mazeFinishW > 0 && mazeFinishH > 0) {
                columnTexture = texMazeFinish;
                texW = mazeFinishW;
                texHTotal = mazeFinishH;
            } else if(wallType == MAZE_TILE_SWALL && texMazeSwall && mazeSwallW > 0 && mazeSwallH > 0) {
                columnTexture = texMazeSwall;
                texW = mazeSwallW;
                texHTotal = mazeSwallH;
            }

            drawFanOverlay = wallType == MAZE_TILE_FAN &&
                             texMazeFan &&
                             mazeFanW > 0 &&
                             mazeFanH > 0 &&
                             mazeFanFrameCount > 0;

            perpWallDist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
            if(perpWallDist < 0.03f) perpWallDist = 0.03f;

            lineHeight = (int)(MAZE_RENDER_H / perpWallDist);
            drawStartRaw = (-lineHeight / 2) + (MAZE_RENDER_H / 2);
            drawEndRaw = (lineHeight / 2) + (MAZE_RENDER_H / 2);
            drawStart = drawStartRaw < 0 ? 0 : drawStartRaw;
            drawEnd = drawEndRaw >= MAZE_RENDER_H ? (MAZE_RENDER_H - 1) : drawEndRaw;
            if(drawEnd < drawStart) continue;

            if(side == 0) wallX = mazePlayer->y + perpWallDist * rayDirY;
            else          wallX = mazePlayer->x + perpWallDist * rayDirX;
            wallX -= floorf(wallX);

            texX = (int)(wallX * (float)texW);
            if(texX < 0) texX = 0;
            else if(texX >= texW) texX = texW - 1;

            if((side == 0 && rayDirX > 0.0f) || (side == 1 && rayDirY < 0.0f)) {
                texX = texW - texX - 1;
            }

            drawHeight = drawEnd - drawStart + 1;
            texStep = lineHeight > 0 ? ((float)texHTotal / (float)lineHeight) : 0.0f;
            texY = (int)((drawStart - drawStartRaw) * texStep);
            if(texY < 0) texY = 0;
            texH = (int)(drawHeight * texStep);
            if(texH < 1) texH = 1;
            if(texY + texH > texHTotal) texH = texHTotal - texY;
            if(texH < 1) texH = 1;

            shade = 1.0f / (1.0f + 0.12f * perpWallDist * perpWallDist);
            if(side == 1) shade *= 0.78f;
            mod = (Uint8)(55.0f + 200.0f * shade);
            SDL_SetTextureColorMod(columnTexture, mod, mod, mod);

            {
                SDL_Rect src = {texX, texY, 1, texH};
                SDL_Rect dst = {x, drawStart, 1, drawHeight};
                SDL_RenderCopy(renderer, columnTexture, &src, &dst);
            }

            if(drawFanOverlay) {
                int frameW = mazeFanCols > 0 ? (mazeFanW / mazeFanCols) : 0;
                int frameH = mazeFanRows > 0 ? (mazeFanH / mazeFanRows) : 0;
                if(frameW > 0 && frameH > 0) {
                    float fanHalfWidth = MAZE_FAN_OVERLAY_WIDTH_FRACTION * 0.5f;
                    float fanLeft = 0.5f - fanHalfWidth;
                    float fanRight = 0.5f + fanHalfWidth;
                    if(wallX >= fanLeft && wallX <= fanRight) {
                        int frameIndex = (int)((SDL_GetTicks() / MAZE_FAN_FRAME_TIME_MS) % (Uint32)mazeFanFrameCount);
                        int frameCol = frameIndex % mazeFanCols;
                        int frameRow = frameIndex / mazeFanCols;
                        float fanU = (wallX - fanLeft) / (fanRight - fanLeft);
                        int fanTexX;
                        int fanDrawStartRaw;
                        int fanDrawEndRaw;
                        int fanDrawStart;
                        int fanDrawEnd;
                        int fanDrawH;
                        int fanRawH;
                        int fanTexY;
                        int fanTexH;

                        if((side == 0 && rayDirX > 0.0f) || (side == 1 && rayDirY < 0.0f)) {
                            fanU = 1.0f - fanU;
                        }

                        fanTexX = (int)(fanU * (float)frameW);
                        if(fanTexX < 0) fanTexX = 0;
                        else if(fanTexX >= frameW) fanTexX = frameW - 1;

                        fanDrawStartRaw = drawStartRaw + (int)((lineHeight * (1.0f - MAZE_FAN_OVERLAY_HEIGHT_FRACTION)) * 0.5f);
                        fanDrawEndRaw = fanDrawStartRaw + (int)(lineHeight * MAZE_FAN_OVERLAY_HEIGHT_FRACTION);
                        fanDrawStart = fanDrawStartRaw < drawStart ? drawStart : fanDrawStartRaw;
                        fanDrawEnd = fanDrawEndRaw > drawEnd ? drawEnd : fanDrawEndRaw;
                        if(fanDrawEnd >= fanDrawStart) {
                            fanDrawH = fanDrawEnd - fanDrawStart + 1;
                            fanRawH = fanDrawEndRaw - fanDrawStartRaw + 1;
                            if(fanRawH < 1) fanRawH = 1;
                            fanTexY = (int)(((fanDrawStart - fanDrawStartRaw) / (float)fanRawH) * frameH);
                            if(fanTexY < 0) fanTexY = 0;
                            fanTexH = (int)((fanDrawH / (float)fanRawH) * frameH);
                            if(fanTexH < 1) fanTexH = 1;
                            if(fanTexY + fanTexH > frameH) fanTexH = frameH - fanTexY;
                            if(fanTexH < 1) fanTexH = 1;

                            {
                                SDL_Rect fanSrc = {frameCol * frameW + fanTexX, frameRow * frameH + fanTexY, 1, fanTexH};
                                SDL_Rect fanDst = {x, fanDrawStart, 1, fanDrawH};
                                SDL_SetTextureColorMod(texMazeFan, mod, mod, mod);
                                SDL_RenderCopy(renderer, texMazeFan, &fanSrc, &fanDst);
                            }
                        }
                    }
                }
            }
        }
    }

    if(texMazeWall) SDL_SetTextureColorMod(texMazeWall,255,255,255);
    if(texMazeFinish) SDL_SetTextureColorMod(texMazeFinish,255,255,255);
    if(texMazeSwall) SDL_SetTextureColorMod(texMazeSwall,255,255,255);
    if(texMazeFan) SDL_SetTextureColorMod(texMazeFan,255,255,255);
}

static void drawMazeCarOverlay(SDL_Renderer *renderer)
{
    int dstW;
    int dstH;
    int maxH;
    SDL_Rect dst;
    SDL_Texture *carTexture = texMazeCar ? texMazeCar : texLvl6Car;

    if(!carTexture || mazeCarW <= 0 || mazeCarH <= 0) return;

    dstW = (MAZE_RENDER_W * 70) / 100;
    dstH = (int)((float)dstW * ((float)mazeCarH / (float)mazeCarW));
    maxH = (MAZE_RENDER_H * 75) / 100;
    if(dstH > maxH) {
        dstH = maxH;
        dstW = (int)((float)dstH * ((float)mazeCarW / (float)mazeCarH));
    }

    dstW /= 2;
    dstH /= 2;
    dstW = (dstW * 3) / 2;
    if(dstW < 1) dstW = 1;
    if(dstH < 1) dstH = 1;

    dst.x = (MAZE_RENDER_W - dstW) / 2;
    dst.y = MAZE_RENDER_H - dstH;
    dst.w = dstW;
    dst.h = dstH;
    SDL_RenderCopy(renderer, carTexture, NULL, &dst);
}

static void loadLvl6MazeAssets(void)
{
    SDL_Surface *surface;

    freeLvl6MazeAssets();
    if(!ren) return;

    surface = IMG_Load("assets/walls.png");
    if(surface) {
        texMazeWall = SDL_CreateTextureFromSurface(ren, surface);
        SDL_FreeSurface(surface);
        if(texMazeWall) SDL_QueryTexture(texMazeWall, NULL, NULL, &mazeWallW, &mazeWallH);
    }

    surface = IMG_Load("assets/finall.png");
    if(surface) {
        texMazeFinish = SDL_CreateTextureFromSurface(ren, surface);
        SDL_FreeSurface(surface);
        if(texMazeFinish) SDL_QueryTexture(texMazeFinish, NULL, NULL, &mazeFinishW, &mazeFinishH);
    }

    surface = IMG_Load("assets/swall.png");
    if(surface) {
        texMazeSwall = SDL_CreateTextureFromSurface(ren, surface);
        SDL_FreeSurface(surface);
        if(texMazeSwall) SDL_QueryTexture(texMazeSwall, NULL, NULL, &mazeSwallW, &mazeSwallH);
    }

    surface = IMG_Load("assets/fan.png");
    if(surface) {
        texMazeFan = SDL_CreateTextureFromSurface(ren, surface);
        SDL_FreeSurface(surface);
        if(texMazeFan) {
            SDL_SetTextureBlendMode(texMazeFan, SDL_BLENDMODE_BLEND);
            SDL_QueryTexture(texMazeFan, NULL, NULL, &mazeFanW, &mazeFanH);
            mazeDetectFanSheetLayout(mazeFanW, mazeFanH, &mazeFanCols, &mazeFanRows);
            mazeFanFrameCount = mazeFanCols * mazeFanRows;
            if(mazeFanFrameCount < 1) {
                mazeFanFrameCount = 1;
                mazeFanCols = 1;
                mazeFanRows = 1;
            }
        }
    }

    surface = IMG_Load("game maze./game maze/raycaster/car.png");
    if(surface) {
        texMazeCar = SDL_CreateTextureFromSurface(ren, surface);
        SDL_FreeSurface(surface);
        if(texMazeCar) {
            SDL_SetTextureBlendMode(texMazeCar, SDL_BLENDMODE_BLEND);
            SDL_QueryTexture(texMazeCar, NULL, NULL, &mazeCarW, &mazeCarH);
        }
    } else if(texLvl6Car) {
        SDL_QueryTexture(texLvl6Car, NULL, NULL, &mazeCarW, &mazeCarH);
    }

    texMazeBackground = buildMazeStaticBackground(ren);
    texMazeFrame = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, MAZE_RENDER_W, MAZE_RENDER_H);
    sndMazeEngine = Mix_LoadWAV("game maze./game maze/raycaster/carsound.mp3");
    sndMazeRadio = Mix_LoadWAV("game maze./game maze/raycaster/radio.mp3");

    lvl6MazeAssetsReady = texMazeWall &&
                          mazeWallW > 0 &&
                          mazeWallH > 0 &&
                          texMazeFinish &&
                          mazeFinishW > 0 &&
                          mazeFinishH > 0 &&
                          texMazeSwall &&
                          mazeSwallW > 0 &&
                          mazeSwallH > 0 &&
                          texMazeFrame;
}

static void stopLvl6MazeAudio(void)
{
    Mix_HaltChannel(MAZE_ENGINE_CHANNEL);
    Mix_HaltChannel(MAZE_RADIO_CHANNEL);
    lvl6MazeEnginePlaying = 0;
    lvl6MazeRadioPlaying = 0;
}

static void freeLvl6MazeAssets(void)
{
    stopLvl6MazeAudio();
    lvl6MazeAssetsReady = 0;
    lvl6MazeActive = 0;
    lvl6MazeFadePhase = MAZE_FADE_NONE;
    lvl6MazeFadeStartTick = 0;
    mazeWallW = 0;
    mazeWallH = 0;
    mazeFinishW = 0;
    mazeFinishH = 0;
    mazeSwallW = 0;
    mazeSwallH = 0;
    mazeFanW = 0;
    mazeFanH = 0;
    mazeFanCols = 1;
    mazeFanRows = 1;
    mazeFanFrameCount = 1;
    mazeCarW = 0;
    mazeCarH = 0;
    if(texMazeFrame) {
        SDL_DestroyTexture(texMazeFrame);
        texMazeFrame = NULL;
    }
    if(texMazeBackground) {
        SDL_DestroyTexture(texMazeBackground);
        texMazeBackground = NULL;
    }
    if(texMazeCar) {
        SDL_DestroyTexture(texMazeCar);
        texMazeCar = NULL;
    }
    if(texMazeFan) {
        SDL_DestroyTexture(texMazeFan);
        texMazeFan = NULL;
    }
    if(texMazeSwall) {
        SDL_DestroyTexture(texMazeSwall);
        texMazeSwall = NULL;
    }
    if(texMazeFinish) {
        SDL_DestroyTexture(texMazeFinish);
        texMazeFinish = NULL;
    }
    if(texMazeWall) {
        SDL_DestroyTexture(texMazeWall);
        texMazeWall = NULL;
    }
    if(sndMazeEngine) {
        Mix_FreeChunk(sndMazeEngine);
        sndMazeEngine = NULL;
    }
    if(sndMazeRadio) {
        Mix_FreeChunk(sndMazeRadio);
        sndMazeRadio = NULL;
    }
}

static void startLvl6Maze(void)
{
    if(!lvl6MazeAssetsReady) return;

    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    lvl6MazeActive = 1;
    lvl6MazeCompleted = 0;
    lvl6MazeRadioPlaying = 0;
    lvl6MazeEnginePlaying = 0;
    lvl6MazeEngineVolume = (float)MAZE_BASE_ENGINE_VOLUME;
    lvl6MazePrevTick = SDL_GetTicks();
    lvl6MazePlayer.x = 1.5f;
    lvl6MazePlayer.y = 1.5f;
    lvl6MazePlayer.angle = 0.0f;
    mazeFindSpawnPoint(&lvl6MazePlayer.x, &lvl6MazePlayer.y);

    if(sndMazeEngine) {
        Mix_Volume(MAZE_ENGINE_CHANNEL, MAZE_BASE_ENGINE_VOLUME);
        if(Mix_PlayChannel(MAZE_ENGINE_CHANNEL, sndMazeEngine, -1) >= 0) {
            lvl6MazeEnginePlaying = 1;
        }
    }
}

static void beginLvl6MazeTransition(void)
{
    if(!lvl6MazeAssetsReady || lvl6MazeActive || lvl6MazeFadePhase != MAZE_FADE_NONE) return;

    lvl6MazeFadePhase = MAZE_FADE_OUT;
    lvl6MazeFadeStartTick = SDL_GetTicks();
}

static int updateLvl6MazeTransition(void)
{
    Uint32 now;

    if(lvl6MazeFadePhase == MAZE_FADE_NONE) return 0;

    now = SDL_GetTicks();
    if(now - lvl6MazeFadeStartTick < MAZE_CAR_FADE_MS) return 1;

    if(lvl6MazeFadePhase == MAZE_FADE_OUT) {
        startLvl6Maze();
        lvl6MazeFadePhase = MAZE_FADE_IN;
        lvl6MazeFadeStartTick = now;
        return 1;
    }

    lvl6MazeFadePhase = MAZE_FADE_NONE;
    lvl6MazeFadeStartTick = 0;
    return 0;
}

static void renderLvl6MazeTransitionFade(void)
{
    Uint32 elapsed;
    Uint8 alpha;
    SDL_Rect overlay = {0, 0, SCREEN_W, SCREEN_H};

    if(lvl6MazeFadePhase == MAZE_FADE_NONE) return;

    elapsed = SDL_GetTicks() - lvl6MazeFadeStartTick;
    if(elapsed > MAZE_CAR_FADE_MS) elapsed = MAZE_CAR_FADE_MS;

    if(lvl6MazeFadePhase == MAZE_FADE_OUT) {
        alpha = (Uint8)(255u * elapsed / MAZE_CAR_FADE_MS);
    } else {
        alpha = (Uint8)(255u - (255u * elapsed / MAZE_CAR_FADE_MS));
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void toggleLvl6MazeRadio(void)
{
    if(!lvl6MazeActive || !sndMazeRadio) return;

    if(lvl6MazeRadioPlaying) {
        Mix_HaltChannel(MAZE_RADIO_CHANNEL);
        lvl6MazeRadioPlaying = 0;
    } else {
        Mix_Volume(MAZE_RADIO_CHANNEL, MAZE_RADIO_VOLUME_IDLE);
        if(Mix_PlayChannel(MAZE_RADIO_CHANNEL, sndMazeRadio, -1) >= 0) {
            lvl6MazeRadioPlaying = 1;
        }
    }
}

static void updateLvl6MazeMode(void)
{
    Uint32 now = SDL_GetTicks();
    float dt = (now - lvl6MazePrevTick) / 1000.0f;
    float moveStep = 0.0f;
    float nextX;
    float nextY;

    lvl6MazePrevTick = now;
    if(dt > MAZE_MAX_DT) dt = MAZE_MAX_DT;

    if(isMazeForwardPressed()) moveStep += MAZE_MOVE_SPEED * dt;
    if(isMazeBackwardPressed()) moveStep -= MAZE_MOVE_SPEED * dt;
    if(isMazeTurnLeftPressed()) lvl6MazePlayer.angle -= MAZE_TURN_SPEED * dt;
    if(isMazeTurnRightPressed()) lvl6MazePlayer.angle += MAZE_TURN_SPEED * dt;

    nextX = lvl6MazePlayer.x + cosf(lvl6MazePlayer.angle) * moveStep;
    nextY = lvl6MazePlayer.y + sinf(lvl6MazePlayer.angle) * moveStep;

    if(mazeIsPositionWalkable(nextX, lvl6MazePlayer.y, MAZE_PLAYER_RADIUS)) {
        lvl6MazePlayer.x = nextX;
    }
    if(mazeIsPositionWalkable(lvl6MazePlayer.x, nextY, MAZE_PLAYER_RADIUS)) {
        lvl6MazePlayer.y = nextY;
    }

    if(mazeIsNearTileType(lvl6MazePlayer.x, lvl6MazePlayer.y, MAZE_TILE_FINISH, MAZE_FINISH_TRIGGER_DISTANCE)) {
        lvl6MazeCompleted = 1;
        lvl6MazeActive = 0;
        player.won = 1;
        stopLvl6MazeAudio();
        return;
    }

    if(lvl6MazeEnginePlaying) {
        float targetVolume = isMazeForwardPressed() ? (float)MAZE_BOOST_ENGINE_VOLUME : (float)MAZE_BASE_ENGINE_VOLUME;
        float blend = fminf(1.0f, dt * MAZE_ENGINE_BLEND_SPEED);
        lvl6MazeEngineVolume += (targetVolume - lvl6MazeEngineVolume) * blend;
        Mix_Volume(MAZE_ENGINE_CHANNEL, (int)(lvl6MazeEngineVolume + 0.5f));
    }
    if(lvl6MazeRadioPlaying) {
        Mix_Volume(MAZE_RADIO_CHANNEL, isMazeForwardPressed() ? MAZE_RADIO_VOLUME_MOVING : MAZE_RADIO_VOLUME_IDLE);
    }
}

static void renderLvl6MazeMode(void)
{
    SDL_Texture *previousTarget;
    SDL_Rect dst = {0, 0, SCREEN_W, SCREEN_H};
    const char *radioPrompt = "[r] RADIO";

    if(!texMazeFrame) {
        SDL_SetRenderDrawColor(ren,0,0,0,255);
        SDL_RenderClear(ren);
        renderText(font,"FAILED TO LOAD THE MAZE",SCREEN_H/2-20,255,255,255);
        return;
    }

    previousTarget = SDL_GetRenderTarget(ren);
    SDL_SetRenderTarget(ren, texMazeFrame);
    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);
    renderMazeScene(ren, &lvl6MazePlayer);
    drawMazeCarOverlay(ren);
    SDL_SetRenderTarget(ren, previousTarget);

    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, texMazeFrame, NULL, &dst);
    if(gameMode == MODE_DUO) {
        radioPrompt = (duoInputPlayer == 0) ? "[shift] RADIO" : "[q] RADIO";
    } else {
        renderTextAt(font,"[esc] BACK",SCREEN_W-120,18,255,255,255);
    }
    renderTextAt(font,radioPrompt,110,18,255,255,255);
}

static void saveRuntimeState(RuntimeState *state)
{
#define SAVE_SCALAR(field) state->field = field
#define SAVE_ARRAY(field) memcpy(state->field, field, sizeof(field))
    SAVE_ARRAY(utensils);
    SAVE_SCALAR(utensilTriggered);
    SAVE_SCALAR(utensilTimer);
    SAVE_SCALAR(utensilNext);
    SAVE_SCALAR(utensilFallStartTick);
    SAVE_ARRAY(lvl5Drops);
    SAVE_SCALAR(lvl5RainTriggered);
    SAVE_SCALAR(lvl5RainTimer);
    SAVE_SCALAR(lvl5RainNext);
    SAVE_SCALAR(lvl5HoleVisible);
    SAVE_SCALAR(lvl5HoleX);
    SAVE_SCALAR(lvl5HoleW);
    SAVE_SCALAR(lvl5InHole);
    SAVE_SCALAR(lvl5WallCrushPending);
    SAVE_SCALAR(lvl5WallTouchSoundPlayed);
    SAVE_SCALAR(lvl5CrushX);
    SAVE_SCALAR(lvl5CrushY);
    SAVE_SCALAR(lvl2WallCrushPending);
    SAVE_SCALAR(lvl2WallTouchSoundPlayed);
    SAVE_SCALAR(lvl2CrushX);
    SAVE_SCALAR(lvl2CrushY);
    SAVE_SCALAR(lvl5HoleSpike);
    SAVE_SCALAR(sndBallChannel);
    SAVE_SCALAR(sndFireChannel);
    SAVE_SCALAR(sndEarthChannel);
    SAVE_SCALAR(sndIceChannel);
    SAVE_SCALAR(sndRollChannel);
    SAVE_SCALAR(sndStepChannel);
    SAVE_SCALAR(sndFallPlayed);
    SAVE_SCALAR(fireFrame);
    SAVE_SCALAR(fireLastTick);
    SAVE_SCALAR(lvl4LaserFrame);
    SAVE_SCALAR(lvl4LaserLastTick);
    SAVE_SCALAR(electriseFrame);
    SAVE_SCALAR(electriseLastTick);
    SAVE_SCALAR(burnFrame);
    SAVE_SCALAR(burnLastTick);
    SAVE_SCALAR(dustFrame);
    SAVE_SCALAR(dustLastTick);
    SAVE_SCALAR(dustPendingStart);
    SAVE_SCALAR(dustVisible);
    SAVE_SCALAR(touchFrame);
    SAVE_SCALAR(touchLastTick);
    SAVE_SCALAR(iceFrame);
    SAVE_SCALAR(iceLastTick);
    SAVE_SCALAR(iceAnimFinished);
    SAVE_SCALAR(lvl6MazeActive);
    SAVE_SCALAR(lvl6MazeRadioPlaying);
    SAVE_SCALAR(lvl6MazeEnginePlaying);
    SAVE_SCALAR(lvl6MazeEngineVolume);
    SAVE_SCALAR(lvl6MazePrevTick);
    SAVE_SCALAR(lvl6MazePlayer);
    SAVE_SCALAR(animFrame);
    SAVE_SCALAR(animLastTick);
    SAVE_SCALAR(animState);
    SAVE_SCALAR(currentLevel);
    SAVE_SCALAR(winStartTick);
    SAVE_SCALAR(transitionActive);
    SAVE_SCALAR(transitionNextLevel);
    SAVE_SCALAR(transitionStartTick);
    SAVE_SCALAR(deathType);
    SAVE_SCALAR(deathStartTick);
    SAVE_SCALAR(touchDeathX);
    SAVE_SCALAR(touchDeathY);
    SAVE_SCALAR(touchEffectX);
    SAVE_SCALAR(touchEffectY);
    SAVE_SCALAR(laserDeathVy);
    SAVE_SCALAR(laserDeathVx);
    SAVE_SCALAR(laserDeathTargetX);
    SAVE_SCALAR(player);
    SAVE_SCALAR(lvl6FreezeState);
    SAVE_SCALAR(lvl6MazeCompleted);
    SAVE_ARRAY(lvl1);
    SAVE_SCALAR(lvl1Ground);
    SAVE_SCALAR(spike1);
    SAVE_SCALAR(lvl1LastFloorDelayTimer);
    SAVE_ARRAY(lvl2);
    SAVE_SCALAR(slideSpike);
    SAVE_ARRAY(lvl4);
    SAVE_SCALAR(lvl4LaserVisible);
    SAVE_SCALAR(lvl4FireVisible);
    SAVE_SCALAR(lvl4Saw);
    SAVE_ARRAY(lvl4BallX);
    SAVE_ARRAY(lvl4BallY);
    SAVE_ARRAY(lvl4BallVy);
    SAVE_ARRAY(lvl4BallAngle);
    SAVE_ARRAY(lvl4BallFalling);
    SAVE_ARRAY(lvl4BallActive);
    SAVE_SCALAR(lvl4BallTriggered);
    SAVE_ARRAY(lvl6);
    SAVE_ARRAY(lvl6Chunks);
    SAVE_SCALAR(lvl6CollapseTimer);
    SAVE_SCALAR(lvl6CollapseSoundPlayed);
    SAVE_SCALAR(lvl6IceSoundPlayed);
    SAVE_ARRAY(lvl5);
    SAVE_SCALAR(spike5);
    SAVE_SCALAR(lvl5WallTrap);
    SAVE_ARRAY(lvl3);
    SAVE_SCALAR(ball3X);
    SAVE_SCALAR(ball3Angle);
    SAVE_SCALAR(ball3Active);
    SAVE_SCALAR(ball3State);
    SAVE_SCALAR(lvl3WallBroken);
    SAVE_ARRAY(lvl3WallDebris);
#undef SAVE_SCALAR
#undef SAVE_ARRAY
}

static void loadRuntimeState(const RuntimeState *state)
{
#define LOAD_SCALAR(field) field = state->field
#define LOAD_ARRAY(field) memcpy(field, state->field, sizeof(field))
    LOAD_ARRAY(utensils);
    LOAD_SCALAR(utensilTriggered);
    LOAD_SCALAR(utensilTimer);
    LOAD_SCALAR(utensilNext);
    LOAD_SCALAR(utensilFallStartTick);
    LOAD_ARRAY(lvl5Drops);
    LOAD_SCALAR(lvl5RainTriggered);
    LOAD_SCALAR(lvl5RainTimer);
    LOAD_SCALAR(lvl5RainNext);
    LOAD_SCALAR(lvl5HoleVisible);
    LOAD_SCALAR(lvl5HoleX);
    LOAD_SCALAR(lvl5HoleW);
    LOAD_SCALAR(lvl5InHole);
    LOAD_SCALAR(lvl5WallCrushPending);
    LOAD_SCALAR(lvl5WallTouchSoundPlayed);
    LOAD_SCALAR(lvl5CrushX);
    LOAD_SCALAR(lvl5CrushY);
    LOAD_SCALAR(lvl2WallCrushPending);
    LOAD_SCALAR(lvl2WallTouchSoundPlayed);
    LOAD_SCALAR(lvl2CrushX);
    LOAD_SCALAR(lvl2CrushY);
    LOAD_SCALAR(lvl5HoleSpike);
    LOAD_SCALAR(sndBallChannel);
    LOAD_SCALAR(sndFireChannel);
    LOAD_SCALAR(sndEarthChannel);
    LOAD_SCALAR(sndIceChannel);
    LOAD_SCALAR(sndRollChannel);
    LOAD_SCALAR(sndStepChannel);
    LOAD_SCALAR(sndFallPlayed);
    LOAD_SCALAR(fireFrame);
    LOAD_SCALAR(fireLastTick);
    LOAD_SCALAR(lvl4LaserFrame);
    LOAD_SCALAR(lvl4LaserLastTick);
    LOAD_SCALAR(electriseFrame);
    LOAD_SCALAR(electriseLastTick);
    LOAD_SCALAR(burnFrame);
    LOAD_SCALAR(burnLastTick);
    LOAD_SCALAR(dustFrame);
    LOAD_SCALAR(dustLastTick);
    LOAD_SCALAR(dustPendingStart);
    LOAD_SCALAR(dustVisible);
    LOAD_SCALAR(touchFrame);
    LOAD_SCALAR(touchLastTick);
    LOAD_SCALAR(iceFrame);
    LOAD_SCALAR(iceLastTick);
    LOAD_SCALAR(iceAnimFinished);
    LOAD_SCALAR(lvl6MazeActive);
    LOAD_SCALAR(lvl6MazeRadioPlaying);
    LOAD_SCALAR(lvl6MazeEnginePlaying);
    LOAD_SCALAR(lvl6MazeEngineVolume);
    LOAD_SCALAR(lvl6MazePrevTick);
    LOAD_SCALAR(lvl6MazePlayer);
    LOAD_SCALAR(animFrame);
    LOAD_SCALAR(animLastTick);
    LOAD_SCALAR(animState);
    LOAD_SCALAR(currentLevel);
    LOAD_SCALAR(winStartTick);
    LOAD_SCALAR(transitionActive);
    LOAD_SCALAR(transitionNextLevel);
    LOAD_SCALAR(transitionStartTick);
    LOAD_SCALAR(deathType);
    LOAD_SCALAR(deathStartTick);
    LOAD_SCALAR(touchDeathX);
    LOAD_SCALAR(touchDeathY);
    LOAD_SCALAR(touchEffectX);
    LOAD_SCALAR(touchEffectY);
    LOAD_SCALAR(laserDeathVy);
    LOAD_SCALAR(laserDeathVx);
    LOAD_SCALAR(laserDeathTargetX);
    LOAD_SCALAR(player);
    LOAD_SCALAR(lvl6FreezeState);
    LOAD_SCALAR(lvl6MazeCompleted);
    LOAD_ARRAY(lvl1);
    LOAD_SCALAR(lvl1Ground);
    LOAD_SCALAR(spike1);
    LOAD_SCALAR(lvl1LastFloorDelayTimer);
    LOAD_ARRAY(lvl2);
    LOAD_SCALAR(slideSpike);
    LOAD_ARRAY(lvl4);
    LOAD_SCALAR(lvl4LaserVisible);
    LOAD_SCALAR(lvl4FireVisible);
    LOAD_SCALAR(lvl4Saw);
    LOAD_ARRAY(lvl4BallX);
    LOAD_ARRAY(lvl4BallY);
    LOAD_ARRAY(lvl4BallVy);
    LOAD_ARRAY(lvl4BallAngle);
    LOAD_ARRAY(lvl4BallFalling);
    LOAD_ARRAY(lvl4BallActive);
    LOAD_SCALAR(lvl4BallTriggered);
    LOAD_ARRAY(lvl6);
    LOAD_ARRAY(lvl6Chunks);
    LOAD_SCALAR(lvl6CollapseTimer);
    LOAD_SCALAR(lvl6CollapseSoundPlayed);
    LOAD_SCALAR(lvl6IceSoundPlayed);
    LOAD_ARRAY(lvl5);
    LOAD_SCALAR(spike5);
    LOAD_SCALAR(lvl5WallTrap);
    LOAD_ARRAY(lvl3);
    LOAD_SCALAR(ball3X);
    LOAD_SCALAR(ball3Angle);
    LOAD_SCALAR(ball3Active);
    LOAD_SCALAR(ball3State);
    LOAD_SCALAR(lvl3WallBroken);
    LOAD_ARRAY(lvl3WallDebris);
#undef LOAD_SCALAR
#undef LOAD_ARRAY
}

static int isOnlineRemotePlayerActive(void)
{
    return gameMode == MODE_DUO &&
           duoInputPlayer == 1 &&
           online_client_is_connected() &&
           online_client_is_host();
}

static int remotePlayerScancodeDown(SDL_Scancode scancode)
{
    return isOnlineRemotePlayerActive() && online_client_remote_scancode_down(scancode);
}

static int level3PlayerUsesLocalInput(int playerIndex)
{
    if(playerIndex < 0 || playerIndex > 1) playerIndex = 0;

    if(gameMode != MODE_DUO) return playerIndex == 0;

    if(online_client_is_connected() && online_client_is_host()) {
        return playerIndex == 0;
    }

    return 1;
}

static int level3CurrentPlayerUsesLocalInput(void)
{
    return level3PlayerUsesLocalInput(level3CurrentInputPlayer());
}

static int level3ControlSchemeIsValid(ControlScheme scheme)
{
    return scheme == CONTROL_SCHEME_WASD ||
           scheme == CONTROL_SCHEME_ARROWS ||
           scheme == CONTROL_SCHEME_CONTROLLER;
}

static int level3ControlSchemesConflict(ControlScheme a, ControlScheme b)
{
    if(a == b) return 1;
    if((a == CONTROL_SCHEME_ARROWS || a == CONTROL_SCHEME_CONTROLLER) &&
       (b == CONTROL_SCHEME_ARROWS || b == CONTROL_SCHEME_CONTROLLER)) {
        return 1;
    }
    return 0;
}

static ControlScheme level3AlternateLocalDuoScheme(ControlScheme player0Scheme)
{
    return player0Scheme == CONTROL_SCHEME_WASD
        ? CONTROL_SCHEME_ARROWS
        : CONTROL_SCHEME_WASD;
}

static ControlScheme level3ControlSchemeForPlayer(int playerIndex)
{
    ControlScheme scheme;

    if(playerIndex < 0 || playerIndex > 1) playerIndex = 0;

    if(activeSession) {
        scheme = activeSession->player_control_scheme[playerIndex];
        if(level3ControlSchemeIsValid(scheme)) {
            if(gameMode == MODE_DUO &&
               !online_client_is_connected() &&
               playerIndex == 1 &&
               level3ControlSchemeIsValid(activeSession->player_control_scheme[0]) &&
               level3ControlSchemesConflict(activeSession->player_control_scheme[0], scheme)) {
                return level3AlternateLocalDuoScheme(activeSession->player_control_scheme[0]);
            }
            return scheme;
        }
        if(playerIndex == 0 && level3ControlSchemeIsValid(activeSession->control_scheme)) {
            return activeSession->control_scheme;
        }
    }

    return playerIndex == 1 ? CONTROL_SCHEME_WASD : CONTROL_SCHEME_ARROWS;
}

static InteractBind level3InteractBindForPlayer(int playerIndex)
{
    InteractBind bind;

    if(playerIndex < 0 || playerIndex > 1) playerIndex = 0;

    if(!activeSession) {
        return playerIndex == 1 ? INTERACT_BIND_E : INTERACT_BIND_F;
    }

    if(activeSession) {
        bind = activeSession->player_interact_bind[playerIndex];
        if(bind == INTERACT_BIND_E || bind == INTERACT_BIND_F || bind == INTERACT_BIND_0) {
            if(gameMode == MODE_DUO &&
               !online_client_is_connected() &&
               playerIndex == 1 &&
               activeSession->player_interact_bind[0] == bind) {
                return level3ControlSchemeForPlayer(playerIndex) == CONTROL_SCHEME_WASD
                    ? INTERACT_BIND_F
                    : INTERACT_BIND_0;
            }
            return bind;
        }
    }

    return level3ControlSchemeForPlayer(playerIndex) == CONTROL_SCHEME_WASD
        ? INTERACT_BIND_F
        : INTERACT_BIND_0;
}

static int level3KeyMatchesInteractBind(SDL_Keycode key, InteractBind bind)
{
    switch(bind) {
        case INTERACT_BIND_E:
            return key == SDLK_e;
        case INTERACT_BIND_F:
            return key == SDLK_f;
        case INTERACT_BIND_0:
            return key == SDLK_0 || key == SDLK_KP_0;
        default:
            return 0;
    }
}

static int level3InteractKeyPressedForPlayer(SDL_Keycode key, int playerIndex)
{
    return level3KeyMatchesInteractBind(key, level3InteractBindForPlayer(playerIndex));
}

static int level3SchemeLeftPressed(const Uint8 *kb, ControlScheme scheme)
{
    if(!kb) return 0;
    if(scheme == CONTROL_SCHEME_WASD) return kb[SDL_SCANCODE_A];
    if(scheme == CONTROL_SCHEME_CONTROLLER) {
        return kb[SDL_SCANCODE_J] || arcade_input_scancode_down(SDL_SCANCODE_LEFT);
    }
    return kb[SDL_SCANCODE_LEFT] || arcade_input_scancode_down(SDL_SCANCODE_LEFT);
}

static int level3SchemeRightPressed(const Uint8 *kb, ControlScheme scheme)
{
    if(!kb) return 0;
    if(scheme == CONTROL_SCHEME_WASD) return kb[SDL_SCANCODE_D];
    if(scheme == CONTROL_SCHEME_CONTROLLER) {
        return kb[SDL_SCANCODE_L] || arcade_input_scancode_down(SDL_SCANCODE_RIGHT);
    }
    return kb[SDL_SCANCODE_RIGHT] || arcade_input_scancode_down(SDL_SCANCODE_RIGHT);
}

static int level3SchemeUpPressed(const Uint8 *kb, ControlScheme scheme)
{
    if(!kb) return 0;
    if(scheme == CONTROL_SCHEME_WASD) return kb[SDL_SCANCODE_W];
    if(scheme == CONTROL_SCHEME_CONTROLLER) {
        return kb[SDL_SCANCODE_I] ||
               arcade_input_scancode_down(SDL_SCANCODE_UP) ||
               arcade_input_scancode_down(SDL_SCANCODE_SPACE);
    }
    return kb[SDL_SCANCODE_UP] ||
           arcade_input_scancode_down(SDL_SCANCODE_UP) ||
           arcade_input_scancode_down(SDL_SCANCODE_SPACE);
}

static int level3SchemeDownPressed(const Uint8 *kb, ControlScheme scheme)
{
    if(!kb) return 0;
    if(scheme == CONTROL_SCHEME_WASD) return kb[SDL_SCANCODE_S];
    if(scheme == CONTROL_SCHEME_CONTROLLER) {
        return kb[SDL_SCANCODE_K] || arcade_input_scancode_down(SDL_SCANCODE_DOWN);
    }
    return kb[SDL_SCANCODE_DOWN] || arcade_input_scancode_down(SDL_SCANCODE_DOWN);
}

static int level3CurrentInputPlayer(void)
{
    return gameMode == MODE_DUO ? duoInputPlayer : 0;
}

static int isPlatformLeftPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeLeftPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_A);
}

static int isPlatformRightPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeRightPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_D);
}

static int isPlatformJumpPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeUpPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_W) ||
           remotePlayerScancodeDown(SDL_SCANCODE_SPACE);
}

static int isMazeForwardPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeUpPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_W);
}

static int isMazeBackwardPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeDownPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_S);
}

static int isMazeTurnLeftPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeLeftPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_A);
}

static int isMazeTurnRightPressed(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    ControlScheme scheme = level3ControlSchemeForPlayer(level3CurrentInputPlayer());
    return (level3CurrentPlayerUsesLocalInput() && level3SchemeRightPressed(kb, scheme)) ||
           remotePlayerScancodeDown(SDL_SCANCODE_D);
}

/* ═══════════════════════════════════════════════════════
   INIT JOUEUR
═══════════════════════════════════════════════════════ */
static void initPlayer(void)
{
    Mix_HaltMusic();
    lvl6MazeActive = 0;
    stopLvl6MazeAudio();
    if(sndRollChannel != -1) {
        Mix_HaltChannel(sndRollChannel);
        sndRollChannel = -1;
    }
    if(sndEarthChannel != -1) {
        Mix_HaltChannel(sndEarthChannel);
        sndEarthChannel = -1;
    }
    if(sndIceChannel != -1) {
        Mix_HaltChannel(sndIceChannel);
        sndIceChannel = -1;
    }
    if(sndStepChannel != -1) {
        Mix_HaltChannel(sndStepChannel);
        sndStepChannel = -1;
    }
    if (currentLevel == 0) {
        initLvl1(); initSpike1(); initUtensils();
        player.x = lvl1[0].x + lvl1[0].w/2.0f - PLAYER_W/2.0f;
        player.y = lvl1[0].y - PLAYER_H;
    } else if (currentLevel == 1) {
        initLvl2();
        player.x = SOL_G_X + 40;
        player.y = SOL_Y - PLAYER_H;
    } else if (currentLevel == 2) {
        initLvl3();
        /* joueur demarre a l extremite gauche */
        player.x = 10;
        player.y = (float)(LVL3_FLOOR_Y - PLAYER_H);
    } else if (currentLevel == 3) {
        initLvl4();
        player.x = SOL_G_X + 40;
        player.y = SOL_Y - PLAYER_H;
    } else if (currentLevel == 4) {
        initLvl5();
        player.x = 40;
        player.y = SOL_Y - PLAYER_H;
    } else {
        initLvl6();
        if(sndEarthFx) {
            sndEarthChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_EARTH, sndEarthFx, 0);
            lvl6CollapseSoundPlayed = 1;
        }
        player.x = SOL_G_X + 40;
        player.y = SOL_Y - PLAYER_H;
    }
    player.vx=0; player.vy=0;
    player.onGround=1; player.facingRight=1;
    player.dead=0; player.won=0; player.noJump=0;
    lvl6FreezeState=0;
    deathType=DEATH_NONE;
    deathStartTick=0;
    winStartTick=0;
    touchDeathX=0.0f;
    touchDeathY=0.0f;
    touchEffectX=0.0f;
    touchEffectY=0.0f;
    laserDeathVy=0.0f;
    laserDeathVx=0.0f;
    electriseFrame=0;
    electriseLastTick=SDL_GetTicks();
    burnFrame=0;
    burnLastTick=SDL_GetTicks();
    dustFrame=0;
    dustLastTick=0;
    dustPendingStart=0;
    dustVisible=0;
    touchFrame=0;
    touchLastTick=SDL_GetTicks();
    iceFrame=0;
    iceLastTick=SDL_GetTicks();
    iceAnimFinished=0;
    sndFallPlayed=0;
    animFrame=0; animLastTick=SDL_GetTicks(); animState=PANIM_IDLE;
    transitionActive=0;
    transitionNextLevel=currentLevel;
    transitionStartTick=0;
}

static void startLevelTransition(int nextLevel)
{
    if(nextLevel < 0) nextLevel = 0;
    if(nextLevel >= MAX_LEVELS) nextLevel = MAX_LEVELS - 1;
    transitionActive = 1;
    transitionNextLevel = nextLevel;
    transitionStartTick = SDL_GetTicks();
}

static int isLevelTransitionActive(void)
{
    return transitionActive;
}

static int updateLevelTransition(void)
{
    Uint32 now;

    if(!transitionActive) return 0;

    now = SDL_GetTicks();
    if(now - transitionStartTick < LEVEL_TRANSITION_FADE_MS) return 0;

    currentLevel = transitionNextLevel;
    transitionActive = 0;
    initPlayer();
    return 1;
}

static void renderLevelTransitionFade(void)
{
    Uint32 elapsed;
    Uint8 alpha;
    SDL_Rect overlay = {0, 0, SCREEN_W, SCREEN_H};

    if(!transitionActive) return;

    elapsed = SDL_GetTicks() - transitionStartTick;
    if(elapsed > LEVEL_TRANSITION_FADE_MS) elapsed = LEVEL_TRANSITION_FADE_MS;
    alpha = (Uint8)(255u * elapsed / LEVEL_TRANSITION_FADE_MS);

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, alpha);
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

/* ═══════════════════════════════════════════════════════
   ANIMATION
═══════════════════════════════════════════════════════ */
static void tickAnim(void)
{
    Uint32 now = SDL_GetTicks();
    Uint32 fps = (animState==PANIM_RUN)?14:(animState==PANIM_IDLE)?8:10;
    if (now - animLastTick >= 1000u/fps) {
        animFrame = (animFrame+1) % 36;
        animLastTick = now;
    }
}

static void updateIceAnim(void)
{
    Uint32 now = SDL_GetTicks();
    if(iceFrameCount <= 1 || iceAnimFinished) return;
    if(now - iceLastTick >= ICE_FRAME_DELAY) {
        if(iceFrame < iceFrameCount - 1) iceFrame++;
        else iceAnimFinished = 1;
        iceLastTick = now;
    }
}

static SDL_Rect getIceFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, 0, 0};
    PlayerSkinAssets *skin = getCurrentPlayerSkin();
    if(!skin->ice || iceFrameW <= 0 || iceFrameH <= 0 || iceFrameCols <= 0) return src;

    src.x = (iceFrame % iceFrameCols) * iceFrameW;
    src.y = (iceFrame / iceFrameCols) * iceFrameH;
    src.w = iceFrameW;
    src.h = iceFrameH;
    return src;
}

static void renderIceSnowflakes(int x, int y, int w, int h)
{
    float t = (float)SDL_GetTicks() * 0.001f;

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,235,245,255,220);
    for(int i=0;i<24;i++) {
        float fx = (float)(x + ((i * 23) % (w + 24)) - 12);
        float fall = fmodf(t * (55.0f + (float)(i % 5) * 12.0f) + (float)(i * 19), (float)(h + 36));
        float sway = sinf(t * 1.7f + (float)i * 0.8f) * (8.0f + (float)(i % 4) * 2.0f);
        int cx = (int)(fx + sway);
        int cy = (int)(y - 24.0f + fall);
        int r = 5 + (i % 3);

        SDL_RenderDrawLine(ren,cx-r,cy,cx+r,cy);
        SDL_RenderDrawLine(ren,cx,cy-r,cx,cy+r);
        SDL_RenderDrawLine(ren,cx-r+1,cy-r+1,cx+r-1,cy+r-1);
        SDL_RenderDrawLine(ren,cx-r+1,cy+r-1,cx+r-1,cy-r+1);
        SDL_RenderDrawLine(ren,cx-r/2,cy-r,cx+r/2,cy+r);
        SDL_RenderDrawLine(ren,cx-r/2,cy+r,cx+r/2,cy-r);
    }
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

/* ═══════════════════════════════════════════════════════
   DENTS
═══════════════════════════════════════════════════════ */
static void drawToothUp(int x, int baseY, int w, int h)
{
    for (int i = 0; i < h; i++) {
        float t = (float)i/(float)(h-1);
        int sw = (int)((1.0f-t)*(w-1))+1;
        int sx = x+(w-sw)/2;
        SDL_SetRenderDrawColor(ren,255,255,255,255);
        SDL_RenderDrawLine(ren,sx,baseY-i,sx+sw-1,baseY-i);
    }
}

static void drawToothLeft(int baseX, int y, int w, int h)
{
    for (int i = 0; i < w; i++) {
        float t = (float)i/(float)(w-1);
        int sh = (int)((1.0f-t)*(h-1))+1;
        int sy = y+(h-sh)/2;
        SDL_SetRenderDrawColor(ren,255,255,255,255);
        SDL_RenderDrawLine(ren,baseX-i,sy,baseX-i,sy+sh-1);
    }
}

static void drawToothDown(int x, int baseY, int w, int h)
{
    for (int i = 0; i < h; i++) {
        float t = (float)i/(float)(h-1);
        int sw = (int)((1.0f-t)*(w-1))+1;
        int sx = x+(w-sw)/2;
        SDL_SetRenderDrawColor(ren,255,255,255,255);
        SDL_RenderDrawLine(ren,sx,baseY+i,sx+sw-1,baseY+i);
    }
}

static void drawRotatedTooth(float ax, float ay, float nx, float ny, float tx, float ty, int len, int span)
{
    for(int i=0;i<len;i++) {
        float ratio = (len > 1) ? (float)i / (float)(len - 1) : 0.0f;
        float halfSpan = (1.0f - ratio) * (float)span;
        float cx = ax + nx * (float)i;
        float cy = ay + ny * (float)i;
        float x1 = cx - tx * halfSpan;
        float y1 = cy - ty * halfSpan;
        float x2 = cx + tx * halfSpan;
        float y2 = cy + ty * halfSpan;
        SDL_SetRenderDrawColor(ren,255,255,255,255);
        SDL_RenderDrawLine(ren,(int)x1,(int)y1,(int)x2,(int)y2);
    }
}

/* ═══════════════════════════════════════════════════════
   UPDATE FLOOR QUI TOMBE
═══════════════════════════════════════════════════════ */
static void updateFallingFloor(Platform *p)
{
    if (!p->falling) return;
    p->fallVy += GRAVITY;
    if (p->fallVy > MAX_FALL) p->fallVy = MAX_FALL;
    p->y += p->fallVy;
    float pcx = player.x + PLAYER_W/2.0f;
    if (!player.dead && pcx >= p->x && pcx <= p->x+p->w) {
        player.noJump=1; player.onGround=0;
        player.vy=p->fallVy; player.y+=p->fallVy;
    }
}

static __attribute__((unused)) SDL_Rect getSlideSpikeRect(void)
{
    float pivotX = slideSpike.x;
    float pivotY = slideSpike.y + SLIDE_H;
    float a = slideSpike.angle * (float)M_PI / 180.0f;
    float c = cosf(a);
    float s = sinf(a);
    float pts[4][2] = {
        {0.0f, 0.0f},
        {(float)SLIDE_W, 0.0f},
        {0.0f, (float)-SLIDE_H},
        {(float)SLIDE_W, (float)-SLIDE_H}
    };
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;

    for(int i=0;i<4;i++) {
        float lx = pts[i][0];
        float ly = pts[i][1];
        float wx = pivotX + lx * c + ly * s;
        float wy = pivotY - lx * s + ly * c;
        if(wx < minX) minX = wx;
        if(wx > maxX) maxX = wx;
        if(wy < minY) minY = wy;
        if(wy > maxY) maxY = wy;
    }

    SDL_Rect r={(int)minX,(int)minY,(int)(maxX-minX),(int)(maxY-minY)};
    return r;
}

static void getSlideSpikePoints(float pts[4][2])
{
    float pivotX = slideSpike.x;
    float pivotY = slideSpike.y + SLIDE_H;
    float a = slideSpike.angle * (float)M_PI / 180.0f;
    float c = cosf(a);
    float s = sinf(a);
    float local[4][2] = {
        {0.0f, 0.0f},
        {(float)SLIDE_W, 0.0f},
        {(float)SLIDE_W, (float)-SLIDE_H},
        {0.0f, (float)-SLIDE_H}
    };

    for(int i=0;i<4;i++) {
        float lx = local[i][0];
        float ly = local[i][1];
        pts[i][0] = pivotX + lx * c + ly * s;
        pts[i][1] = pivotY - lx * s + ly * c;
    }
}

static void projectPointsOnAxis(const float pts[][2], int count, float ax, float ay,
                                float *minProj, float *maxProj)
{
    float proj = pts[0][0] * ax + pts[0][1] * ay;
    *minProj = proj;
    *maxProj = proj;
    for(int i=1;i<count;i++) {
        proj = pts[i][0] * ax + pts[i][1] * ay;
        if(proj < *minProj) *minProj = proj;
        if(proj > *maxProj) *maxProj = proj;
    }
}

static int rangesOverlap(float minA, float maxA, float minB, float maxB)
{
    return !(maxA < minB || maxB < minA);
}

static int slideSpikeTouchesPlayerPrecisely(void)
{
    float wallPts[4][2];
    float playerPts[4][2] = {
        {player.x, player.y},
        {player.x + PLAYER_W, player.y},
        {player.x + PLAYER_W, player.y + PLAYER_H},
        {player.x, player.y + PLAYER_H}
    };
    float axes[4][2];

    getSlideSpikePoints(wallPts);

    axes[0][0] = wallPts[1][0] - wallPts[0][0];
    axes[0][1] = wallPts[1][1] - wallPts[0][1];
    axes[1][0] = wallPts[2][0] - wallPts[1][0];
    axes[1][1] = wallPts[2][1] - wallPts[1][1];
    axes[2][0] = 1.0f;
    axes[2][1] = 0.0f;
    axes[3][0] = 0.0f;
    axes[3][1] = 1.0f;

    for(int i=0;i<4;i++) {
        float ax = -axes[i][1];
        float ay = axes[i][0];
        float len = sqrtf(ax * ax + ay * ay);
        float wallMin, wallMax, playerMin, playerMax;

        if(len <= 0.0001f) continue;
        ax /= len;
        ay /= len;

        projectPointsOnAxis(wallPts, 4, ax, ay, &wallMin, &wallMax);
        projectPointsOnAxis(playerPts, 4, ax, ay, &playerMin, &playerMax);
        if(!rangesOverlap(wallMin, wallMax, playerMin, playerMax)) return 0;
    }

    return 1;
}

/* ═══════════════════════════════════════════════════════
   UPDATE PIEGE COULISSANT
═══════════════════════════════════════════════════════ */
static void updateSlideSpike(void)
{
    if(slideSpike.dustTimer > 0) slideSpike.dustTimer--;
    if (slideSpike.landed) return;
    if (!slideSpike.triggered &&
        player.x > slideSpike.triggerX &&
        player.y+PLAYER_H >= SOL_Y) {
        slideSpike.triggered=1; slideSpike.sliding=1;
    }
    if (slideSpike.sliding) {
        slideSpike.x -= SLIDE_SPD;
        slideSpike.slideDone += SLIDE_SPD;
        slideSpike.angle = 0;
        if (slideSpike.slideDone >= SLIDE_DIST) {
            slideSpike.x += (float)SLIDE_FALL_SHIFT_X;
            slideSpike.sliding=0;
            slideSpike.falling=1;
            slideSpike.vy=0;
            playWallSound();
        }
    }
    if (slideSpike.falling) {
        slideSpike.angle += SLIDE_FALL_ROT_SPD;
        if (slideSpike.angle > 90.0f) slideSpike.angle=90.0f;
        if (slideSpike.angle >= 90.0f) {
            slideSpike.falling=0;
            slideSpike.landed=1;
            slideSpike.angle=90.0f;
            slideSpike.dustTimer = 18;
        }
    }
    if (!slideSpike.triggered || slideSpike.landed) return;
    if (slideSpikeTouchesPlayerPrecisely()) {
        if (slideSpike.falling) {
            if (!godMode && !lvl2WallCrushPending) {
                if(!lvl2WallTouchSoundPlayed) {
                    playTouchSound();
                    lvl2WallTouchSoundPlayed = 1;
                }
                lvl2WallCrushPending = 1;
                lvl2CrushX = player.x;
                lvl2CrushY = player.y;
                crushPlayer();
            }
        } else if(!player.dead) {
            killPlayerWithTouch();
        }
    }
}

/* ═══════════════════════════════════════════════════════
   UPDATE USTENSILES
═══════════════════════════════════════════════════════ */
static int utensilHalfWidth(UtensilType t)
{
    (void)t;
    return MICROWAVE_HW;
}

static int lvl5DropHalfWidth(RainType t)
{
    if (t==RAIN_POT) return POT_HW;
    if (t==RAIN_FORK) return FORK_RAIN_HW;
    if (t==RAIN_MICROWAVE) return MICROWAVE_HW;
    return PAN_HW;
}

static void updateUtensils(void)
{
    if (!utensilTriggered) return;
    float landY = (float)lvl1[3].y;

    if (utensilNext == 0) {
        for (int j=0;j<UTENSIL_COUNT;j++) {
            utensils[j].state = USTATE_FALLING;
            utensils[j].y     = -10.0f;
            utensils[j].vy    = 150.0f;
        }
        utensilFallStartTick = SDL_GetTicks();
        utensilNext = UTENSIL_COUNT;
    }

    for (int i=0;i<UTENSIL_COUNT;i++) {
        Utensil *u = &utensils[i];
        if (u->state == USTATE_WAIT) continue;

        if (u->state == USTATE_FALLING) {
            float prevY = u->y;
            Uint32 elapsed = SDL_GetTicks() - utensilFallStartTick;
            float progress = (float)elapsed / (float)UTENSIL_FALL_DURATION_MS;
            if (progress > 1.0f) progress = 1.0f;
            u->y = -10.0f + (landY + 10.0f) * progress;

            int hw = utensilHalfWidth(u->type);
            float ux1=u->x-hw, ux2=u->x+hw;
            float uy2=u->y;
            float prevBottom = prevY;
            float headY = player.y;
            float headBandBottom = player.y + 10.0f;

            if (!player.dead &&
                player.x+PLAYER_W>ux1 && player.x<ux2 &&
                prevBottom <= headBandBottom && uy2 >= headY) {
                float effectX = player.x;
                float effectY = player.y - PLAYER_H * 0.5f;
                killPlayerWithTouchEffectAt(effectX, effectY);
            }

            if (progress >= 1.0f || u->y >= landY) {
                u->y=landY; u->state=USTATE_SMASHED;
                u->smashY=landY; u->smashTimer=SMASH_FRAMES;
            }
        }

        if (u->state==USTATE_SMASHED && u->smashTimer>0)
            u->smashTimer--;
    }
}

static int lvl1UtensilAnimationActive(void)
{
    for(int i=0;i<UTENSIL_COUNT;i++) {
        if(utensils[i].state == USTATE_FALLING) return 1;
        if(utensils[i].state == USTATE_SMASHED && utensils[i].smashTimer > 0) return 1;
    }
    return 0;
}

static int lvl1FallingPlatformActive(void)
{
    return lvl1[7].falling && lvl1[7].y <= SCREEN_H + lvl1[7].h;
}

static void updateLvl1DeathSequence(void)
{
    updateLvl1LastFloorTrap();
    updateUtensils();
    if(deathType == DEATH_TOUCH) tickTouchAnim();
}

static void updateLvl5Rain(void)
{
    if (!lvl5RainTriggered) return;

    float landY = (float)lvl5[0].y;

    if(lvl5RainNext < LVL5_DROP_COUNT) {
        lvl5RainTimer++;
        if(lvl5RainTimer >= LVL5_DROP_DELAY) {
            RainDrop *d = &lvl5Drops[lvl5RainNext];
            d->state = USTATE_FALLING;
            d->y = -20.0f;
            d->vy = 150.0f;
            lvl5RainNext++;
            lvl5RainTimer = 0;
        }
    }

    for(int i=0;i<LVL5_DROP_COUNT;i++) {
        RainDrop *d=&lvl5Drops[i];
        if(d->state==USTATE_WAIT) continue;

        if(d->state==USTATE_FALLING) {
            float topOffset = (d->type==RAIN_MICROWAVE)?22.0f:28.0f;
            float prevY = d->y;
            d->vy += UTENSIL_GRAVITY;
            if(d->vy > 500.0f) d->vy = 500.0f;
            d->y += d->vy;

            int hw = lvl5DropHalfWidth(d->type);
            float ux1=d->x-hw, ux2=d->x+hw;
            float uy1=d->y-topOffset, uy2=d->y;

            if(!player.dead &&
               player.x+PLAYER_W>ux1 && player.x<ux2 &&
               player.y+PLAYER_H>uy1 && player.y<uy2) {
                float effectX = player.x;
                float effectY = player.y - PLAYER_H * 0.5f;
                killPlayerWithTouchEffectAt(effectX, effectY);
            }

            if(prevY < landY && d->y >= landY && d->type == RAIN_POT) {
                playPanLandSound();
            }
            if(prevY < landY && d->y >= landY && d->type == RAIN_FORK) {
                playForkLandSound();
            }
            if(prevY < landY && d->y >= landY && d->type == RAIN_MICROWAVE) {
                playWallSound();
            }

            if(d->y >= landY) {
                d->y = landY;
                d->state = USTATE_SMASHED;
                d->smashTimer = SMASH_FRAMES;
            }
        }

        if(d->state==USTATE_SMASHED && d->smashTimer>0) d->smashTimer--;
    }
}

// SAW UPDATE
static __attribute__((unused)) void updateSawBlade(SawBlade *saw)
{
    if(!saw->active) return;

    Uint32 now = SDL_GetTicks();
    if(now - saw->lastTick >= saw->frameDelay) {
        saw->frame = (saw->frame + 1) % saw->frameCount;
        saw->lastTick = now;
    }

    float hitInset = (float)(SAW_FRAME_SIZE - SAW_HITBOX) / 2.0f;
    float hx = saw->x + hitInset;
    float hy = saw->y + hitInset + 2.0f;
    float hw = (float)SAW_HITBOX;
    float hh = (float)SAW_HITBOX;

    if(player.x + PLAYER_W > hx &&
       player.x < hx + hw &&
       player.y + PLAYER_H > hy &&
       player.y < hy + hh) {
        killPlayer();
    }
}

static void updateFireAnim(void)
{
    if(!lvl4FireVisible || fireFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();
    if(now - fireLastTick >= FIRE_FRAME_DELAY) {
        fireFrame = (fireFrame + 1) % fireFrameCount;
        fireLastTick = now;
    }
}

static void updateLvl4LaserAnim(void)
{
    if(!lvl4LaserVisible || lvl4LaserFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();
    if(now - lvl4LaserLastTick >= LVL4_LAZER_FRAME_DELAY) {
        lvl4LaserFrame = (lvl4LaserFrame + 1) % lvl4LaserFrameCount;
        lvl4LaserLastTick = now;
    }
}

static SDL_Rect getFireFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, fireFrameW, fireFrameH};

    if(fireFrameCount <= 1 || fireFrameW <= 0 || fireFrameH <= 0) {
        return src;
    }

    if(fireFrame >= fireFrameCount) fireFrame = 0;

    src.x = (fireFrame % FIRE_SHEET_COLS) * fireFrameW;
    src.y = (fireFrame / FIRE_SHEET_COLS) * fireFrameH;

    return src;
}

static SDL_Rect getLvl4LaserFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, lvl4LaserFrameW, lvl4LaserFrameH};

    if(lvl4LaserFrameCount <= 1 || lvl4LaserFrameW <= 0 || lvl4LaserFrameH <= 0) {
        return src;
    }

    if(lvl4LaserFrame >= lvl4LaserFrameCount) lvl4LaserFrame = 0;

    src.x = (lvl4LaserFrame % LVL4_LAZER_SHEET_COLS) * lvl4LaserFrameW;
    src.y = (lvl4LaserFrame / LVL4_LAZER_SHEET_COLS) * lvl4LaserFrameH;
    return src;
}

static void tickBurnAnim(void)
{
    if(!player.dead || deathType != DEATH_FIRE || burnFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();
    if(now - burnLastTick >= BURN_FRAME_DELAY) {
        if(burnFrame < burnFrameCount - 1) burnFrame++;
        burnLastTick = now;
    }
}

static void tickDustAnim(void)
{
    if(!player.dead || deathType != DEATH_FIRE || !isBurnAnimFinished() || dustFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();

    if(!dustPendingStart && !dustVisible) {
        dustPendingStart = 1;
        return;
    }

    if(dustPendingStart) {
        if(sndFireChannel != -1) {
            Mix_HaltChannel(sndFireChannel);
            sndFireChannel = -1;
        }
        dustPendingStart = 0;
        dustVisible = 1;
        lvl4FireVisible = 0;
        dustLastTick = now;
        dustFrame = 0;
        return;
    }

    if(now - dustLastTick >= DUST_FRAME_DELAY) {
        if(dustFrame < dustFrameCount - 1) dustFrame++;
        dustLastTick = now;
    }
}

static SDL_Rect getBurnFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, burnFrameW, burnFrameH};

    if(burnFrameCount <= 1 || burnFrameW <= 0 || burnFrameH <= 0) {
        return src;
    }

    if(burnFrame >= burnFrameCount) burnFrame = burnFrameCount - 1;

    src.x = (burnFrame % BURN_SHEET_COLS) * burnFrameW;
    src.y = (burnFrame / BURN_SHEET_COLS) * burnFrameH;
    return src;
}

static SDL_Rect getDustFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, dustFrameW, dustFrameH};

    if(dustFrameCount <= 1 || dustFrameW <= 0 || dustFrameH <= 0) {
        return src;
    }

    if(dustFrame >= dustFrameCount) dustFrame = dustFrameCount - 1;

    src.x = (dustFrame % DUST_SHEET_COLS) * dustFrameW;
    src.y = (dustFrame / DUST_SHEET_COLS) * dustFrameH;
    return src;
}

static SDL_Rect getTouchFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, touchFrameW, touchFrameH};

    if(touchFrameCount <= 1 || touchFrameW <= 0 || touchFrameH <= 0) {
        return src;
    }

    if(touchFrame >= touchFrameCount) touchFrame = touchFrameCount - 1;

    src.x = (touchFrame % TOUCH_SHEET_COLS) * touchFrameW;
    src.y = (touchFrame / TOUCH_SHEET_COLS) * touchFrameH;
    return src;
}

static int isBurnAnimFinished(void)
{
    if(deathType != DEATH_FIRE) return 1;
    if(burnFrameCount <= 1) return 1;
    return burnFrame >= burnFrameCount - 1;
}

static int isDustAnimFinished(void)
{
    if(deathType != DEATH_FIRE) return 1;
    if(dustFrameCount <= 1) return 1;
    if(!dustVisible) return 0;
    return dustFrame >= dustFrameCount - 1;
}

static int isTouchAnimFinished(void)
{
    if(deathType != DEATH_TOUCH) return 1;
    if(touchFrameCount <= 1) return 1;
    return touchFrame >= touchFrameCount - 1;
}

static SDL_Rect getPlayerBodyRect(void)
{
    SDL_Rect body = {(int)player.x, (int)player.y, PLAYER_W, PLAYER_H};
    return body;
}

static void tickElectriseAnim(void)
{
    if(!player.dead || deathType != DEATH_LASER || electriseFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();
    if(now - electriseLastTick >= ELECTRISE_FRAME_DELAY &&
       electriseFrame < electriseFrameCount - 1) {
        electriseFrame++;
        electriseLastTick = now;
    }
}

static void updateLaserDeath(void)
{
    if(!player.dead || deathType != DEATH_LASER) return;

    tickElectriseAnim();

    if(laserDeathVy >= -1.0f) {
        if(player.x < laserDeathTargetX) {
            player.x += laserDeathVx;
            if(player.x > laserDeathTargetX) player.x = laserDeathTargetX;
        } else if(player.x > laserDeathTargetX) {
            player.x -= laserDeathVx;
            if(player.x < laserDeathTargetX) player.x = laserDeathTargetX;
        }
    }
    player.y += laserDeathVy;
    laserDeathVy += GRAVITY * 0.9f;
    if(laserDeathVy > MAX_FALL) laserDeathVy = MAX_FALL;
}

static void tickTouchAnim(void)
{
    int shouldAdvance = 1;

    if(!player.dead || deathType != DEATH_TOUCH || touchFrameCount <= 1) return;

    Uint32 now = SDL_GetTicks();
#if TOUCH_FRAME_DELAY > 0
    shouldAdvance = (now - touchLastTick) >= (Uint32)TOUCH_FRAME_DELAY;
#endif
    if(shouldAdvance) {
        touchFrame += TOUCH_FRAME_STEP;
        if(touchFrame >= touchFrameCount) touchFrame = touchFrameCount - 1;
        touchLastTick = now;
    }
}

static SDL_Rect getElectriseFrameSrcRect(void)
{
    SDL_Rect src = {0, 0, electriseFrameW, electriseFrameH};

    if(electriseFrameCount <= 1 || electriseFrameW <= 0 || electriseFrameH <= 0) {
        return src;
    }

    if(electriseFrame >= electriseFrameCount) electriseFrame = 0;

    src.x = (electriseFrame % ELECTRISE_SHEET_COLS) * electriseFrameW;
    src.y = (electriseFrame / ELECTRISE_SHEET_COLS) * electriseFrameH;
    return src;
}

/* ═══════════════════════════════════════════════════════
   UPDATE NIVEAU 1
═══════════════════════════════════════════════════════ */
static void updateLvl1(void)
{
    if (isPlatformRightPressed()) { player.vx=PLAYER_SPEED; player.facingRight=1; }
    else if (isPlatformLeftPressed()) { player.vx=-PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if (isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL1_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    if(player.x+PLAYER_W<0) player.x=(float)SCREEN_W;
    if(player.x>SCREEN_W)   player.x=-(float)PLAYER_W;

    updateLvl1LastFloorTrap();
    updateUtensils();

    player.onGround=0;
    if(player.vy>=0) {
        if(player.x + PLAYER_W > lvl1Ground.x &&
           player.x < lvl1Ground.x + lvl1Ground.w &&
           fb <= lvl1Ground.y && fa >= lvl1Ground.y) {
            player.y = lvl1Ground.y - PLAYER_H;
            player.vy = 0;
            player.onGround = 1;
        }

        for(int i=0;i<LVL1_COUNT;i++) {
            if(player.onGround) break;
            if(i==7 && lvl1[7].falling) continue;
            Platform *p=&lvl1[i];
            if(player.x+PLAYER_W<=p->x || player.x>=p->x+p->w) continue;
            if(fb<=p->y && fa>=p->y) {
                player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
                if(i==3 && !utensilTriggered) { utensilTriggered=1; utensilTimer=0; utensilNext=0; }
                if(i==4 && !spike1.triggered) { spike1.visible=1; spike1.triggered=1; }
                if(i==7 && !lvl1[7].triggered) {
                    lvl1[7].triggered = 1;
                    lvl1LastFloorDelayTimer = LVL1_LAST_FLOOR_DELAY;
                    playTrapSound();
                }
                break;
            }
        }
    }

    if(!player.dead && playerIsOnPlatformTop(&lvl1[LVL1_COUNT-1]) &&
       player.x + PLAYER_W >= SCREEN_W) {
        player.won = 1;
    }

    if(spike1.visible)
        if(player.x+PLAYER_W>spike1.x && player.x<spike1.x+SPIKE1_W &&
           player.y+PLAYER_H>spike1.y && player.y<spike1.y+TOOTH_H) killPlayerWithTouch();

    if(player.x + PLAYER_W > lvl1Ground.x &&
       player.x < lvl1Ground.x + lvl1Ground.w &&
       player.y + PLAYER_H > lvl1Ground.y - TOOTH_H &&
       player.y < lvl1Ground.y) {
        killPlayerWithTouch();
    }

    if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
    if(player.y>SCREEN_H+100) killPlayerWithFall();
}

/* ═══════════════════════════════════════════════════════
   UPDATE NIVEAU 2
═══════════════════════════════════════════════════════ */
static void updateLvl2(void)
{
    if(player.dead) {
        updateFallingFloor(&lvl2[2]);
        updateSlideSpike();
        if(lvl2WallCrushPending) {
            player.vx = 0.0f;
            player.vy = 0.0f;
            player.onGround = 1;
            player.x = lvl2CrushX;
            player.y = lvl2CrushY;
            if(slideSpike.landed) lvl2WallCrushPending = 0;
        }
        return;
    }

    if(isPlatformRightPressed()) { player.vx=PLAYER_SPEED; player.facingRight=1; }
    else if(isPlatformLeftPressed()) { player.vx=-PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if(isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL2_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    if(player.x<SOL_G_X)                         player.x=(float)SOL_G_X;
    if(player.x+PLAYER_W>LVL6_RIGHT_X+LVL6_RIGHT_W) player.x=(float)(LVL6_RIGHT_X+LVL6_RIGHT_W-PLAYER_W);

    updateFallingFloor(&lvl2[2]);
    updateSlideSpike();

    player.onGround=0;
    if(player.vy>=0) {
        for(int i=0;i<LVL2_COUNT;i++) {
            if(i==2 && lvl2[2].falling) continue;
            Platform *p=&lvl2[i];
            if(player.x+PLAYER_W<=p->x || player.x>=p->x+p->w) continue;
            if(fb<=p->y && fa>=p->y) {
                player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
                if(i==2 && !lvl2[2].triggered) {
                    lvl2[2].falling=1;
                    lvl2[2].triggered=1;
                    playTrapSound();
                }
                break;
            }
        }
        if(slideSpike.landed) {
            int cw=SLIDE_H, drawX=(int)(slideSpike.x)-cw;
            int dosY=(int)(SOL_Y-TOOTH_H-SLIDE_W);
            if(player.x+PLAYER_W>drawX && player.x<drawX+cw &&
               fb<=(float)dosY && fa>=(float)dosY) {
                player.y=(float)(dosY-PLAYER_H); player.vy=0; player.onGround=1;
            }
        }
    }
    if(!player.dead && slideSpike.landed && playerIsOnPlatformTop(&lvl2[5]) &&
       player.x + PLAYER_W >= SCREEN_W) {
        player.won = 1;
    }
    if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
    if(player.y>SCREEN_H+100) killPlayerWithFall();
}

/* ═══════════════════════════════════════════════════════
   UPDATE NIVEAU 3
   - ballon roule de droite vers gauche
   - s arrete au bord gauche
       - repart vers la droite quand le joueur s approche
       - casse le mur a droite a son retour
       - puis revient se poser a gauche
       - joueur doit se cacher dans le trou
═══════════════════════════════════════════════════════ */
static void updateLvl3(void)
{
    if(isPlatformRightPressed()) { player.vx=LVL3_PLAYER_SPEED; player.facingRight=1; }
    else if(isPlatformLeftPressed()) { player.vx=-LVL3_PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if(isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL3_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    /* limites horizontales */
    if(player.x<0)                   player.x=0;
    if(player.x+PLAYER_W>SCREEN_W)   player.x=(float)(SCREEN_W-PLAYER_W);

    /* plafond */
    if(player.y<(float)LVL3_CEIL_H) {
        player.y=(float)LVL3_CEIL_H;
        if(player.vy<0) player.vy=0;
    }

    /* ══════════════════════════════════════════════════
       BALLON :
       - part de la droite vers la gauche
       - attend au bord gauche
       - repart vers la droite quand le joueur s approche
       - s arrete au bord droit sans sortir de l ecran
    ══════════════════════════════════════════════════ */
    if(ball3Active) {
        if(ball3State == -1) {
            ball3X     -= BALL3_SPD;
            ball3Angle += BALL3_SPD / (float)BALL3_R;
            if(ball3X - BALL3_R < 0) {
                ball3X = (float)BALL3_R;
                ball3State = 0;
            }
        } else if(ball3State == 0) {
            if(!lvl3WallBroken && player.x <= BALL3_TRIGGER_X) {
                ball3State = 1;
            }
        } else if(ball3State == 1) {
            ball3X     += BALL3_SPD;
            ball3Angle -= BALL3_SPD / (float)BALL3_R;
            if(!lvl3WallBroken && ball3X + BALL3_R >= (float)LVL3_WALL_X) {
                ball3X = (float)(LVL3_WALL_X - BALL3_R);
                lvl3WallBroken = 1;
                if(sndBreak) {
                    Mix_HaltMusic();
                    Mix_PlayMusic(sndBreak, 0);
                }
                spawnLvl3WallDebris();
                ball3State = 2;
            } else if(ball3X + BALL3_R > SCREEN_W) {
                ball3X = (float)(SCREEN_W - BALL3_R);
                ball3State = 2;
            }
        } else if(ball3State == 2) {
            ball3X     -= BALL3_SPD;
            ball3Angle += BALL3_SPD / (float)BALL3_R;
            if(ball3X - BALL3_R < 0) {
                ball3X = (float)BALL3_R;
                ball3State = 3;
            }
        }
    }

    for(int i=0;i<LVL3_WALL_DEBRIS_COUNT;i++) {
        WallDebris *d=&lvl3WallDebris[i];
        if(!d->active) continue;
        d->x += d->vx;
        d->y += d->vy;
        d->vy += 0.45f;
        d->vx *= 0.98f;
        d->life--;
        if(d->life <= 0 || d->y > SCREEN_H + 40) d->active=0;
    }

    /* collision ballon (cercle) */
    if(ball3Active) {
        float pcx=player.x+PLAYER_W/2.0f;
        float pcy=player.y+PLAYER_H/2.0f;
        float dx=pcx-ball3X;
        float dy=pcy-(float)BALL3_CY;
        float rHit=(float)(BALL3_R-15);
        if(dx*dx+dy*dy<=rHit*rHit) {
            float hitX = player.x;
            float hitY = player.y;
            killPlayerWithTouchEffectAt(hitX, hitY);
        }
    }

    /* collision plateformes */
    player.onGround=0;
    if(player.vy>=0) {
        for(int i=0;i<LVL3_COUNT;i++) {
            Platform *p=&lvl3[i];
            if(player.x+PLAYER_W<=p->x || player.x>=p->x+p->w) continue;
            if(fb<=p->y && fa>=p->y) {
                player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
                break;
            }
        }
    }

    /* murs du trou : confine le joueur dans le gap */
    if(player.y+PLAYER_H > (float)LVL3_FLOOR_Y) {
        if(player.x < (float)LVL3_GAP_X)
            player.x=(float)LVL3_GAP_X;
        if(player.x+PLAYER_W > (float)(LVL3_GAP_X+LVL3_GAP_W))
            player.x=(float)(LVL3_GAP_X+LVL3_GAP_W-PLAYER_W);
    }

    /* mur de sortie a droite : le joueur ne passe pas tant que le ballon ne l a pas casse */
    if(!lvl3WallBroken) {
        float wallLeft=(float)LVL3_WALL_X;
        float wallTop=(float)LVL3_WALL_Y;
        float wallBottom=(float)(LVL3_WALL_Y + LVL3_WALL_H);
        if(player.x + PLAYER_W > wallLeft &&
           player.x < (float)SCREEN_W &&
           player.y + PLAYER_H > wallTop &&
           player.y < wallBottom) {
            player.x = wallLeft - PLAYER_W;
            if(player.vx > 0) player.vx = 0;
        }
    }

    /* victoire : atteindre l extremite droite */
    if(!player.dead && playerIsOnPlatformTop(&(Platform){(float)(LVL3_GAP_X+LVL3_GAP_W),(float)LVL3_FLOOR_Y,(SCREEN_W-(LVL3_GAP_X+LVL3_GAP_W)),LVL3_FLOOR_H,0,0,0}) &&
       player.x+PLAYER_W >= SCREEN_W)
        player.won=1;

    updateBall3Sound();

    if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
    if(player.y>SCREEN_H+100) killPlayerWithFall();
}

/* ═══════════════════════════════════════════════════════
   UPDATE NIVEAU 4
═══════════════════════════════════════════════════════ */
static void updateLvl4(void)
{
    updateFireAnim();

    if(isPlatformRightPressed()) { player.vx=PLAYER_SPEED; player.facingRight=1; }
    else if(isPlatformLeftPressed()) { player.vx=-PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if(isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL4_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    if(player.x<SOL_G_X)                           player.x=(float)SOL_G_X;
    if(player.x+PLAYER_W>LVL6_RIGHT_X+LVL6_RIGHT_W) player.x=(float)(LVL6_RIGHT_X+LVL6_RIGHT_W-PLAYER_W);

    updateFallingFloor(&lvl4[3]);

    for(int i=0;i<LVL4_BALL_COUNT;i++) {
        if(!lvl4BallActive[i]) continue;

        if(!lvl4BallFalling[i]) {
            lvl4BallX[i] -= LVL4_BALL_SPD;
            lvl4BallAngle[i] += LVL4_BALL_SPD / (float)LVL4_BALL_R;
            if(lvl4BallX[i] - LVL4_BALL_R <= (float)SOL_D_X) {
                lvl4BallFalling[i] = 1;
                lvl4BallVy[i] = 2.0f;
            }
        } else {
            lvl4BallX[i] -= LVL4_BALL_SPD * 0.55f;
            lvl4BallY[i] += lvl4BallVy[i];
            lvl4BallVy[i] += GRAVITY * 1.15f;
            lvl4BallAngle[i] += LVL4_BALL_SPD / (float)LVL4_BALL_R;
            if(lvl4BallY[i] - LVL4_BALL_R > SCREEN_H + 80) {
                lvl4BallActive[i] = 0;
            }
        }
    }

    player.onGround=0;
    if(player.vy>=0) {
        for(int i=0;i<LVL4_COUNT;i++) {
            if(i==3 && lvl4[3].falling) continue;
            Platform *p=&lvl4[i];
            if(player.x+PLAYER_W<=p->x || player.x>=p->x+p->w) continue;
            if(fb<=p->y && fa>=p->y) {
                player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
                if(i==1 && !lvl4LaserVisible) {
                    lvl4LaserVisible=1;
                    if(sndLaserFx) {
                        Mix_PlayChannel(-1, sndLaserFx, 0);
                    } else if(sndLaser) {
                        Mix_HaltMusic();
                        Mix_PlayMusic(sndLaser, 0);
                    }
                }
                if(i==3 && !lvl4[3].triggered) {
                    lvl4[3].falling=1;
                    lvl4[3].triggered=1;
                    playTrapSound();
                }
                if(i==5 && !lvl4FireVisible) {
                    lvl4FireVisible = 1;
                    fireFrame = 0;
                    fireLastTick = SDL_GetTicks();
                    if(sndFireFx) {
                        sndFireChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_FIRE, sndFireFx, 0);
                    }
                }
                if(i==LVL4_COUNT-1 && !lvl4BallTriggered) {
                    lvl4BallTriggered = 1;
                    for(int j=0;j<LVL4_BALL_COUNT;j++) {
                        lvl4BallActive[j] = 1;
                        lvl4BallFalling[j] = 0;
                        lvl4BallX[j] = (float)(SCREEN_W + LVL4_BALL_R + j * 92);
                        lvl4BallY[j] = (float)lvl4[6].y - LVL4_BALL_R;
                        lvl4BallVy[j] = 0.0f;
                        lvl4BallAngle[j] = 0.0f;
                    }
                }
                break;
            }
        }
    }

    updateLvl4LaserAnim();

    if(lvl4LaserVisible) {
        Platform *p=&lvl4[1];
        float laserX=p->x + p->w/2.0f - LVL4_LASER_W/2.0f;
        float laserY=(float)LVL4_LASER_TOP;
        float laserH=p->y - laserY;
        if(player.x+PLAYER_W>laserX && player.x<laserX+LVL4_LASER_W &&
           player.y+PLAYER_H>laserY && player.y<laserY+laserH) {
            killPlayerWithLaser();
        }
    }

    if(lvl4FireVisible) {
        Platform *p=&lvl4[5];
        float fireX=p->x + FIRE_PAD_X;
        float fireY=p->y - FIRE_RENDER_H;
        float fireW=(float)(p->w - 2 * FIRE_PAD_X);
        float fireH=FIRE_RENDER_H;
        if(player.x+PLAYER_W>fireX && player.x<fireX+fireW &&
           player.y+PLAYER_H>fireY && player.y<fireY+fireH) {
            killPlayerWithFire();
        }
    }

    for(int i=0;i<LVL4_BALL_COUNT;i++) {
        if(!lvl4BallActive[i]) continue;
        float pcx=player.x + PLAYER_W/2.0f;
        float pcy=player.y + PLAYER_H/2.0f;
        float dx=pcx - lvl4BallX[i];
        float dy=pcy - lvl4BallY[i];
        float rHit=(float)(LVL4_BALL_R - 4);
        if(dx*dx + dy*dy <= rHit*rHit) {
            float effectX = lvl4BallX[i] - PLAYER_W * 0.5f;
            float effectY = lvl4BallY[i] - PLAYER_H * 0.5f;
            killPlayerWithTouchEffectAt(effectX, effectY);
        }
    }

    if(!player.dead && playerIsOnPlatformTop(&lvl4[LVL4_COUNT-1]) &&
       lvl4BallTriggered && !lvl4BallActive[0] && !lvl4BallActive[1] &&
       player.x + PLAYER_W >= SCREEN_W) {
        player.won = 1;
    }

    if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
    if(player.y>SCREEN_H+100) killPlayerWithFall();
}

static void updateLvl5(void)
{
    if(isPlatformRightPressed()) { player.vx=PLAYER_SPEED; player.facingRight=1; }
    else if(isPlatformLeftPressed()) { player.vx=-PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if(isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL5_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    if(player.x<0) player.x=0;
    if(player.x+PLAYER_W>SCREEN_W) player.x=(float)(SCREEN_W-PLAYER_W);

    if(!spike5.triggered && player.x >= 40.0f + LVL5_SPIKE_TRIGGER_DIST) {
        spike5.visible = 1;
        spike5.triggered = 1;
    }

    if(!lvl5RainTriggered) {
        float rainStart = spike5.x + 3.0f * LVL5_STEP_W;
        if(player.x + PLAYER_W/2.0f >= rainStart) {
            lvl5RainTriggered = 1;
            lvl5RainTimer = 0;
            lvl5RainNext = 0;
        }
    }

    updateLvl5Rain();

    {
        SDL_Rect playerRect = getPlayerRect();
        /* The trap can only trigger once. */
        if(!lvl5WallTrap.triggered &&
           SDL_HasIntersection(&playerRect, &lvl5WallTrap.triggerZone)) {
            lvl5WallTrap.triggered = 1;
            lvl5WallTrap.triggerTick = SDL_GetTicks();
        }
    }

    updateFallingWallTrap(&lvl5WallTrap);

    player.onGround=0;
    if(player.vy>=0) {
        Platform *p=&lvl5[0];
        int overHole = 0;
        float px1 = player.x;
        float px2 = player.x + PLAYER_W;
        if(px2 > lvl5HoleX && px1 < lvl5HoleX + lvl5HoleW) {
            overHole = 1;
            if(!lvl5HoleVisible &&
               fb <= p->y && fa >= p->y) {
                lvl5HoleVisible = 1;
            }
        }
        if(player.x+PLAYER_W>p->x && player.x<p->x+p->w &&
           fb<=p->y && fa>=p->y && !overHole) {
            player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
        }
    }

    if(lvl5WallTrap.landed && player.vy >= 0.0f) {
        SDL_Rect wallRect = getFallingWallRect(&lvl5WallTrap);
        float wallTop = (float)wallRect.y;
        float wallLeft = (float)wallRect.x;
        float wallRight = (float)(wallRect.x + wallRect.w);
        if(player.x + PLAYER_W > wallLeft && player.x < wallRight &&
           fb <= wallTop && fa >= wallTop) {
            player.y = wallTop - PLAYER_H;
            player.vy = 0.0f;
            player.onGround = 1;
        }
    }

    if(lvl5HoleVisible && !lvl5HoleSpike.triggered) {
        float px1 = player.x;
        float px2 = player.x + PLAYER_W;
        if(player.vy > 0.0f &&
           player.y + PLAYER_H > SOL_Y + 10 &&
           px2 > lvl5HoleX && px1 < lvl5HoleX + lvl5HoleW) {
            lvl5HoleSpike.visible = 1;
            lvl5HoleSpike.triggered = 1;
        }
    }

    if(spike5.visible)
        if(player.x+PLAYER_W >= spike5.x && player.x <= spike5.x+SPIKE1_W &&
           player.y+PLAYER_H >= spike5.y-2 && player.y <= spike5.y+TOOTH_H) {
            float hitX = spike5.x + SPIKE1_W * 0.5f - PLAYER_W * 0.5f;
            float hitY = spike5.y - PLAYER_H;
            killPlayerWithTouchAt(hitX, hitY);
            return;
        }

    if(lvl5HoleSpike.visible)
        if(player.x+PLAYER_W >= lvl5HoleSpike.x && player.x <= lvl5HoleSpike.x+SPIKE1_W &&
           player.y+PLAYER_H >= lvl5HoleSpike.y-2 && player.y <= lvl5HoleSpike.y+TOOTH_H) {
            float hitX = lvl5HoleSpike.x + SPIKE1_W * 0.5f - PLAYER_W * 0.5f;
            float hitY = lvl5HoleSpike.y - PLAYER_H;
            killPlayerWithTouchAt(hitX, hitY);
            return;
        }

    if(lvl5HoleVisible || (player.x + PLAYER_W > lvl5HoleX && player.x < lvl5HoleX + lvl5HoleW)) {
        float holeBottom = (float)(SOL_Y + LVL5_HOLE_H);
        float px1 = player.x;
        float px2 = player.x + PLAYER_W;
        if(px2 > lvl5HoleX && px1 < lvl5HoleX + lvl5HoleW) {
            if(!lvl5HoleVisible && player.y + PLAYER_H >= SOL_Y) lvl5HoleVisible = 1;
            if(player.y + PLAYER_H >= SOL_Y) lvl5InHole = 1;
        }
        if(lvl5InHole && !godMode) {
            if(player.x < lvl5HoleX) player.x = lvl5HoleX;
            if(player.x + PLAYER_W > lvl5HoleX + lvl5HoleW)
                player.x = lvl5HoleX + lvl5HoleW - PLAYER_W;
        }
        if((lvl5InHole || (px2 > lvl5HoleX && px1 < lvl5HoleX + lvl5HoleW)) &&
           player.y < holeBottom && fa >= holeBottom) {
            player.y = holeBottom - PLAYER_H;
            player.vy = 0.0f;
            player.onGround = 1;
        }
        if(lvl5InHole &&
           player.y > holeBottom - PLAYER_H) {
            player.y = holeBottom - PLAYER_H;
            player.vy = 0.0f;
            player.onGround = 1;
        }
        if(godMode && lvl5InHole && player.y + PLAYER_H <= SOL_Y) {
            lvl5InHole = 0;
        }
    }

    {
        SDL_Rect playerRect = getPlayerRect();
        SDL_Rect wallRect = getFallingWallRect(&lvl5WallTrap);
        if(lvl5WallTrap.landed &&
           player.y + PLAYER_H == wallRect.y &&
           player.x + PLAYER_W > wallRect.x &&
           player.x < wallRect.x + wallRect.w) {
            goto lvl5_wall_collision_done;
        }
        if(SDL_HasIntersection(&playerRect, &wallRect)) {
            if(lvl5WallTrap.falling) {
                if(!godMode && !lvl5WallCrushPending) {
                    if(!lvl5WallTouchSoundPlayed) {
                        playTouchSound();
                        lvl5WallTouchSoundPlayed = 1;
                    }
                    lvl5WallCrushPending = 1;
                    lvl5CrushX = player.x;
                    lvl5CrushY = player.y;
                }
            } else {
                float prevX = player.x - player.vx;
                float prevY = player.y - player.vy;
                float prevLeft = prevX;
                float prevRight = prevX + PLAYER_W;
                float prevTop = prevY;
                float prevBottom = prevY + PLAYER_H;
                float wallLeft = (float)wallRect.x;
                float wallRight = (float)(wallRect.x + wallRect.w);
                float wallTop = (float)wallRect.y;
                float wallBottom = (float)(wallRect.y + wallRect.h);

                if(player.vy >= 0.0f && prevBottom <= wallTop) {
                    player.y = wallTop - PLAYER_H;
                    player.vy = 0.0f;
                    player.onGround = 1;
                } else if(player.vy < 0.0f && prevTop >= wallBottom) {
                    player.y = wallBottom;
                    player.vy = 0.0f;
                } else if(player.vx > 0.0f && prevRight <= wallLeft) {
                    player.x = wallLeft - PLAYER_W;
                    player.vx = 0.0f;
                } else if(player.vx < 0.0f && prevLeft >= wallRight) {
                    player.x = wallRight;
                    player.vx = 0.0f;
                } else {
                    float overlapLeft = player.x + PLAYER_W - wallLeft;
                    float overlapRight = wallRight - player.x;
                    float overlapTop = player.y + PLAYER_H - wallTop;
                    float overlapBottom = wallBottom - player.y;
                    float minOverlap = overlapLeft;

                    if(overlapRight < minOverlap) minOverlap = overlapRight;
                    if(overlapTop < minOverlap) minOverlap = overlapTop;
                    if(overlapBottom < minOverlap) minOverlap = overlapBottom;

                    if(minOverlap == overlapTop) {
                        player.y = wallTop - PLAYER_H;
                        player.vy = 0.0f;
                        player.onGround = 1;
                    } else if(minOverlap == overlapBottom) {
                        player.y = wallBottom;
                        if(player.vy < 0.0f) player.vy = 0.0f;
                    } else if(minOverlap == overlapLeft) {
                        player.x = wallLeft - PLAYER_W;
                        if(player.vx > 0.0f) player.vx = 0.0f;
                    } else {
                        player.x = wallRight;
                        if(player.vx < 0.0f) player.vx = 0.0f;
                    }
                }
            }
        }
lvl5_wall_collision_done:
        ;
    }

    if(lvl5WallCrushPending) {
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.onGround = 1;
        player.x = lvl5CrushX;
        player.y = lvl5CrushY;
        if(lvl5WallTrap.landed) {
            if(!lvl5WallTouchSoundPlayed) {
                playTouchSound();
                lvl5WallTouchSoundPlayed = 1;
            }
            crushPlayer();
            lvl5WallCrushPending = 0;
        }
    }

    if(!player.dead && playerIsOnPlatformTop(&lvl5[0]) &&
       player.x+PLAYER_W >= SCREEN_W) player.won=1;

    if(player.y>getFallDeathY()) {
        if(shouldTriggerFallSound()) triggerFallSound();
        if(lvl5InHole || (player.x + PLAYER_W > lvl5HoleX && player.x < lvl5HoleX + lvl5HoleW)) {
            player.y = (float)(SOL_Y + LVL5_HOLE_H - PLAYER_H);
            player.vy = 0.0f;
            player.onGround = 1;
            lvl5InHole = 1;
        } else {
            if(player.y > SCREEN_H + 100) killPlayerWithFall();
        }
    }
}

static void updateLvl6(void)
{
    int allChunksFalling = 1;

    if(lvl6FreezeState != 0 && !iceAnimFinished) updateIceAnim();

    lvl6CollapseTimer++;
    for(int i=0;i<LVL6_CHUNK_COUNT;i++) {
        Platform *c=&lvl6Chunks[i];
        if(!c->falling && lvl6CollapseTimer >= i * LVL6_COLLAPSE_DELAY) {
            c->falling = 1;
        }
        if(!c->falling) allChunksFalling = 0;
        if(c->falling) {
            c->fallVy += GRAVITY * 1.15f;
            if(c->fallVy > MAX_FALL + 4.0f) c->fallVy = MAX_FALL + 4.0f;
            c->y += c->fallVy;

            float pcx = player.x + PLAYER_W/2.0f;
            if(!player.dead &&
               pcx >= c->x && pcx <= c->x + c->w &&
               player.y + PLAYER_H <= c->y + c->fallVy + 6.0f &&
               player.y + PLAYER_H >= c->y - c->fallVy - 10.0f) {
                player.noJump = 1;
                player.onGround = 0;
                player.vy = c->fallVy;
                player.y += c->fallVy;
            }
        }
    }

    if(allChunksFalling && sndEarthChannel != -1) {
        Mix_HaltChannel(sndEarthChannel);
        sndEarthChannel = -1;
    }

    if(lvl6FreezeState != 0) {
        Platform *freezePlat = &lvl6[2];
        Platform *nextPlat = &lvl6[3];
        float gapCenter = (freezePlat->x + freezePlat->w + nextPlat->x) * 0.5f;

        player.noJump = 1;
        player.facingRight = 1;

        if(lvl6FreezeState == 1) {
            player.vx = 0.0f;
            player.vy = 0.0f;
            player.onGround = 1;
            player.y = freezePlat->y - PLAYER_H;
            if(iceAnimFinished) lvl6FreezeState = 2;
        } else if(lvl6FreezeState == 2) {
            player.vx = LVL6_FREEZE_SLIDE_SPEED;
            player.vy = 0.0f;
            player.onGround = 1;
            player.y = freezePlat->y - PLAYER_H;
            player.x += player.vx;
            if(player.x + PLAYER_W * 0.5f >= gapCenter) {
                player.x = gapCenter - PLAYER_W * 0.5f;
                lvl6FreezeState = 3;
                player.vx = LVL6_FREEZE_SLIDE_SPEED * 0.65f;
                player.vy = 1.0f;
                player.onGround = 0;
            }
        } else {
            player.x += player.vx;
            player.vy += GRAVITY;
            if(player.vy > MAX_FALL) player.vy = MAX_FALL;
            player.y += player.vy;
            player.onGround = 0;
        }

        if(player.x<SOL_G_X)                  player.x=(float)SOL_G_X;
        if(player.x+PLAYER_W>SOL_D_X+SOL_D_W) player.x=(float)(SOL_D_X+SOL_D_W-PLAYER_W);
        if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
        if(player.y>SCREEN_H+100) killPlayerWithFall();
        return;
    }

    if(isPlatformRightPressed()) { player.vx=PLAYER_SPEED; player.facingRight=1; }
    else if(isPlatformLeftPressed()) { player.vx=-PLAYER_SPEED; player.facingRight=0; }
    else { player.vx*=0.75f; if(fabsf(player.vx)<0.5f) player.vx=0; }

    if(isPlatformJumpPressed() && player.onGround && !player.noJump) {
        player.vy=LVL4_JUMP_VY; player.onGround=0; playJumpSound();
    }
    player.vy+=GRAVITY; if(player.vy>MAX_FALL) player.vy=MAX_FALL;

    float fb=player.y+PLAYER_H;
    player.x+=player.vx; player.y+=player.vy;
    float fa=player.y+PLAYER_H;

    if(player.x<SOL_G_X)                  player.x=(float)SOL_G_X;
    if(player.x+PLAYER_W>SOL_D_X+SOL_D_W) player.x=(float)(SOL_D_X+SOL_D_W-PLAYER_W);

    player.onGround=0;
    player.noJump=0;
    if(player.vy>=0) {
        for(int i=0;i<LVL6_COUNT;i++) {
            Platform *p=&lvl6[i];
            if(i==0) {
                for(int j=0;j<LVL6_CHUNK_COUNT;j++) {
                    Platform *c=&lvl6Chunks[j];
                    if(c->falling) continue;
                    if(player.x+PLAYER_W<=c->x || player.x>=c->x+c->w) continue;
                    if(fb<=c->y && fa>=c->y) {
                        player.y=c->y-PLAYER_H; player.vy=0; player.onGround=1;
                        break;
                    }
                }
                if(player.onGround) break;
                continue;
            }
            if(player.x+PLAYER_W<=p->x || player.x>=p->x+p->w) continue;
            if(fb<=p->y && fa>=p->y) {
                player.y=p->y-PLAYER_H; player.vy=0; player.onGround=1;
                if(i==2 && lvl6FreezeState == 0) {
                    lvl6FreezeState=1;
                    player.vx=0.0f;
                    player.vy=0.0f;
                    player.noJump=1;
                    iceFrame=0;
                    iceLastTick=SDL_GetTicks();
                    iceAnimFinished=0;
                    if(!lvl6IceSoundPlayed && sndIceFx) {
                        sndIceChannel = Mix_PlayChannel(GAMEPLAY_CHANNEL_ICE, sndIceFx, 0);
                    }
                    lvl6IceSoundPlayed = 1;
                }
                break;
            }
        }
    }

    if(player.y>getFallDeathY() && shouldTriggerFallSound()) triggerFallSound();
    if(player.y>SCREEN_H+100) killPlayerWithFall();
}

/* ═══════════════════════════════════════════════════════
   RENDER BRIQUE
═══════════════════════════════════════════════════════ */
static void renderBrick(int x,int y,int w,int h,int orange)
{
    if(y>SCREEN_H+h) return;
    if(orange) SDL_SetRenderDrawColor(ren,200,60,20,255);
    else       SDL_SetRenderDrawColor(ren,120,125,130,255);
    SDL_Rect body={x,y,w,h}; SDL_RenderFillRect(ren,&body);
    if(orange) SDL_SetRenderDrawColor(ren,230,100,50,255);
    else       SDL_SetRenderDrawColor(ren,180,185,190,255);
    SDL_Rect top={x,y,w,3}; SDL_RenderFillRect(ren,&top);
    if(orange) SDL_SetRenderDrawColor(ren,140,40,10,255);
    else       SDL_SetRenderDrawColor(ren,70,75,80,255);
    SDL_Rect bot={x,y+h-3,w,3}; SDL_RenderFillRect(ren,&bot);
    if(orange) SDL_SetRenderDrawColor(ren,160,50,15,255);
    else       SDL_SetRenderDrawColor(ren,90,95,100,255);
    for(int bx=x+28;bx<x+w;bx+=28)
        SDL_RenderDrawLine(ren,bx,y+3,bx,y+h-3);
    for(int by2=y+18;by2<y+h;by2+=18)
        SDL_RenderDrawLine(ren,x,by2,x+w,by2);
}

static void renderLvl2FloorPlain(int x,int y,int w,int h)
{
    if(y>SCREEN_H+h) return;
    SDL_SetRenderDrawColor(ren,55,55,55,255);
    SDL_Rect body={x,y,w,h};
    SDL_RenderFillRect(ren,&body);
}

static void renderLvl4BreakingFloor(const Platform *p)
{
    int y=(int)p->y;
    int h=p->h;
    if(y > SCREEN_H + h) return;
    int crackW=(int)fminf(18.0f, 8.0f + p->fallVy * 1.6f);
    int drift=(int)fminf(12.0f, 3.0f + p->fallVy * 1.2f);
    int leftW=(p->w - crackW) / 2;
    int rightW=p->w - crackW - leftW;
    int leftX=(int)p->x - drift;
    int crackX=leftX + leftW;
    int rightX=crackX + crackW + drift * 2;

    renderLvl2FloorPlain(leftX,y,leftW,h);
    renderLvl2FloorPlain(rightX,y,rightW,h);

    SDL_SetRenderDrawColor(ren,15,15,15,255);
    SDL_Rect hole={crackX,y+1,crackW,h-2}; SDL_RenderFillRect(ren,&hole);

    SDL_SetRenderDrawColor(ren,55,55,55,255);
    SDL_RenderDrawLine(ren,crackX,y+2,crackX+3,y+h-4);
    SDL_RenderDrawLine(ren,crackX+crackW-1,y+2,crackX+crackW-4,y+h-4);

    SDL_SetRenderDrawColor(ren,180,185,190,255);
    SDL_Rect leftEdge={leftX+leftW-2,y+3,2,h-6};
    SDL_Rect rightEdge={rightX,y+3,2,h-6};
    SDL_RenderFillRect(ren,&leftEdge);
    SDL_RenderFillRect(ren,&rightEdge);

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,90,90,90,120);
    SDL_Rect shadow={crackX-3,y+h-1,crackW+6,4}; SDL_RenderFillRect(ren,&shadow);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

static void renderLvl6Chunk(const Platform *p, int vibY)
{
    int x=(int)p->x;
    int y=(int)p->y + vibY;

    renderLvl2FloorPlain(x,y,p->w,p->h);

    if(p->falling) {
        int crackSeed = ((int)(p->fallVy * 3.0f)) % 5;
        SDL_SetRenderDrawColor(ren,20,20,20,255);
        for(int i=0;i<4;i++) {
            int sy = y + 5 + i * 9;
            int sx = x + 3 + ((i + crackSeed) % 3);
            int ex = x + p->w - 4 - ((i + crackSeed) % 4);
            SDL_RenderDrawLine(ren,sx,sy,ex,sy + 3 + (i % 2));
        }

        SDL_SetRenderDrawColor(ren,180,185,190,255);
        for(int i=0;i<3;i++) {
            int fx = x + 5 + i * (p->w / 3);
            SDL_RenderDrawLine(ren,fx,y+3,fx+4,y+p->h-7);
        }

        SDL_SetRenderDrawColor(ren,80,80,85,255);
        for(int i=0;i<6;i++) {
            int dx = x + 4 + (i * 13) % (p->w - 8);
            int dy = y + p->h + ((int)SDL_GetTicks()/20 + i * 7) % 42;
            SDL_Rect debris={dx,dy,4 + (i%3),3 + (i%2)};
            SDL_RenderFillRect(ren,&debris);
        }
    }
}

/* ═══════════════════════════════════════════════════════
   RENDER PIEGE COULISSANT
═══════════════════════════════════════════════════════ */
static void renderSlideSpike(void)
{
    if(!slideSpike.triggered) return;
    float a=slideSpike.angle;
    int by=(int)slideSpike.y;
    if(a<=0.0f) {
        int bx=(int)slideSpike.x;
        SDL_SetRenderDrawColor(ren,80,80,80,255);
        SDL_Rect body={bx,by,SLIDE_W,SLIDE_H}; SDL_RenderFillRect(ren,&body);
        SDL_SetRenderDrawColor(ren,120,120,120,255);
        SDL_Rect shine={bx,by,3,SLIDE_H}; SDL_RenderFillRect(ren,&shine);
        int spacing=SLIDE_H/SLIDE_TEETH,th=spacing-6;
        for(int i=0;i<SLIDE_TEETH;i++) {
            int ty=by+i*spacing+spacing/2-th/2;
            drawToothLeft(bx,ty,TOOTH_H,th);
        }
    } else if(a<90.0f) {
        float pivotX = slideSpike.x;
        float pivotY = slideSpike.y + SLIDE_H;
        float rad = a * (float)M_PI / 180.0f;
        float c = cosf(rad);
        float s = sinf(rad);
        float edgeTx = -s;
        float edgeTy = -c;
        float normalX = -c;
        float normalY = s;

        for(int i=0;i<SLIDE_W;i++) {
            float lx = (float)i;
            float byX = pivotX + lx * c;
            float byY = pivotY - lx * s;
            float tyX = pivotX + lx * c - (float)SLIDE_H * s;
            float tyY = pivotY - lx * s - (float)SLIDE_H * c;

            SDL_SetRenderDrawColor(ren,80,80,80,255);
            SDL_RenderDrawLine(ren,(int)byX,(int)byY,(int)tyX,(int)tyY);
        }

        SDL_SetRenderDrawColor(ren,150,150,150,255);
        for(int i=0;i<2;i++) {
            float lx = 4.0f + (float)i * ((float)SLIDE_W / 2.0f);
            float byX = pivotX + lx * c;
            float byY = pivotY - lx * s;
            float tyX = pivotX + lx * c - (float)SLIDE_H * s;
            float tyY = pivotY - lx * s - (float)SLIDE_H * c;
            SDL_RenderDrawLine(ren,(int)byX,(int)byY,(int)tyX,(int)tyY);
        }

        int spacing = SLIDE_H / SLIDE_TEETH;
        int toothSpan = (spacing - 6) / 2;
        if(toothSpan < 1) toothSpan = 1;
        for(int i=0;i<SLIDE_TEETH;i++) {
            float edgeOffset = (float)(i * spacing + spacing / 2);
            float ax = pivotX + edgeTx * edgeOffset;
            float ay = pivotY + edgeTy * edgeOffset;
            drawRotatedTooth(ax, ay, normalX, normalY, edgeTx, edgeTy, TOOTH_H, toothSpan);
        }
    } else {
        int cw=SLIDE_H,drawX=(int)(slideSpike.x)-cw;
        int dosY=(int)(SOL_Y-TOOTH_H-SLIDE_W),dentY=(int)(SOL_Y-TOOTH_H);
        SDL_SetRenderDrawColor(ren,80,80,80,255);
        SDL_Rect corps={drawX,dosY,cw,SLIDE_W}; SDL_RenderFillRect(ren,&corps);
        SDL_SetRenderDrawColor(ren,160,160,160,255);
        SDL_Rect dos={drawX,dosY,cw,4}; SDL_RenderFillRect(ren,&dos);
        SDL_SetRenderDrawColor(ren,120,120,120,255);
        SDL_Rect shine={drawX,dosY,3,SLIDE_W}; SDL_RenderFillRect(ren,&shine);
        SDL_SetRenderDrawColor(ren,60,60,60,255);
        SDL_Rect base={drawX,dosY+SLIDE_W,cw,4}; SDL_RenderFillRect(ren,&base);
        int spacing=cw/SLIDE_TEETH,tw=spacing-6;
        for(int i=0;i<SLIDE_TEETH;i++) {
            int tx=drawX+i*spacing+spacing/2-tw/2;
            drawToothDown(tx,dentY,tw,TOOTH_H);
        }
    }
}

// SAW RENDER
static __attribute__((unused)) void renderSawBlade(const SawBlade *saw)
{
    if(!saw->active) return;

    SDL_Rect dst={(int)saw->x,(int)saw->y,saw->w,saw->h};
    if(texSaw) {
        SDL_Rect src={saw->frame * SAW_FRAME_SIZE,0,SAW_FRAME_SIZE,SAW_FRAME_SIZE};
        SDL_RenderCopy(ren,texSaw,&src,&dst);
    } else {
        SDL_SetRenderDrawColor(ren,110,110,115,255);
        SDL_RenderFillRect(ren,&dst);
    }
}

static void renderFallingWallTrap(const FallingWallTrap *wall)
{
    float pivotX = wall->x;
    float pivotY = wall->y + wall->h;
    float a = wall->angle * (float)M_PI / 180.0f;
    float c = cosf(a);
    float s = sinf(a);

    for(int i=0;i<wall->w;i++) {
        float lx = (float)i;
        float byX = pivotX + lx * c;
        float byY = pivotY - lx * s;
        float tyX = pivotX + lx * c - (float)wall->h * s;
        float tyY = pivotY - lx * s - (float)wall->h * c;

        SDL_SetRenderDrawColor(ren,120,125,130,255);
        SDL_RenderDrawLine(ren,(int)byX,(int)byY,(int)tyX,(int)tyY);
    }

    SDL_SetRenderDrawColor(ren,180,185,190,255);
    for(int i=0;i<3;i++) {
        float lx = 4.0f + (float)i * ((float)wall->w / 3.0f);
        float byX = pivotX + lx * c;
        float byY = pivotY - lx * s;
        float tyX = pivotX + lx * c - (float)wall->h * s;
        float tyY = pivotY - lx * s - (float)wall->h * c;
        SDL_RenderDrawLine(ren,(int)byX,(int)byY,(int)tyX,(int)tyY);
    }

    if(wall->dustTimer > 0) {
        int spread = 18 + (18 - wall->dustTimer) * 5;
        int alpha = 28 + wall->dustTimer * 7;
        int baseX = (int)pivotX - wall->h;
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,165,155,145,alpha);
        for(int i=0;i<10;i++) {
            int puffW = spread + i * 9;
            int puffH = 10 + i * 3;
            int offset = (wall->h * i) / 9;
            SDL_Rect puff={baseX + offset - puffW/3,(int)pivotY - puffH/2,puffW,puffH};
            SDL_RenderFillRect(ren,&puff);
        }
        SDL_SetRenderDrawColor(ren,145,135,125,alpha > 40 ? alpha - 20 : alpha);
        for(int i=0;i<12;i++) {
            int dustW = spread/2 + i * 6;
            int dustH = 6 + i * 2;
            int offset = (wall->h * i) / 11;
            SDL_Rect dust={baseX + offset - dustW/4,(int)pivotY - 5 - (i%3),dustW,dustH};
            SDL_RenderFillRect(ren,&dust);
        }
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    }
}

/* ═══════════════════════════════════════════════════════
   DESSIN CASSEROLE
═══════════════════════════════════════════════════════ */
static void drawPot(int x,int y,int smash)
{
    float t=(smash>0)?1.0f-(float)smash/(float)SMASH_FRAMES:0.0f;
    int bodyW=26;
    int bodyH=(smash>0)?(int)(22*(1.0f-t*0.80f))+2:22;
    int debrisR=(int)(t*22);
    SDL_SetRenderDrawColor(ren,185,50,50,255);
    SDL_Rect body={x-bodyW/2,y-bodyH,bodyW,bodyH}; SDL_RenderFillRect(ren,&body);
    SDL_SetRenderDrawColor(ren,220,90,90,255);
    SDL_Rect shine={x-bodyW/2,y-bodyH,3,bodyH}; SDL_RenderFillRect(ren,&shine);
    SDL_SetRenderDrawColor(ren,150,150,155,255);
    SDL_Rect rim={x-bodyW/2-2,y-bodyH-3,bodyW+4,4}; SDL_RenderFillRect(ren,&rim);
    int lidH=(smash>0)?(int)(6*(1.0f-t*0.7f))+1:6;
    SDL_SetRenderDrawColor(ren,170,170,175,255);
    SDL_Rect lid={x-bodyW/2-2,y-bodyH-3-lidH,bodyW+4,lidH}; SDL_RenderFillRect(ren,&lid);
    SDL_SetRenderDrawColor(ren,100,100,105,255);
    SDL_Rect handle={x+bodyW/2+1,y-bodyH+bodyH/2-2,8,4}; SDL_RenderFillRect(ren,&handle);
    if(smash>0&&debrisR>2) {
        SDL_SetRenderDrawColor(ren,185,50,50,200);
        SDL_Rect dl={x-bodyW/2-debrisR,y-5,6,5};
        SDL_Rect dr={x+bodyW/2+debrisR-6,y-5,6,5};
        SDL_Rect du={x-3,y-bodyH-debrisR/2,6,5};
        SDL_RenderFillRect(ren,&dl); SDL_RenderFillRect(ren,&dr); SDL_RenderFillRect(ren,&du);
        SDL_SetRenderDrawColor(ren,190,190,195,200);
        SDL_Rect ml={x-bodyW/2-debrisR+3,y-10,4,4};
        SDL_Rect mr={x+bodyW/2+debrisR-7,y-10,4,4};
        SDL_RenderFillRect(ren,&ml); SDL_RenderFillRect(ren,&mr);
    }
}

/* ═══════════════════════════════════════════════════════
   DESSIN FOURCHETTE
═══════════════════════════════════════════════════════ */
static __attribute__((unused)) void drawFork(int x,int y,int smash)
{
    float t=(smash>0)?1.0f-(float)smash/(float)SMASH_FRAMES:0.0f;
    int shaftH=(smash>0)?(int)(34*(1.0f-t*0.85f))+2:34;
    int tineH=(smash>0)?(int)(12*(1.0f-t*0.90f))+1:12;
    int debrisR=(int)(t*18);
    SDL_SetRenderDrawColor(ren,200,200,210,255);
    SDL_Rect shaft={x-2,y-shaftH,4,shaftH}; SDL_RenderFillRect(ren,&shaft);
    SDL_SetRenderDrawColor(ren,210,210,220,255);
    SDL_Rect head={x-4,y-shaftH-3,8,3}; SDL_RenderFillRect(ren,&head);
    SDL_SetRenderDrawColor(ren,225,225,235,255);
    int positions[3]={x-4,x-1,x+2};
    for(int i=0;i<3;i++) {
        SDL_Rect tine={positions[i],y-shaftH-3-tineH,2,tineH};
        SDL_RenderFillRect(ren,&tine);
    }
    if(smash>0&&debrisR>2) {
        SDL_SetRenderDrawColor(ren,200,200,210,190);
        SDL_Rect dl={x-debrisR,y-4,3,3};
        SDL_Rect dr={x+debrisR-3,y-4,3,3};
        SDL_Rect du={x-1,y-shaftH-debrisR/2,3,3};
        SDL_RenderFillRect(ren,&dl); SDL_RenderFillRect(ren,&dr); SDL_RenderFillRect(ren,&du);
    }
}

/* ═══════════════════════════════════════════════════════
   DESSIN CUILLERE
═══════════════════════════════════════════════════════ */
static void drawSpoon(int x,int y,int smash)
{
    float t=(smash>0)?1.0f-(float)smash/(float)SMASH_FRAMES:0.0f;
    int shaftH=(smash>0)?(int)(30*(1.0f-t*0.85f))+2:30;
    int bowlH=(smash>0)?(int)(9*(1.0f-t*0.88f))+1:9;
    int bowlW=11;
    int debrisR=(int)(t*18);
    SDL_SetRenderDrawColor(ren,200,200,210,255);
    SDL_Rect shaft={x-2,y-shaftH,4,shaftH}; SDL_RenderFillRect(ren,&shaft);
    SDL_SetRenderDrawColor(ren,215,215,225,255);
    SDL_Rect bowl={x-bowlW/2,y-shaftH-bowlH,bowlW,bowlH}; SDL_RenderFillRect(ren,&bowl);
    SDL_SetRenderDrawColor(ren,240,240,248,255);
    SDL_Rect shine={x-bowlW/2+1,y-shaftH-bowlH+1,bowlW/2,2}; SDL_RenderFillRect(ren,&shine);
    if(smash>0&&debrisR>2) {
        SDL_SetRenderDrawColor(ren,200,200,210,190);
        SDL_Rect dl={x-debrisR,y-4,4,3};
        SDL_Rect dr={x+debrisR-4,y-4,4,3};
        SDL_Rect du={x-2,y-shaftH-debrisR/2,4,3};
        SDL_RenderFillRect(ren,&dl); SDL_RenderFillRect(ren,&dr); SDL_RenderFillRect(ren,&du);
    }
}

static void drawMicrowave(int x,int y,int smash)
{
    float t=(smash>0)?1.0f-(float)smash/(float)SMASH_FRAMES:0.0f;
    int bodyW=32;
    int bodyH=(smash>0)?(int)(22*(1.0f-t*0.80f))+4:22;
    int debrisR=(int)(t*18);
    SDL_SetRenderDrawColor(ren,150,150,155,255);
    SDL_Rect body={x-bodyW/2,y-bodyH,bodyW,bodyH}; SDL_RenderFillRect(ren,&body);
    SDL_SetRenderDrawColor(ren,205,205,210,255);
    SDL_Rect top={x-bodyW/2,y-bodyH,bodyW,3}; SDL_RenderFillRect(ren,&top);
    SDL_SetRenderDrawColor(ren,60,65,75,255);
    SDL_Rect door={x-bodyW/2+4,y-bodyH+4,17,bodyH-8}; SDL_RenderFillRect(ren,&door);
    SDL_SetRenderDrawColor(ren,90,95,100,255);
    SDL_Rect panel={x+6,y-bodyH+4,6,bodyH-8}; SDL_RenderFillRect(ren,&panel);
    SDL_SetRenderDrawColor(ren,230,120,60,255);
    SDL_Rect light={x+7,y-bodyH+7,4,4}; SDL_RenderFillRect(ren,&light);
    if(smash>0&&debrisR>2) {
        SDL_SetRenderDrawColor(ren,170,170,175,200);
        SDL_Rect dl={x-bodyW/2-debrisR,y-5,5,4};
        SDL_Rect dr={x+bodyW/2+debrisR-5,y-6,5,4};
        SDL_Rect du={x-2,y-bodyH-debrisR/2,5,4};
        SDL_RenderFillRect(ren,&dl); SDL_RenderFillRect(ren,&dr); SDL_RenderFillRect(ren,&du);
    }
}

static void drawMicrowaveTexture(int x,int y,int smash)
{
    if(!texMicrowave) {
        drawMicrowave(x,y,smash);
        return;
    }

    (void)smash;
    int w = MICROWAVE_W;
    int h = MICROWAVE_H;
    SDL_Rect dst = {x - w/2, y - h, w, h};
    SDL_RenderCopy(ren, texMicrowave, NULL, &dst);
}

static void drawPanTexture(int x,int y,int smash)
{
    if(!texPan) {
        drawSpoon(x,y,smash);
        return;
    }

    (void)smash;
    SDL_Rect dst = {x - 20, y - 28, 40, 28};
    SDL_RenderCopy(ren, texPan, NULL, &dst);
}

static void drawForkRainTexture(int x,int y,int smash)
{
    if(!texForkRain) {
        drawPot(x,y,smash);
        return;
    }

    (void)smash;
    SDL_Rect dst = {x - 30, y - 42, 60, 42};
    SDL_RenderCopy(ren, texForkRain, NULL, &dst);
}

/* ═══════════════════════════════════════════════════════
   RENDER USTENSILES
═══════════════════════════════════════════════════════ */
static void renderUtensils(void)
{
    float landY=(float)lvl1[3].y;
    for(int i=0;i<UTENSIL_COUNT;i++) {
        Utensil *u=&utensils[i];
        if(u->state==USTATE_WAIT) continue;
        int ix=(int)u->x, iy=(int)u->y;
        if(u->state==USTATE_FALLING) {
            SDL_SetRenderDrawColor(ren,255,60,60,255);
            SDL_Rect warn={ix-6,4,12,8}; SDL_RenderFillRect(ren,&warn);
            for(int row=0;row<6;row++) {
                int w2=6-row;
                SDL_RenderDrawLine(ren,ix-w2,12+row,ix+w2,12+row);
            }
            float ratio=(u->y+100.0f)/(landY+100.0f);
            if(ratio<0) ratio=0;
            int sw=(int)(ratio*30), sa=(int)(ratio*110);
            if(sw>2&&sa>10) {
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren,0,0,0,(Uint8)sa);
                SDL_Rect shadow={ix-sw/2,(int)landY-3,sw,4}; SDL_RenderFillRect(ren,&shadow);
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            }
        }
        int smash=(u->state==USTATE_SMASHED)?u->smashTimer:0;
        drawMicrowaveTexture(ix,iy,smash);
    }
}

static void renderLvl5Rain(void)
{
    float landY=(float)lvl5[0].y;
    for(int i=0;i<LVL5_DROP_COUNT;i++) {
        RainDrop *d=&lvl5Drops[i];
        if(d->state==USTATE_WAIT) continue;

        int ix=(int)d->x, iy=(int)d->y;
        if(d->state==USTATE_FALLING) {
            SDL_SetRenderDrawColor(ren,255,60,60,255);
            SDL_Rect warn={ix-6,4,12,8}; SDL_RenderFillRect(ren,&warn);
            for(int row=0;row<6;row++) {
                int w2=6-row;
                SDL_RenderDrawLine(ren,ix-w2,12+row,ix+w2,12+row);
            }
            float ratio=(d->y+100.0f)/(landY+100.0f);
            if(ratio<0) ratio=0;
            int sw=(int)(ratio*34), sa=(int)(ratio*110);
            if(sw>2&&sa>10) {
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(ren,0,0,0,(Uint8)sa);
                SDL_Rect shadow={ix-sw/2,(int)landY-3,sw,4}; SDL_RenderFillRect(ren,&shadow);
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            }
        }

        int smash=(d->state==USTATE_SMASHED)?d->smashTimer:0;
        switch(d->type) {
            case RAIN_POT:       drawPot(ix,iy,smash); break;
            case RAIN_FORK:      drawForkRainTexture(ix,iy,smash); break;
            case RAIN_SPOON:     drawPanTexture(ix,iy,smash); break;
            case RAIN_MICROWAVE: drawMicrowaveTexture(ix,iy,smash); break;
        }
    }
}

static void renderBackground(void)
{
    SDL_Texture *bg = (currentLevel == 5 && texBackground6) ? texBackground6 : texBackground;
    if(bg) {
        SDL_Rect dst = {0, 0, SCREEN_W, SCREEN_H};
        SDL_RenderCopy(ren, bg, NULL, &dst);
        return;
    }
    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);
}

static void renderLvl1(void)
{
    renderLvl2FloorPlain((int)lvl1Ground.x, (int)lvl1Ground.y, lvl1Ground.w, lvl1Ground.h);
    SDL_SetRenderDrawColor(ren,100,100,100,255);
    SDL_Rect groundBase={(int)lvl1Ground.x,(int)lvl1Ground.y-4,lvl1Ground.w,4};
    SDL_RenderFillRect(ren,&groundBase);
    for(int x=(int)lvl1Ground.x; x<(int)(lvl1Ground.x + lvl1Ground.w); x+=LVL1_GROUND_TOOTH_W) {
        int tw = LVL1_GROUND_TOOTH_W - 2;
        if(x + tw > (int)(lvl1Ground.x + lvl1Ground.w)) {
            tw = (int)(lvl1Ground.x + lvl1Ground.w) - x;
        }
        if(tw > 4) drawToothUp(x + 1, (int)lvl1Ground.y, tw, TOOTH_H);
    }

    for(int i=0;i<LVL1_COUNT;i++) {
        Platform *p=&lvl1[i];
        if(p->y>SCREEN_H+p->h) continue;
        renderLvl2FloorPlain((int)p->x,(int)p->y,p->w,p->h);
        if(i==0) {
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,85,85,85,100);
            SDL_Rect sp={(int)p->x,(int)p->y,p->w,p->h}; SDL_RenderFillRect(ren,&sp);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        }
        if(i==LVL1_COUNT-1) {
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,85,85,85,140);
            SDL_Rect fin={(int)p->x,(int)p->y,p->w,p->h}; SDL_RenderFillRect(ren,&fin);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        }
    }
    if(spike1.visible) {
        int bx=(int)spike1.x, baseY=(int)lvl1[4].y;
        int spacing=SPIKE1_W/TOOTH_COUNT;
        SDL_SetRenderDrawColor(ren,100,100,100,255);
        SDL_Rect base={bx,baseY-4,SPIKE1_W,4}; SDL_RenderFillRect(ren,&base);
        for(int i=0;i<TOOTH_COUNT;i++) {
            int tx=bx+i*spacing+2, tw=spacing-4;
            drawToothUp(tx,baseY,tw,TOOTH_H);
        }
    }
    renderUtensils();
}

static void renderLvl2(void)
{
    renderLvl2FloorPlain(SOL_G_X,SOL_Y,SOL_G_W,SOL_H);
    renderLvl2FloorPlain(SOL_D_X,SOL_Y,SOL_D_W,SOL_H);
    for(int i=1;i<=4;i++) {
        Platform *p=&lvl2[i];
        if(p->y>SCREEN_H+p->h) continue;
        renderLvl2FloorPlain((int)p->x,(int)p->y,p->w,p->h);
    }
    renderSlideSpike();
    if(slideSpike.dustTimer > 0) {
        int spread = 18 + (18 - slideSpike.dustTimer) * 5;
        int alpha = 28 + slideSpike.dustTimer * 7;
        int baseX = (int)slideSpike.x - SLIDE_H;
        int baseY = (int)(slideSpike.y + SLIDE_H);
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren,165,155,145,alpha);
        for(int i=0;i<10;i++) {
            int puffW = spread + i * 9;
            int puffH = 10 + i * 3;
            int offset = (SLIDE_H * i) / 9;
            SDL_Rect puff={baseX + offset - puffW/3,baseY - puffH/2,puffW,puffH};
            SDL_RenderFillRect(ren,&puff);
        }
        SDL_SetRenderDrawColor(ren,145,135,125,alpha > 40 ? alpha - 20 : alpha);
        for(int i=0;i<12;i++) {
            int dustW = spread/2 + i * 6;
            int dustH = 6 + i * 2;
            int offset = (SLIDE_H * i) / 11;
            SDL_Rect dust={baseX + offset - dustW/4,baseY - 5 - (i%3),dustW,dustH};
            SDL_RenderFillRect(ren,&dust);
        }
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    }
    if(slideSpike.landed) {
        renderLvl2FloorPlain(SOL_D_X,SOL_Y,SOL_D_W,SOL_H);
    }
}

static void renderLvl4(void)
{
    // LEVEL 4
    renderLvl2FloorPlain(SOL_G_X,SOL_Y,SOL_G_W,SOL_H);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,60);
    SDL_Rect sp={SOL_G_X,SOL_Y,SOL_G_W,18}; SDL_RenderFillRect(ren,&sp);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    renderLvl2FloorPlain(SOL_D_X,SOL_Y,SOL_D_W,SOL_H);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,140);
    SDL_Rect fin={SOL_D_X,SOL_Y,SOL_D_W,18}; SDL_RenderFillRect(ren,&fin);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    for(int i=1;i<=5;i++) {
        Platform *p=&lvl4[i];
        if(i==3 && p->triggered) renderLvl4BreakingFloor(p);
        else renderLvl2FloorPlain((int)p->x,(int)p->y,p->w,p->h);
    }

    if(lvl4LaserVisible) {
        Platform *p=&lvl4[1];
        int lx=(int)(p->x + p->w/2.0f - LVL4_LASER_W/2.0f);
        int ly=LVL4_LASER_TOP;
        int lh=(int)p->y - ly;
        int emitterSize=16;
        int ex=lx + LVL4_LASER_W/2 - emitterSize/2;
        int ey=ly - emitterSize/2;

        if(texLvl4Laser) {
            SDL_Rect core={lx,ly,LVL4_LASER_W,lh};
            SDL_Rect src = getLvl4LaserFrameSrcRect();
            if(src.w > 0 && src.h > 0) {
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
                SDL_RenderCopy(ren,texLvl4Laser,&src,&core);
                SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
            }
        }

        SDL_SetRenderDrawColor(ren,15,15,15,255);
        SDL_Rect emitter={ex,ey,emitterSize,emitterSize}; SDL_RenderFillRect(ren,&emitter);
        SDL_SetRenderDrawColor(ren,45,45,45,255);
        SDL_Rect emitterTop={ex,ey,emitterSize,3}; SDL_RenderFillRect(ren,&emitterTop);
        SDL_SetRenderDrawColor(ren,210,40,40,255);
        SDL_RenderDrawLine(ren,ex+3,ey+emitterSize-1,ex+emitterSize-4,ey+emitterSize-1);
    }

    if(lvl4FireVisible) {
        Platform *p=&lvl4[5];
        int baseX=(int)p->x + FIRE_PAD_X;
        int baseY=(int)p->y;
        int fireW=p->w - 2 * FIRE_PAD_X;
        SDL_Rect dst={baseX,baseY-FIRE_RENDER_H,fireW,FIRE_RENDER_H};

        if(texFire) {
            SDL_Rect src = getFireFrameSrcRect();
            if(src.w > 0 && src.h > 0) SDL_RenderCopy(ren,texFire,&src,&dst);
        } else {
            int cols=4;
            int spacing=fireW/cols;

            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,255,110,20,170);
            SDL_Rect glow={baseX-4,baseY-20,fireW+8,20}; SDL_RenderFillRect(ren,&glow);
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

            SDL_SetRenderDrawColor(ren,120,40,10,255);
            SDL_Rect grate={baseX,baseY-4,fireW,4}; SDL_RenderFillRect(ren,&grate);

            for(int i=0;i<cols;i++) {
                int fx=baseX + i*spacing + spacing/2;
                int outerH=12 + (i%2)*8;
                int innerH=7 + (i%2)*5;
                SDL_SetRenderDrawColor(ren,255,120,20,255);
                drawToothUp(fx-6,baseY,12,outerH);
                SDL_SetRenderDrawColor(ren,255,220,120,255);
                drawToothUp(fx-3,baseY,6,innerH);
            }
        }
    }

    drawLvl4Ball();
}

static void renderLvl5(void)
{
    renderLvl2FloorPlain(0,SOL_Y,SCREEN_W,SOL_H);

    if(lvl5HoleVisible) {
        SDL_SetRenderDrawColor(ren,10,10,10,255);
        SDL_Rect hole={(int)lvl5HoleX,SOL_Y,(int)lvl5HoleW,LVL5_HOLE_H};
        SDL_RenderFillRect(ren,&hole);
        SDL_SetRenderDrawColor(ren,55,55,55,255);
        SDL_Rect leftLip={(int)lvl5HoleX,SOL_Y,3,20};
        SDL_Rect rightLip={(int)(lvl5HoleX + lvl5HoleW - 3),SOL_Y,3,20};
        SDL_RenderFillRect(ren,&leftLip);
        SDL_RenderFillRect(ren,&rightLip);
    }

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,60);
    SDL_Rect sp={0,SOL_Y,140,18}; SDL_RenderFillRect(ren,&sp);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,140);
    SDL_Rect fin={SCREEN_W-180,SOL_Y,180,18}; SDL_RenderFillRect(ren,&fin);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    renderFallingWallTrap(&lvl5WallTrap);

    if(spike5.visible) {
        int bx=(int)spike5.x, baseY=(int)lvl5[0].y;
        int spacing=SPIKE1_W/TOOTH_COUNT;
        SDL_SetRenderDrawColor(ren,100,100,100,255);
        SDL_Rect base={bx,baseY-4,SPIKE1_W,4}; SDL_RenderFillRect(ren,&base);
        for(int i=0;i<TOOTH_COUNT;i++) {
            int tx=bx+i*spacing+2, tw=spacing-4;
            drawToothUp(tx,baseY,tw,TOOTH_H);
        }
    }

    if(lvl5HoleSpike.visible) {
        int bx=(int)lvl5HoleSpike.x;
        int baseY=(int)lvl5HoleSpike.y + TOOTH_H;
        int spacing=SPIKE1_W/TOOTH_COUNT;
        SDL_SetRenderDrawColor(ren,100,100,100,255);
        SDL_Rect base={bx,baseY-4,SPIKE1_W,4}; SDL_RenderFillRect(ren,&base);
        for(int i=0;i<TOOTH_COUNT;i++) {
            int tx=bx+i*spacing+2, tw=spacing-4;
            drawToothUp(tx,baseY,tw,TOOTH_H);
        }
    }

    renderLvl5Rain();
}

static void renderLvl6Car(int x, int groundY)
{
    if(texLvl6Car) {
        SDL_Rect dst={x-LVL6_CAR_DRAW_W/2,groundY-LVL6_CAR_DRAW_H+4,LVL6_CAR_DRAW_W,LVL6_CAR_DRAW_H};
        SDL_RenderCopy(ren,texLvl6Car,NULL,&dst);
        return;
    }

    SDL_SetRenderDrawColor(ren,180,35,28,255);
    SDL_Rect body={x-44,groundY-30,88,20};
    SDL_RenderFillRect(ren,&body);
}

static void renderLvl6(void)
{
    int collapseActive = 0;
    int vibY = 0;
    for(int i=0;i<LVL6_CHUNK_COUNT;i++) {
        Platform *c=&lvl6Chunks[i];
        if(c->falling && c->y < SCREEN_H + c->h) {
            collapseActive = 1;
            break;
        }
    }
    if(collapseActive) vibY=(int)(2.5f*sinf((float)SDL_GetTicks()*0.03f));

    for(int i=0;i<LVL6_CHUNK_COUNT;i++) {
        renderLvl6Chunk(&lvl6Chunks[i],vibY);
    }
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,60);
    for(int i=0;i<LVL6_CHUNK_COUNT;i++) {
        Platform *c=&lvl6Chunks[i];
        if(c->falling) continue;
        SDL_Rect sp={(int)c->x,SOL_Y+vibY,c->w,18};
        SDL_RenderFillRect(ren,&sp);
    }
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    renderLvl2FloorPlain(LVL6_RIGHT_X,SOL_Y+vibY,LVL6_RIGHT_W,SOL_H);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,140);
    SDL_Rect fin={LVL6_RIGHT_X,SOL_Y+vibY,LVL6_RIGHT_W,18}; SDL_RenderFillRect(ren,&fin);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    if(!player.won) {
        renderLvl6Car(LVL6_CAR_X, SOL_Y + vibY);
    }

    for(int i=1;i<=3;i++) {
        Platform *p=&lvl6[i];
        renderLvl2FloorPlain((int)p->x,(int)p->y+vibY,p->w,p->h);
        if(i==2 && lvl6FreezeState != 0) {
            int px = (int)p->x;
            int py = (int)p->y + vibY;
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren,90,175,255,150);
            SDL_Rect freeze={px,py,p->w,p->h};
            SDL_RenderFillRect(ren,&freeze);
            SDL_SetRenderDrawColor(ren,220,245,255,115);
            SDL_Rect shine={px,py,p->w,6};
            SDL_RenderFillRect(ren,&shine);
            SDL_SetRenderDrawColor(ren,235,250,255,95);
            for(int j=0;j<4;j++) {
                int crackX = px + 8 + j * (p->w / 4);
                SDL_RenderDrawLine(ren,crackX,py+4,crackX+8,py+p->h-4);
                SDL_RenderDrawLine(ren,crackX+3,py+10,crackX-5,py+p->h-6);
            }
            SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
        }
    }

    if(lvl6FreezeState != 0) {
        Platform *freezePlat = &lvl6[2];
        renderIceSnowflakes((int)freezePlat->x, 0, freezePlat->w, (int)freezePlat->y + vibY);
    }
}

/* ═══════════════════════════════════════════════════════
   DESSIN BALLON
═══════════════════════════════════════════════════════ */
static void drawBall3(void)
{
    if(!ball3Active) return;
    int cx=(int)ball3X, cy=BALL3_CY, r=BALL3_R;
    SDL_Rect dst={cx-r,cy-r,r*2,r*2};
    if(texBalloon) {
        double angle = -(double)ball3Angle * 180.0 / M_PI;
        SDL_RenderCopyEx(ren,texBalloon,NULL,&dst,angle,NULL,SDL_FLIP_NONE);
        return;
    }

    for(int dy=-r;dy<=r;dy++) {
        float fdx=sqrtf((float)(r*r-dy*dy));
        int shade=(dy<0)?(int)(55.0f*(float)(-dy)/(float)r):0;
        SDL_SetRenderDrawColor(ren,
            (Uint8)(130+shade),(Uint8)(45+shade/3),(Uint8)(35+shade/3),255);
        SDL_RenderDrawLine(ren,cx-(int)fdx,cy+dy,cx+(int)fdx,cy+dy);
    }
}

static void drawLvl4Ball(void)
{
    for(int i=0;i<LVL4_BALL_COUNT;i++) {
        if(!lvl4BallActive[i]) continue;

        int cx=(int)lvl4BallX[i];
        int cy=(int)lvl4BallY[i];
        int r=LVL4_BALL_R;

        for(int dy=-r;dy<=r;dy++) {
            float fdx=sqrtf((float)(r*r-dy*dy));
            int shade=(dy<0)?(int)(60.0f*(float)(-dy)/(float)r):0;
            int rim=(int)(25.0f*(1.0f - fabsf((float)dy)/(float)r));
            SDL_SetRenderDrawColor(ren,
                (Uint8)(130 + shade + rim),
                (Uint8)(138 + shade + rim),
                (Uint8)(148 + shade + rim),255);
            SDL_RenderDrawLine(ren,cx-(int)fdx,cy+dy,cx+(int)fdx,cy+dy);
        }

        SDL_SetRenderDrawColor(ren,88,96,108,255);
        int seamR=r-6;
        for(int s=0;s<2;s++) {
            float ra=lvl4BallAngle[i]+s*1.5708f;
            int x1=cx+(int)(seamR*cosf(ra));
            int y1=cy+(int)(seamR*sinf(ra));
            int x2=cx-(int)(seamR*cosf(ra));
            int y2=cy-(int)(seamR*sinf(ra));
            SDL_RenderDrawLine(ren,x1,y1,x2,y2);
        }

        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
        int sr=r/4, hcx=cx-r/3, hcy=cy-r/3;
        for(int dy=-sr;dy<=0;dy++) {
            int hw=(int)(sqrtf((float)(sr*sr-dy*dy))*0.8f);
            SDL_SetRenderDrawColor(ren,255,255,255,95);
            SDL_RenderDrawLine(ren,hcx-hw,hcy+dy+sr,hcx+hw,hcy+dy+sr);
        }
        SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    }
}

static void renderLvl3(void)
{
    int rx=LVL3_GAP_X+LVL3_GAP_W;
    int rw=SCREEN_W-rx;
    int vibY=0;
    if(isBall3Rolling()) vibY=(int)(2.5f*sinf((float)SDL_GetTicks()*0.03f));

    /* plafond */
    renderBrick(0,0,SCREEN_W,LVL3_CEIL_H,1);

    /* sol gauche */
    renderBrick(0,LVL3_FLOOR_Y+vibY,LVL3_GAP_X,LVL3_FLOOR_H,1);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,60);
    SDL_Rect sp={0,LVL3_FLOOR_Y+vibY,LVL3_GAP_X,18}; SDL_RenderFillRect(ren,&sp);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    /* fond du trou */
    renderBrick(LVL3_GAP_X,LVL3_HOLE_Y+vibY,LVL3_GAP_W,LVL3_HOLE_H,1);

    /* sol droit */
    renderBrick(rx,LVL3_FLOOR_Y+vibY,rw,LVL3_FLOOR_H,1);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,85,85,85,140);
    SDL_Rect fin={rx,LVL3_FLOOR_Y+vibY,rw,18}; SDL_RenderFillRect(ren,&fin);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);

    if(!lvl3WallBroken) {
        renderBrick(LVL3_WALL_X,LVL3_WALL_Y,LVL3_WALL_W,LVL3_WALL_H,1);
    }

    for(int i=0;i<LVL3_WALL_DEBRIS_COUNT;i++) {
        WallDebris *d=&lvl3WallDebris[i];
        if(!d->active) continue;
        SDL_SetRenderDrawColor(ren,120,125,130,255);
        SDL_Rect piece={(int)d->x,(int)d->y,d->size,d->size};
        SDL_RenderFillRect(ren,&piece);
        SDL_SetRenderDrawColor(ren,180,185,190,255);
        SDL_RenderDrawLine(ren,piece.x,piece.y,piece.x+piece.w-1,piece.y);
    }

    drawBall3();
}

static void renderPlayer(void)
{
    PlayerSkinAssets *skin = getCurrentPlayerSkin();

    if(player.dead && deathType == DEATH_CRUSH) return;
    if(gameMode == MODE_DUO && player.dead) return;

    if(player.dead && deathType == DEATH_LASER && skin->electrise) {
        int dw = PLAYER_W + 18;
        int dh = PLAYER_H + 18;
        int sx = (int)player.x + PLAYER_W/2 - dw/2;
        int sy = (int)player.y + PLAYER_H - dh;
        SDL_Rect src = getElectriseFrameSrcRect();
        SDL_Rect dst = {sx, sy, dw, dh};
        if(src.w > 0 && src.h > 0) {
            SDL_RenderCopy(ren, skin->electrise, &src, &dst);
            return;
        }
    }

    if(player.dead && deathType == DEATH_FIRE && skin->burn && !isBurnAnimFinished()) {
        int dw = PLAYER_W;
        int dh = PLAYER_H;
        int sx = (int)player.x + PLAYER_W/2 - dw/2;
        int sy = (int)player.y + PLAYER_H - dh;
        SDL_Rect src = getBurnFrameSrcRect();
        SDL_Rect dst = {sx, sy, dw, dh};
        if(src.w > 0 && src.h > 0) {
            SDL_RenderCopy(ren, skin->burn, &src, &dst);
            return;
        }
    }

    if(player.dead && deathType == DEATH_FIRE && skin->dust && isBurnAnimFinished() && dustVisible) {
        SDL_Rect src = getDustFrameSrcRect();
        SDL_Rect dst = getPlayerBodyRect();
        if(src.w > 0 && src.h > 0) {
            SDL_RenderCopy(ren, skin->dust, &src, &dst);
            return;
        }
    }

    if(player.dead && deathType == DEATH_TOUCH && skin->touch) {
        int dw = PLAYER_W + 18;
        int dh = PLAYER_H + 18;
        int sx = (int)touchDeathX + PLAYER_W/2 - dw/2;
        int sy = (int)touchDeathY + PLAYER_H - dh + TOUCH_FLOOR_OFFSET;
        SDL_Rect src = getTouchFrameSrcRect();
        SDL_Rect dst = {sx, sy, dw, dh};
        if(src.w > 0 && src.h > 0) {
            SDL_RenderCopy(ren, skin->touch, &src, &dst);
            return;
        }
    }

    if(currentLevel == 5 && lvl6FreezeState != 0 && skin->ice) {
        int dw = PLAYER_W + 18;
        int dh = PLAYER_H + 18;
        int sx = (int)player.x + PLAYER_W/2 - dw/2;
        int sy = (int)player.y + PLAYER_H - dh;
        SDL_Rect src = getIceFrameSrcRect();
        SDL_Rect dst = {sx, sy, dw, dh};
        if(src.w > 0 && src.h > 0) {
            SDL_RenderCopy(ren, skin->ice, &src, &dst);
            return;
        }
    }

    if(!player.onGround)             animState=PANIM_JUMP;
    else if(fabsf(player.vx)>0.5f)  animState=PANIM_RUN;
    else                             animState=PANIM_IDLE;
    tickAnim();
    SDL_RendererFlip flip=player.facingRight?SDL_FLIP_NONE:SDL_FLIP_HORIZONTAL;
    int dw,dh; SDL_Texture *tex=NULL;
    switch(animState) {
        case PANIM_RUN:  tex=skin->run;  dw=RUN_DW;  dh=RUN_DH;  break;
        case PANIM_JUMP: tex=skin->jump; dw=JUMP_DW; dh=JUMP_DH; break;
        default:         tex=skin->idle; dw=IDLE_DW; dh=IDLE_DH; break;
    }
    int sx=(int)player.x+PLAYER_W/2-dw/2;
    int sy=(int)player.y+PLAYER_H-dh;
    if(tex) {
        int texW,texH;
        SDL_QueryTexture(tex,NULL,NULL,&texW,&texH);
        int fw=texW/6,fh=texH/6;
        int col=animFrame%6,row=animFrame/6;
        SDL_Rect src={col*fw,row*fh,fw,fh};
        SDL_Rect dst={sx,sy,dw,dh};
        SDL_RenderCopyEx(ren,tex,&src,&dst,0,NULL,flip);
    } else {
        SDL_SetRenderDrawColor(ren,80,200,120,255);
        SDL_Rect body=getPlayerBodyRect();
        SDL_RenderFillRect(ren,&body);
    }
}

static void renderTextAt(TTF_Font *f,const char *txt,int x,int y,Uint8 r,Uint8 g,Uint8 b)
{
    if(!f) return;
    SDL_Color col={r,g,b,255};
    SDL_Surface *sf=TTF_RenderText_Blended(f,txt,col);
    if(!sf) return;
    SDL_Texture *t=SDL_CreateTextureFromSurface(ren,sf);
    SDL_FreeSurface(sf); if(!t) return;
    int tw,th; SDL_QueryTexture(t,NULL,NULL,&tw,&th);
    SDL_Rect dst={x-tw/2,y,tw,th};
    SDL_RenderCopy(ren,t,NULL,&dst);
    SDL_DestroyTexture(t);
}

static void renderText(TTF_Font *f,const char *txt,int y,Uint8 r,Uint8 g,Uint8 b)
{
    renderTextAt(f,txt,SCREEN_W/2,y,r,g,b);
}

static void renderYouDied(void)
{
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,160);
    SDL_Rect ov={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&ov);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
}

static void renderYouWin(void)
{
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,160);
    SDL_Rect ov={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&ov);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    renderText(bigFont,"YOU WIN !",SCREEN_H/2-60,255,215,0);
}

static __attribute__((unused)) void renderLevelClear(void)
{
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren,0,0,0,160);
    SDL_Rect ov={0,0,SCREEN_W,SCREEN_H}; SDL_RenderFillRect(ren,&ov);
    SDL_SetRenderDrawBlendMode(ren,SDL_BLENDMODE_NONE);
    renderText(bigFont,"LEVEL COMPLETE!",      SCREEN_H/2-60,80,220,80);
}

static int shouldShowDeathScreen(void)
{
    if(!player.dead) return 0;
    if(SDL_GetTicks() - deathStartTick < DEATH_SCREEN_DELAY_MS) return 0;
    if(currentLevel == 1 && deathType == DEATH_CRUSH && slideSpike.falling) {
        return 0;
    }
    if(deathType == DEATH_TOUCH && !isTouchAnimFinished()) return 0;
    if(deathType == DEATH_FIRE && (!isBurnAnimFinished() || !isDustAnimFinished())) return 0;
    if(currentLevel == 4 && deathType == DEATH_CRUSH &&
       (lvl5WallTrap.falling || lvl5WallTrap.dustTimer > 0)) {
        return 0;
    }
    return 1;
}

static int shouldAutoRestart(void)
{
    if(!player.dead) return 0;
    if(!shouldShowDeathScreen()) return 0;
    return (SDL_GetTicks() - deathStartTick) >= AUTO_RESTART_DELAY_MS;
}

static void renderHUD(void)
{
    if(lvl6CarPromptVisible()) {
        const char *prompt = "PRESS F";
        int promptX = LVL6_CAR_X;
        int promptY = SOL_Y - LVL6_CAR_DRAW_H - 36;
        if(promptY < 18) promptY = 18;
        renderTextAt(font,prompt,promptX+2,promptY+2,0,0,0);
        renderTextAt(font,prompt,promptX,promptY,255,255,255);
    }
}

static void renderStartMenu(void)
{
    char soloControls[96];
    char duoP1Controls[96];
    char duoP2Controls[96];

    if(activeSession) {
        snprintf(soloControls, sizeof(soloControls), "SOLO: %s, [%s] TO INTERACT",
                 session_control_scheme_label(level3ControlSchemeForPlayer(0)),
                 session_interact_bind_label(level3InteractBindForPlayer(0)));
        snprintf(duoP1Controls, sizeof(duoP1Controls), "DUO P1: %s, [%s] TO INTERACT",
                 session_control_scheme_label(level3ControlSchemeForPlayer(0)),
                 session_interact_bind_label(level3InteractBindForPlayer(0)));
        snprintf(duoP2Controls, sizeof(duoP2Controls), "DUO P2: %s, [%s] TO INTERACT",
                 session_control_scheme_label(level3ControlSchemeForPlayer(1)),
                 session_interact_bind_label(level3InteractBindForPlayer(1)));
    } else {
        snprintf(soloControls, sizeof(soloControls), "SOLO: ARROWS TO MOVE, [f] TO INTERACT");
        snprintf(duoP1Controls, sizeof(duoP1Controls), "DUO P1: ARROWS, RIGHT CTRL TO INTERACT");
        snprintf(duoP2Controls, sizeof(duoP2Controls), "DUO P2: WASD, [e] TO INTERACT");
    }

    SDL_SetRenderDrawColor(ren,12,12,18,255);
    SDL_RenderClear(ren);
    renderText(bigFont,"TRAP ADVENTURE",120,255,255,255);
    renderText(font,"PRESS [1] FOR SOLO",260,255,255,255);
    renderText(font,"PRESS [2] FOR DUO",310,255,255,255);
    renderText(font,soloControls,410,210,210,210);
    renderText(font,duoP1Controls,455,210,210,210);
    renderText(font,duoP2Controls,500,210,210,210);
}

static SDL_Rect getAspectFitRect(SDL_Rect area, int contentW, int contentH)
{
    SDL_Rect fitted = area;

    if(area.w <= 0 || area.h <= 0 || contentW <= 0 || contentH <= 0) {
        return fitted;
    }

    if((long long)area.w * (long long)contentH <= (long long)area.h * (long long)contentW) {
        fitted.w = area.w;
        fitted.h = (int)((long long)area.w * (long long)contentH / (long long)contentW);
        if(fitted.h < 1) fitted.h = 1;
        fitted.y = area.y + (area.h - fitted.h) / 2;
    } else {
        fitted.h = area.h;
        fitted.w = (int)((long long)area.h * (long long)contentW / (long long)contentH);
        if(fitted.w < 1) fitted.w = 1;
        fitted.x = area.x + (area.w - fitted.w) / 2;
    }

    return fitted;
}

static SDL_Rect getDuoZoomSourceRect(int focusX)
{
    SDL_Rect src = {0, 0, SCREEN_W / 2, SCREEN_H};

    src.x = focusX - src.w / 2;

    if(src.x < 0) src.x = 0;
    if(src.x + src.w > SCREEN_W) src.x = SCREEN_W - src.w;

    return src;
}

static int duoPlayerNeedsFullscreenAnimation(const DuoPlayerState *state)
{
    const RuntimeState *runtime = &state->runtime;

    if(state->finished) return 0;
    return runtime->currentLevel == 5 &&
           runtime->lvl6FreezeState != 0 &&
           !runtime->iceAnimFinished;
}

static void renderCurrentPlatformScene(void)
{
    renderBackground();
    if(currentLevel==0)      renderLvl1();
    else if(currentLevel==1) renderLvl2();
    else if(currentLevel==2) renderLvl3();
    else if(currentLevel==3) renderLvl4();
    else if(currentLevel==4) renderLvl5();
    else                     renderLvl6();
    renderPlayer();
    renderHUD();
}

static void renderSoloGame(void)
{
    SDL_Rect srcRect;
    SDL_Rect dst = {0, 0, SCREEN_W, SCREEN_H};
    SDL_Texture *previousTarget;

    if(!soloFrame) {
        soloFrame = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_W, SCREEN_H);
    }

    if(!soloFrame) {
        SDL_RenderClear(ren);
        renderCurrentPlatformScene();
        renderLevelTransitionFade();
        if(shouldShowDeathScreen()) renderYouDied();
        if(player.won&&currentLevel==MAX_LEVELS-1) renderYouWin();
        return;
    }

    previousTarget = SDL_GetRenderTarget(ren);
    SDL_SetRenderTarget(ren, soloFrame);
    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);
    renderCurrentPlatformScene();
    SDL_SetRenderTarget(ren, previousTarget);

    srcRect = (SDL_Rect){0, 0, SCREEN_W, SCREEN_H};
    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, soloFrame, &srcRect, &dst);

    renderLevelTransitionFade();
    if(shouldShowDeathScreen()) renderYouDied();
    if(player.won&&currentLevel==MAX_LEVELS-1) renderYouWin();
}

static void startSoloGame(void)
{
    gameMode = MODE_SOLO;
    currentLevel=0;
    transitionActive = 0;
    transitionNextLevel = 0;
    transitionStartTick = 0;
    initLvl1(); initSpike1(); initLvl2(); initLvl3(); initLvl4(); initLvl5(); initLvl6(); initPlayer();
    loadIntroCinematic();
}

static void startDuoGame(void)
{
    int levels[MAX_LEVELS] = {0,1,2,3,4,5};
    int i;

    gameMode = MODE_DUO;
    introActive = 0;
    introFrameCount = 0;
    duoAllDoneTick = 0;
    transitionActive = 0;
    transitionNextLevel = 0;
    transitionStartTick = 0;
    stopIntroCinematicSound();
    Mix_HaltChannel(-1);
    Mix_HaltMusic();

    for(i=MAX_LEVELS-1;i>0;i--) {
        int j = rand() % (i + 1);
        int tmp = levels[i];
        levels[i] = levels[j];
        levels[j] = tmp;
    }

    for(i=0;i<2;i++) {
        int k;
        duoPlayers[i].progress = 0;
        duoPlayers[i].finished = 0;
        for(k=0;k<3;k++) {
            duoPlayers[i].assignedLevels[k] = levels[i*3 + k];
        }
        currentLevel = duoPlayers[i].assignedLevels[0];
        initPlayer();
        saveRuntimeState(&duoPlayers[i].runtime);
        duoInteractPressed[i] = 0;
        duoRadioPressed[i] = 0;
        if(!duoFrames[i]) {
            duoFrames[i] = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, SCREEN_W, SCREEN_H);
        }
    }

    loadIntroCinematic();
}

static void updateOnlineRemoteDuoActions(void)
{
    int remoteInteractDown;

    if(gameMode != MODE_DUO || !online_client_is_connected() || !online_client_is_host()) {
        previousRemoteInteractDown = 0;
        return;
    }

    remoteInteractDown =
        online_client_remote_scancode_down(SDL_SCANCODE_F) ||
        online_client_remote_scancode_down(SDL_SCANCODE_0);

    if(remoteInteractDown && !previousRemoteInteractDown) {
        duoInteractPressed[1] = 1;
        duoRadioPressed[1] = 1;
    }
    previousRemoteInteractDown = remoteInteractDown;
}

static void level3CarryForwardLives(const GameSession *session, int *outP1Lives, int *outP2Lives)
{
    int p1Lives = 9;
    int p2Lives = 0;

    if(session) {
        if(session->level2.starting_lives > 0) {
            p1Lives = session->level2.starting_lives - session->level2.player_lives_lost;
            if(session->mode == GAME_MODE_DUO) {
                if(session->level2.multiplayer) {
                    p2Lives = session->level2.starting_lives - session->level2.marv_lives_lost;
                } else {
                    p2Lives = p1Lives;
                }
            }
        } else if(session->level1.completed) {
            p1Lives = session->level1.lives_remaining + session->bonus_lives_for_level2;
            if(session->mode == GAME_MODE_DUO) p2Lives = p1Lives;
        }
    }

    if(p1Lives < 0) p1Lives = 0;
    if(p2Lives < 0) p2Lives = 0;
    if(outP1Lives) *outP1Lives = p1Lives;
    if(outP2Lives) *outP2Lives = p2Lives;
}

static int updateDuoGame(void)
{
    int i;
    int finishedCount = 0;

    for(i=0;i<2;i++) {
        if(duoPlayers[i].finished) {
            finishedCount++;
            duoInteractPressed[i] = 0;
            duoRadioPressed[i] = 0;
            continue;
        }

        loadRuntimeState(&duoPlayers[i].runtime);
        duoInputPlayer = i;

        if(isLevelTransitionActive()) {
            updateLevelTransition();
        } else if(lvl6MazeActive) {
            if(duoRadioPressed[i]) toggleLvl6MazeRadio();
            updateLvl6MazeMode();
        } else if(!player.dead && !player.won) {
            if(duoInteractPressed[i] && lvl6CarInteractionAvailable()) {
                startLvl6Maze();
            }
            if(!lvl6MazeActive) {
                if(currentLevel==0)      updateLvl1();
                else if(currentLevel==1) updateLvl2();
                else if(currentLevel==2) updateLvl3();
                else if(currentLevel==3) updateLvl4();
                else if(currentLevel==4) updateLvl5();
                else                     updateLvl6();
            }
        } else if(player.dead && currentLevel==0 &&
                  (lvl1UtensilAnimationActive() || lvl1FallingPlatformActive())) {
            updateLvl1DeathSequence();
        } else if(player.dead && currentLevel==1 &&
                  (lvl2[2].falling ||
                   (deathType == DEATH_CRUSH &&
                    (slideSpike.falling || slideSpike.dustTimer > 0)))) {
            updateLvl2();
        } else if(player.dead && currentLevel==3 && lvl4[3].falling) {
            updateLvl4();
        } else if(player.dead && currentLevel==4 && deathType == DEATH_CRUSH &&
                  (lvl5WallTrap.falling || lvl5WallTrap.dustTimer > 0)) {
            updateLvl5();
        } else if(player.dead && deathType == DEATH_LASER) {
            updateLaserDeath();
        } else if(player.dead && deathType == DEATH_TOUCH) {
            tickTouchAnim();
        } else if(player.dead && deathType == DEATH_FIRE) {
            if(currentLevel == 3) updateFireAnim();
            tickBurnAnim();
            tickDustAnim();
        }

        if(shouldAutoRestart()) {
            initPlayer();
        } else if(player.won) {
            if(duoPlayers[i].progress < 2) {
                duoPlayers[i].progress++;
                startLevelTransition(duoPlayers[i].assignedLevels[duoPlayers[i].progress]);
                player.won = 0;
            } else {
                duoPlayers[i].finished = 1;
                finishedCount++;
                player.won = 0;
            }
        }

        saveRuntimeState(&duoPlayers[i].runtime);
        duoInteractPressed[i] = 0;
        duoRadioPressed[i] = 0;
    }

    if(finishedCount >= 2) {
        if(duoAllDoneTick == 0) duoAllDoneTick = SDL_GetTicks();
        if((SDL_GetTicks() - duoAllDoneTick) >= AUTO_RESTART_DELAY_MS) return 1;
    } else {
        duoAllDoneTick = 0;
    }
    return 0;
}

static void renderDuoGame(void)
{
    int i;
    SDL_Rect leftDst = {0, 0, SCREEN_W/2, SCREEN_H};
    SDL_Rect rightDst = {SCREEN_W/2, 0, SCREEN_W/2, SCREEN_H};
    int fullscreenPlayer = -1;

    SDL_SetRenderDrawColor(ren,0,0,0,255);
    SDL_RenderClear(ren);

    for(i=0;i<2;i++) {
        if(duoPlayerNeedsFullscreenAnimation(&duoPlayers[i])) {
            fullscreenPlayer = i;
            break;
        }
    }

    if(fullscreenPlayer != -1 && duoFrames[fullscreenPlayer]) {
        SDL_Rect dst = {0, 0, SCREEN_W, SCREEN_H};
        SDL_Rect srcRect;

        loadRuntimeState(&duoPlayers[fullscreenPlayer].runtime);
        duoInputPlayer = fullscreenPlayer;
        SDL_SetRenderTarget(ren, duoFrames[fullscreenPlayer]);
        SDL_SetRenderDrawColor(ren,0,0,0,255);
        SDL_RenderClear(ren);
        if(lvl6MazeActive) {
            renderLvl6MazeMode();
        } else {
            renderCurrentPlatformScene();
            renderLevelTransitionFade();
            if(shouldShowDeathScreen()) renderYouDied();
        }
        SDL_SetRenderTarget(ren, NULL);

        if(lvl6MazeActive) {
            srcRect = (SDL_Rect){0, 0, SCREEN_W, SCREEN_H};
        } else {
            int focusX = (int)player.x + PLAYER_W / 2;
            srcRect = getDuoZoomSourceRect(focusX);
        }
        SDL_RenderCopy(ren, duoFrames[fullscreenPlayer], &srcRect, &dst);
        saveRuntimeState(&duoPlayers[fullscreenPlayer].runtime);

        if(duoAllDoneTick != 0) {
            renderText(bigFont,"ALL LEVELS COMPLETE",SCREEN_H/2 - 60,255,215,0);
        }
        return;
    }

    for(i=0;i<2;i++) {
        SDL_Rect pane = (i == 0) ? leftDst : rightDst;
        SDL_Rect contentDst;
        int centerX = pane.x + pane.w/2;

        if(!duoPlayers[i].finished && duoFrames[i]) {
            SDL_Rect srcRect;
            loadRuntimeState(&duoPlayers[i].runtime);
            duoInputPlayer = i;
            SDL_SetRenderTarget(ren, duoFrames[i]);
            SDL_SetRenderDrawColor(ren,0,0,0,255);
            SDL_RenderClear(ren);
            if(lvl6MazeActive) {
                renderLvl6MazeMode();
            } else {
                renderCurrentPlatformScene();
                renderLevelTransitionFade();
                if(shouldShowDeathScreen()) renderYouDied();
            }
            SDL_SetRenderTarget(ren, NULL);
            if(lvl6MazeActive) {
                srcRect = (SDL_Rect){0, 0, SCREEN_W, SCREEN_H};
            } else {
                int focusX = (int)player.x + PLAYER_W / 2;
                srcRect = getDuoZoomSourceRect(focusX);
            }
            contentDst = getAspectFitRect(pane, srcRect.w, srcRect.h);
            SDL_RenderCopy(ren, duoFrames[i], &srcRect, &contentDst);
            saveRuntimeState(&duoPlayers[i].runtime);
        } else {
            SDL_SetRenderDrawColor(ren,20,20,28,255);
            SDL_RenderFillRect(ren, &pane);
            renderTextAt(bigFont,"DONE",centerX,pane.y + SCREEN_H/2 - 120,255,215,0);
        }
    }

    SDL_SetRenderDrawColor(ren,255,255,255,180);
    SDL_RenderDrawLine(ren, SCREEN_W/2, 0, SCREEN_W/2, SCREEN_H);

    if(duoAllDoneTick != 0) {
        renderText(bigFont,"ALL LEVELS COMPLETE",SCREEN_H/2 - 60,255,215,0);
    }
}

/* ═══════════════════════════════════════════════════════
   MAIN
═══════════════════════════════════════════════════════ */
static int runLevel3Hell(GameSession *session, SDL_Window *sharedWin, SDL_Renderer *sharedRen, int standalone)
{
    int ownsSdl = 0;
    int ownsWindow = 0;
    int ownsRenderer = 0;
    int ownsTtf = 0;
    int ownsImg = 0;
    int ownsMixer = 0;
    int ownsAudio = 0;

    activeSession = session;
    level3HellCompleted = 0;
    pauseMenuReady = 0;
    pauseMenuActive = 0;
    previousRemoteInteractDown = 0;
    gameMode = standalone ? MODE_MENU : MODE_SOLO;

    srand((unsigned)time(NULL));

    if(standalone) {
        if(SDL_Init(SDL_INIT_VIDEO)!=0){printf("SDL error: %s\n",SDL_GetError());return 1;}
        ownsSdl = 1;
    }

    if(!TTF_WasInit()) {
        if(TTF_Init()!=0){printf("TTF error: %s\n",TTF_GetError());return 1;}
        ownsTtf = 1;
    }
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    ownsImg = 1;
    Mix_Init(MIX_INIT_MP3);
    ownsMixer = 1;
    {
        int freq = 0;
        Uint16 format = 0;
        int channels = 0;
        if(!Mix_QuerySpec(&freq, &format, &channels)) {
            if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 256)==0) ownsAudio = 1;
        }
    }
    Mix_AllocateChannels(16);
    Mix_ReserveChannels(RESERVED_GAMEPLAY_CHANNELS);

    if(standalone) {
        win=SDL_CreateWindow("Trap Adventure",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);
        ownsWindow = 1;
        ren=SDL_CreateRenderer(win,-1,
            SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        ownsRenderer = 1;
    } else {
        win = sharedWin;
        ren = sharedRen;
        if(!win || !ren) {
            fprintf(stderr, "Level 3 requires a shared SDL window and renderer.\n");
            return 1;
        }
        SDL_SetWindowTitle(win, "Home Alone - Level 3");
    }
    if(!win || !ren) {
        fprintf(stderr, "Level 3 setup failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    arcade_input_init();

    const char *fontPaths[]={
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",NULL
    };
    for(int i=0;fontPaths[i]&&!font;   i++) font   =TTF_OpenFont(fontPaths[i],22);
    for(int i=0;fontPaths[i]&&!bigFont;i++) bigFont=TTF_OpenFont(fontPaths[i],80);

    SDL_Surface *s;
    loadPlayerSkinAssets();
    s=IMG_Load("assets/saw.png");
    if(!s) s=IMG_Load("/tmp/saw.png");
    if(s){texSaw=SDL_CreateTextureFromSurface(ren,s);SDL_FreeSurface(s);
          SDL_SetTextureBlendMode(texSaw,SDL_BLENDMODE_BLEND);}
    s=IMG_Load("assets/feuu.png");
    if(!s) s=IMG_Load("assets/feu.png");
    if(s){
          int frameCount = FIRE_SHEET_COLS * FIRE_SHEET_ROWS;
          fireFrameW = s->w;
          fireFrameH = s->h;
          if(s->w > 0 && s->h > 0) {
              fireFrameW = s->w / FIRE_SHEET_COLS;
              fireFrameH = s->h / FIRE_SHEET_ROWS;
          }

          texFire=SDL_CreateTextureFromSurface(ren,s);
          fireFrame = 0;
          fireFrameCount = frameCount;
          fireLastTick = SDL_GetTicks();
          if(texFire) {
              SDL_SetTextureBlendMode(texFire,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/lazer1.png");
    if(s){
          lvl4LaserFrameCount = LVL4_LAZER_SHEET_COLS * LVL4_LAZER_SHEET_ROWS;
          lvl4LaserFrameW = s->w;
          lvl4LaserFrameH = s->h;
          if(s->w > 0 && s->h > 0) {
              lvl4LaserFrameW = s->w / LVL4_LAZER_SHEET_COLS;
              lvl4LaserFrameH = s->h / LVL4_LAZER_SHEET_ROWS;
          }

          texLvl4Laser=SDL_CreateTextureFromSurface(ren,s);
          lvl4LaserFrame = 0;
          lvl4LaserLastTick = SDL_GetTicks();
          if(texLvl4Laser) {
              SDL_SetTextureBlendMode(texLvl4Laser,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/microonde.png");
    if(s){
          texMicrowave=SDL_CreateTextureFromSurface(ren,s);
          if(texMicrowave) {
              SDL_SetTextureBlendMode(texMicrowave,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/car.png");
    if(s){
          texLvl6Car=SDL_CreateTextureFromSurface(ren,s);
          if(texLvl6Car) {
              SDL_SetTextureBlendMode(texLvl6Car,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/poele1.png");
    if(s){
          texPan=SDL_CreateTextureFromSurface(ren,s);
          if(texPan) {
              SDL_SetTextureBlendMode(texPan,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/fourchette.png");
    if(s){
          texForkRain=SDL_CreateTextureFromSurface(ren,s);
          if(texForkRain) {
              SDL_SetTextureBlendMode(texForkRain,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    s=IMG_Load("assets/ballon.png");
    if(s){
          texBalloon=SDL_CreateTextureFromSurface(ren,s);
          if(texBalloon) {
              SDL_SetTextureBlendMode(texBalloon,SDL_BLENDMODE_BLEND);
          }
          SDL_FreeSurface(s);}
    sndTouch = Mix_LoadWAV("assets/touche.wav");
    if(!sndTouch) sndTouch = Mix_LoadWAV("assets/touche.ogg");
    if(!sndTouch) sndTouch = Mix_LoadWAV("assets/touche.mp3");
    if(!sndTouch) sndTouchMusic = Mix_LoadMUS("assets/touche.mp3");
    sndBreak = Mix_LoadMUS("assets/casse.mp3");
    sndLaserFx = Mix_LoadWAV("assets/lazer.mp3");
    sndFireFx = Mix_LoadWAV("assets/feu.mp3");
    sndEarthFx = Mix_LoadWAV("assets/terre1.mp3");
    sndIceFx = Mix_LoadWAV("assets/glace.mp3");
    sndWallFx = Mix_LoadWAV("assets/mur.wav");
    sndTrapFx = Mix_LoadWAV("assets/piege.wav");
    sndStepFx = Mix_LoadWAV("assets/pas1.mp3");
    sndJumpFx = Mix_LoadWAV("assets/saut.wav");
    if(!sndWallFx) sndWallFx = Mix_LoadWAV("assets/mur.ogg");
    if(!sndWallFx) sndWallFx = Mix_LoadWAV("assets/mur.mp3");
    if(!sndWallFx) sndWallMusic = Mix_LoadMUS("assets/mur.mp3");
    sndRollFx = Mix_LoadWAV("assets/roule.wav");
    if(!sndRollFx) sndRollFx = Mix_LoadWAV("assets/roule.ogg");
    if(!sndRollFx) sndRollFx = Mix_LoadWAV("assets/roule.mp3");
    if(!sndRollFx) sndRollMusic = Mix_LoadMUS("assets/roule.mp3");
    sndPanFx = Mix_LoadWAV("assets/poele.wav");
    if(!sndPanFx) sndPanFx = Mix_LoadWAV("assets/poele.ogg");
    if(!sndPanFx) sndPanFx = Mix_LoadWAV("assets/poele.mp3");
    if(!sndPanFx) sndPanMusic = Mix_LoadMUS("assets/poele.mp3");
    sndForkFx = Mix_LoadWAV("assets/fourchette.wav");
    if(!sndForkFx) sndForkFx = Mix_LoadWAV("assets/fourchette.ogg");
    if(!sndForkFx) sndForkFx = Mix_LoadWAV("assets/fourchette1.mp3");
    if(!sndForkFx) sndForkMusic = Mix_LoadMUS("assets/fourchette1.mp3");
    sndIntroMusic = Mix_LoadMUS("assets/cinematique.mp3");
    sndLaser = Mix_LoadMUS("assets/lazer.mp3");
    sndFall = Mix_LoadMUS("/home/vboxuser/Downloads/Falling - sound effect (mp3cut.net).mp3");
    if(!sndBall) sndBall = Mix_LoadWAV("assets/ball.wav");
    if(!sndBall) sndBall = Mix_LoadWAV("assets/ball.mp3");
    if(!sndBall) sndBall = Mix_LoadWAV("assets/ball.ogg");
    if(!sndBall) sndBall = Mix_LoadWAV("assets/ball.png");
    s=IMG_Load(BACKGROUND_PATH);
    if(s){texBackground=SDL_CreateTextureFromSurface(ren,s);SDL_FreeSurface(s);}
    s=IMG_Load(BACKGROUND6_PATH);
    if(s){texBackground6=SDL_CreateTextureFromSurface(ren,s);SDL_FreeSurface(s);}
    loadLvl6MazeAssets();

    pauseMenuReady = options_scene_init(win, ren);
    if(pauseMenuReady) options_scene_set_audio_enabled(0);

    if(!standalone) {
        soloSkinIndex = 0;
        duoSkinIndex[0] = 0;
        duoSkinIndex[1] = 1;
        if(session) {
            soloSkinIndex = (session->player_skin_number[0] == 2) ? 1 : 0;
            duoSkinIndex[0] = (session->player_skin_number[0] == 2) ? 1 : 0;
            duoSkinIndex[1] = (session->player_skin_number[1] == 2) ? 1 : 0;
        }
        if(session && session->mode == GAME_MODE_DUO) startDuoGame();
        else startSoloGame();
    }

    int running=1;
    int userQuit=0;
    int skipLevelShortcutPresses = 0;
    Uint32 lastAutosaveTick = SDL_GetTicks();
    const Uint32 autosaveIntervalMs = 5000;
    SDL_Event ev;

    while(running) {
        int syncedPause = 0;

        arcade_input_begin_frame();
        online_client_pump();
        while(online_client_consume_pause_state_change(&syncedPause)) {
            if(!pauseMenuReady) continue;
            if(syncedPause) {
                if(!pauseMenuActive) {
                    pauseMenuActive = 1;
                    options_scene_enter();
                }
                online_client_send_pause_state(1);
            } else {
                if(pauseMenuActive) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                }
                online_client_send_pause_state(0);
            }
        }
        updateOnlineRemoteDuoActions();

        while(SDL_PollEvent(&ev)) {
            arcade_input_handle_event(&ev);
            if(ev.type==SDL_QUIT) {
                userQuit=1;
                if(activeSession) activeSession->quit_requested = 1;
                running=0;
                break;
            }
            if(pauseMenuActive) {
                OptionsSceneResult result = {0};
                options_scene_handle_event(&ev, &result);
                if(result.quit_to_menu) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    userQuit = 1;
                    if(activeSession) activeSession->quit_requested = 1;
                    running = 0;
                    break;
                } else if(result.return_to_main) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    online_client_send_pause_state(0);
                }
                continue;
            }
            if(ev.type==SDL_KEYDOWN) {
                SDL_Keycode k=ev.key.keysym.sym;
                SDL_Keymod mod = SDL_GetModState();
                if(gameMode == MODE_MENU) {
                    if(k==SDLK_ESCAPE) {
                        userQuit=1;
                        running=0;
                    }
                    else if(k==SDLK_1 || k==SDLK_KP_1 || k==SDLK_RETURN) startSoloGame();
                    else if(k==SDLK_2 || k==SDLK_KP_2) startDuoGame();
                    continue;
                }
                if(introActive) {
                    if(k==SDLK_ESCAPE) {
                        userQuit=1;
                        if(activeSession) activeSession->quit_requested = 1;
                        running=0;
                    }
                    if(k==SDLK_RETURN || k==SDLK_SPACE) skipIntroCinematic();
                    continue;
                }
                if(k==SDLK_ESCAPE && pauseMenuReady) {
                    pauseMenuActive = 1;
                    options_scene_enter();
                    online_client_send_pause_state(1);
                    continue;
                }
                if(k==SDLK_p && ev.key.repeat == 0 &&
                   (mod & KMOD_ALT) &&
                   (mod & KMOD_SHIFT) &&
                   !(mod & KMOD_CTRL)) {
                    SDL_Log("Level 3 dev shortcut: jump to final cutscene chase");
                    if(activeSession) activeSession->dev_jump_to_final_cutscene = 1;
                    level3HellCompleted = 1;
                    running = 0;
                    break;
                }
                if(k==SDLK_p && ev.key.repeat == 0 &&
                   (mod & KMOD_CTRL) &&
                   ((mod & KMOD_ALT) || (mod & KMOD_SHIFT))) {
                    SDL_Log("Level 3 dev shortcut: jump to final level");
                    if(activeSession) activeSession->dev_jump_to_final = 1;
                    level3HellCompleted = 1;
                    running = 0;
                    break;
                }
                if(k==SDLK_p && ev.key.repeat == 0 &&
                   !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT | KMOD_GUI)) &&
                   gameMode == MODE_SOLO &&
                   !isLevelTransitionActive()) {
                    if(lvl6MazeActive) {
                        lvl6MazeActive = 0;
                        stopLvl6MazeAudio();
                    }
                    if(currentLevel < MAX_LEVELS - 1) {
                        SDL_Log("Level 3 P shortcut: skip layer %d -> %d",
                                currentLevel + 1,
                                currentLevel + 2);
                        player.dead = 0;
                        player.won = 0;
                        startLevelTransition(currentLevel + 1);
                    } else {
                        SDL_Log("Level 3 P shortcut: finish final layer");
                        level3HellCompleted = 1;
                        running = 0;
                        break;
                    }
                    continue;
                }
                if(k==SDLK_a && ev.key.repeat == 0 && (mod & KMOD_SHIFT)) {
                    SDL_Log("Level 3 Shift+A shortcut: skip to next level");
                    level3HellCompleted = 1;
                    running = 0;
                    break;
                }
                if(k==SDLK_n && ev.key.repeat == 0 && !isLevelTransitionActive()) {
                    skipLevelShortcutPresses++;
                    SDL_Log("Level 3 skip shortcut: %d/3", skipLevelShortcutPresses);
                    if(skipLevelShortcutPresses >= 3) {
                        level3HellCompleted = 1;
                        running = 0;
                        break;
                    }
                    continue;
                }
                if(gameMode == MODE_DUO) {
                    if(activeSession &&
                       level3PlayerUsesLocalInput(0) &&
                       level3InteractKeyPressedForPlayer(k, 0)) {
                        duoInteractPressed[0] = 1;
                    } else if(activeSession &&
                              level3PlayerUsesLocalInput(1) &&
                              level3InteractKeyPressedForPlayer(k, 1)) {
                        duoInteractPressed[1] = 1;
                    } else if(!activeSession && (k==SDLK_RCTRL || k==SDLK_RGUI || k==SDLK_KP_ENTER)) {
                        duoInteractPressed[0] = 1;
                    } else if(!activeSession && k==SDLK_e) {
                        duoInteractPressed[1] = 1;
                    } else if(level3PlayerUsesLocalInput(0) && k==SDLK_RSHIFT) {
                        duoRadioPressed[0] = 1;
                    } else if(level3PlayerUsesLocalInput(1) && k==SDLK_q) {
                        duoRadioPressed[1] = 1;
                    }
                    continue;
                }
                if(lvl6MazeFadePhase != MAZE_FADE_NONE) {
                    continue;
                }
                if(lvl6MazeActive) {
                    if(k==SDLK_ESCAPE) {
                        lvl6MazeActive = 0;
                        stopLvl6MazeAudio();
                    } else if(k==SDLK_r && ev.key.repeat == 0) {
                        toggleLvl6MazeRadio();
                    }
                    continue;
                }
                if(k==SDLK_d && (mod & KMOD_CTRL)) {
                    godMode = !godMode;
                    if(godMode) player.dead = 0;
                }
                if(k == SDLK_f && ev.key.repeat == 0 && lvl6CarInteractionAvailable()) {
                    beginLvl6MazeTransition();
                }
            }
        }

        if(!running) break;

        if(pauseMenuActive) {
            options_scene_update(1.0f / 60.0f);
        } else if(introActive) {
            updateIntroCinematic();
        } else if(gameMode == MODE_DUO) {
            if(updateDuoGame()) {
                level3HellCompleted = 1;
                running = 0;
                continue;
            }
        } else if(lvl6MazeFadePhase != MAZE_FADE_NONE) {
            updateLvl6MazeTransition();
        } else if(isLevelTransitionActive()) {
            updateLevelTransition();
        } else if(lvl6MazeActive) {
            updateLvl6MazeMode();
            if(lvl6MazeCompleted) {
                level3HellCompleted = 1;
                running = 0;
                continue;
            }
        } else if(!player.dead&&!player.won) {
            if(currentLevel==0)      updateLvl1();
            else if(currentLevel==1) updateLvl2();
            else if(currentLevel==2) updateLvl3();
            else if(currentLevel==3) updateLvl4();
            else if(currentLevel==4) updateLvl5();
            else                     updateLvl6();
        } else if(player.dead && currentLevel==0 &&
                  (lvl1UtensilAnimationActive() || lvl1FallingPlatformActive())) {
            updateLvl1DeathSequence();
        } else if(player.dead && currentLevel==1 &&
                  (lvl2[2].falling ||
                   (deathType == DEATH_CRUSH &&
                    (slideSpike.falling || slideSpike.dustTimer > 0)))) {
            updateLvl2();
        } else if(player.dead && currentLevel==3 && lvl4[3].falling) {
            updateLvl4();
        } else if(player.dead && currentLevel==4 && deathType == DEATH_CRUSH &&
                  (lvl5WallTrap.falling || lvl5WallTrap.dustTimer > 0)) {
            updateLvl5();
        } else if(player.dead && deathType == DEATH_LASER) {
            updateLaserDeath();
        } else if(player.dead && deathType == DEATH_TOUCH) {
            tickTouchAnim();
        } else if(player.dead && deathType == DEATH_FIRE) {
            if(currentLevel == 3) updateFireAnim();
            tickBurnAnim();
            tickDustAnim();
        }

        if(!introActive && shouldAutoRestart()) {
            initPlayer();
            continue;
        }

        if(!introActive && player.won && currentLevel < MAX_LEVELS-1) {
            startLevelTransition(currentLevel + 1);
            player.won = 0;
            continue;
        }

        if(!introActive && player.won && currentLevel == MAX_LEVELS-1) {
            if(winStartTick == 0) winStartTick = SDL_GetTicks();
            if((SDL_GetTicks() - winStartTick) >= AUTO_RESTART_DELAY_MS) {
                level3HellCompleted = 1;
                running = 0;
                continue;
            }
        }

        if(gameMode == MODE_SOLO && !lvl6MazeActive) {
            updateBall3Sound();
            updateLvl4RollSound();
            updateStepSound();
        }

        if(activeSession && activeSession->save_enabled && running && !pauseMenuActive) {
            Uint32 autosaveNow = SDL_GetTicks();
            if(autosaveNow - lastAutosaveTick >= autosaveIntervalMs) {
                int p1Lives = 0;
                int p2Lives = 0;
                level3CarryForwardLives(activeSession, &p1Lives, &p2Lives);
                activeSession->level3.completed = 0;
                session_autosave_progress(activeSession, 3, p1Lives, p2Lives);
                lastAutosaveTick = autosaveNow;
            }
        }

        if(gameMode == MODE_MENU) {
            renderStartMenu();
        } else if(introActive) {
            renderIntroCinematic();
        } else if(gameMode == MODE_DUO) {
            renderDuoGame();
        } else if(lvl6MazeActive) {
            renderLvl6MazeMode();
        } else {
            renderSoloGame();
        }
        renderLvl6MazeTransitionFade();
        if(pauseMenuActive) {
            online_client_submit_frame(ren, 3);
            options_scene_render();
        } else {
            options_scene_render_global_brightness_overlay(ren);
            online_client_submit_frame(ren, 3);
        }
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    if(userQuit && activeSession) activeSession->quit_requested = 1;
    if(activeSession) {
        activeSession->level3.completed =
            (!activeSession->quit_requested && level3HellCompleted) ? 1 : 0;
        activeSession->level3.points = 0;
        session_calculate_total_points(activeSession);
    }
    if(pauseMenuReady) {
        if(pauseMenuActive) options_scene_leave();
        options_scene_cleanup();
        pauseMenuReady = 0;
        pauseMenuActive = 0;
    }
    arcade_input_shutdown();

    for(int i=0;i<PLAYER_SKIN_COUNT;i++) {
        if(playerSkins[i].idle)       SDL_DestroyTexture(playerSkins[i].idle);
        if(playerSkins[i].run)        SDL_DestroyTexture(playerSkins[i].run);
        if(playerSkins[i].jump)       SDL_DestroyTexture(playerSkins[i].jump);
        if(playerSkins[i].electrise)  SDL_DestroyTexture(playerSkins[i].electrise);
        if(playerSkins[i].burn)       SDL_DestroyTexture(playerSkins[i].burn);
        if(playerSkins[i].touch)      SDL_DestroyTexture(playerSkins[i].touch);
        if(playerSkins[i].dust)       SDL_DestroyTexture(playerSkins[i].dust);
        if(playerSkins[i].ice)        SDL_DestroyTexture(playerSkins[i].ice);
    }
    if(texSaw)        SDL_DestroyTexture(texSaw);
    if(texFire)       SDL_DestroyTexture(texFire);
    if(texLvl4Laser)  SDL_DestroyTexture(texLvl4Laser);
    if(texMicrowave)  SDL_DestroyTexture(texMicrowave);
    if(texLvl6Car)    SDL_DestroyTexture(texLvl6Car);
    if(texPan)        SDL_DestroyTexture(texPan);
    if(texForkRain)   SDL_DestroyTexture(texForkRain);
    if(texBalloon)    SDL_DestroyTexture(texBalloon);
    freeLvl6MazeAssets();
    freeIntroCinematic();
    if(soloFrame)     SDL_DestroyTexture(soloFrame);
    if(duoFrames[0])  SDL_DestroyTexture(duoFrames[0]);
    if(duoFrames[1])  SDL_DestroyTexture(duoFrames[1]);
    if(texBackground) SDL_DestroyTexture(texBackground);
    if(texBackground6) SDL_DestroyTexture(texBackground6);
    if(sndTouch)      Mix_FreeChunk(sndTouch);
    if(sndTouchMusic) Mix_FreeMusic(sndTouchMusic);
    if(sndBreak)      Mix_FreeMusic(sndBreak);
    if(sndLaser)      Mix_FreeMusic(sndLaser);
    if(sndFall)       Mix_FreeMusic(sndFall);
    if(sndIntroMusic) Mix_FreeMusic(sndIntroMusic);
    if(sndLaserFx)    Mix_FreeChunk(sndLaserFx);
    if(sndFireFx)     Mix_FreeChunk(sndFireFx);
    if(sndEarthFx)    Mix_FreeChunk(sndEarthFx);
    if(sndIceFx)      Mix_FreeChunk(sndIceFx);
    if(sndWallFx)     Mix_FreeChunk(sndWallFx);
    if(sndTrapFx)     Mix_FreeChunk(sndTrapFx);
    if(sndStepFx)     Mix_FreeChunk(sndStepFx);
    if(sndJumpFx)     Mix_FreeChunk(sndJumpFx);
    if(sndWallMusic)  Mix_FreeMusic(sndWallMusic);
    if(sndRollFx)     Mix_FreeChunk(sndRollFx);
    if(sndRollMusic)  Mix_FreeMusic(sndRollMusic);
    if(sndPanFx)      Mix_FreeChunk(sndPanFx);
    if(sndPanMusic)   Mix_FreeMusic(sndPanMusic);
    if(sndForkFx)     Mix_FreeChunk(sndForkFx);
    if(sndForkMusic)  Mix_FreeMusic(sndForkMusic);
    if(sndBall)       Mix_FreeChunk(sndBall);
    if(font)    TTF_CloseFont(font);
    if(bigFont) TTF_CloseFont(bigFont);
    if(ownsRenderer && ren) SDL_DestroyRenderer(ren);
    if(ownsWindow && win) SDL_DestroyWindow(win);
    if(ownsAudio) Mix_CloseAudio();
    if(ownsMixer) Mix_Quit();
    if(ownsTtf) TTF_Quit();
    if(ownsImg) IMG_Quit();
    if(ownsSdl) SDL_Quit();
    activeSession = NULL;
    win = NULL;
    ren = NULL;
    return 0;
}

int runLevel3(GameSession *session, SDL_Window *window, SDL_Renderer *renderer)
{
    return runLevel3Hell(session, window, renderer, 0);
}

#ifndef MERGED_BUILD
int main(void)
{
    return runLevel3Hell(NULL, NULL, NULL, 1);
}
#endif
