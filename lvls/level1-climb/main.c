#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#include "../shared/arcade_input.h"
#include "../shared/session.h"
#include "../../src/options/options_scene.h"
#include "online_client.h"
#include "../newshoplvl1/newshop.h"
/* PERF_MARKER_LEVEL1 */

/* ═══════════════════════════════════════════════════════════
   CONSTANTS
═══════════════════════════════════════════════════════════ */
#define SCREEN_W        1280
#define SCREEN_H        720
#define TARGET_FPS      30
#define LOGIC_TICK_HZ   30
#define FIXED_MAX_CATCHUP_STEPS 4
#define INPUT_BUFFER_MS 110u
#define LEVEL1_ONLINE_REMOTE_WINDOW_ID 0xFFFFFFFFu
#define FLOOR_GAP       160
#define FLOOR_H         16
#define GAP_W           90
#define MAX_FLOORS      512
#define SCROLL_SPEED    1.0f
#define CLEANUP_MARGIN_Y 900.0f
#define UPDATE_PAD_Y     260.0f
#define RENDER_PAD_Y     120.0f

#define PLAYER_W        50
#define PLAYER_H        72
#define PLAYER_SPEED    6.2f
#define GRAVITY_RISE    0.78f
#define GRAVITY_FALL    1.18f
#define JUMP_VY        -18.5f
#define MAX_FALL        26.0f
#define MAX_LIVES       9

/* Keys */
#define KEY_W           48
#define KEY_H           48
#define KEY_COLLECT_R   32.0f   /* normal pickup radius  */
#define MAGNET_R        90.0f   /* magnet pull radius    */
#define MAGNET_SPD      4.0f
#define MAX_KEYS        64

/* House / shop entry */
#define HOUSE_W         160
#define HOUSE_H         130
#define DOOR_W          22
#define DOOR_H          32
#define HOUSE_PROB      7       /* 1-in-N chance per floor */

/* Jetpack / shoes / magnet params */
#define JETPACK_VY         -3.2f
#define JETPACK_FLOORS      5
#define SHOES_DURATION_MS   15000
#define MAGNET_DURATION_MS  10000

/* Final floor needs this many keys to count as WIN */
#define KEYS_TO_WIN    10

/* Background colour */
#define BG 18,18,32,255

/* ── Parallax background ── */
#define BG_LAYER_W       SCREEN_W
#define BG_LAYER_H_FAR   2160    /* 3× screen height = 1 tile  */
#define BG_LAYER_H_MID   1440
#define BG_SPEED_FAR     0.05f
#define BG_SPEED_MID     0.20f
/* Zone trigger: transition fires after far bg has tiled 3 times   */
/* Each tile = BG_LAYER_H_FAR world-pixels at BG_SPEED_FAR scroll  */
/* cameraY at which sewer→street triggers: 3 * 2160 / 0.05 = very large
   but we track tile-count directly instead (see bgTileCount).     */
/* Transition system */
#define TRANS_OBJ_MAX    200     /* wall-to-wall debris coverage    */
#define TRANS_DURATION_MS 2800   /* total transition time ms        */
/* ← Tweak this: world-px of camera travel before transition fires.
   160px = 1 floor. Default 3200 = ~20 floors (~53s at base speed). */
#define TRANS_CAM_TRIGGER 3200.0f
/* Infinite floor generation */
#define FLOOR_GEN_LOOKAHEAD 800  /* generate floors this many px above camera */
#define MAX_FLOORS_INF   512

/* Shared level 1 type declarations. */
#include "level1_types.h"

/* ═══════════════════════════════════════════════════════════
   GLOBALS
═══════════════════════════════════════════════════════════ */
static SDL_Window   *win     = NULL;
static SDL_Renderer *ren     = NULL;
static TTF_Font     *font    = NULL;
static TTF_Font     *bigFont = NULL;
static GameSession  *activeSession = NULL;
static int           ownsSDL = 1;
static float         fpsDisplay = 0.0f;
static Uint32        fpsWindowStart = 0;
static int           fpsFrameCounter = 0;
static Uint64        framePacerFreq = 0;
static char          fpsLabel[32] = "FPS 0.0";
static int           fpsLabelW = 0;
static int           fpsLabelH = 0;
static int           sharedRendererVSyncWasForced = 0;
static int           logicClockNeedsReset = 0;

#define TEXT_CACHE_MAX      256
#define TEXT_CACHE_STR_MAX  512
#define TEX_SIZE_CACHE_MAX   96

static TextCacheEntry        textCache[TEXT_CACHE_MAX];
static TextureSizeCacheEntry texSizeCache[TEX_SIZE_CACHE_MAX];
static int                   texSizeCacheNext = 0;
static Uint32                textCacheStamp   = 1;
static WrapCache             dlgWrapCache     = {0};
static InputSnapshot         inputSnap         = {0};
static ActionBuffer          actionBuffers[ACT_COUNT];
static int                   bufferedInteractPlayer = 0;

/* ── Background layer textures (NULL = use procedural fallback) ── */
static SDL_Texture *bgTex[3][2];  /* [zone 0-2][layer far/mid] */
static SDL_Texture *bgProc[3][2];

/* house zone = zone index 2 but shares street art until real art is added */

/* Wipe textures: transTex[0]=street.png  transTex[1]=basement.png */
static SDL_Texture *transTex[2];
/* Debris textures per zone */
static SDL_Texture *debrisTex[3];  /* [0]=sewer [1]=street [2]=basement */

/* ── Background zone ── */
static int   bgZone           = 0;     /* 0=sewer 1=street 2=house  */
static int   finishFloorIdx   = -1;    /* index of the win floor, set after zone 2 */
static float bgCamAtZoneStart = 0.0f;

/* ── Transition (wipe) state ── */
static Uint32 transStartMs        = 0;
static int    transNextZone       = 0;
static float  transFreezeX        = 0.0f;
static float  transFreezeY        = 0.0f;
static int    transPlayerReleased = 0;
static int    transPlayerHidden   = 0;  /* 1 = under wipe, skip drawing */

/* ── Respawn flicker ── */
static int    flickerActive = 0;
static Uint32 flickerStart  = 0;
static int    flickerActiveP2 = 0;
static Uint32 flickerStartP2  = 0;
#define FLICKER_DURATION_MS 2000   /* total flicker time after respawn */
#define FLICKER_PERIOD_MS    80    /* on/off period */

/* ── Sprite textures ── */
/* ── Platform textures [0]=sewer [1]=street [2]=house ── */
static SDL_Texture *texPlatform[3] = {NULL, NULL, NULL};

static SDL_Texture *texIdle[3] = {NULL, NULL, NULL};
static SDL_Texture *texRun[3]  = {NULL, NULL, NULL};
static SDL_Texture *texJump[3] = {NULL, NULL, NULL};
static SDL_Texture *texKeyAnim = NULL;

/* ── Rat (death line) ── */
static SDL_Texture *texRat = NULL;
#define RAT_W           72    /* drawn width  per rat  */
#define RAT_H           56    /* drawn height per rat  */
#define RAT_COUNT       200   /* number of rats in the river */
#define RAT_ANIM_FPS    24

static Rat rats[RAT_COUNT];
static int ratsInited = 0;

/* ── Hurt animation ── */
static SDL_Texture *texHurt = NULL;

/* ── Dog enemy ── */
static SDL_Texture *texDog = NULL;
static SDL_Texture *texPortrait[3] = {NULL, NULL, NULL};
#define DOG_W           40
#define DOG_H           50
#define DOG_SPEED       3.8f
#define DOG_ANIM_FPS    14
#define MAX_DOGS        64
#define DOG_SPAWN_CHANCE 5   /* 1-in-5 = 20% per floor */
#define DOG_HIT_COOLDOWN_MS 1000  /* invincibility after being hit */
#define DOG_BARK_NEAR_INTERVAL_MS 1200

static Dog   dogs[MAX_DOGS];
static int   dogCount = 0;
static Uint32 dogHitTime = 0;    /* last time player was hit by a dog */
static Uint32 dogHitTimeP2 = 0;  /* last time player 2 was hit by a dog */
static Uint32 dogBarkNearTime = 0;

/* ── Sewer gauge + water flow ── */
static SDL_Texture *texWater = NULL;
static SDL_Texture *texGauge = NULL;
#define MAX_GAUGES         32
#define GAUGE_SPAWN_CHANCE 3     /* 1-in-3 = 33% — bumped for zone 2 visibility */
#define GAUGE_COOLDOWN_MS  3000
#define GAUGE_FLOW_MS      1000
#define GAUGE_W            18
#define GAUGE_H            28    /* visual height of the pipe nozzle */
#define WATER_SPEED        4.5f  /* px/frame leftward */
#define WATER_TILE_W       32    /* each water tile drawn width  */
#define WATER_TILE_H       24    /* each water tile drawn height */
#define WATER_TILES_VERT   3     /* stack 3 tiles vertically     */
#define WATER_HIT_COOLDOWN 1200

#define DROP_MAX 80   /* droplets per gauge */

static Gauge  gauges[MAX_GAUGES];
static int    gaugeCount = 0;
static Uint32 waterHitTime = 0;
static Uint32 waterHitTimeP2 = 0;
#define HURT_ANIM_FPS  12

/* ── Debris / falling danger system ── */
#define DEBRIS_PIECE_MAX   10          /* fewer pieces keeps drops snappy */
#define DEBRIS_COOLDOWN_MS 9000        /* ms between events            */
#define DEBRIS_WARN_MS      550        /* shorter warning before drop  */
#define DEBRIS_SPEED       22.0f       /* faster drop so screen clears sooner */
#define DEBRIS_PIECE_W     (SCREEN_W/4 - 8)
#define DEBRIS_PIECE_H     80          /* each piece height            */
#define HURT_LOCK_MS       1000        /* player loses control 1s      */
#define HURT_PUSH_VX       18.0f       /* horizontal throw speed       */
#define HURT_PUSH_VY       0.0f        /* no upward kick — flat slide  */

static DebrisPhase debrisPhase    = DEB_IDLE;
static Uint32      debrisTimer    = 0;        /* phase start timestamp   */
static int         debrisQuarter  = 0;        /* 0-3 which screen quarter*/
static DebrisPiece debrisPieces[DEBRIS_PIECE_MAX];
static int         debrisPieceCount = 0;
static int         playerHurt     = 0;        /* 1 = locked + hurt anim  */
static Uint32      hurtStart      = 0;
static int         hurtAnimFrame  = 0;
static Uint32      hurtAnimTick   = 0;
static int         playerHurtP2   = 0;
static Uint32      hurtStartP2    = 0;
/* screen shake */
static float       shakeAmt       = 0.0f;    /* current shake intensity  */
static int         shakeOX        = 0;
static int         shakeOY        = 0;
static int         playerHitThisEvent = 0;   /* avoid multi-hit per drop */

/* Animation state */
#define ANIM_COLS  6
#define ANIM_ROWS  6
#define ANIM_FPS   10          /* frames per second */
#define KEY_ANIM_FPS 6
static int    animFrame    = 0;
static Uint32 animLastTick = 0;
static int    keyAnimFrame    = 0;
static Uint32 keyAnimLastTick = 0;
static PlayerAnim animState = PANIM_IDLE;
/* shop anim state */
static PlayerAnim shopAnimState = PANIM_IDLE;
static int    shopAnimFrame    = 0;
static Uint32 shopAnimLastTick = 0;

static Floor   floors[MAX_FLOORS];
static int     floorCount = 0;
static int     totalFloorsGenerated = 0;
static int     lastHouseSpawnSerial = -99;

static Key     keys[MAX_KEYS];
static int     keyCount  = 0;
static House   house;
static Player  player;
static Player  player2;
static int     player2Enabled = 0;

static float     cameraY          = 0;
static float     deathLineY       = 0;
static int       lives            = MAX_LIVES;
static int       livesP2          = MAX_LIVES;
static int       keysHeld         = 0;
static int       keysCollectedLifetime = 0;
static int       score            = 0;
static GameState gameState        = GS_INTRO_FADE_IN;
static float     fadeAlpha        = 0;
static float     shopTransitionFadeAlpha = 0.0f;
static const char *customDeathMsg = NULL;

static int level1DuoEnabled(void);

static int level1PlayerHasHeartsRemaining(int playerIndex)
{
    if (playerIndex == 1)
        return (livesP2 > 0 || player2.extraHearts > 0);
    return (lives > 0 || player.extraHearts > 0);
}

static int level1ShouldUseDuoDeathRules(void)
{
    return level1DuoEnabled() && player2Enabled;
}

static void level1EnterGameOver(const char *deathMsg)
{
    if (deathMsg) customDeathMsg = deathMsg;
    gameState = GS_GAME_OVER;
    fadeAlpha = 0;
}

static int level1ApplyDamage(int playerIndex, int instantKill, const char *deathMsg)
{
    int *targetLives = (playerIndex == 1) ? &livesP2 : &lives;
    int *targetExtraHearts = (playerIndex == 1) ? &player2.extraHearts : &player.extraHearts;

    if (instantKill) {
        *targetLives = 0;
        *targetExtraHearts = 0;
    } else if (*targetExtraHearts > 0) {
        (*targetExtraHearts)--;
    } else {
        (*targetLives)--;
    }

    if (*targetLives < 0) *targetLives = 0;

    if (*targetLives <= 0 && *targetExtraHearts <= 0) {
        *targetLives = 0;
        if (!level1ShouldUseDuoDeathRules() || !level1PlayerHasHeartsRemaining(1 - playerIndex))
            level1EnterGameOver(deathMsg);
        return 0;
    }

    return 1;
}

static void runShopTransitionFadeOut(void);

/* ── Intro sequence ── */
static float  introWhite      = 0.0f;   /* 0..255 white overlay alpha   */
static float  introTitleAlpha = 0.0f;   /* 0..255 title text alpha      */
static Uint32 introTimer      = 0;      /* phase start timestamp        */
static Uint32 levelStartTime  = 0;
static Uint32 timerPauseStart = 0;
static Uint32 totalPausedMs   = 0;
static int    levelTimerStarted = 0;
static int    levelTimerPaused  = 0;
static int    level1ResultReady = 0;
#define INTRO_FADE_IN_MS    300
#define INTRO_TITLE_IN_MS   600
#define INTRO_TITLE_HOLD_MS 1000
#define INTRO_FADE_OUT_MS   800
#define SHOP_TRANSITION_MS  260u

/* ── Rat slide-in (played after last dialogue line) ── */
static int   ratsVisible     = 0;       /* 0 = hidden, 1 = sliding/visible */
static float ratSlideY       = 0.0f;    /* extra screen-Y offset (0=normal, >0=below) */
#define RAT_SLIDE_SPEED  3.0f           /* px per frame sliding upward  */

/* ── Dialogue system ── */
#define DLGBOX_X        60
#define DLGBOX_Y        (SCREEN_H - 200)
#define DLGBOX_W        (SCREEN_W - 120)
#define DLGBOX_H        180
#define DLGBOX_PORTRAIT_SIZE 80
#define DLGBOX_TEXT_X   (DLGBOX_X + DLGBOX_PORTRAIT_SIZE + 24)
#define DLGBOX_TEXT_Y   (DLGBOX_Y + 44)
#define DLGBOX_TEXT_W   (DLGBOX_W - DLGBOX_PORTRAIT_SIZE - 36)
#define DLGBOX_CHARS_PER_SEC 40         /* base typewriter speed (chars/sec) */

/* Per-character delay multipliers for dramatic effect.
   Returns ms to wait AFTER revealing this character.   */
static Uint32 dlgCharDelay(const char *text, int idx)
{
    char c  = text[idx];
    char c1 = (idx + 1 < (int)strlen(text)) ? text[idx + 1] : '\0';
    char c2 = (idx + 2 < (int)strlen(text)) ? text[idx + 2] : '\0';

    /* Hard pause: end of sentence */
    if (c == '.' && c1 == '.' && c2 == '.') return 300;  /* ellipsis start */
    if (c == '!' && c1 == '!')              return 120;   /* !! each beat */
    if (c == '?' && c1 == '?')              return 110;   /* ?? each beat */
    if (c == '.' || c == '!' || c == '?')   return 280;   /* sentence end */
    if (c == ',')                            return 160;   /* comma breath */
    if (c == ' ')                            return 30;    /* word gap */

    /* Normal character — base speed */
    return (Uint32)(1000 / DLGBOX_CHARS_PER_SEC);  /* ~25ms */
}

static const DlgLine dlgLines[] = {
    { CHAR_HARRY, "Marph, I forgot to leave the door open, we're stuck here!!" },
    { CHAR_MARV,  "You fucking retard how are we going to get out of here" },
    { CHAR_HARRY, "Don't worry we can live down here, I shot a rat earlier so we can live off eating rats" },
    { CHAR_MARV,  "U SHOT A RAT???????? YOU FOOL YOU DOOMED US !!!" },
    { CHAR_HARRY, "Hold on are you hearing that? Holy fuck climb Marph climbbbbbbb !" },
};
#define DLG_LINE_COUNT 5

/* ── Dialogue sequence IDs ── */
/* Forward declaration — defined later, called from updateBgTileCounter */
static void startDialogueSeq(DlgSeqID seq, const DlgLine *lines, int count);
/* Forward declaration — defined later, called from GS_PLAYING update */
static void updateFridge(void);
static void updateMicrowave(void);

/* ── Sewer mid-level dialogue (choice + two branches) ── */
static const DlgLine dlgSewerPre[] = {
    { CHAR_HARRY, "Oh btw Merv, I think we forgot to water the plants before leaving the house... I feel bad about them" },
};
#define DLG_SEWER_PRE_COUNT 1

static const char *dlgChoiceLabels[2] = { "Ignore", "Crashout" };
#define DLG_CHOICE_COUNT 2

static const DlgLine dlgSewerIgnore[] = {
    { CHAR_MARV, "I'll pretend I didn't hear shit Harry, keep climbing we're almost out of the sewers" },
};
#define DLG_SEWER_IGNORE_COUNT 1

static const DlgLine dlgSewerCrashout[] = {
    { CHAR_MARV,  "NIGGA FUCK YO FLOWERS I'M COVERED IN POO AND YO ASS TALKING ABOUT FLOWERS" },
    { CHAR_HARRY, "Chillllllllllll Merv my bad I'm sorry, we're almost out of the sewers don't worry" },
};
#define DLG_SEWER_CRASHOUT_COUNT 2

/* ── Street zone fridge dialogue (non-freezing) ── */
static const DlgLine dlgStreetFridge[] = {
    { CHAR_HARRY, "Hey Marv, I think that kid is preparing to drop something on us" },
    { CHAR_MARV,  "Nah you're tripping, you just forgot your glasses, you can't see anything" },
    { CHAR_HARRY, "I swear I'm seeing him at the top holding something, watch the left side" },
    { CHAR_MARV,  "Whatever" },
};
#define DLG_STREET_COUNT 4

/* ── Post-fridge dialogue (non-freeze, auto) ── */
static const DlgLine dlgPostFridge[] = {
    { CHAR_MARV,  "WAS THAT A FUCKING FRIDGE??" },
    { CHAR_HARRY, "I told you Merv, he was dropping something" },
    { CHAR_MARV,  "I swear I'm killing that kid when I catch him" },
    { CHAR_HARRY, "We're almost at the basement, we will catch him" },
};
#define DLG_POSTFRIDGE_COUNT 4

/* ── Microwave zone dialogue (non-freeze, auto) ── */
static const DlgLine dlgMicrowave[] = {
    { CHAR_HARRY, "Ummmm Merv...." },
    { CHAR_MARV,  "WHAT NOW?" },
    { CHAR_HARRY, "THE NIGGA IS DROPPING MICROWAVES" },
};
#define DLG_MICROWAVE_COUNT 3

/* ── Post-microwave dialogue (non-freeze, auto) ── */
static const DlgLine dlgPostMicro[] = {
    { CHAR_MARV,  "I'm gonna break every bone in his body, trust me" },
    { CHAR_HARRY, "I swear I saw a hot dog in one of those microwaves bro" },
    { CHAR_MARV,  "Bruh..." },
    { CHAR_HARRY, "Hey Merv look, we're almost there, cmon!" },
};
#define DLG_POSTMICRO_COUNT 4

/* ── Final platform dialogue (freeze, choice) ── */
static const DlgLine dlgFinalPre[] = {
    { CHAR_MARV,  "*coughs* How the fuck did you get here before me??" },
    { CHAR_HARRY, "I became friends with one of the rats and he showed me a shortcut here" },
    { CHAR_MARV,  "NIGGA WHAT? THEY ALMOST ATE ME??" },
    { CHAR_HARRY, "But they're nice though :)" },
    { CHAR_MARV,  "ALL OF THIS BCZ I GAVE YOU A GUN YOU DUMB FUCK, YOU KNOW WHAT..." },
};
#define DLG_FINALPRE_COUNT 5

static const char *dlgFinalChoiceLabels[2] = { "Take Harry's gun", "Forget about it" };

/* Branch A: Take gun */
static const DlgLine dlgFinalTakeGun[] = {
    { CHAR_MARV,  "Give me that gun before I smack yo ass" },
    { CHAR_MARV,  "How am I going to defend myself now???" },
    { CHAR_HARRY, "You're so stupid, nobody would want to hurt yo ass" },
    { CHAR_HARRY, "Whatever... let's go now, this door takes us straight into the house" },
};
#define DLG_FINAL_TAKEGUN_COUNT 4

/* Branch B: Forget it */
static const DlgLine dlgFinalForget[] = {
    { CHAR_MARV,  "Man fuck you, you almost got us killed" },
    { CHAR_HARRY, "You should've seen the rat man, first time seeing a rat with down syndrome, I had to shoot it" },
    { CHAR_MARV,  "Bruh..." },
    { CHAR_HARRY, "Let's go now, this door takes us straight into the house" },
};
#define DLG_FINAL_FORGET_COUNT 4

/* ── Active dialogue state ── */
static DlgSeqID  dlgSeqActive    = DLGSEQ_INTRO;
static const DlgLine *dlgCurLines  = NULL;
static int            dlgCurCount  = 0;

static int   dlgLineIdx      = 0;
static int   dlgCharShown    = 0;
static Uint32 dlgNextCharMs  = 0;
static Uint32 dlgAutoAdvanceMs = 0;  /* non-zero = auto-advance at this time */
#define DLG_AUTO_ADVANCE_MS 2200     /* pause after line fully shown before auto-flip */
static int   dlgFullyShown   = 0;

/* Choice state */
static int   dlgShowChoice   = 0;
static int   dlgChoiceSel    = 0;
static int   dlgChoiceMade   = 0;

/* Dialogue freeze flag — STREET dialogue runs without freezing */
static int   dlgFreezeGame   = 1;       /* 0 = dialogue overlays gameplay */

/* Dialogue triggers */
static int   sewerDlgTriggered     = 0;
static int   streetDlgTriggered    = 0;
static int   microwaveDlgTriggered = 0;
static int   finalDlgTriggered     = 0;  /* 1 = final dialogue started */
/* Player walk-to-door intro animation on final platform */
static FinalWalkPhase finalWalkPhase = FWALK_NONE;
static float  finalWalkTargetX = 0.0f;  /* X to walk to (right of door) */
static int    finalDebrisStopped = 0;   /* 1 = debris frozen for finale */

/* ── Final floor NPC (Harry) ── */
/* Shop exterior icons per zone */
static SDL_Texture *texShopIcon[3] = {NULL, NULL, NULL};
/* Shoes animation textures */
static SDL_Texture *texShoesIdle[3] = {NULL, NULL, NULL};
static SDL_Texture *texShoesRun[3]  = {NULL, NULL, NULL};
static SDL_Texture *texShoesJump[3] = {NULL, NULL, NULL};
/* Jetpack animation */
static SDL_Texture *texJetpackAnim[3] = {NULL, NULL, NULL};
static int          harryAnimFrame = 0;
static Uint32       harryAnimTick  = 0;
static float        harryX         = 0.0f;   /* world X                 */
static float        harryWorldY    = 0.0f;   /* world Y (feet on floor) */
static float        harrySlideVx   = 0.0f;   /* sliding toward door     */
static float        harryAlpha     = 255.0f; /* for fade-into-door      */
static int          harryActive    = 0;      /* 1 = draw Harry on floor */

/* ── Final door ── */
static SDL_Texture *texDoorClosed  = NULL;
static SDL_Texture *texDoorOpen    = NULL;   /* static open             */
static SDL_Texture *texDoorOpening = NULL;   /* opening animation sheet */
static SDL_Texture *texDoorClosing = NULL;   /* closing animation sheet */
static DoorState    doorState      = DOOR_CLOSED;
static int          doorAnimFrame  = 0;
static Uint32       doorAnimTick   = 0;
#define FDOOR_W     80
#define FDOOR_H     112
#define FDOOR_COLS  6
#define FDOOR_ROWS  6
#define HARRY_COLS  6
#define HARRY_ROWS  6
#define DOOR_FPS    12
static float        doorWorldX     = 0.0f;   /* center of final floor   */
static float        doorWorldY     = 0.0f;
static int          doorSpawned    = 0;

/* ── Camera pan state ── */
static int   camPanActive  = 0;
static float camPanTargetY = 0.0f;
#define CAM_PAN_SPEED 3.0f

/* ── Player-enters-door state ── */
static int    playerEnteringDoor = 0;  /* 1 = fading player into door */
static float  playerDoorAlpha    = 255.0f;
static int    harryEnteredDoor   = 0;  /* 1 = harry already inside    */
static int    playerEnteredDoor  = 0;  /* 1 = player inside too       */

/* ── Consequence screen ── */
static float  conseqFade    = 0.0f;    /* 0=transparent 255=black     */
static int    conseqPhase   = 0;       /* 0=fadein 1=show 2=fadeout   */
static Uint32 conseqTimer   = 0;
static int    finalChoiceMade = 0;     /* 0=gun 1=forget              */
#define CONSEQ_FADE_MS  600
#define CONSEQ_HOLD_MS  2000

/* ── Fridge drop system ── */
static SDL_Texture *texFridge   = NULL;
static FridgePhase  fridgePhase = FRIDGE_IDLE;
static Uint32       fridgeTimer = 0;
static float        fridgeY     = -1500.0f; /* world-y of fridge top   */
static float        fridgeAlpha = 255.0f;
static int          fridgeDebrisPaused = 0; /* 1 = debris suppressed   */
static int          fridgeKillTriggered = 0;
#define FRIDGE_W        780         /* wider than half screen               */
#define FRIDGE_H        1800        /* taller asset render size             */
#define FRIDGE_X        0           /* left side of screen                  */
#define FRIDGE_SPEED    130.0f      /* faster drop keeps pacing moving      */
#define FRIDGE_SHAKE_MS 650         /* shorter shake before drop            */
#define FRIDGE_WAIT_MS  2200        /* shorter wait after street dialogue   */
#define FRIDGE_FADE_MS  600         /* fade-out after drop                  */

/* ── Microwave drop system ── */
#define MW_W           480        /* gigantic — ~1/3 screen wide     */
#define MW_H           360        /* gigantic — half screen tall      */
#define MW_SPEED       120.0f
#define MW_WARN_MS     1400       /* longer warning before each drop  */
#define MW_INTERVAL_MS  450       /* faster chain between drops       */
#define MW_FADE_MS     500
#define MW_DROPS_TRACKED 7        /* first 7: track player X          */
/* after 7: simultaneous pairs — handled by mwSimulStage         */
#define MW_TOTAL_DROPS   7        /* tracked drops before simul phase */

static SDL_Texture *texMicrowave    = NULL;
static MwPhase      mwPhase         = MW_IDLE;
static Uint32       mwTimer         = 0;
/* Single-drop state */
static float        mwY             = -400.0f;
static float        mwX             = 0.0f;
static float        mwAlpha         = 255.0f;
static int          mwDropsDone     = 0;        /* tracked drops done     */
static int          mwSequenceActive= 0;
static int          mwDebrisPaused  = 0;
static float        mwWarnX         = 0.0f;
/* Simultaneous-drop state (up to 2 at once) */
static int          mwSimulStage    = 0;  /* 0=tracked,1=LR,2=CC,done=3 */
static float        mwSimulX[2]     = {0,0};
static float        mwSimulY[2]     = {-400,-400};
static int          mwSimulCount    = 0;  /* 1 or 2 active              */

/* ── Invincibility cheat ── */
static int   cheatInvincible   = 0;   /* 1 = god mode active            */
static int   cheatEverShown    = 0;   /* 1 = HUD text shown at least once */

/* SOUND HOOK: add SDL_mixer chunk pointers here per character:
   static Mix_Chunk *dlgBlipHarry = NULL;
   static Mix_Chunk *dlgBlipMarv  = NULL; */
/* ── Level 1 SFX ── */
static Mix_Chunk *sfxJump[2]       = {NULL, NULL};  /* jump1, jump2 */
static Mix_Chunk *sfxDamage[4]     = {NULL, NULL, NULL, NULL};  /* taking_damage x4 */
static Mix_Chunk *sfxLand          = NULL;
static Mix_Chunk *sfxKeyPickup     = NULL;
static Mix_Chunk *sfxDlgLetter     = NULL;
static Mix_Chunk *sfxFallingItems  = NULL;
static Mix_Chunk *sfxApplianceFall = NULL;
static Mix_Chunk *sfxDoorOpen      = NULL;
static Mix_Chunk *sfxRats          = NULL;
static Mix_Chunk *sfxDogBark       = NULL;
static int        sfxRatsChannel = -1;
static int        sfxAudioOwned  = 0;
static int        sfxMixerOwned  = 0;

static void startRatsLoopSfx(void)
{
    if (!sfxRats) return;
    if (Mix_QuerySpec(NULL, NULL, NULL) == 0) return;

    if (sfxRatsChannel >= 0 && Mix_Playing(sfxRatsChannel)) {
        return;
    }

    if (sfxRatsChannel >= 0) {
        Mix_HaltChannel(sfxRatsChannel);
        sfxRatsChannel = -1;
    }
    sfxRatsChannel = Mix_PlayChannel(-1, sfxRats, -1);
}

static void stopRatsLoopSfx(void)
{
    if (sfxRatsChannel < 0) return;
    if (Mix_QuerySpec(NULL, NULL, NULL) != 0)
        Mix_HaltChannel(sfxRatsChannel);
    sfxRatsChannel = -1;
}

static void resumeWorldAudioAfterShop(void)
{
    if (Mix_QuerySpec(NULL, NULL, NULL) == 0) return;

    if (Mix_PausedMusic()) {
        Mix_ResumeMusic();
    }
    Mix_Resume(-1);

    if (ratsVisible) {
        startRatsLoopSfx();
    }
}

static Uint32    scTimer          = 0;  /* second-chance timer */
static int       scRespawnTarget  = 0;  /* 0 = player1, 1 = player2 */

/* saved world state on shop entry */
static float savedCamY = 0, savedPX = 0, savedPY = 0;
static float savedPX2 = 0, savedPY2 = 0;

/* ── Magnet slide state ── */
static int    magnetSlideIdx  = -1;  /* which key is currently sliding */
static float  magnetSlideSpd  = 8.0f;

/* countdown after leaving shop */
static int    countdown     = 3;
static Uint32 countdownStart = 0;

/* ═══════════════════════════════════════════════════════════
   SDL INIT / CLOSE
═══════════════════════════════════════════════════════════ */
static int level1DuoEnabled(void)
{
    return activeSession && activeSession->mode == GAME_MODE_DUO;
}

static int level1ControlSchemeIsValid(ControlScheme scheme)
{
    return scheme == CONTROL_SCHEME_WASD ||
           scheme == CONTROL_SCHEME_ARROWS ||
           scheme == CONTROL_SCHEME_CONTROLLER;
}

static int level1ControlSchemesConflict(ControlScheme a, ControlScheme b)
{
    if (a == b) return 1;
    if ((a == CONTROL_SCHEME_ARROWS || a == CONTROL_SCHEME_CONTROLLER) &&
        (b == CONTROL_SCHEME_ARROWS || b == CONTROL_SCHEME_CONTROLLER)) {
        return 1;
    }
    return 0;
}

static ControlScheme level1AlternateLocalDuoScheme(ControlScheme player0Scheme)
{
    return player0Scheme == CONTROL_SCHEME_WASD
        ? CONTROL_SCHEME_ARROWS
        : CONTROL_SCHEME_WASD;
}

static ControlScheme level1ControlSchemeForPlayer(int playerIndex)
{
    ControlScheme scheme;

    if (playerIndex < 0 || playerIndex > 1) playerIndex = 0;

    if (activeSession) {
        scheme = activeSession->player_control_scheme[playerIndex];
        if (level1ControlSchemeIsValid(scheme)) {
            if (level1DuoEnabled() &&
                !online_client_is_connected() &&
                playerIndex == 1 &&
                level1ControlSchemeIsValid(activeSession->player_control_scheme[0]) &&
                level1ControlSchemesConflict(activeSession->player_control_scheme[0], scheme)) {
                return level1AlternateLocalDuoScheme(activeSession->player_control_scheme[0]);
            }
            return scheme;
        }
        if (playerIndex == 0 && level1ControlSchemeIsValid(activeSession->control_scheme)) {
            return activeSession->control_scheme;
        }
    }

    return playerIndex == 1 ? CONTROL_SCHEME_WASD : CONTROL_SCHEME_ARROWS;
}

static InteractBind level1PrimaryInteractBind(void)
{
    if (!activeSession) return INTERACT_BIND_F;
    if (activeSession->player_interact_bind[0] == INTERACT_BIND_0
        || activeSession->player_interact_bind[0] == INTERACT_BIND_F
        || activeSession->player_interact_bind[0] == INTERACT_BIND_E)
        return activeSession->player_interact_bind[0];
    return INTERACT_BIND_F;
}

static InteractBind level1SecondaryInteractBind(void)
{
    if (!activeSession) return INTERACT_BIND_F;
    if (activeSession->player_interact_bind[1] == INTERACT_BIND_0
        || activeSession->player_interact_bind[1] == INTERACT_BIND_F
        || activeSession->player_interact_bind[1] == INTERACT_BIND_E)
        return activeSession->player_interact_bind[1];
    return INTERACT_BIND_F;
}

static int keyMatchesInteractBind(SDL_Keycode key, InteractBind bind)
{
    switch (bind) {
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

static void level1FormatInteractPrompt(char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (level1DuoEnabled()) {
        snprintf(out, out_size, "%s/%s",
                 session_interact_bind_label(level1PrimaryInteractBind()),
                 session_interact_bind_label(level1SecondaryInteractBind()));
        return;
    }
    snprintf(out, out_size, "%s",
             session_interact_bind_label(level1PrimaryInteractBind()));
}

static int isLevel1InteractKey(SDL_Keycode key)
{
    if (!activeSession) return key == SDLK_f;
    if (keyMatchesInteractBind(key, level1PrimaryInteractBind())) return 1;
    if (level1DuoEnabled() && keyMatchesInteractBind(key, level1SecondaryInteractBind())) return 1;
    return 0;
}

static int level1InteractPlayerForKey(SDL_Keycode key)
{
    if (level1DuoEnabled() && keyMatchesInteractBind(key, level1SecondaryInteractBind()) &&
        !keyMatchesInteractBind(key, level1PrimaryInteractBind())) {
        return 1;
    }
    return 0;
}

static int selectedCharacterNumber(void)
{
    if (!activeSession) return 1;
    if (activeSession->player_skin_number[0] == 2) return 2;
    if (activeSession->player_skin_number[0] == 1) return 1;
    if (activeSession->skin_number == 2) return 2;
    return 1;
}

static int partnerCharacterNumber(void)
{
    if (!activeSession) return (selectedCharacterNumber() == 1) ? 2 : 1;
    if (!level1DuoEnabled()) return (selectedCharacterNumber() == 1) ? 2 : 1;
    if (activeSession->player_skin_number[1] == 2) return 2;
    if (activeSession->player_skin_number[1] == 1) return 1;
    return (selectedCharacterNumber() == 1) ? 2 : 1;
}

static int speakerCharacterNumber(DlgCharacter speaker)
{
    if (!level1DuoEnabled() && selectedCharacterNumber() == 2)
        return (speaker == CHAR_MARV) ? 2 : 1;
    return (speaker == CHAR_MARV) ? 1 : 2;
}

static const char *characterNameUpper(int characterNumber)
{
    return (characterNumber == 2) ? "HARRY" : "MARV";
}

static SDL_Texture *characterPortraitTexture(int characterNumber)
{
    if (characterNumber < 1 || characterNumber > 2) return NULL;
    return texPortrait[characterNumber];
}

static SDL_Texture *selectedIdleTexture(void)   { return texIdle[selectedCharacterNumber()]; }
static SDL_Texture *selectedRunTexture(void)    { return texRun[selectedCharacterNumber()]; }
static SDL_Texture *selectedJumpTexture(void)   { return texJump[selectedCharacterNumber()]; }
static SDL_Texture *selectedShoesIdleTexture(void) { return texShoesIdle[selectedCharacterNumber()]; }
static SDL_Texture *selectedShoesRunTexture(void)  { return texShoesRun[selectedCharacterNumber()]; }
static SDL_Texture *selectedShoesJumpTexture(void) { return texShoesJump[selectedCharacterNumber()]; }
static SDL_Texture *partnerIdleTexture(void)    { return texIdle[partnerCharacterNumber()]; }

static int level1SchemeLeftPressed(const Uint8 *kb,
                                   ControlScheme scheme,
                                   int useLocal,
                                   int useArcade,
                                   int useRemote)
{
    if (!kb) return 0;
    if (useRemote && online_client_remote_scancode_down(SDL_SCANCODE_A))
        return 1;
    if (!useLocal) return 0;
    if (scheme == CONTROL_SCHEME_WASD) {
        return kb[SDL_SCANCODE_A];
    }
    return kb[SDL_SCANCODE_LEFT] ||
           (useArcade && arcade_input_scancode_down(SDL_SCANCODE_LEFT));
}

static int level1SchemeRightPressed(const Uint8 *kb,
                                    ControlScheme scheme,
                                    int useLocal,
                                    int useArcade,
                                    int useRemote)
{
    if (!kb) return 0;
    if (useRemote && online_client_remote_scancode_down(SDL_SCANCODE_D))
        return 1;
    if (!useLocal) return 0;
    if (scheme == CONTROL_SCHEME_WASD) {
        return kb[SDL_SCANCODE_D];
    }
    return kb[SDL_SCANCODE_RIGHT] ||
           (useArcade && arcade_input_scancode_down(SDL_SCANCODE_RIGHT));
}

static int level1SchemeJumpPressed(const Uint8 *kb,
                                   ControlScheme scheme,
                                   int useLocal,
                                   int useArcade,
                                   int useRemote)
{
    if (!kb) return 0;
    if (useRemote &&
        (online_client_remote_scancode_down(SDL_SCANCODE_W) ||
         online_client_remote_scancode_down(SDL_SCANCODE_SPACE))) {
        return 1;
    }
    if (!useLocal) return 0;
    if (scheme == CONTROL_SCHEME_WASD) {
        return kb[SDL_SCANCODE_W] ||
               kb[SDL_SCANCODE_SPACE];
    }
    return kb[SDL_SCANCODE_UP] ||
           (useArcade && arcade_input_scancode_down(SDL_SCANCODE_UP)) ||
           (useArcade && arcade_input_scancode_down(SDL_SCANCODE_SPACE));
}

static int level1LeftPressed(const Uint8 *kb)
{
    return level1SchemeLeftPressed(kb, level1ControlSchemeForPlayer(0), 1, 1, 0);
}

static int level1RightPressed(const Uint8 *kb)
{
    return level1SchemeRightPressed(kb, level1ControlSchemeForPlayer(0), 1, 1, 0);
}

static int level1JumpPressed(const Uint8 *kb)
{
    return level1SchemeJumpPressed(kb, level1ControlSchemeForPlayer(0), 1, 1, 0);
}

static int level1LeftPressedP2(const Uint8 *kb)
{
    int remoteOnly = online_client_is_connected();
    return level1SchemeLeftPressed(kb, level1ControlSchemeForPlayer(1), !remoteOnly, 0, remoteOnly);
}

static int level1RightPressedP2(const Uint8 *kb)
{
    int remoteOnly = online_client_is_connected();
    return level1SchemeRightPressed(kb, level1ControlSchemeForPlayer(1), !remoteOnly, 0, remoteOnly);
}

static int level1JumpPressedP2(const Uint8 *kb)
{
    int remoteOnly = online_client_is_connected();
    return level1SchemeJumpPressed(kb, level1ControlSchemeForPlayer(1), !remoteOnly, 0, remoteOnly);
}

static int isLevel1JumpKeyForPlayer(SDL_Keycode key, int playerIndex)
{
    ControlScheme scheme = level1ControlSchemeForPlayer(playerIndex);
    if (scheme == CONTROL_SCHEME_WASD)
        return key == SDLK_w || key == SDLK_SPACE;
    return key == SDLK_UP;
}

static int isLevel1JumpKey(SDL_Keycode key)
{
    return isLevel1JumpKeyForPlayer(key, 0);
}

static int isLevel1RemoteJumpKey(SDL_Keycode key)
{
    return key == SDLK_w || key == SDLK_SPACE;
}

static int isConfirmKey(SDL_Keycode key)
{
    return key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_z;
}

static void queueBufferedAction(BufferedAction action, Uint32 now)
{
    if (action < 0 || action >= ACT_COUNT) return;
    actionBuffers[action].pending     = 1;
    actionBuffers[action].pressedAtMs = now;
}

static int consumeBufferedAction(BufferedAction action, Uint32 now)
{
    if (action < 0 || action >= ACT_COUNT) return 0;
    ActionBuffer *buf = &actionBuffers[action];
    if (!buf->pending) return 0;
    if (now - buf->pressedAtMs > INPUT_BUFFER_MS) {
        buf->pending = 0;
        return 0;
    }
    buf->pending = 0;
    return 1;
}

static void clearFrameInputEdges(void)
{
    inputSnap.jumpPressed     = 0;
    inputSnap.jumpReleased    = 0;
    inputSnap.jumpPressedP2   = 0;
    inputSnap.jumpReleasedP2  = 0;
    inputSnap.interactPressed = 0;
    inputSnap.interactReleased= 0;
    inputSnap.confirmPressed  = 0;
    inputSnap.confirmReleased = 0;
}

static void captureHeldInputSnapshot(void)
{
    const Uint8 *kb = SDL_GetKeyboardState(NULL);
    inputSnap.moveLeftHeld  = level1LeftPressed(kb);
    inputSnap.moveRightHeld = level1RightPressed(kb);
    inputSnap.moveJumpHeld  = level1JumpPressed(kb);
    inputSnap.moveLeftHeldP2  = level1DuoEnabled() ? level1LeftPressedP2(kb) : 0;
    inputSnap.moveRightHeldP2 = level1DuoEnabled() ? level1RightPressedP2(kb) : 0;
    inputSnap.moveJumpHeldP2  = level1DuoEnabled() ? level1JumpPressedP2(kb) : 0;
}

static void resetInputPipeline(void)
{
    memset(&inputSnap, 0, sizeof(inputSnap));
    memset(actionBuffers, 0, sizeof(actionBuffers));
}

static void startLevelTimer(void)
{
    if (levelTimerStarted) return;
    levelTimerStarted = 1;
    levelTimerPaused = 0;
    levelStartTime = SDL_GetTicks();
    timerPauseStart = 0;
    totalPausedMs = 0;
}

static void pauseLevelTimer(void)
{
    if (!levelTimerStarted || levelTimerPaused) return;
    levelTimerPaused = 1;
    timerPauseStart = SDL_GetTicks();
}

static void resumeLevelTimer(void)
{
    if (!levelTimerStarted || !levelTimerPaused) return;
    totalPausedMs += SDL_GetTicks() - timerPauseStart;
    timerPauseStart = 0;
    levelTimerPaused = 0;
}

static Uint32 shiftTick(Uint32 tick, Uint32 deltaMs)
{
    const Uint32 maxTick = (Uint32)~0u;

    if (tick == 0 || deltaMs == 0) return tick;
    if (tick > maxTick - deltaMs) return maxTick;
    return tick + deltaMs;
}

static void shiftTickRef(Uint32 *tick, Uint32 deltaMs)
{
    if (!tick || *tick == 0 || deltaMs == 0) return;
    *tick = shiftTick(*tick, deltaMs);
}

static void shiftWorldTimersAfterShop(Uint32 deltaMs)
{
    if (deltaMs == 0) return;

    shiftTickRef(&fpsWindowStart, deltaMs);
    shiftTickRef(&transStartMs, deltaMs);
    shiftTickRef(&flickerStart, deltaMs);
    shiftTickRef(&flickerStartP2, deltaMs);
    shiftTickRef(&dogHitTime, deltaMs);
    shiftTickRef(&dogHitTimeP2, deltaMs);
    shiftTickRef(&dogBarkNearTime, deltaMs);
    shiftTickRef(&waterHitTime, deltaMs);
    shiftTickRef(&waterHitTimeP2, deltaMs);
    shiftTickRef(&debrisTimer, deltaMs);
    shiftTickRef(&hurtStart, deltaMs);
    shiftTickRef(&hurtAnimTick, deltaMs);
    shiftTickRef(&hurtStartP2, deltaMs);
    shiftTickRef(&animLastTick, deltaMs);
    shiftTickRef(&keyAnimLastTick, deltaMs);
    shiftTickRef(&shopAnimLastTick, deltaMs);
    shiftTickRef(&introTimer, deltaMs);
    shiftTickRef(&dlgNextCharMs, deltaMs);
    shiftTickRef(&dlgAutoAdvanceMs, deltaMs);
    shiftTickRef(&harryAnimTick, deltaMs);
    shiftTickRef(&doorAnimTick, deltaMs);
    shiftTickRef(&conseqTimer, deltaMs);
    shiftTickRef(&fridgeTimer, deltaMs);
    shiftTickRef(&mwTimer, deltaMs);
    shiftTickRef(&scTimer, deltaMs);
    shiftTickRef(&countdownStart, deltaMs);

    shiftTickRef(&player.magnetTimer, deltaMs);
    shiftTickRef(&player.shoesTimer, deltaMs);
    shiftTickRef(&player2.magnetTimer, deltaMs);
    shiftTickRef(&player2.shoesTimer, deltaMs);

    for (int i = 0; i < dogCount; ++i) {
        if (!dogs[i].active) continue;
        shiftTickRef(&dogs[i].lastTick, deltaMs);
    }

    if (ratsInited) {
        for (int i = 0; i < RAT_COUNT; ++i) {
            shiftTickRef(&rats[i].lastTick, deltaMs);
        }
    }

    for (int i = 0; i < gaugeCount; ++i) {
        Gauge *g = &gauges[i];
        if (!g->active) continue;
        shiftTickRef(&g->phaseStart, deltaMs);
        shiftTickRef(&g->animTick, deltaMs);
        shiftTickRef(&g->lastSpawn, deltaMs);
    }
}

static int getLevelElapsedSeconds(Uint32 now)
{
    Uint32 pausedMs;

    if (!levelTimerStarted) return 0;

    pausedMs = totalPausedMs;
    if (levelTimerPaused && timerPauseStart > 0)
        pausedMs += now - timerPauseStart;

    if (now <= levelStartTime + pausedMs)
        return 0;

    return (int)((now - levelStartTime - pausedMs) / 1000u);
}

static void prepareLevel1Result(int completed)
{
    int currentHeight;

    if (!activeSession || level1ResultReady) return;

    currentHeight = (int)((float)(SCREEN_H - 80) - player.y);
    if (currentHeight > score) score = currentHeight;

    activeSession->level1.keys_collected = keysCollectedLifetime;
    activeSession->level1.keys_left = keysHeld;
    activeSession->level1.keys_spent = keysCollectedLifetime - keysHeld;
    if (activeSession->level1.keys_spent < 0) activeSession->level1.keys_spent = 0;
    activeSession->level1.height_reached = score;
    activeSession->level1.time_taken_sec = getLevelElapsedSeconds(SDL_GetTicks());
    activeSession->level1.lives_remaining = lives;
    activeSession->level1.completed = completed ? 1 : 0;
    session_calculate_level1_points(activeSession);
    session_save_level_life_carry(activeSession->level1.completed,
                                  activeSession->level1.lives_remaining,
                                  activeSession->bonus_lives_for_level2);
    session_calculate_total_points(activeSession);
    level1ResultReady = 1;
}

static void resetLevel1RunState(void)
{
    stopRatsLoopSfx();
    resetInputPipeline();

    cameraY = 0.0f;
    deathLineY = 0.0f;
    lives = MAX_LIVES;
    livesP2 = MAX_LIVES;
    keysHeld = 0;
    keysCollectedLifetime = 0;
    score = 0;
    gameState = GS_INTRO_FADE_IN;
    fadeAlpha = 0.0f;
    introWhite = 0.0f;
    introTitleAlpha = 0.0f;
    introTimer = 0;

    floorCount = 0;
    totalFloorsGenerated = 0;
    lastHouseSpawnSerial = -99;
    keyCount = 0;
    dogCount = 0;
    gaugeCount = 0;
    finishFloorIdx = -1;
    bgZone = 0;
    bgCamAtZoneStart = 0.0f;
    memset(floors, 0, sizeof(floors));
    memset(keys, 0, sizeof(keys));
    memset(dogs, 0, sizeof(dogs));
    memset(gauges, 0, sizeof(gauges));
    memset(debrisPieces, 0, sizeof(debrisPieces));
    memset(rats, 0, sizeof(rats));
    memset(&player, 0, sizeof(player));
    memset(&player2, 0, sizeof(player2));
    house.x = 0.0f;
    house.y = 0.0f;
    house.active = 0;
    house.floorIdx = 0;
    ratsInited = 0;
    player2Enabled = 0;

    animFrame = 0;
    animLastTick = 0;
    keyAnimFrame = 0;
    keyAnimLastTick = 0;
    animState = PANIM_IDLE;
    shopAnimState = PANIM_IDLE;
    shopAnimFrame = 0;
    shopAnimLastTick = 0;

    sewerDlgTriggered = 0;
    streetDlgTriggered = 0;
    microwaveDlgTriggered = 0;
    finalDlgTriggered = 0;
    dlgSeqActive = DLGSEQ_INTRO;
    dlgCurLines = NULL;
    dlgCurCount = 0;
    dlgLineIdx = 0;
    dlgCharShown = 0;
    dlgNextCharMs = 0;
    dlgAutoAdvanceMs = 0;
    dlgFullyShown = 0;
    dlgShowChoice = 0;
    dlgChoiceSel = 0;
    dlgChoiceMade = 0;
    dlgFreezeGame = 1;
    ratsVisible = 0;
    ratSlideY = 0.0f;

    finalWalkPhase = FWALK_NONE;
    finalWalkTargetX = 0.0f;
    finalDebrisStopped = 0;
    harryAnimFrame = 0;
    harryAnimTick = 0;
    harryX = 0.0f;
    harryWorldY = 0.0f;
    harrySlideVx = 0.0f;
    harryAlpha = 255.0f;
    harryActive = 0;
    doorState = DOOR_CLOSED;
    doorAnimFrame = 0;
    doorAnimTick = 0;
    doorWorldX = 0.0f;
    doorWorldY = 0.0f;
    doorSpawned = 0;
    camPanActive = 0;
    camPanTargetY = 0.0f;
    playerEnteringDoor = 0;
    playerDoorAlpha = 255.0f;
    harryEnteredDoor = 0;
    playerEnteredDoor = 0;
    conseqFade = 0.0f;
    conseqPhase = 0;
    conseqTimer = 0;
    finalChoiceMade = 0;

    fridgePhase = FRIDGE_IDLE;
    fridgeTimer = 0;
    fridgeY = -1500.0f;
    fridgeAlpha = 255.0f;
    fridgeDebrisPaused = 0;
    fridgeKillTriggered = 0;
    mwPhase = MW_IDLE;
    mwTimer = 0;
    mwY = -400.0f;
    mwX = 0.0f;
    mwAlpha = 255.0f;
    mwDropsDone = 0;
    mwSequenceActive = 0;
    mwDebrisPaused = 0;
    mwWarnX = 0.0f;
    mwSimulStage = 0;
    mwSimulX[0] = mwSimulX[1] = 0.0f;
    mwSimulY[0] = mwSimulY[1] = -400.0f;
    mwSimulCount = 0;

    debrisPhase = DEB_IDLE;
    debrisTimer = 0;
    debrisQuarter = 0;
    debrisPieceCount = 0;
    playerHurt = 0;
    hurtStart = 0;
    hurtAnimFrame = 0;
    hurtAnimTick = 0;
    playerHurtP2 = 0;
    hurtStartP2 = 0;
    shakeAmt = 0.0f;
    shakeOX = 0;
    shakeOY = 0;
    playerHitThisEvent = 0;
    dogHitTime = 0;
    dogHitTimeP2 = 0;
    dogBarkNearTime = 0;
    waterHitTime = 0;
    waterHitTimeP2 = 0;
    flickerActive = 0;
    flickerStart = 0;
    flickerActiveP2 = 0;
    flickerStartP2 = 0;
    transStartMs = 0;
    transNextZone = 0;
    transFreezeX = 0.0f;
    transFreezeY = 0.0f;
    transPlayerReleased = 0;
    transPlayerHidden = 0;

    cheatInvincible = 0;
    cheatEverShown = 0;
    customDeathMsg = NULL;
    scTimer = 0;
    scRespawnTarget = 0;
    savedCamY = 0.0f;
    savedPX = 0.0f;
    savedPY = 0.0f;
    magnetSlideIdx = -1;
    magnetSlideSpd = 8.0f;
    countdown = 3;
    countdownStart = 0;
}

#define LEVEL1_LOAD_STEPS 15

static void clearTextureSizeCache(void)
{
    memset(texSizeCache, 0, sizeof(texSizeCache));
    texSizeCacheNext = 0;
}

static void seedTextureSizeCache(SDL_Texture *tex)
{
    if (!tex) return;

    int w = 0, h = 0;
    if (SDL_QueryTexture(tex, NULL, NULL, &w, &h) != 0) return;

    for (int i = 0; i < TEX_SIZE_CACHE_MAX; i++) {
        if (texSizeCache[i].tex == tex) return;
        if (!texSizeCache[i].tex) {
            texSizeCache[i].tex = tex;
            texSizeCache[i].w = w;
            texSizeCache[i].h = h;
            return;
        }
    }

    int slot = texSizeCacheNext;
    texSizeCacheNext = (texSizeCacheNext + 1) % TEX_SIZE_CACHE_MAX;
    texSizeCache[slot].tex = tex;
    texSizeCache[slot].w = w;
    texSizeCache[slot].h = h;
}

static void prewarmTextureSizeCache(void)
{
    clearTextureSizeCache();

    for (int characterNumber = 1; characterNumber <= 2; characterNumber++) {
        seedTextureSizeCache(texPortrait[characterNumber]);
        seedTextureSizeCache(texIdle[characterNumber]);
        seedTextureSizeCache(texRun[characterNumber]);
        seedTextureSizeCache(texJump[characterNumber]);
        seedTextureSizeCache(texShoesIdle[characterNumber]);
        seedTextureSizeCache(texShoesRun[characterNumber]);
        seedTextureSizeCache(texShoesJump[characterNumber]);
        seedTextureSizeCache(texJetpackAnim[characterNumber]);
    }

    seedTextureSizeCache(texKeyAnim);
    seedTextureSizeCache(texHurt);
    seedTextureSizeCache(texDog);
    seedTextureSizeCache(texWater);
    seedTextureSizeCache(texGauge);
    seedTextureSizeCache(texFridge);
    seedTextureSizeCache(texMicrowave);
    seedTextureSizeCache(texDoorClosed);
    seedTextureSizeCache(texDoorOpen);
    seedTextureSizeCache(texDoorOpening);
    seedTextureSizeCache(texDoorClosing);
    seedTextureSizeCache(texRat);

    for (int i = 0; i < 3; i++) {
        seedTextureSizeCache(texShopIcon[i]);
        seedTextureSizeCache(texPlatform[i]);
        seedTextureSizeCache(debrisTex[i]);
    }

    for (int z = 0; z < 3; z++) {
        for (int l = 0; l < 2; l++) {
            seedTextureSizeCache(bgTex[z][l]);
            seedTextureSizeCache(bgProc[z][l]);
        }
    }

    for (int t2 = 0; t2 < 2; t2++)
        seedTextureSizeCache(transTex[t2]);
}

static int processLoadingEvents(void)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) {
            if (activeSession) activeSession->quit_requested = 1;
            return 0;
        }
    }
    return 1;
}

static void drawLoadingText(TTF_Font *f, const char *text, int y, SDL_Color color, int centered)
{
    if (!f || !text || !text[0]) return;

    SDL_Surface *sf = TTF_RenderUTF8_Blended(f, text, color);
    if (!sf) return;

    SDL_Texture *tx = SDL_CreateTextureFromSurface(ren, sf);
    if (tx) {
        SDL_Rect dst = {
            centered ? (SCREEN_W / 2 - sf->w / 2) : 0,
            y,
            sf->w,
            sf->h
        };
        SDL_RenderCopy(ren, tx, NULL, &dst);
        SDL_DestroyTexture(tx);
    }
    SDL_FreeSurface(sf);
}

static void renderLoadingScreen(const char *status, int step, int totalSteps)
{
    if (!ren) return;

    if (totalSteps < 1) totalSteps = 1;
    if (step < 0) step = 0;
    if (step > totalSteps) step = totalSteps;

    float progress = (float)step / (float)totalSteps;
    Uint32 ticks = SDL_GetTicks();

    SDL_SetRenderDrawColor(ren, 10, 14, 20, 255);
    SDL_RenderClear(ren);

    int bandX = (int)((ticks / 7u) % (SCREEN_W + 300)) - 300;
    SDL_SetRenderDrawColor(ren, 22, 30, 44, 255);
    for (int i = 0; i < 4; i++) {
        SDL_Rect band = {bandX + i * 240, 0, 120, SCREEN_H};
        SDL_RenderFillRect(ren, &band);
    }

    SDL_Rect panel = {SCREEN_W / 2 - 360, SCREEN_H / 2 - 110, 720, 220};
    SDL_SetRenderDrawColor(ren, 24, 34, 50, 220);
    SDL_RenderFillRect(ren, &panel);
    SDL_SetRenderDrawColor(ren, 95, 165, 235, 255);
    SDL_RenderDrawRect(ren, &panel);

    int barX = panel.x + 38;
    int barY = panel.y + 128;
    int barW = panel.w - 76;
    int barH = 26;
    SDL_Rect barBg = {barX, barY, barW, barH};
    SDL_SetRenderDrawColor(ren, 14, 18, 28, 255);
    SDL_RenderFillRect(ren, &barBg);
    SDL_SetRenderDrawColor(ren, 90, 120, 160, 255);
    SDL_RenderDrawRect(ren, &barBg);

    int innerW = (int)((float)(barW - 4) * progress);
    if (innerW > 0) {
        SDL_Rect fill = {barX + 2, barY + 2, innerW, barH - 4};
        SDL_SetRenderDrawColor(ren, 90, 170, 255, 255);
        SDL_RenderFillRect(ren, &fill);
    }

    char percent[24];
    snprintf(percent, sizeof(percent), "%d%%", (int)(progress * 100.0f + 0.5f));
    char statusLine[192];
    int dots = (int)((ticks / 240u) % 4u);
    snprintf(statusLine, sizeof(statusLine), "%s%.*s", status ? status : "Loading", dots, "...");

    SDL_Color titleColor = {240, 244, 250, 255};
    SDL_Color bodyColor = {195, 215, 240, 255};
    drawLoadingText(bigFont ? bigFont : font, "Preparing Level 1", panel.y + 24, titleColor, 1);
    drawLoadingText(font, statusLine, panel.y + 88, bodyColor, 1);
    drawLoadingText(font, percent, barY + 34, bodyColor, 1);

    SDL_RenderPresent(ren);
}

static int advanceLoadingScreen(int *step, int totalSteps, const char *status)
{
    if (!processLoadingEvents()) return 0;
    if (step && *step < totalSteps) (*step)++;
    renderLoadingScreen(status, step ? *step : 0, totalSteps);
    return 1;
}

static int initSDL(void)
{
    int loadStep = 0;

    srand((unsigned)time(NULL));
    if (ownsSDL) {
        if (SDL_Init(SDL_INIT_VIDEO) || TTF_Init()) return 0;
        IMG_Init(IMG_INIT_PNG);

        /* ── Temporary window/renderer needed to create bg textures ──
           We create them after the renderer is built (below).           */
        win = SDL_CreateWindow("Climb or Die",
              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
        ren = SDL_CreateRenderer(win, -1,
              SDL_RENDERER_ACCELERATED);
        if (!win || !ren) return 0;
    } else if (!win || !ren) {
        return 0;
    }

    arcade_input_init();

    renderLoadingScreen("Starting renderer", loadStep, LEVEL1_LOAD_STEPS);
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Configuring renderer")) return 0;

    sharedRendererVSyncWasForced = 0;
    if (!ownsSDL) {
        if (SDL_RenderSetVSync(ren, 0) == 0) {
            /* Level 1 uses explicit 60 FPS pacing, so shared renderer VSync is disabled temporarily. */
            sharedRendererVSyncWasForced = 1;
        } else {
            SDL_Log("WARNING: Level 1 couldn't disable shared renderer VSync: %s", SDL_GetError());
        }
    }

    sfxAudioOwned = 0;
    sfxMixerOwned = 0;
    {
        int mixFlags = Mix_Init(0);
        if ((mixFlags & MIX_INIT_MP3) == 0) {
            mixFlags = Mix_Init(MIX_INIT_MP3);
            if ((mixFlags & MIX_INIT_MP3) != 0) sfxMixerOwned = 1;
            else SDL_Log("WARNING: Level 1 MP3 mixer init failed: %s", Mix_GetError());
        }

        if (Mix_QuerySpec(NULL, NULL, NULL) == 0) {
            if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
                sfxAudioOwned = 1;
            } else {
                SDL_Log("WARNING: Level 1 Mix_OpenAudio failed: %s", Mix_GetError());
            }
        }
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Preparing audio")) return 0;

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    /* Try local font.ttf first, then common system paths */
    const char *fontPaths[] = {
        "font.ttf",
        "assets/font.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        NULL
    };
    for (int i = 0; fontPaths[i] && !font; i++)
        font = TTF_OpenFont(fontPaths[i], 18);
    for (int i = 0; fontPaths[i] && !bigFont; i++)
        bigFont = TTF_OpenFont(fontPaths[i], 52);
    if (!font)    printf("WARNING: no font loaded — text will be invisible\n");
    if (!bigFont) printf("WARNING: no bigFont loaded\n");
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading fonts")) return 0;

    /* Load sprite sheets */
    SDL_Surface *s;
    for (int characterNumber = 1; characterNumber <= 2; characterNumber++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/animations/skin%d_idle.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texIdle[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
                 if (texIdle[characterNumber]) SDL_SetTextureBlendMode(texIdle[characterNumber], SDL_BLENDMODE_BLEND); }
        snprintf(path, sizeof(path), "assets/animations/skin%d_run.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texRun[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
                 if (texRun[characterNumber]) SDL_SetTextureBlendMode(texRun[characterNumber], SDL_BLENDMODE_BLEND); }
        snprintf(path, sizeof(path), "assets/animations/skin%d_jump.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texJump[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
                 if (texJump[characterNumber]) SDL_SetTextureBlendMode(texJump[characterNumber], SDL_BLENDMODE_BLEND); }
    }
    s = IMG_Load("assets/animations/key_idle.png");
    if (s) { texKeyAnim = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texKeyAnim) SDL_SetTextureBlendMode(texKeyAnim, SDL_BLENDMODE_BLEND); }
    if (!texIdle[1] || !texRun[1] || !texJump[1]) printf("WARNING: character 1 climb sprites not fully loaded\n");
    if (!texIdle[2] || !texRun[2] || !texJump[2]) printf("WARNING: character 2 climb sprites not fully loaded\n");
    if (!texKeyAnim) printf("WARNING: key_idle.png not loaded\n");
    s = IMG_Load("assets/animations/skin_hurt.png");
    if (s) { texHurt = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texHurt) SDL_SetTextureBlendMode(texHurt, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/static/item_water.png");
    if (s) { texWater = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texWater) SDL_SetTextureBlendMode(texWater, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/static/ui_gauge.png");
    if (s) { texGauge = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texGauge) SDL_SetTextureBlendMode(texGauge, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/animations/dog_idle.png");
    if (s) { texDog = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texDog) SDL_SetTextureBlendMode(texDog, SDL_BLENDMODE_BLEND); }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading player and hazard textures")) return 0;

    /* Portrait textures for dialogue box */
    for (int characterNumber = 1; characterNumber <= 2; characterNumber++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/portraits/skin%d_portrait.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texPortrait[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
                 if (texPortrait[characterNumber]) SDL_SetTextureBlendMode(texPortrait[characterNumber], SDL_BLENDMODE_BLEND); }
    }

    /* Fridge drop texture */
    s = IMG_Load("assets/static/fridge.png");
    if (s) { texFridge = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texFridge) SDL_SetTextureBlendMode(texFridge, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/static/microwave.png");
    if (s) { texMicrowave = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texMicrowave) SDL_SetTextureBlendMode(texMicrowave, SDL_BLENDMODE_BLEND); }
    /* Final floor assets */
    s = IMG_Load("assets/static/door_closed.png");
    if (s) { texDoorClosed = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texDoorClosed) SDL_SetTextureBlendMode(texDoorClosed, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/static/door_open.png");
    if (s) { texDoorOpen = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texDoorOpen) SDL_SetTextureBlendMode(texDoorOpen, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/animations/door_open.png");
    if (s) { texDoorOpening = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texDoorOpening) SDL_SetTextureBlendMode(texDoorOpening, SDL_BLENDMODE_BLEND); }
    s = IMG_Load("assets/animations/door_close.png");
    if (s) { texDoorClosing = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texDoorClosing) SDL_SetTextureBlendMode(texDoorClosing, SDL_BLENDMODE_BLEND); }

    s = IMG_Load("assets/animations/rat_idle.png");
    if (s) { texRat = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s);
             if (texRat) SDL_SetTextureBlendMode(texRat, SDL_BLENDMODE_BLEND); }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading dialogue and event assets")) return 0;

    for (int characterNumber = 1; characterNumber <= 2; characterNumber++) {
        char path[128];
        snprintf(path, sizeof(path), "assets/animations/skin%d_idle_shoes.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texShoesIdle[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
        snprintf(path, sizeof(path), "assets/animations/skin%d_run_shoes.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texShoesRun[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
        snprintf(path, sizeof(path), "assets/animations/skin%d_jump_shoes.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texShoesJump[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
        snprintf(path, sizeof(path), "assets/animations/skin%d_jetpack.png", characterNumber);
        s = IMG_Load(path);
        if (s) { texJetpackAnim[characterNumber] = SDL_CreateTextureFromSurface(ren, s); SDL_FreeSurface(s); }
    }
    /* Shop exterior icons */
    {
        const char *shopIconPaths[3] = {
            "assets/static/shop_item_1.png",
            "assets/static/shop_item_2.png",
            "assets/static/shop_item_3.png"
        };
        for (int si = 0; si < 3; si++) {
            SDL_Surface *ss = IMG_Load(shopIconPaths[si]);
            if (ss) { texShopIcon[si] = SDL_CreateTextureFromSurface(ren, ss); SDL_FreeSurface(ss); }
        }
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading shop assets")) return 0;

    /* Platform textures */
    const char *platPaths[3] = {
        "assets/platform_sewer.png",
        "assets/platform_street.png",
        "assets/platform_house.png",
    };
    for (int pi = 0; pi < 3; pi++) {
        SDL_Surface *ps = IMG_Load(platPaths[pi]);
        if (ps) { texPlatform[pi] = SDL_CreateTextureFromSurface(ren, ps);
                  SDL_FreeSurface(ps); }
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading platform textures")) return 0;

    /* ── Try loading real bg art; fall back to procedural ── */
    const char *bgPaths[3][2] = {
        {"assets/bg/bg_sewer_far.png",  "assets/bg/bg_sewer_mid.png"},
        {"assets/bg/bg_street_far.png", "assets/bg/bg_street_mid.png"},
        {"assets/bg/bg_house_far.png",  "assets/bg/bg_house_mid.png"},
    };
    for (int z = 0; z < 3; z++)
        for (int l = 0; l < 2; l++) {
            SDL_Surface *bs = IMG_Load(bgPaths[z][l]);
            if (bs) { bgTex[z][l] = SDL_CreateTextureFromSurface(ren, bs);
                      if (bgTex[z][l]) SDL_SetTextureBlendMode(bgTex[z][l], SDL_BLENDMODE_BLEND);
                      SDL_FreeSurface(bs); }
            else bgTex[z][l] = NULL;
        }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading background art")) return 0;

    /* ── Wipe textures ── */
    const char *transPaths[2] = {
        "assets/bg/bg_street.png",    /* sewer → street  */
        "assets/bg/bg_basement.png",  /* street → basement */
    };
    for (int t2 = 0; t2 < 2; t2++) {
        SDL_Surface *ts = IMG_Load(transPaths[t2]);
        if (ts) { transTex[t2] = SDL_CreateTextureFromSurface(ren, ts);
                  if (transTex[t2]) SDL_SetTextureBlendMode(transTex[t2], SDL_BLENDMODE_BLEND);
                  SDL_FreeSurface(ts); }
        else transTex[t2] = NULL;
    }
    /* Debris zone textures */
    const char *debrisPaths[3] = {
        "assets/debris_1.png",
        "assets/debris_2.png",
        "assets/debris_3.png",
    };
    for (int d = 0; d < 3; d++) {
        SDL_Surface *ds = IMG_Load(debrisPaths[d]);
        if (ds) { debrisTex[d] = SDL_CreateTextureFromSurface(ren, ds);
                  if (debrisTex[d]) SDL_SetTextureBlendMode(debrisTex[d], SDL_BLENDMODE_BLEND);
                  SDL_FreeSurface(ds); }
        else debrisTex[d] = NULL;
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading transition assets")) return 0;

    /* ── Build procedural fallback textures ── */
    int layerH[2] = {BG_LAYER_H_FAR, BG_LAYER_H_MID};

    /* Zone 0 — Sewers */
    /* far: dark green-grey base + horizontal pipe silhouettes */
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, layerH[0]);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 22, 32, 24, 255);
        SDL_RenderClear(ren);
        /* horizontal pipes every 180px */
        for (int y = 60; y < layerH[0]; y += 180) {
            SDL_SetRenderDrawColor(ren, 40, 55, 40, 255);
            SDL_Rect pipe = {0, y, BG_LAYER_W, 22};
            SDL_RenderFillRect(ren, &pipe);
            SDL_SetRenderDrawColor(ren, 55, 75, 55, 255);
            SDL_Rect hi = {0, y, BG_LAYER_W, 3};
            SDL_RenderFillRect(ren, &hi);
        }
        bgProc[0][0] = t;
    }
    /* mid: brick wall */
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, layerH[1]);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        SDL_RenderClear(ren);
        int bw = 64, bh = 28;
        for (int row = 0; row * bh < layerH[1]; row++) {
            int offx = (row % 2) * (bw / 2);
            for (int col = -1; col * bw < BG_LAYER_W + bw; col++) {
                int x = col * bw + offx, y = row * bh;
                SDL_SetRenderDrawColor(ren, 55, 35, 28, 140);
                SDL_Rect br = {x+1, y+1, bw-2, bh-2};
                SDL_RenderFillRect(ren, &br);
                SDL_SetRenderDrawColor(ren, 38, 24, 18, 160);
                SDL_Rect mo = {x, y, bw, 1};
                SDL_RenderFillRect(ren, &mo);
            }
        }
        bgProc[0][1] = t;
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Building sewer background")) return 0;


    /* Zone 1 — Underground Street */
    /* far: concrete wall, hanging bare bulbs */
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, layerH[0]);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 38, 36, 34, 255);
        SDL_RenderClear(ren);
        /* concrete panel lines */
        SDL_SetRenderDrawColor(ren, 28, 26, 24, 255);
        for (int y = 0; y < layerH[0]; y += 120) {
            SDL_RenderDrawLine(ren, 0, y, BG_LAYER_W, y);
        }
        for (int x = 0; x < BG_LAYER_W; x += 200) {
            SDL_RenderDrawLine(ren, x, 0, x, layerH[0]);
        }
        /* bare bulbs */
        SDL_SetRenderDrawColor(ren, 200, 180, 80, 255);
        for (int x = 100; x < BG_LAYER_W; x += 280) {
            SDL_Rect bulb = {x-4, 10, 8, 12};
            SDL_RenderFillRect(ren, &bulb);
            SDL_SetRenderDrawColor(ren, 200, 180, 80, 40);
            SDL_Rect glow = {x-30, 0, 60, 50};
            SDL_RenderFillRect(ren, &glow);
            SDL_SetRenderDrawColor(ren, 200, 180, 80, 255);
        }
        bgProc[1][0] = t;
    }
    /* mid: exposed wiring + cracks */
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, layerH[1]);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        SDL_RenderClear(ren);
        /* wiring conduits */
        SDL_SetRenderDrawColor(ren, 80, 70, 40, 180);
        int wireYs[] = {90, 270, 480, 720, 960, 1200};
        for (int i = 0; i < 6; i++) {
            SDL_Rect wire = {0, wireYs[i], BG_LAYER_W, 5};
            SDL_RenderFillRect(ren, &wire);
        }
        /* crack lines */
        SDL_SetRenderDrawColor(ren, 20, 18, 16, 120);
        SDL_RenderDrawLine(ren, 300, 0,   340, 200);
        SDL_RenderDrawLine(ren, 340, 200, 310, 380);
        SDL_RenderDrawLine(ren, 900, 100, 860, 340);
        SDL_RenderDrawLine(ren, 860, 340, 920, 500);
        bgProc[1][1] = t;
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Building street background")) return 0;


    /* Zone 2 — House: warm wallpaper far + furniture mid */
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, BG_LAYER_H_FAR);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 72, 48, 38, 255);
        SDL_RenderClear(ren);
        for (int x = 0; x < BG_LAYER_W; x += 60) {
            SDL_SetRenderDrawColor(ren, 85, 58, 46, 255);
            SDL_Rect stripe = {x, 0, 28, BG_LAYER_H_FAR};
            SDL_RenderFillRect(ren, &stripe);
        }
        SDL_SetRenderDrawColor(ren, 120, 90, 60, 255);
        SDL_Rect rail = {0, BG_LAYER_H_FAR/2, BG_LAYER_W, 8};
        SDL_RenderFillRect(ren, &rail);
        bgProc[2][0] = t;
    }
    {
        SDL_Texture *t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                             SDL_TEXTUREACCESS_TARGET, BG_LAYER_W, BG_LAYER_H_MID);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetRenderTarget(ren, t);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
        SDL_RenderClear(ren);
        int frames[][4] = {{160,80,90,70},{500,60,110,80},{900,90,80,60},{1100,70,90,70}};
        for (int i = 0; i < 4; i++) {
            SDL_SetRenderDrawColor(ren, 100, 75, 50, 180);
            SDL_Rect fr = {frames[i][0], frames[i][1], frames[i][2], frames[i][3]};
            SDL_RenderFillRect(ren, &fr);
            SDL_SetRenderDrawColor(ren, 140, 110, 70, 200);
            SDL_RenderDrawRect(ren, &fr);
        }
        SDL_SetRenderDrawColor(ren, 55, 38, 25, 160);
        SDL_Rect sh1 = {30, 400, 120, 180}; SDL_RenderFillRect(ren, &sh1);
        SDL_Rect sh2 = {1130, 420, 120, 160}; SDL_RenderFillRect(ren, &sh2);
        bgProc[2][1] = t;
    }
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Building house background")) return 0;

    /* Restore render target to screen */
    SDL_SetRenderTarget(ren, NULL);

    /* ── Load Level 1 SFX ── */
    sfxJump[0]       = Mix_LoadWAV("assets/sounds/jump_1.wav");
    sfxJump[1]       = Mix_LoadWAV("assets/sounds/jump_2.wav");
    sfxDamage[0]     = Mix_LoadWAV("assets/sounds/taking_damage.mp3");
    sfxDamage[1]     = Mix_LoadWAV("assets/sounds/taking_damage.mp3");
    sfxDamage[2]     = Mix_LoadWAV("assets/sounds/taking_damage.mp3");
    sfxDamage[3]     = Mix_LoadWAV("assets/sounds/taking_damage.mp3");
    sfxLand          = Mix_LoadWAV("assets/sounds/land.wav");
    sfxKeyPickup     = Mix_LoadWAV("assets/sounds/key_pickup.mp3");
    sfxDlgLetter     = Mix_LoadWAV("assets/sounds/dialogue_letter_appear.wav");
    sfxFallingItems  = Mix_LoadWAV("assets/sounds/falling items.mp3");
    sfxApplianceFall = Mix_LoadWAV("assets/sounds/faling fridge and micro wave.mp3");
    sfxDoorOpen      = Mix_LoadWAV("assets/sounds/door_open.mp3");
    sfxRats          = Mix_LoadWAV("assets/sounds/rats.mp3");
    sfxDogBark       = Mix_LoadWAV("assets/sounds/dog.mp3");
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Loading sound effects")) return 0;

    prewarmTextureSizeCache();
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Precompiling texture metadata")) return 0;
    if (!advanceLoadingScreen(&loadStep, LEVEL1_LOAD_STEPS, "Finalizing level setup")) return 0;

    return 1;
}

static void clearTextCache(void)
{
    for (int i = 0; i < TEXT_CACHE_MAX; i++) {
        if (textCache[i].tex) SDL_DestroyTexture(textCache[i].tex);
        textCache[i].tex = NULL;
        textCache[i].text[0] = '\0';
        textCache[i].lastUsed = 0;
    }
    dlgWrapCache.valid = 0;
}

static int getTextureSize(SDL_Texture *tex, int *w, int *h)
{
    if (!tex) return 0;

    int emptySlot = -1;
    for (int i = 0; i < TEX_SIZE_CACHE_MAX; i++) {
        if (texSizeCache[i].tex == tex) {
            if (w) *w = texSizeCache[i].w;
            if (h) *h = texSizeCache[i].h;
            return 1;
        }
        if (emptySlot < 0 && !texSizeCache[i].tex) emptySlot = i;
    }

    int texW = 0, texH = 0;
    if (SDL_QueryTexture(tex, NULL, NULL, &texW, &texH) != 0) return 0;

    int slot = emptySlot;
    if (slot < 0) {
        slot = texSizeCacheNext;
        texSizeCacheNext = (texSizeCacheNext + 1) % TEX_SIZE_CACHE_MAX;
    }

    texSizeCache[slot].tex = tex;
    texSizeCache[slot].w = texW;
    texSizeCache[slot].h = texH;

    if (w) *w = texW;
    if (h) *h = texH;
    return 1;
}

static SDL_Texture *getCachedTextTexture(TTF_Font *f, const char *s, SDL_Color c,
                                         int *w, int *h)
{
    if (!f || !s || !s[0]) return NULL;

    if (++textCacheStamp == 0) textCacheStamp = 1;

    for (int i = 0; i < TEXT_CACHE_MAX; i++) {
        TextCacheEntry *entry = &textCache[i];
        if (!entry->tex) continue;
        if (entry->font != f || entry->r != c.r || entry->g != c.g || entry->b != c.b)
            continue;
        if (strcmp(entry->text, s) != 0) continue;
        entry->lastUsed = textCacheStamp;
        if (w) *w = entry->w;
        if (h) *h = entry->h;
        return entry->tex;
    }

    SDL_Surface *sf = TTF_RenderText_Blended(f, s, c);
    if (!sf) return NULL;

    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, sf);
    int texW = sf->w;
    int texH = sf->h;
    SDL_FreeSurface(sf);
    if (!tex) return NULL;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    int slot = -1;
    Uint32 oldest = 0;
    for (int i = 0; i < TEXT_CACHE_MAX; i++) {
        if (!textCache[i].tex) {
            slot = i;
            break;
        }
        if (slot < 0 || textCache[i].lastUsed < oldest) {
            slot = i;
            oldest = textCache[i].lastUsed;
        }
    }

    if (textCache[slot].tex) SDL_DestroyTexture(textCache[slot].tex);

    textCache[slot].tex = tex;
    textCache[slot].font = f;
    textCache[slot].w = texW;
    textCache[slot].h = texH;
    textCache[slot].r = c.r;
    textCache[slot].g = c.g;
    textCache[slot].b = c.b;
    textCache[slot].lastUsed = textCacheStamp;
    snprintf(textCache[slot].text, sizeof(textCache[slot].text), "%s", s);

    if (w) *w = texW;
    if (h) *h = texH;
    return tex;
}

static void drawCachedText(TTF_Font *f, const char *s, int x, int y,
                           Uint8 r, Uint8 g, Uint8 b, Uint8 a, int centered)
{
    SDL_Color c = {r, g, b, 255};
    int tw = 0, th = 0;
    SDL_Texture *tex = getCachedTextTexture(f, s, c, &tw, &th);
    if (!tex) return;

    SDL_Rect dst = {
        centered ? (SCREEN_W / 2 - tw / 2) : x,
        y,
        tw,
        th
    };
    if (a != 255) SDL_SetTextureAlphaMod(tex, a);
    SDL_RenderCopy(ren, tex, NULL, &dst);
    if (a != 255) SDL_SetTextureAlphaMod(tex, 255);
}

static void prewarmStaticTextCache(void)
{
    if (!font) return;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color muted = {180, 180, 180, 255};
    SDL_Color gold  = {255, 210, 50, 255};
    SDL_Color cyan  = {160, 220, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    SDL_Color green = {80, 255, 80, 255};

    const char *commonFontLabels[] = {
        "HEIGHT  0",
        "SECOND CHANCE!",
        "YOU DIED",
        "Press Enter to continue",
        "invi : on",
        "invi : off",
        "FPS 0.0",
        "J",
        "Keys: 0",
        "Not enough keys!",
        "SHOP",
        "Keys: 10",
        "Press Enter / Space / E to continue",
        NULL
    };

    for (int i = 0; commonFontLabels[i]; i++)
        getCachedTextTexture(font, commonFontLabels[i], white, NULL, NULL);

    getCachedTextTexture(font, "HEIGHT  0", white, NULL, NULL);
    getCachedTextTexture(font, "SECOND CHANCE!", muted, NULL, NULL);
    getCachedTextTexture(font, "invi : on", green, NULL, NULL);
    getCachedTextTexture(font, "invi : off", muted, NULL, NULL);
    getCachedTextTexture(font, "FPS 0.0", cyan, NULL, NULL);

    for (int i = 1; i <= 15; i++) {
        char timerBuf[16];
        snprintf(timerBuf, sizeof(timerBuf), "%us", (unsigned)i);
        getCachedTextTexture(font, timerBuf, white, NULL, NULL);
    }

    if (bigFont) {
        getCachedTextTexture(bigFont, "Chapter 1 : The Sneak", black, NULL, NULL);
        getCachedTextTexture(bigFont, "YOU DIED", white, NULL, NULL);
        for (int i = 0; i <= KEYS_TO_WIN; i++) {
            char keysBuf[32];
            snprintf(keysBuf, sizeof(keysBuf), "KEYS: %d / 10", i);
            getCachedTextTexture(bigFont, keysBuf, gold, NULL, NULL);
        }
        getCachedTextTexture(bigFont, "3", gold, NULL, NULL);
        getCachedTextTexture(bigFont, "2", gold, NULL, NULL);
        getCachedTextTexture(bigFont, "1", gold, NULL, NULL);
    }
}

static void closeSDL(void)
{
    arcade_input_shutdown();

    if (!ownsSDL && sharedRendererVSyncWasForced) {
        if (SDL_RenderSetVSync(ren, 1) != 0)
            SDL_Log("WARNING: Level 1 couldn't restore shared renderer VSync: %s", SDL_GetError());
        sharedRendererVSyncWasForced = 0;
    }

    clearTextCache();
    clearTextureSizeCache();
    if (font)    TTF_CloseFont(font);
    if (bigFont) TTF_CloseFont(bigFont);
    for (int z=0;z<3;z++) for (int l=0;l<2;l++) {
        if (bgTex[z][l])  SDL_DestroyTexture(bgTex[z][l]);
        if (bgProc[z][l]) SDL_DestroyTexture(bgProc[z][l]);
    }
    for (int t2=0;t2<2;t2++)
        if (transTex[t2]) SDL_DestroyTexture(transTex[t2]);
    for (int d=0;d<3;d++) if (debrisTex[d]) SDL_DestroyTexture(debrisTex[d]);
    for (int pi=0;pi<3;pi++) if (texPlatform[pi]) SDL_DestroyTexture(texPlatform[pi]);
    if (texRat)  SDL_DestroyTexture(texRat);
    if (texHurt) SDL_DestroyTexture(texHurt);
    if (texDog)  SDL_DestroyTexture(texDog);
    for (int characterNumber = 1; characterNumber <= 2; characterNumber++) {
        if (texPortrait[characterNumber]) SDL_DestroyTexture(texPortrait[characterNumber]);
        if (texIdle[characterNumber]) SDL_DestroyTexture(texIdle[characterNumber]);
        if (texRun[characterNumber]) SDL_DestroyTexture(texRun[characterNumber]);
        if (texJump[characterNumber]) SDL_DestroyTexture(texJump[characterNumber]);
        if (texShoesIdle[characterNumber]) SDL_DestroyTexture(texShoesIdle[characterNumber]);
        if (texShoesRun[characterNumber]) SDL_DestroyTexture(texShoesRun[characterNumber]);
        if (texShoesJump[characterNumber]) SDL_DestroyTexture(texShoesJump[characterNumber]);
        if (texJetpackAnim[characterNumber]) SDL_DestroyTexture(texJetpackAnim[characterNumber]);
    }
    if (texFridge)        SDL_DestroyTexture(texFridge);
    if (texMicrowave)     SDL_DestroyTexture(texMicrowave);
    if (texDoorClosed)    SDL_DestroyTexture(texDoorClosed);
    if (texDoorOpen)      SDL_DestroyTexture(texDoorOpen);
    if (texDoorOpening)   SDL_DestroyTexture(texDoorOpening);
    if (texDoorClosing)   SDL_DestroyTexture(texDoorClosing);
    if (texWater) SDL_DestroyTexture(texWater);
    if (texKeyAnim) SDL_DestroyTexture(texKeyAnim);
    for (int i = 0; i < 3; i++) if (texShopIcon[i]) SDL_DestroyTexture(texShopIcon[i]);
    if (texGauge) SDL_DestroyTexture(texGauge);
    stopRatsLoopSfx();
    for (int i = 0; i < 2; i++) { Mix_FreeChunk(sfxJump[i]);   sfxJump[i]   = NULL; }
    for (int i = 0; i < 4; i++) { Mix_FreeChunk(sfxDamage[i]); sfxDamage[i] = NULL; }
    Mix_FreeChunk(sfxLand);          sfxLand          = NULL;
    Mix_FreeChunk(sfxKeyPickup);     sfxKeyPickup     = NULL;
    Mix_FreeChunk(sfxDlgLetter);     sfxDlgLetter     = NULL;
    Mix_FreeChunk(sfxFallingItems);  sfxFallingItems  = NULL;
    Mix_FreeChunk(sfxApplianceFall); sfxApplianceFall = NULL;
    Mix_FreeChunk(sfxDoorOpen);      sfxDoorOpen      = NULL;
    Mix_FreeChunk(sfxRats);          sfxRats          = NULL;
    Mix_FreeChunk(sfxDogBark);       sfxDogBark       = NULL;
    if (sfxAudioOwned) {
        Mix_CloseAudio();
        sfxAudioOwned = 0;
    }
    if (sfxMixerOwned) {
        Mix_Quit();
        sfxMixerOwned = 0;
    }
    if (ownsSDL) {
        TTF_Quit();
        IMG_Quit();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        ren = NULL;
        win = NULL;
    } else if (ren) {
        SDL_SetRenderTarget(ren, NULL);
        SDL_RenderSetViewport(ren, NULL);
        SDL_RenderSetClipRect(ren, NULL);
        SDL_RenderSetLogicalSize(ren, 0, 0);
    }
    font = NULL;
    bigFont = NULL;
}

/* ═══════════════════════════════════════════════════════════
   DRAW HELPERS
═══════════════════════════════════════════════════════════ */
static void dr(int x, int y, int w, int h,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(ren, &rc);
}

static void dro(int x, int y, int w, int h,
                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderDrawRect(ren, &rc);
}

/* World-space rect projected through camera */
static void dwr(float wx, float wy, int w, int h,
                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    dr((int)wx, (int)(wy - cameraY), w, h, r, g, b, a);
}

/* Draw text at screen position */
static void dt(TTF_Font *f, const char *s, int x, int y,
               Uint8 r, Uint8 g, Uint8 b)
{
    drawCachedText(f, s, x, y, r, g, b, 255, 0);
}

/* Draw text centred on screen at given Y */
static void dtc(TTF_Font *f, const char *s, int y,
                Uint8 r, Uint8 g, Uint8 b)
{
    drawCachedText(f, s, 0, y, r, g, b, 255, 1);
}

static void refreshFpsLabel(void)
{
    snprintf(fpsLabel, sizeof(fpsLabel), "FPS %.1f", fpsDisplay);
    fpsLabelW = 0;
    fpsLabelH = 0;
    if (font && TTF_SizeUTF8(font, fpsLabel, &fpsLabelW, &fpsLabelH) != 0) {
        fpsLabelW = 0;
        fpsLabelH = 0;
    }
}

static void updateFpsCounter(Uint32 now)
{
    if (fpsWindowStart == 0) fpsWindowStart = now;
    fpsFrameCounter++;
    Uint32 elapsed = now - fpsWindowStart;
    if (elapsed >= 500) {
        fpsDisplay = (elapsed > 0) ? ((float)fpsFrameCounter * 1000.0f / (float)elapsed) : 0.0f;
        fpsFrameCounter = 0;
        fpsWindowStart = now;
        refreshFpsLabel();
    }
}

static void renderFpsCounter(void)
{
    if (!font) return;
    if (fpsLabelW > 0)
        dt(font, fpsLabel, SCREEN_W - fpsLabelW - 14, 12, 160, 220, 255);
    else
        dt(font, fpsLabel, SCREEN_W - 120, 12, 160, 220, 255);
}

static void capFrameRate(Uint64 frameStartCounter)
{
    const double targetFrameSeconds = 1.0 / (double)TARGET_FPS;
    if (framePacerFreq == 0) {
        framePacerFreq = SDL_GetPerformanceFrequency();
        if (framePacerFreq == 0) return;
    }

    while (1) {
        Uint64 nowCounter = SDL_GetPerformanceCounter();
        double elapsedSeconds = (double)(nowCounter - frameStartCounter) / (double)framePacerFreq;
        double remainingSeconds = targetFrameSeconds - elapsedSeconds;
        if (remainingSeconds <= 0.0) break;

        SDL_PumpEvents();
        if (remainingSeconds > 0.0045) {
            Uint32 sleepMs = (Uint32)((remainingSeconds - 0.0015) * 1000.0);
            if (sleepMs > 0) SDL_Delay(sleepMs);
            else SDL_Delay(1);
        } else {
            SDL_Delay(0);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   LEVEL LOADING
   Level 1 is procedurally generated at runtime.
═══════════════════════════════════════════════════════════ */
/* Generate the next floor above the current highest floor */
static void genOneFloor(void)
{
    if (floorCount >= MAX_FLOORS) return;
    int floorSerial = totalFloorsGenerated;
    float topY = (floorCount > 0)
                ? floors[floorCount-1].worldY - FLOOR_GAP
                : (float)(SCREEN_H - 80);
    /* Random gap — never too close to edges */
    int margin = 60;
    int gx = margin + rand() % (SCREEN_W - GAP_W - margin*2);
    floors[floorCount].worldY = topY;
    floors[floorCount].gapX  = gx;

    /* Spawn key on this floor ~50% chance (skip floor 0) */
    if (floorSerial > 0 && keyCount < MAX_KEYS && rand()%2==0) {
        float kx;
        if (rand()%2==0 && gx > KEY_W+10)
            kx = (float)(rand()%(gx-KEY_W-8)+4);
        else {
            int rx=gx+GAP_W, rw=SCREEN_W-rx;
            kx = (rw>KEY_W+10)?(float)(rx+rand()%(rw-KEY_W-8)+4):4.0f;
        }
        keys[keyCount].x = kx;
        keys[keyCount].y = topY - KEY_H - 4;
        keys[keyCount].collected = 0;
        keyCount++;
    }

    /* Spawn dog ~20% chance (skip first 3 floors, skip finish floor) */
    if (floorSerial > 3 && dogCount < MAX_DOGS && rand() % DOG_SPAWN_CHANCE == 0
        && floorCount != finishFloorIdx) {
        /* left slab: 0..gx, right slab: gx+GAP_W..SCREEN_W */
        /* pick whichever slab is wider, needs at least 2*DOG_W space */
        int leftW  = gx;
        int rightW = SCREEN_W - (gx + GAP_W);
        if (leftW >= DOG_W * 2 || rightW >= DOG_W * 2) {
            Dog *d = &dogs[dogCount];
            int useLeft = (leftW >= DOG_W * 2 &&
                           (rightW < DOG_W * 2 || rand() % 2 == 0));
            if (leftW < DOG_W * 2) useLeft = 0;

            d->floorY    = topY;
            d->floorIdx  = floorCount;
            d->active    = 1;
            d->dir       = 1;
            d->frame     = rand() % 36;
            d->lastTick  = SDL_GetTicks();
            if (useLeft) {
                d->leftBound  = 0;
                d->rightBound = gx - DOG_W;
                d->x          = (float)(rand() % (gx - DOG_W));
            } else {
                d->leftBound  = gx + GAP_W;
                d->rightBound = SCREEN_W - DOG_W;
                d->x          = (float)(gx + GAP_W + rand() % rightW);
            }
            dogCount++;
        }
    }

    /* Spawn sewer gauge ~20% — only if no dog on this floor */
    {
        int hasDog = (dogCount > 0 && dogs[dogCount-1].floorIdx == floorCount) ? 1 : 0;
        int rightSlabW = SCREEN_W - (gx + GAP_W);
        if (!hasDog && gaugeCount < MAX_GAUGES
            && floorSerial > 4 && rightSlabW >= 40
            && floorCount != finishFloorIdx
            && rand() % GAUGE_SPAWN_CHANCE == 0) {
            Gauge *g      = &gauges[gaugeCount];
            g->floorY     = topY;
            g->floorIdx   = floorCount;
            g->nozzleX    = SCREEN_W;
            g->phase      = GAUGE_IDLE;
            g->phaseStart = SDL_GetTicks();   /* start cooldown now, no future offset */
            g->flowX      = (float)SCREEN_W;
            g->animFrame  = 0;
            g->animTick   = SDL_GetTicks();
            g->active     = 1;
            gaugeCount++;
        }
    }

    /* Try to place a house — cooldown: at least 12 floors since last one */
    if (!house.active && floorSerial >= 3 &&
        floorSerial - lastHouseSpawnSerial >= 5 &&
        rand() % 3 == 0) {
        /* Place shop on whichever side has more room — icon is 160px wide */
        int rx = gx + GAP_W, rw = SCREEN_W - rx;
        if (gx >= rw) {
            /* left side bigger */
            house.x = (float)(gx / 2 - HOUSE_W / 2);
            if (house.x < 0) house.x = 0;
        } else {
            /* right side bigger */
            house.x = (float)(rx + rw / 2 - HOUSE_W / 2);
            if (house.x + HOUSE_W > SCREEN_W) house.x = (float)(SCREEN_W - HOUSE_W);
        }
        house.y        = topY - HOUSE_H;
        house.floorIdx = floorCount;
        house.active   = 1;
    }

    if (house.active) lastHouseSpawnSerial = floorSerial;
    floorCount++;
    totalFloorsGenerated++;
}

static void initLevel(void)
{
    stopRatsLoopSfx();

    floorCount = 0; keyCount = 0; dogCount = 0; gaugeCount = 0;
    totalFloorsGenerated = 0;
    lastHouseSpawnSerial = -99;
    house.active = 0;
    /* Seed initial screen-worth of floors */
    while (floorCount < 8) genOneFloor();
}

/* Called every frame to extend floors ahead of camera */
static void extendFloors(void)
{
    if (floorCount == 0) return;
    if (finishFloorIdx >= 0) return;  /* stop generating once finish is placed */
    float topY = floors[floorCount-1].worldY;
    float camTop = cameraY - FLOOR_GEN_LOOKAHEAD;
    while (topY > camTop && floorCount < MAX_FLOORS) {
        genOneFloor();
        topY = floors[floorCount-1].worldY;
    }
}

static void initProceduralLevel(void)
{
    initLevel();
    finishFloorIdx = -1;
}

static int worldYOutsideUpdateRange(float worldY)
{
    if (worldY < cameraY - UPDATE_PAD_Y) return 1;
    if (worldY > cameraY + SCREEN_H + UPDATE_PAD_Y) return 1;
    return 0;
}

static void cleanupWorldBelowCamera(void)
{
    float cleanupY = cameraY + SCREEN_H + CLEANUP_MARGIN_Y;

    int removedFloors = 0;
    while (removedFloors < floorCount &&
           floors[removedFloors].worldY > cleanupY) {
        removedFloors++;
    }

    if (removedFloors > 0) {
        int keep = floorCount - removedFloors;
        if (keep > 0)
            memmove(floors, floors + removedFloors, (size_t)keep * sizeof(Floor));
        floorCount = keep;

        if (finishFloorIdx >= 0) {
            finishFloorIdx -= removedFloors;
            if (finishFloorIdx < 0 || finishFloorIdx >= floorCount)
                finishFloorIdx = -1;
        }

        if (house.active) {
            if (house.floorIdx < removedFloors) house.active = 0;
            else house.floorIdx -= removedFloors;
        }

        for (int i = 0; i < dogCount; i++) {
            if (!dogs[i].active) continue;
            if (dogs[i].floorIdx < removedFloors) {
                dogs[i].active = 0;
                continue;
            }
            dogs[i].floorIdx -= removedFloors;
        }

        for (int i = 0; i < gaugeCount; i++) {
            if (!gauges[i].active) continue;
            if (gauges[i].floorIdx < removedFloors) {
                gauges[i].active = 0;
                continue;
            }
            gauges[i].floorIdx -= removedFloors;
        }
    }

    int keyWrite = 0;
    for (int i = 0; i < keyCount; i++) {
        if (keys[i].collected) continue;
        if (keys[i].y > cleanupY) continue;
        if (keyWrite != i) keys[keyWrite] = keys[i];
        keyWrite++;
    }
    if (keyWrite != keyCount) magnetSlideIdx = -1;
    keyCount = keyWrite;

    int dogWrite = 0;
    for (int i = 0; i < dogCount; i++) {
        Dog *d = &dogs[i];
        if (!d->active) continue;
        if (d->floorY > cleanupY) continue;
        if (d->floorIdx < 0 || d->floorIdx >= floorCount) continue;
        if (dogWrite != i) dogs[dogWrite] = *d;
        dogWrite++;
    }
    dogCount = dogWrite;

    int gaugeWrite = 0;
    for (int i = 0; i < gaugeCount; i++) {
        Gauge *g = &gauges[i];
        if (!g->active) continue;
        if (g->floorY > cleanupY) continue;
        if (g->floorIdx < 0 || g->floorIdx >= floorCount) continue;
        if (gaugeWrite != i) gauges[gaugeWrite] = *g;
        gaugeWrite++;
    }
    gaugeCount = gaugeWrite;
}

/* ═══════════════════════════════════════════════════════════
   FLOOR COLLISION
   - hitsSlabs : player is NOT fully inside the gap
   - landOn    : player falls onto top surface
   - bumpBot   : player rises into bottom surface
═══════════════════════════════════════════════════════════ */
static int hitsSlabs(Floor *fl, float px)
{
    return !(px >= (float)fl->gapX &&
             px + PLAYER_W <= (float)(fl->gapX + GAP_W));
}

static int landOn(Floor *fl, float pf0, float pf1, float px, float *sy)
{
    float wy = fl->worldY;
    if (pf0 > wy || pf1 < wy)    return 0;
    if (!hitsSlabs(fl, px))       return 0;
    *sy = wy - PLAYER_H;          return 1;
}

static int floorSearchIndex(float worldY)
{
    int lo = 0;
    int hi = floorCount;

    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (floors[mid].worldY > worldY) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static void floorSearchWindow(float worldY, int *start, int *end)
{
    if (floorCount <= 0) {
        *start = 0;
        *end = 0;
        return;
    }

    int idx = floorSearchIndex(worldY);
    if (idx >= floorCount) idx = floorCount - 1;

    *start = idx - 1;
    *end = idx + 3;
    if (*start < 0) *start = 0;
    if (*end > floorCount) *end = floorCount;
}

static int bumpBot(Floor *fl, float ph1, float px, float *sy)
{
    float top = fl->worldY;
    float bot = fl->worldY + FLOOR_H;
    /* Head (ph1 = player.y after move) is inside the floor slab */
    if (ph1 < top || ph1 >= bot) return 0;   /* not clipping into slab  */
    if (!hitsSlabs(fl, px))      return 0;
    *sy = bot; return 1;                      /* push head to slab bottom */
}

/* ═══════════════════════════════════════════════════════════
   PLAYER INIT / UPDATE / RENDER
═══════════════════════════════════════════════════════════ */
static void initPlayer(float wx, float wy)
{
    player.x = wx; player.y = wy;
    player.vx = 0; player.vy = 0;
    player.onGround    = 1;   /* start grounded so idle anim plays */
    player.facingRight = 1;
    player.jetpackFloors  = 0;
    player.magnetTimer    = 0;
    player.shoesTimer     = 0;
    player.extraHearts    = 0;
    player.buyCountJetpack= 0;
    player.buyCountMagnet = 0;
    player.buyCountShoes  = 0;
    animState = PANIM_IDLE;
}

static void initPartnerPlayer(float wx, float wy)
{
    player2.x = wx;
    player2.y = wy;
    player2.vx = 0;
    player2.vy = 0;
    player2.onGround = 1;
    player2.facingRight = 1;
    player2.jetpackFloors = 0;
    player2.magnetTimer = 0;
    player2.shoesTimer = 0;
    player2.extraHearts = 0;
    player2.buyCountJetpack = 0;
    player2.buyCountMagnet = 0;
    player2.buyCountShoes = 0;
}

static void updatePlayer(void)
{
    Uint32 now = SDL_GetTicks();
    int shoesActive = (player.shoesTimer > 0 && now < player.shoesTimer);
    int magnetActive = (player.magnetTimer > 0 && now < player.magnetTimer);
    int wasOnGround = player.onGround;

    /* expire invincibility flicker independent of render loop */
    if (flickerActive && now - flickerStart >= FLICKER_DURATION_MS)
        flickerActive = 0;

    /* ── Jetpack mode: rise continuously, decrement on each floor crossed ── */
    if (player.jetpackFloors > 0) {
        float prevTop = player.y;
        player.vy = JETPACK_VY;
        player.y += player.vy;
        float currTop = player.y;
        /* count each floor top we cross going upward */
        for (int i = 0; i < floorCount; i++) {
            float wy = floors[i].worldY;
            if (prevTop >= wy && currTop < wy) {
                player.jetpackFloors--;
                break;
            }
        }
        if (player.jetpackFloors <= 0) player.vy = 0;
        player.onGround = 0;
        /* horizontal steering during jetpack */
        if (inputSnap.moveRightHeld) { player.vx = PLAYER_SPEED; player.facingRight = 1; }
        else if (inputSnap.moveLeftHeld) { player.vx = -PLAYER_SPEED; player.facingRight = 0; }
        else { player.vx *= 0.75f; if (fabsf(player.vx) < 0.5f) player.vx = 0; }
        player.x += player.vx;
        if (player.x + PLAYER_W < 0) player.x = (float)SCREEN_W;
        if (player.x > SCREEN_W)     player.x = -(float)PLAYER_W;
        /* update anim */
        animState = PANIM_JUMP;
        return;
    }

    /* ── Normal input ── */
    if (inputSnap.moveRightHeld)      { player.vx = PLAYER_SPEED;  player.facingRight = 1; }
    else if (inputSnap.moveLeftHeld)  { player.vx = -PLAYER_SPEED; player.facingRight = 0; }
    else {
        player.vx *= 0.75f;
        if (fabsf(player.vx) < 0.5f) player.vx = 0;
    }

    if (consumeBufferedAction(ACT_JUMP, now) && player.onGround) {
        player.vy = shoesActive ? JUMP_VY * 1.35f : JUMP_VY;  /* shoes = higher jump */
        player.onGround = 0;
        {
            int jumpIdx = rand() % 2;
            if (sfxJump[jumpIdx]) Mix_PlayChannel(-1, sfxJump[jumpIdx], 0);
        }
    }

    /* ── Physics ── */
    player.vy += (player.vy < 0.0f) ? GRAVITY_RISE : GRAVITY_FALL;
    if (player.vy > MAX_FALL) player.vy = MAX_FALL;

    float pf0 = player.y + PLAYER_H;  /* feet before move */
    player.x += player.vx;
    player.y += player.vy;
    float pf1 = player.y + PLAYER_H;  /* feet after move  */
    float ph1 = player.y;              /* head after move  */

    if (player.x + PLAYER_W < 0) player.x = (float)SCREEN_W;
    if (player.x > SCREEN_W)     player.x = -(float)PLAYER_W;

    /* ── Floor collision ── */
    player.onGround = 0;
    if (player.vy >= 0.0f) {
        /* falling: check top surfaces */
        int start = 0, end = 0;
        floorSearchWindow(pf1, &start, &end);
        for (int i = start; i < end; i++) {
            float sy;
            if (landOn(&floors[i], pf0, pf1, player.x, &sy)) {
                if (!wasOnGround && player.vy > 4.0f && sfxLand)
                    Mix_PlayChannel(-1, sfxLand, 0);
                player.y = sy; player.vy = 0; player.onGround = 1; break;
            }
        }
    } else {
        /* rising: check bottom surfaces */
        if (!shoesActive) {
            /* normal: bump head.
               Exception: final floor is always passable from below. */
            int start = 0, end = 0;
            floorSearchWindow(ph1, &start, &end);
            for (int i = start; i < end; i++) {
                if (i == finishFloorIdx) continue;
                float sy;
                if (bumpBot(&floors[i], ph1, player.x, &sy)) {
                    player.y = sy; player.vy = 0; break;
                }
            }
        }
    }

    /* ── Key pickup / magnet ── */
    float pcx = player.x + PLAYER_W / 2.0f;
    float pcy = player.y + PLAYER_H / 2.0f;

    /* Magnet: slide keys one-by-one toward player across whole screen */
    if (magnetActive) {
        /* Find next un-collected, un-sliding key to target */
        if (magnetSlideIdx < 0 || magnetSlideIdx >= keyCount ||
            keys[magnetSlideIdx].collected) {
            magnetSlideIdx = -1;
            for (int i = 0; i < keyCount; i++) {
                if (!keys[i].collected) { magnetSlideIdx = i; break; }
            }
        }
        if (magnetSlideIdx >= 0) {
            float kcx = keys[magnetSlideIdx].x + KEY_W / 2.0f;
            float kcy = keys[magnetSlideIdx].y + KEY_H / 2.0f;
            float dx  = pcx - kcx, dy = pcy - kcy;
            float d   = sqrtf(dx * dx + dy * dy);
            if (d < 20.0f) {
                keys[magnetSlideIdx].collected = 1;
                keysHeld++;
                keysCollectedLifetime++;
                if (sfxKeyPickup) Mix_PlayChannel(-1, sfxKeyPickup, 0);
                magnetSlideIdx = -1;
            } else {
                keys[magnetSlideIdx].x += (dx / d) * magnetSlideSpd;
                keys[magnetSlideIdx].y += (dy / d) * magnetSlideSpd;
            }
        }
    }

    /* Normal key collection */
    for (int i = 0; i < keyCount; i++) {
        if (keys[i].collected) continue;
        float kcx = keys[i].x + KEY_W / 2.0f;
        float kcy = keys[i].y + KEY_H / 2.0f;
        float dx = kcx - pcx, dy = kcy - pcy;
        float dist2 = dx * dx + dy * dy;
        if (dist2 < KEY_COLLECT_R * KEY_COLLECT_R) {
            keys[i].collected = 1;
            keysHeld++;
            keysCollectedLifetime++;
            if (sfxKeyPickup) Mix_PlayChannel(-1, sfxKeyPickup, 0);
        }
    }

    /* ── House door trigger: show prompt, enter on F ── */
    if (house.active) {
        int dx2 = (int)house.x + HOUSE_W / 2 - DOOR_W / 2;
        int dy2 = (int)house.y + HOUSE_H - DOOR_H;
        /* just standing near the door is enough — F key handled in event loop */
        (void)dx2; (void)dy2;
    }

    /* ── Final floor: trigger camera pan + final dialogue ── */
    if (!finalDlgTriggered &&
        finishFloorIdx >= 0 && finishFloorIdx < floorCount) {
        Floor *top = &floors[finishFloorIdx];
        float  wy  = top->worldY;
        if (player.y + PLAYER_H >= wy - 2.0f &&
            player.y + PLAYER_H <= wy + FLOOR_H + 4.0f &&
            player.onGround) {
            finalDlgTriggered = 1;
            /* Stop debris, slide death line off screen */
            finalDebrisStopped = 1;
            stopRatsLoopSfx();
            debrisPhase  = DEB_IDLE;
            /* Walk player to LEFT side of door: 20px gap from door left edge */
            finalWalkTargetX = doorWorldX - PLAYER_W - 20.0f;
            finalWalkPhase   = FWALK_RUNNING;  /* start running immediately */
            /* Start camera pan */
            camPanTargetY = wy - SCREEN_H * 0.55f;
            camPanActive  = 1;
            gameState     = GS_FINAL_CAMPAN;
        }
    }
}

static void updatePartnerPlayer(void)
{
    Uint32 now = SDL_GetTicks();
    int wasOnGround;
    float pf0;
    float pf1;
    float ph1;
    float pcx;
    float pcy;

    if (!player2Enabled || !level1DuoEnabled())
        return;

    if (flickerActiveP2 && now - flickerStartP2 >= FLICKER_DURATION_MS)
        flickerActiveP2 = 0;

    if (playerHurtP2) {
        if (now - hurtStartP2 >= (Uint32)HURT_LOCK_MS) {
            playerHurtP2 = 0;
            player2.vx = 0;
        } else {
            player2.x += player2.vx;
            player2.vx *= 0.97f;
            if (player2.x + PLAYER_W < 0) player2.x = (float)SCREEN_W;
            if (player2.x > SCREEN_W)     player2.x = -(float)PLAYER_W;
            return;
        }
    }

    wasOnGround = player2.onGround;

    if (inputSnap.moveRightHeldP2)      { player2.vx = PLAYER_SPEED;  player2.facingRight = 1; }
    else if (inputSnap.moveLeftHeldP2)  { player2.vx = -PLAYER_SPEED; player2.facingRight = 0; }
    else {
        player2.vx *= 0.75f;
        if (fabsf(player2.vx) < 0.5f) player2.vx = 0;
    }

    if (inputSnap.jumpPressedP2 && player2.onGround) {
        player2.vy = JUMP_VY;
        player2.onGround = 0;
        if (sfxJump[0]) Mix_PlayChannel(-1, sfxJump[rand() % 2], 0);
    }

    player2.vy += (player2.vy < 0.0f) ? GRAVITY_RISE : GRAVITY_FALL;
    if (player2.vy > MAX_FALL) player2.vy = MAX_FALL;

    pf0 = player2.y + PLAYER_H;
    player2.x += player2.vx;
    player2.y += player2.vy;
    pf1 = player2.y + PLAYER_H;
    ph1 = player2.y;

    if (player2.x + PLAYER_W < 0) player2.x = (float)SCREEN_W;
    if (player2.x > SCREEN_W)     player2.x = -(float)PLAYER_W;

    player2.onGround = 0;
    if (player2.vy >= 0.0f) {
        int start = 0, end = 0;
        floorSearchWindow(pf1, &start, &end);
        for (int i = start; i < end; i++) {
            float sy;
            if (landOn(&floors[i], pf0, pf1, player2.x, &sy)) {
                if (!wasOnGround && player2.vy > 4.0f && sfxLand)
                    Mix_PlayChannel(-1, sfxLand, 0);
                player2.y = sy;
                player2.vy = 0;
                player2.onGround = 1;
                break;
            }
        }
    } else {
        int start = 0, end = 0;
        floorSearchWindow(ph1, &start, &end);
        for (int i = start; i < end; i++) {
            if (i == finishFloorIdx) continue;
            float sy;
            if (bumpBot(&floors[i], ph1, player2.x, &sy)) {
                player2.y = sy;
                player2.vy = 0;
                break;
            }
        }
    }

    pcx = player2.x + PLAYER_W / 2.0f;
    pcy = player2.y + PLAYER_H / 2.0f;
    for (int i = 0; i < keyCount; i++) {
        if (keys[i].collected) continue;
        {
            float kcx = keys[i].x + KEY_W / 2.0f;
            float kcy = keys[i].y + KEY_H / 2.0f;
            float dx = kcx - pcx;
            float dy = kcy - pcy;
            if (dx * dx + dy * dy < KEY_COLLECT_R * KEY_COLLECT_R) {
                keys[i].collected = 1;
                keysHeld++;
                keysCollectedLifetime++;
                if (sfxKeyPickup) Mix_PlayChannel(-1, sfxKeyPickup, 0);
            }
        }
    }

    if (!finalDlgTriggered &&
        finishFloorIdx >= 0 && finishFloorIdx < floorCount) {
        Floor *top = &floors[finishFloorIdx];
        float wy = top->worldY;
        if (player2.y + PLAYER_H >= wy - 2.0f &&
            player2.y + PLAYER_H <= wy + FLOOR_H + 4.0f &&
            player2.onGround) {
            finalDlgTriggered = 1;
            finalDebrisStopped = 1;
            stopRatsLoopSfx();
            debrisPhase = DEB_IDLE;
            finalWalkTargetX = doorWorldX - PLAYER_W - 20.0f;
            finalWalkPhase = FWALK_RUNNING;
            camPanTargetY = wy - SCREEN_H * 0.55f;
            camPanActive = 1;
            gameState = GS_FINAL_CAMPAN;
        }
    }

    (void)now;
}

/* Advance animation frame — speed varies per animation state */
static void tickAnim(void)
{
    Uint32 now = SDL_GetTicks();
    /* idle: 8fps, run: 14fps, jump: 10fps */
    Uint32 fps = (animState == PANIM_RUN)  ? 14 :
                 (animState == PANIM_IDLE) ?  8 : 10;
    Uint32 interval = 1000 / fps;
    if (now - animLastTick >= interval) {
        animFrame = (animFrame + 1) % 36;
        animLastTick = now;
    }
}

/* ═══════════════════════════════════════════════════════════
   SPRITE DRAW HELPER
   Draws one frame of a spritesheet at a custom display size,
   anchored so the bottom of the sprite aligns with the player feet.
   Tweak dw/dh independently per state below.
═══════════════════════════════════════════════════════════ */
static void drawSprite(SDL_Texture *tex, int frame,
                       int screenX, int screenY,
                       int dw, int dh,
                       SDL_RendererFlip flip)
{
    int texW, texH;
    if (!getTextureSize(tex, &texW, &texH)) return;
    int fw = texW / 6;
    int fh = texH / 6;
    int col = frame % 6;
    int row = frame / 6;
    SDL_Rect src = { col * fw, row * fh, fw, fh };
    /* Centre horizontally on hitbox, anchor bottom to feet */
    int ox = screenX - (dw - PLAYER_W) / 2;
    int oy = screenY - (dh - PLAYER_H);
    SDL_Rect dst = { ox, oy, dw, dh };
    SDL_RenderCopyEx(ren, tex, &src, &dst, 0, NULL, flip);
}

/* ── Per-state render functions — tweak dw/dh freely here ── */

static void renderIdle(int sx, int sy, SDL_RendererFlip flip)
{
    /* ↓ adjust these two values to resize the idle sprite */
    int dw = 53;
    int dh = 65;
    SDL_Texture *tex = selectedIdleTexture();
    if (tex) drawSprite(tex, animFrame, sx, sy, dw, dh, flip);
    else { dr(sx, sy, PLAYER_W, PLAYER_H, 80, 200, 120, 255); }
}

static void renderRun(int sx, int sy, SDL_RendererFlip flip)
{
    /* ↓ adjust these two values to resize the run sprite */
    int dw = 65;
    int dh = 63;
    SDL_Texture *tex = selectedRunTexture();
    if (tex) drawSprite(tex, animFrame, sx, sy, dw, dh, flip);
    else { dr(sx, sy, PLAYER_W, PLAYER_H, 80, 200, 120, 255); }
}

static void renderJump(int sx, int sy, SDL_RendererFlip flip)
{
    /* ↓ adjust these two values to resize the jump sprite */
    int dw = 76;
    int dh = 96;
    SDL_Texture *tex = selectedJumpTexture();
    if (tex) drawSprite(tex, animFrame, sx, sy, dw, dh, flip);
    else { dr(sx, sy, PLAYER_W, PLAYER_H, 80, 200, 120, 255); }
}

static void renderPlayer(void)
{
    Uint32 now = SDL_GetTicks();
    /* ── Determine animation state ── */
    if (finalWalkPhase == FWALK_DONE ||
        gameState == GS_FINAL_DIALOGUE ||
        gameState == GS_DOOR_OPENING   ||
        gameState == GS_HARRY_ENTER) {
        /* Lock to idle once player has walked to position */
        animState = PANIM_IDLE;
    } else if (player.jetpackFloors > 0 || !player.onGround) {
        animState = PANIM_JUMP;
    } else if (fabsf(player.vx) > 0.5f) {
        animState = PANIM_RUN;
    } else {
        animState = PANIM_IDLE;
    }
    tickAnim();

    SDL_RendererFlip flip = player.facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    int sx = (int)player.x;
    int sy = (int)(player.y - cameraY);

    /* ── Jetpack tanks (drawn behind player) ── */
    if (player.jetpackFloors > 0) {
        dwr(player.x + PLAYER_W,     player.y + 8,  8, 22, 180,  80, 30, 255);
        dwr(player.x - 8,            player.y + 8,  8, 22, 180,  80, 30, 255);
        dwr(player.x + PLAYER_W + 1, player.y + 28, 6,  8, 255, 150, 20, 220);
        dwr(player.x - 7,            player.y + 28, 6,  8, 255, 150, 20, 220);
    }
    /* ── Magnet aura ── */
    if (player.magnetTimer > 0 && now < player.magnetTimer) {
        SDL_SetRenderDrawColor(ren, 100, 100, 255, 30);
        SDL_Rect mg = {
            (int)(player.x + PLAYER_W / 2.0f - MAGNET_R),
            (int)(player.y - cameraY + PLAYER_H / 2.0f - MAGNET_R),
            (int)(MAGNET_R * 2), (int)(MAGNET_R * 2)
        };
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(ren, &mg);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
    /* ── Shoe glow ── */
    int shoesOn = (player.shoesTimer > 0 && now < player.shoesTimer);
    if (shoesOn)
        dwr(player.x - 2, player.y + PLAYER_H - 6, PLAYER_W + 4, 8, 80, 255, 180, 180);

    /* ── Draw correct state (shoes/jetpack override anims) ── */
    if (shoesOn && selectedShoesIdleTexture() && selectedShoesRunTexture() && selectedShoesJumpTexture()) {
        /* Helper: draw from a shoes sheet using global animFrame */
        SDL_Texture *stex = (animState == PANIM_RUN)  ? selectedShoesRunTexture()  :
                            (animState == PANIM_JUMP)  ? selectedShoesJumpTexture() : selectedShoesIdleTexture();
        int stW, stH;
        if (!getTextureSize(stex, &stW, &stH)) return;
        int sfw = stW / 6, sfh = stH / 6;
        int scol = animFrame % 6, srow = animFrame / 6;
        SDL_Rect ssrc = { scol*sfw, srow*sfh, sfw, sfh };
        SDL_Rect sdst = { sx, sy, PLAYER_W, PLAYER_H };
        SDL_RenderCopyEx(ren, stex, &ssrc, &sdst, 0, NULL, flip);
    } else {
        switch (animState) {
            case PANIM_RUN:  renderRun (sx, sy, flip); break;
            case PANIM_JUMP: renderJump(sx, sy, flip); break;
            default:         renderIdle(sx, sy, flip); break;
        }
    }
}

static void renderPartnerPlayer(void)
{
    PlayerAnim partnerAnimState;
    SDL_RendererFlip flip;
    SDL_Texture *tex;
    int sx;
    int sy;
    int tw, th;
    int fw, fh;
    int col, row;
    SDL_Rect src;
    SDL_Rect dst;

    if (!player2Enabled || !level1DuoEnabled()) return;
    if (playerEnteringDoor || playerEnteredDoor) return;
    if (flickerActiveP2) {
        Uint32 fe = SDL_GetTicks() - flickerStartP2;
        if ((fe / 80) % 2 == 1) return;
    }

    if (!player2.onGround) partnerAnimState = PANIM_JUMP;
    else if (fabsf(player2.vx) > 0.5f) partnerAnimState = PANIM_RUN;
    else partnerAnimState = PANIM_IDLE;

    flip = player2.facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    sx = (int)player2.x;
    sy = (int)(player2.y - cameraY);

    if (partnerAnimState == PANIM_RUN) tex = texRun[partnerCharacterNumber()];
    else if (partnerAnimState == PANIM_JUMP) tex = texJump[partnerCharacterNumber()];
    else tex = texIdle[partnerCharacterNumber()];

    if (!tex) {
        dr(sx, sy, PLAYER_W, PLAYER_H, 180, 120, 80, 255);
        return;
    }

    if (!getTextureSize(tex, &tw, &th)) return;
    fw = tw / 6;
    fh = th / 6;
    col = animFrame % 6;
    row = animFrame / 6;
    src.x = col * fw;
    src.y = row * fh;
    src.w = fw;
    src.h = fh;
    dst.x = sx;
    dst.y = sy;
    dst.w = PLAYER_W;
    dst.h = PLAYER_H;
    SDL_RenderCopyEx(ren, tex, &src, &dst, 0, NULL, flip);
}

/* ═══════════════════════════════════════════════════════════
   CAMERA + DEATH LINE
═══════════════════════════════════════════════════════════ */
static void updateCamera(void)
{
    float anchorPlayerY;

    /* Fully frozen during story intro and freezing dialogue */
    if (gameState == GS_INTRO_FADE_IN || gameState == GS_INTRO_TITLE ||
        gameState == GS_INTRO_FADE_OUT) return;
    if (gameState == GS_DIALOGUE && dlgFreezeGame) return;
    if (gameState == GS_FINAL_DIALOGUE || gameState == GS_FINAL_CAMPAN ||
        gameState == GS_HARRY_ENTER    || gameState == GS_DOOR_OPENING  ||
        gameState == GS_CONSEQUENCE    || gameState == GS_CREDITS) return;
    if (finalDlgTriggered) return;  /* camera locked once final floor reached */

    anchorPlayerY = player.y;
    if (player2Enabled && level1DuoEnabled() && player2.y < anchorPlayerY)
        anchorPlayerY = player2.y;

    /* Freeze auto-scroll ONLY when the final floor is very close to the
       death line — so the player can see it without it scrolling away.
       "Close" = final floor is on screen AND within 12% of screen height
       from the top of the visible area.                                  */
    if (floorCount > 0) {
        float topScreenY = floors[floorCount - 1].worldY - cameraY;
        float lineScreenY = deathLineY - cameraY;           /* ≈ SCREEN_H-18 */
        /* Only freeze when final floor is near the TOP of screen AND
           the gap between it and the death line is small (end is near) */
        if (topScreenY >= 0.0f && topScreenY < SCREEN_H * 0.12f &&
            lineScreenY - topScreenY < SCREEN_H * 0.50f) {
            float ps = anchorPlayerY - cameraY;
            float ub = SCREEN_H * 0.30f;
            if (ps < ub) cameraY = anchorPlayerY - ub;
            return;
        }
    }
    /* Normal auto-scroll */
    cameraY -= SCROLL_SPEED;
    float ps = anchorPlayerY - cameraY;
    float ub = SCREEN_H * 0.30f;
    if (ps < ub) cameraY = anchorPlayerY - ub;
}

static void updateDeathLine(void) { deathLineY = cameraY + SCREEN_H - 18.0f; }

static void checkDeathLine(void)
{
    if (gameState != GS_PLAYING) return;
    if (!ratsVisible || ratSlideY > 0) return;
    if (cheatInvincible) return;  /* god mode */

    if (!level1DuoEnabled() || !player2Enabled) {
        if (player.y + PLAYER_H < deathLineY) return;

        if (player.extraHearts > 0) player.extraHearts--;
        else lives--;

        if (lives <= 0 && player.extraHearts <= 0) {
            lives = 0; gameState = GS_GAME_OVER; fadeAlpha = 0;
        } else {
            scRespawnTarget = 0;
            gameState = GS_SECOND_CHANCE; scTimer = SDL_GetTicks();
        }
        return;
    }

    {
        int fellP1 = (player.y + PLAYER_H >= deathLineY);
        int fellP2 = (player2.y + PLAYER_H >= deathLineY);
        int p1CanRespawn = 0;
        int p2CanRespawn = 0;

        if (!fellP1 && !fellP2) return;

        if (fellP1 && (lives > 0 || player.extraHearts > 0)) {
            if (player.extraHearts > 0) player.extraHearts--;
            else lives--;
            if (lives < 0) lives = 0;
            p1CanRespawn = (lives > 0 || player.extraHearts > 0);
        }

        if (fellP2 && (livesP2 > 0 || player2.extraHearts > 0)) {
            if (player2.extraHearts > 0) player2.extraHearts--;
            else livesP2--;
            if (livesP2 < 0) livesP2 = 0;
            p2CanRespawn = (livesP2 > 0 || player2.extraHearts > 0);
        }

        if (lives <= 0 && player.extraHearts <= 0 &&
            livesP2 <= 0 && player2.extraHearts <= 0) {
            gameState = GS_GAME_OVER;
            fadeAlpha = 0;
            return;
        }

        if (p1CanRespawn) {
            scRespawnTarget = 0;
            gameState = GS_SECOND_CHANCE;
            scTimer = SDL_GetTicks();
            return;
        }

        if (p2CanRespawn) {
            scRespawnTarget = 1;
            gameState = GS_SECOND_CHANCE;
            scTimer = SDL_GetTicks();
            return;
        }
    }
}

static void doRespawn(void)
{
    float target = cameraY + SCREEN_H * 0.45f;
    float bestD = 1e9f; int bi = 0;
    for (int i = 0; i < floorCount; i++) {
        float d = fabsf(floors[i].worldY - target);
        if (d < bestD) { bestD = d; bi = i; }
    }
    Floor *fl = &floors[bi];
    float px = (fl->gapX >= PLAYER_W + 20)
             ? (float)(fl->gapX / 2 - PLAYER_W / 2)
             : (float)(fl->gapX + GAP_W + 10);

    if (level1DuoEnabled() && player2Enabled) {
        if (scRespawnTarget == 1) {
            float p2x = px + 68.0f;
            if (p2x + PLAYER_W > SCREEN_W) p2x = px - 68.0f;
            player2.x = p2x;
            player2.y = fl->worldY - PLAYER_H;
            player2.vx = player2.vy = 0;
            player2.onGround = 1;
        } else {
            player.x = px;
            player.y = fl->worldY - PLAYER_H;
            player.vx = player.vy = 0;
            player.onGround = 1;
        }
    } else {
        player.x = px; player.y = fl->worldY - PLAYER_H;
        player.vx = player.vy = 0; player.onGround = 1;
        if (player2Enabled) {
            float p2x = px + 68.0f;
            if (p2x + PLAYER_W > SCREEN_W) p2x = px - 68.0f;
            player2.x = p2x;
            player2.y = fl->worldY - PLAYER_H;
            player2.vx = player2.vy = 0;
            player2.onGround = 1;
        }
    }
    /* Clear hurt state so game resumes immediately, no freeze */
    playerHurt      = 0;
    hurtStart       = 0;
    customDeathMsg  = NULL;   /* reset custom death message */
    if (!level1DuoEnabled() || scRespawnTarget == 0) {
        flickerActive   = 1;
        flickerStart    = SDL_GetTicks();
    }
}

/* ═══════════════════════════════════════════════════════════
   WORLD RENDERING
═══════════════════════════════════════════════════════════ */

/* Draw one tiling background layer */
static void drawBgLayer(SDL_Texture *tex, int layerH, float speed, Uint8 alpha)
{
    if (!tex) return;
    SDL_SetTextureAlphaMod(tex, alpha);
    /* How far has this layer scrolled? */
    int scrollY = (int)(cameraY * speed);
    /* Tile start — always negative so we start one tile above screen */
    int startY  = -((-scrollY) % layerH);
    if (startY > 0) startY -= layerH;
    for (int y = startY; y < SCREEN_H; y += layerH) {
        SDL_Rect dst = {0, y, BG_LAYER_W, layerH};
        SDL_RenderCopy(ren, tex, NULL, &dst);
    }
    SDL_SetTextureAlphaMod(tex, 255);
}


/* ── Zone transition: fires after TRANS_CAM_TRIGGER px of camera travel ── */
static void fireTransition(void)
{
    transNextZone       = bgZone + 1;
    gameState           = GS_TRANSITION;
    transStartMs        = SDL_GetTicks();
    bgCamAtZoneStart    = cameraY;
    transPlayerReleased = 0;
    /* player keeps playing normally — no freeze at start */
}

static void updateBgTileCounter(void)
{
    float travelled = bgCamAtZoneStart - cameraY;

    /* Sewer mid-dialogue: fires once at 80% through zone 0 */
    if (bgZone == 0 && !sewerDlgTriggered &&
        travelled >= TRANS_CAM_TRIGGER * 0.80f &&
        player.onGround) {
        sewerDlgTriggered = 1;
        startDialogueSeq(DLGSEQ_SEWER, dlgSewerPre, DLG_SEWER_PRE_COUNT);
    }

    /* Street fridge dialogue: fires once at 40% through zone 1 (non-freezing) */
    if (bgZone == 1 && !streetDlgTriggered &&
        travelled >= TRANS_CAM_TRIGGER * 0.40f) {
        streetDlgTriggered = 1;
        startDialogueSeq(DLGSEQ_STREET, dlgStreetFridge, DLG_STREET_COUNT);
    }

    /* Microwave dialogue: fires once at 30% through zone 2 */
    if (bgZone == 2 && !microwaveDlgTriggered &&
        travelled >= TRANS_CAM_TRIGGER * 0.30f) {
        microwaveDlgTriggered = 1;
        startDialogueSeq(DLGSEQ_MICROWAVE, dlgMicrowave, DLG_MICROWAVE_COUNT);
    }

    if (travelled >= TRANS_CAM_TRIGGER && bgZone < 2)
        fireTransition();
}

/*
   Fade transition:
     0%  – 45%  fade IN  (alpha 0 → 255) — world + player fully playable
     45% – 55%  fully opaque — teleport player, switch zone
     55% – 100% fade OUT (alpha 255 → 0) — new world revealed, player flickering in
*/

static void updateTransition(void)
{
    /* World always runs */
    updatePlayer();
    if (!playerHurt) updatePartnerPlayer();
    updateCamera();
    updateDeathLine();
    extendFloors();

    Uint32 elapsed  = SDL_GetTicks() - transStartMs;
    float  progress = (float)elapsed / (float)TRANS_DURATION_MS;
    if (progress > 1.0f) progress = 1.0f;

    /* At full cover: teleport player + switch zone (once) */
    if (progress >= 0.45f && !transPlayerReleased) {
        transPlayerReleased = 1;
        bgZone = transNextZone;
        bgCamAtZoneStart = cameraY;   /* reset travel counter for new zone */

        /* On second transition (entering zone 2), plant finish floor 20 floors up */
        if (transNextZone == 2 && finishFloorIdx < 0) {
            /* generate floors until we have 20 above current top */
            int target = floorCount + 20;
            while (floorCount < target && floorCount < MAX_FLOORS) genOneFloor();
            finishFloorIdx = floorCount - 1;
            /* make finish floor solid (gapX = 0, width = full screen) */
            floors[finishFloorIdx].gapX = SCREEN_W; /* no gap = solid */
            /* Spawn door and Harry immediately on the finish floor */
            {
                float wy = floors[finishFloorIdx].worldY;
                doorWorldX    = (float)(SCREEN_W / 2 - FDOOR_W / 2);
                doorWorldY    = wy - FDOOR_H;
                doorSpawned   = 1;
                doorState     = DOOR_CLOSED;
                doorAnimFrame = 0;
                harryX        = doorWorldX + FDOOR_W + 20.0f;
                harryWorldY   = wy - PLAYER_H;
                harryActive   = 1;
                harryAlpha    = 255.0f;
                harryAnimFrame= 0;
                harryAnimTick = SDL_GetTicks();
            }
        }

        /* Place player on floor closest to screen middle */
        float targetWorldY = cameraY + SCREEN_H * 0.50f;
        int bestFl = 1; float bestDist = 1e9f;
        for (int i = 1; i < floorCount - 1; i++) {
            float d = fabsf(floors[i].worldY - targetWorldY);
            if (d < bestDist) { bestDist = d; bestFl = i; }
        }
        Floor *tfl = &floors[bestFl];
        float px2 = (float)(SCREEN_W / 2 - PLAYER_W / 2);
        /* keep X centred but make sure not inside gap */
        if (px2 > tfl->gapX - PLAYER_W && px2 < tfl->gapX + GAP_W)
            px2 = (tfl->gapX >= PLAYER_W + 20)
                ? (float)(tfl->gapX / 2 - PLAYER_W / 2)
                : (float)(tfl->gapX + GAP_W + 10);
        player.x = px2;
        player.y = tfl->worldY - PLAYER_H;
        player.vx = 0; player.vy = 0;
        player.onGround = 1;

        /* Start flicker for reveal */
        flickerActive = 1;
        flickerStart  = SDL_GetTicks();
    }

    /* Player hidden only while fully opaque (45%–55%) */
    transPlayerHidden = (progress >= 0.45f && progress < 0.55f) ? 1 : 0;

    if (elapsed >= TRANS_DURATION_MS) {
        gameState           = GS_PLAYING;
        bgZone              = transNextZone;
        transPlayerReleased = 0;
        transPlayerHidden   = 0;
    }
}

/* ── Render the fade overlay ── */
static void renderTransition(void)
{
    Uint32 elapsed  = SDL_GetTicks() - transStartMs;
    float  progress = (float)elapsed / (float)TRANS_DURATION_MS;
    if (progress > 1.0f) progress = 1.0f;

    /* Alpha curve: ramp up 0→255 in first 45%, ramp down 255→0 in last 45% */
    float alpha;
    if (progress <= 0.45f)
        alpha = progress / 0.45f;
    else if (progress <= 0.55f)
        alpha = 1.0f;
    else
        alpha = 1.0f - (progress - 0.55f) / 0.45f;

    Uint8 a = (Uint8)(alpha * 255.0f);
    if (a == 0) return;

    int ti = (transNextZone == 1) ? 0 : 1;
    SDL_Texture *fade = transTex[ti];

    SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
    if (fade) {
        SDL_SetTextureAlphaMod(fade, a);
        SDL_RenderCopy(ren, fade, NULL, &full);
        SDL_SetTextureAlphaMod(fade, 255);
    } else {
        /* fallback: dark overlay */
        SDL_SetRenderDrawColor(ren, 10, 8, 20, a);
        SDL_RenderFillRect(ren, &full);
    }
}

/* ── Pick active bg texture (real art or procedural) ── */
static SDL_Texture *bgGet(int zone, int layer)
{
    /* zone 2 = house; falls back to street procedural if no art loaded */
    int z = (zone >= 0 && zone <= 2) ? zone : 1;
    /* zone 2 has no procedural — fall back to zone 1 procedural */
    SDL_Texture *t = bgTex[z][layer];
    if (!t) t = bgProc[z][layer];
    if (!t) t = bgProc[1][layer];  /* ultimate fallback: street */
    return t;
}

static void renderBackground(void)
{
    int layerH[2]   = {BG_LAYER_H_FAR, BG_LAYER_H_MID};
    float speeds[2] = {BG_SPEED_FAR,   BG_SPEED_MID};

    for (int l = 0; l < 2; l++)
        drawBgLayer(bgGet(bgZone, l), layerH[l], speeds[l], 255);
}

/* Tile a platform texture strip across [x, x+w] at screen y */
static void drawPlatformStrip(int x, int y, int w, SDL_Texture *tex)
{
    if (!tex) return;
    int texW = 0;
    if (!getTextureSize(tex, &texW, NULL) || texW <= 0) return;
    int drawn = 0;
    while (drawn < w) {
        int srcX  = drawn % texW;
        int chunk = texW - srcX;
        if (drawn + chunk > w) chunk = w - drawn;
        SDL_Rect src2 = {srcX, 0, chunk, FLOOR_H};
        SDL_Rect dst  = {x + drawn, y, chunk, FLOOR_H};
        SDL_RenderCopy(ren, tex, &src2, &dst);
        drawn += chunk;
    }
}

static void renderFloors(void)
{
    SDL_Texture *platTex = texPlatform[bgZone < 3 ? bgZone : 2];

    for (int i = 0; i < floorCount; i++) {
        int sy = (int)(floors[i].worldY - cameraY);
        if (sy > SCREEN_H + (int)RENDER_PAD_Y || sy + FLOOR_H < -(int)RENDER_PAD_Y) continue;
        int gx = floors[i].gapX;

        /* left slab */
        if (gx > 0) {
            if (platTex)
                drawPlatformStrip(0, sy, gx, platTex);
            else {
                dr(0, sy, gx, FLOOR_H, 70, 130, 180, 255);
                dr(0, sy, gx, 2, 130, 190, 230, 255);
            }
        }
        /* right slab */
        int rx = gx + GAP_W, rw = SCREEN_W - rx;
        if (rw > 0) {
            if (platTex)
                drawPlatformStrip(rx, sy, rw, platTex);
            else {
                dr(rx, sy, rw, FLOOR_H, 70, 130, 180, 255);
                dr(rx, sy, rw, 2, 130, 190, 230, 255);
            }
        }
        /* gap edge shadow */
        dr(gx - 2,     sy, 2, FLOOR_H, 0, 0, 0, 120);
        dr(gx + GAP_W, sy, 2, FLOOR_H, 0, 0, 0, 120);
    }
}

static void renderTopFloor(void)
{
    if (finishFloorIdx < 0 || finishFloorIdx >= floorCount) return;
    int sy = (int)(floors[finishFloorIdx].worldY - cameraY);
    if (sy < -(int)RENDER_PAD_Y - FLOOR_H || sy > SCREEN_H + (int)RENDER_PAD_Y) return;
    /* Final floor — same look as regular floor, no gold/label */
    dr(0, sy, SCREEN_W, FLOOR_H, 80, 60, 50, 255);
    dr(0, sy, SCREEN_W, 2,       120, 100, 80, 255);
}

static void renderKeys(void)
{
    /* Tick key animation once per frame, independent of which keys are visible */
    if (texKeyAnim) {
        Uint32 now = SDL_GetTicks();
        if (now - keyAnimLastTick >= 1000u / KEY_ANIM_FPS) {
            keyAnimFrame = (keyAnimFrame + 1) % 36;
            keyAnimLastTick = now;
        }
    }

    /* Cache frame src rect — same for every key this frame */
    int kfw = 0, kfh = 0, kcol = 0, krow = 0;
    if (texKeyAnim) {
        int texW, texH;
        if (!getTextureSize(texKeyAnim, &texW, &texH)) return;
        kfw = texW / 6;
        kfh = texH / 6;
        kcol = keyAnimFrame % 6;
        krow = keyAnimFrame / 6;
    }

    for (int i = 0; i < keyCount; i++) {
        if (keys[i].collected) continue;
        int sx = (int)keys[i].x;
        int sy = (int)(keys[i].y - cameraY);
        if (sy < -(int)RENDER_PAD_Y - KEY_H || sy > SCREEN_H + (int)RENDER_PAD_Y) continue;
        if (texKeyAnim) {
            SDL_Rect ksrc = { kcol * kfw, krow * kfh, kfw, kfh };
            SDL_Rect dst  = { sx, sy, KEY_W, KEY_H };
            SDL_RenderCopy(ren, texKeyAnim, &ksrc, &dst);
        } else {
            int cx = sx + KEY_W / 2, cy = sy + KEY_H / 2;
            SDL_Point pts[5] = {{cx,sy},{sx+KEY_W,cy},{cx,sy+KEY_H},{sx,cy},{cx,sy}};
            SDL_SetRenderDrawColor(ren, 255, 210, 50, 255);
            SDL_RenderDrawLines(ren, pts, 5);
            dr(cx-2, cy-2, 4, 4, 255, 240, 120, 255);
        }
    }
}

static void renderHouse(void)
{
    char prompt[48];

    if (!house.active) return;
    int sx = (int)house.x;
    int sy = (int)(house.y - cameraY);
    if (sy > SCREEN_H + (int)RENDER_PAD_Y || sy + HOUSE_H < -(int)RENDER_PAD_Y) return;

    /* Zone-appropriate shop exterior icon */
    int zone = bgZone < 3 ? bgZone : 0;
    if (texShopIcon[zone]) {
        SDL_Rect dst = { sx, sy, HOUSE_W, HOUSE_H };
        SDL_RenderCopy(ren, texShopIcon[zone], NULL, &dst);
    } else {
        /* Fallback: drawn rectangles */
        dr(sx, sy + 18, HOUSE_W, HOUSE_H - 18, 160, 100, 60, 255);
        for (int row = 0; row < 18; row++) {
            int half = (row * HOUSE_W / 2) / 18;
            dr(sx + HOUSE_W / 2 - half, sy + row, half * 2, 1, 180, 60, 60, 255);
        }
        int dx2 = sx + HOUSE_W / 2 - DOOR_W / 2;
        int dy2 = sy + HOUSE_H - DOOR_H;
        dr(dx2, dy2, DOOR_W, DOOR_H, 80, 45, 20, 255);
        dr(dx2 + DOOR_W - 6, dy2 + DOOR_H / 2, 4, 4, 255, 200, 80, 255);
        dr(sx + 8, sy + 22, 14, 12, 150, 200, 240, 200);
        dro(sx + 8, sy + 22, 14, 12, 60, 40, 20, 255);
    }
    dt(font, "SHOP", sx, sy - 18, 255, 220, 80);

    /* Interact prompt when player is near the door */
    if (font) {
        int pdx = (int)house.x + HOUSE_W / 2 - DOOR_W / 2;
        int pdy = (int)(house.y - cameraY) + HOUSE_H - DOOR_H;
        float nearX = player.x + PLAYER_W / 2.0f;
        float nearY = player.y + PLAYER_H;
        int nearDoor = (fabsf(nearX - (house.x + HOUSE_W / 2.0f)) < 60 &&
                        fabsf(nearY - (house.y + HOUSE_H)) < 80);
        if (!nearDoor && player2Enabled && level1DuoEnabled()) {
            float nearX2 = player2.x + PLAYER_W / 2.0f;
            float nearY2 = player2.y + PLAYER_H;
            nearDoor = (fabsf(nearX2 - (house.x + HOUSE_W / 2.0f)) < 60 &&
                        fabsf(nearY2 - (house.y + HOUSE_H)) < 80);
        }
        if (nearDoor) {
            char keyLabel[24];
            level1FormatInteractPrompt(keyLabel, sizeof(keyLabel));
            snprintf(prompt, sizeof(prompt), "[%s] Enter",
                     keyLabel);
            dt(font, prompt, pdx - 8, pdy - 20, 255, 255, 80);
        }
    }
}

/* ─── Debris system ─────────────────────────────────────────────── */

static void spawnDebrisEvent(void)
{
    debrisQuarter = rand() % 4;
    debrisPhase   = DEB_WARNING;
    debrisTimer   = SDL_GetTicks();
    playerHitThisEvent = 0;
}

static void launchPieces(void)
{
    int qx = debrisQuarter * (SCREEN_W / 4);
    int qw = SCREEN_W / 4;
    debrisPieceCount = DEBRIS_PIECE_MAX;
    for (int i = 0; i < DEBRIS_PIECE_MAX; i++) {
        DebrisPiece *p = &debrisPieces[i];
        /* stagger start Y so pieces cascade, not all at once */
        p->y        = (float)(-(i * (SCREEN_H * 2 / DEBRIS_PIECE_MAX)));
        p->angle    = (float)(rand() % 360);
        p->angleSpd = (float)((rand() % 40) - 20) * 0.5f;  /* -10..+10 deg/frame */
        p->texIdx   = (bgZone < 3) ? bgZone : 2;
        p->srcY     = rand() % 400;
        p->wobble   = (float)(qx + rand() % (qw - DEBRIS_PIECE_W));
        p->wobbleSpd = (float)((rand() % 6) - 3) * 0.3f;
    }
}

static void updateDebris(void)
{
    Uint32 now     = SDL_GetTicks();
    Uint32 elapsed = now - debrisTimer;

    if (debrisPhase == DEB_IDLE) {
        /* If fridge or microwave sequence wants debris paused, skip spawn only */
        if (fridgeDebrisPaused || mwDebrisPaused || finalDebrisStopped) return;
        if (elapsed >= DEBRIS_COOLDOWN_MS) spawnDebrisEvent();
        shakeAmt *= 0.70f;  /* decay shake faster so the camera settles quickly */
        if (shakeAmt < 0.5f) { shakeAmt = 0; shakeOX = 0; shakeOY = 0; }
        else {
            shakeOX = (int)((float)(rand()%3 - 1) * shakeAmt);
            shakeOY = (int)((float)(rand()%3 - 1) * shakeAmt);
        }
        return;
    }

    if (debrisPhase == DEB_WARNING) {
        if (elapsed >= (Uint32)DEBRIS_WARN_MS) {
            debrisPhase = DEB_FALLING;
            debrisTimer = now;
            launchPieces();
            shakeAmt    = 4.5f;
            if (sfxFallingItems) Mix_PlayChannel(-1, sfxFallingItems, 0);
        }
        return;
    }

    /* DEB_FALLING */
    shakeAmt = 4.0f;
    shakeOX  = (int)((float)(rand()%5 - 2) * shakeAmt * 0.5f);
    shakeOY  = (int)((float)(rand()%5 - 2) * shakeAmt * 0.5f);

    int qx   = debrisQuarter * (SCREEN_W / 4);
    int qw   = SCREEN_W / 4;
    int allGone = 1;

    for (int i = 0; i < debrisPieceCount; i++) {
        DebrisPiece *p = &debrisPieces[i];
        p->y      += DEBRIS_SPEED;
        p->angle  += p->angleSpd;
        p->wobble += p->wobbleSpd;
        /* clamp wobble within quarter */
        if (p->wobble < (float)qx) { p->wobble = (float)qx; p->wobbleSpd *= -1; }
        if (p->wobble + DEBRIS_PIECE_W > qx + qw) {
            p->wobble = (float)(qx + qw - DEBRIS_PIECE_W); p->wobbleSpd *= -1;
        }
        if (p->y < SCREEN_H * 2 + DEBRIS_PIECE_H) allGone = 0;
    }

    /* Check player collision — only once per event */
    if (!playerHitThisEvent && gameState == GS_PLAYING && !playerHurt) {
        float px = player.x + PLAYER_W * 0.5f;
        float psy = player.y - cameraY;   /* player screen Y */
        /* is player horizontally in the quarter? */
        if ((int)px >= qx && (int)px <= qx + qw) {
            /* is any piece overlapping player vertically? */
            for (int i = 0; i < debrisPieceCount; i++) {
                DebrisPiece *p = &debrisPieces[i];
                float pieceBot = p->y + DEBRIS_PIECE_H;
                if (p->y <= psy + PLAYER_H && pieceBot >= psy) {
                    /* HIT */
                    if (cheatInvincible) break;  /* god mode — skip damage */
                    playerHitThisEvent = 1;
                    playerHurt   = 1;
                    hurtStart    = now;
                    hurtAnimFrame = 0;
                    hurtAnimTick  = now;
                    {
                        int dmgIdx = rand() % 4;
                        if (sfxDamage[dmgIdx]) Mix_PlayChannel(-1, sfxDamage[dmgIdx], 0);
                    }
                    /* push direction: if player right-of-zone-centre → right, else left */
                    int zCentre = qx + qw / 2;
                    float pushDir = ((int)px >= zCentre) ? 1.0f : -1.0f;
                    player.vx  = HURT_PUSH_VX * pushDir;
                    player.vy  = 0.0f;   /* no bounce — flat slide */
                    /* keep onGround so he stays on platform */
                    level1ApplyDamage(0, 0, NULL);
                    break;
                }
            }
        }
    }

        if (level1ShouldUseDuoDeathRules() && gameState == GS_PLAYING && !flickerActiveP2) {
        float p2x = player2.x + PLAYER_W * 0.5f;
        float p2sy = player2.y - cameraY;
        if ((int)p2x >= qx && (int)p2x <= qx + qw) {
            for (int i = 0; i < debrisPieceCount; i++) {
                DebrisPiece *p = &debrisPieces[i];
                float pieceBot = p->y + DEBRIS_PIECE_H;
                if (p->y <= p2sy + PLAYER_H && pieceBot >= p2sy) {
                    if (cheatInvincible) break;
                    if (level1ApplyDamage(1, 0, NULL)) {
                        int zCentre = qx + qw / 2;
                        float pushDir = ((int)p2x >= zCentre) ? 1.0f : -1.0f;
                        playerHurtP2 = 1;
                        hurtStartP2 = now;
                        player2.vx = HURT_PUSH_VX * pushDir;
                        player2.vy = 0.0f;
                        flickerActiveP2 = 1;
                        flickerStartP2 = now;
                    }
                    break;
                }
            }
        }
    }

    /* Update hurt state */
    if (playerHurt) {
        Uint32 hElapsed = now - hurtStart;
        if (hElapsed >= (Uint32)HURT_LOCK_MS) {
            playerHurt    = 0;
            player.vx     = 0;
            hurtAnimFrame = 0;
        } else {
            /* map elapsed time evenly across all 36 frames — no looping */
            hurtAnimFrame = (int)(hElapsed * 36 / HURT_LOCK_MS);
            if (hurtAnimFrame > 35) hurtAnimFrame = 35;
        }
    }

    if (allGone) {
        debrisPhase = DEB_IDLE;
        debrisTimer = now;
        shakeAmt    = 1.5f;
    }
}

static void renderDebris(void)
{
    if (finalDebrisStopped) return;
    if (debrisPhase == DEB_IDLE && shakeAmt < 0.5f) return;

    int qx = debrisQuarter * (SCREEN_W / 4);
    int qw = SCREEN_W / 4;
    Uint32 now = SDL_GetTicks();

    /* Warning flash */
    if (debrisPhase == DEB_WARNING) {
        float t = (float)(now % 400) / 400.0f;
        float pulse = 0.40f + 0.20f * sinf(t * 6.2831f);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 220, 20, 20, (Uint8)(pulse * 255));
        SDL_Rect warn = {qx + shakeOX, 0 + shakeOY, qw, SCREEN_H};
        SDL_RenderFillRect(ren, &warn);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        return;
    }

    /* Falling pieces */
    if (debrisPhase == DEB_FALLING) {
        int fw = 1280, fh = 720;  /* transTex dimensions */
        for (int i = 0; i < debrisPieceCount; i++) {
            DebrisPiece *p = &debrisPieces[i];
            if (p->y > SCREEN_H + 4 || p->y + DEBRIS_PIECE_H < 0) continue;

            SDL_Texture *tx = debrisTex[p->texIdx];
            SDL_Rect dst = {
                (int)p->wobble + shakeOX,
                (int)p->y      + shakeOY,
                DEBRIS_PIECE_W, DEBRIS_PIECE_H
            };
            SDL_Point centre = {DEBRIS_PIECE_W / 2, DEBRIS_PIECE_H / 2};

            if (tx) {
                /* tile source from within texture */
                int srcY = p->srcY % (fh - DEBRIS_PIECE_H);
                SDL_Rect src2 = {0, srcY, fw, DEBRIS_PIECE_H};
                SDL_RenderCopyEx(ren, tx, &src2, &dst, p->angle, &centre, SDL_FLIP_NONE);
            } else {
                /* fallback grey slab */
                SDL_SetRenderDrawColor(ren, 100, 90, 80, 220);
                SDL_RenderFillRect(ren, &dst);
            }
        }
    }
}

static void renderHurtOverlay(void)
{
    if (!playerHurt || !texHurt) return;

    int tw, th;
    if (!getTextureSize(texHurt, &tw, &th)) return;
    int fw = tw / 6;
    int fh = th / 6;

    /* only use first 20 frames (skip last 16) */
    int frame = hurtAnimFrame;
    if (frame > 19) frame = 19;

    int col  = frame % 6;
    int row2 = frame / 6;
    SDL_Rect src2 = {col * fw, row2 * fh, fw, fh};
    int sx = (int)player.x - (PLAYER_W / 2);
    int sy = (int)(player.y - cameraY);
    SDL_Rect dst = {sx, sy, PLAYER_W+50, PLAYER_H};
    /* flip based on last facing direction */
    SDL_RendererFlip flip = player.facingRight ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
    SDL_RenderCopyEx(ren, texHurt, &src2, &dst, 0, NULL, flip);
}

static void spawnDrop(Gauge *g);
static void updateDrops(Gauge *g, Uint32 now);

static void updateGauges(void)
{
    Uint32 now = SDL_GetTicks();

    for (int i = 0; i < gaugeCount; i++) {
        Gauge *g = &gauges[i];
        if (!g->active) continue;
        if (worldYOutsideUpdateRange(g->floorY)) continue;

        /* animate water frame always when flowing */
        if (g->phase == GAUGE_FLOWING || g->phase == GAUGE_AGITATE || g->phase == GAUGE_DRAINING) {
            if (now - g->animTick >= 1000u / 12) {
                g->animFrame = (g->animFrame + 1) % 36;
                g->animTick  = now;
            }
        }

        /* Start timer only once gauge is within view */
        if (!g->started) {
            float scy2 = g->floorY - cameraY;
            if (scy2 >= 0 && scy2 <= SCREEN_H) {
                g->started    = 1;
                g->phaseStart = now;
            } else {
                continue;  /* not on screen yet — don't tick */
            }
        }

        /* update droplet particles */
        if (g->phase == GAUGE_FLOWING || g->phase == GAUGE_DRAINING)
            updateDrops(g, now);

        Uint32 elapsed = now - g->phaseStart;

        switch (g->phase) {
            case GAUGE_IDLE:
                if (elapsed >= GAUGE_COOLDOWN_MS) {
                    g->phase      = GAUGE_AGITATE;
                    g->phaseStart = now;
                }
                break;

            case GAUGE_AGITATE:
                /* shake for 600ms then shoot */
                if (elapsed >= 600) {
                    g->phase      = GAUGE_FLOWING;
                    g->phaseStart = now;
                    g->flowX      = (float)g->nozzleX-200; /* start at right wall */
                }
                break;

            case GAUGE_FLOWING: {
                /* player collision — per-droplet AABB.
                   dp->y is an offset from baseScreenY (floorY - cameraY),
                   ranging from -(FLOOR_GAP-2) to -2, so the full vertical
                   band between the two platforms is covered.               */
                if (!cheatInvincible && !flickerActive && now - waterHitTime >= (Uint32)WATER_HIT_COOLDOWN) {
                    float px1 = player.x, px2 = player.x + PLAYER_W;
                    float py1 = player.y - cameraY, py2 = py1 + PLAYER_H;
                    int bsy = (int)(g->floorY - cameraY);
                    for (int dd = 0; dd < DROP_MAX && !flickerActive; dd++) {
                        WaterDrop *dp = &g->drops[dd];
                        if (!dp->alive) continue;
                        float dx1 = dp->x, dx2 = dp->x + dp->w;
                        float dy1 = (float)bsy + dp->y;
                        float dy2 = dy1 + (float)dp->h;
                        if (px2 > dx1 && px1 < dx2 && py2 > dy1 && py1 < dy2) {
                            waterHitTime = now;
                            if (level1ApplyDamage(0, 0, NULL)) {
                                player.vy  = -14.0f;
                                player.vx  = 0;
                                player.onGround = 0;
                                flickerActive = 1;
                                flickerStart  = now;
                            }
                        }
                    }
                }

                /* after 1 second: stop spawning, let drops drain out */
                if (elapsed >= GAUGE_FLOW_MS) {
                    g->phase      = GAUGE_DRAINING;
                    g->phaseStart = now;
                }
                break;
            }

            case GAUGE_DRAINING: {
                /* No new spawns. Wait for every drop to exit left, then idle. */
                int anyAlive = 0;
                for (int dd = 0; dd < DROP_MAX; dd++)
                    if (g->drops[dd].alive) { anyAlive = 1; break; }

                if (!anyAlive) {
                    g->phase      = GAUGE_IDLE;
                    g->phaseStart = now;
                    g->flowX      = (float)SCREEN_W;
                    g->nozzleX    = SCREEN_W;
                }

                /* collision while drops are still on screen */
                if (!cheatInvincible && !flickerActive && now - waterHitTime >= (Uint32)WATER_HIT_COOLDOWN) {
                    float px1 = player.x, px2 = player.x + PLAYER_W;
                    float py1 = player.y - cameraY, py2 = py1 + PLAYER_H;
                    int bsy = (int)(g->floorY - cameraY);
                    for (int dd = 0; dd < DROP_MAX && !flickerActive; dd++) {
                        WaterDrop *dp = &g->drops[dd];
                        if (!dp->alive) continue;
                        float dx1 = dp->x, dx2 = dp->x + dp->w;
                        float dy1 = (float)bsy + dp->y;
                        float dy2 = dy1 + (float)dp->h;
                        if (px2 > dx1 && px1 < dx2 && py2 > dy1 && py1 < dy2) {
                            waterHitTime = now;
                            if (level1ApplyDamage(0, 0, NULL)) {
                                player.vy  = -14.0f;
                                player.vx  = 0;
                                player.onGround = 0;
                                flickerActive = 1;
                                flickerStart  = now;
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    if (level1ShouldUseDuoDeathRules() && !cheatInvincible &&
        !flickerActiveP2 && now - waterHitTimeP2 >= (Uint32)WATER_HIT_COOLDOWN) {
        for (int gi = 0; gi < gaugeCount; gi++) {
            Gauge *g = &gauges[gi];
            if (!g->active) continue;
            if (g->phase != GAUGE_FLOWING && g->phase != GAUGE_DRAINING) continue;

            float px1 = player2.x, px2 = player2.x + PLAYER_W;
            float py1 = player2.y - cameraY, py2 = py1 + PLAYER_H;
            int bsy = (int)(g->floorY - cameraY);

            for (int dd = 0; dd < DROP_MAX; dd++) {
                WaterDrop *dp = &g->drops[dd];
                if (!dp->alive) continue;
                if (px2 > dp->x && px1 < dp->x + dp->w &&
                    py2 > (float)bsy + dp->y &&
                    py1 < (float)bsy + dp->y + (float)dp->h) {
                    waterHitTimeP2 = now;
                    if (level1ApplyDamage(1, 0, NULL)) {
                        flickerActiveP2 = 1;
                        flickerStartP2 = now;
                    }
                    break;
                }
            }
        }
    }
}

static void spawnDrop(Gauge *g)
{
    /* find a dead slot */
    for (int d = 0; d < DROP_MAX; d++) {
        WaterDrop *dp = &g->drops[d];
        if (dp->alive) continue;

        /* X: spawn at the nozzle (right wall) with tiny scatter */
        dp->x  = (float)(SCREEN_W - 20 + rand() % 20);

        /* Y: scatter across the FULL vertical band between the two platforms.
           dp->y is a screen-Y offset relative to baseScreenY (= floorY - cameraY).
             0           = floor surface  (bottom margin)
           -FLOOR_GAP    = floor above    (top margin)
           We leave 2px margin top and bottom so drops don't clip edges.  */
        int bandH = FLOOR_GAP - 4;
        dp->y  = (float)(-(rand() % bandH) - 2);

        /* Each drop gets its own random leftward speed → chaotic rhythm.
           Range 4 .. 12 px/frame keeps the overall flow visually fast.   */
        dp->vx = -(4.0f + (float)(rand() % 80) * 0.10f);

        /* No vertical drift — each drop stays in its horizontal lane.    */
        dp->vy = 0.0f;

        /* Smaller drops: 10..18 px wide, 10..16 px tall                  */
        dp->w  = 10 + rand() % 9;
        dp->h  = 10 + rand() % 7;

        dp->alive = 1;
        dp->alpha = 0.55f + (float)(rand() % 45) * 0.01f;
        return;
    }
}

static void updateDrops(Gauge *g, Uint32 now)
{
    /* ── Spawn from nozzle for GAUGE_FLOW_MS (1s) only ──
       After that spawning stops; existing drops finish their travel.   */
    if (g->phase == GAUGE_FLOWING) {
        int burst = 6 + rand() % 5;   /* 6–10 drops per frame = dense stream */
        for (int b = 0; b < burst; b++) spawnDrop(g);
        g->lastSpawn = now;
    }
    /* DRAINING: no new spawns — existing drops exit left on their own */

    /* ── Advance every alive drop leftward at its own speed ── */
    for (int d = 0; d < DROP_MAX; d++) {
        WaterDrop *dp = &g->drops[d];
        if (!dp->alive) continue;
        dp->x += dp->vx;
        if (dp->x + dp->w < 0.0f) dp->alive = 0;
    }
}

static void renderGauges(void)
{
    static const int GDRAW_W = 300;
    static const int GDRAW_H = 160;

    for (int i = 0; i < gaugeCount; i++) {
        Gauge *g = &gauges[i];
        if (!g->active) continue;

        int baseScreenY = (int)(g->floorY - cameraY);
        if (baseScreenY > SCREEN_H + 80 + (int)RENDER_PAD_Y ||
            baseScreenY < -80 - (int)RENDER_PAD_Y) continue;

        /* agitate shake */
        int agShake = 0;
        if (g->phase == GAUGE_AGITATE)
            agShake = (int)(SDL_GetTicks() / 35) % 5 - 2;

        /* ── Gauge image ── */
        int gx2 = SCREEN_W - GDRAW_W + shakeOX + agShake;
        int gy2 = baseScreenY - GDRAW_H + shakeOY;
        SDL_Rect gdst = {gx2 + 150, gy2, GDRAW_W, GDRAW_H};
        if (texGauge)
            SDL_RenderCopy(ren, texGauge, NULL, &gdst);
        else {
            SDL_SetRenderDrawColor(ren, 60, 80, 60, 255);
            SDL_RenderFillRect(ren, &gdst);
        }

        /* ── Droplets ── */
        if (g->phase == GAUGE_FLOWING || g->phase == GAUGE_DRAINING) {
            for (int d = 0; d < DROP_MAX; d++) {
                WaterDrop *dp = &g->drops[d];
                if (!dp->alive) continue;
                /* dp->x  = absolute screen-x
                   dp->y  = offset from baseScreenY (negative = above floor) */
                int sx = (int)dp->x  + shakeOX;
                int sy = baseScreenY + (int)dp->y + shakeOY;
                /* cull if out of the visible band */
                if (sy + dp->h < -(int)RENDER_PAD_Y || sy > SCREEN_H + (int)RENDER_PAD_Y) continue;
                SDL_Rect dst = {sx, sy, dp->w, dp->h};
                if (texWater) {
                    SDL_SetTextureAlphaMod(texWater, (Uint8)(dp->alpha * 255));
                    SDL_RenderCopy(ren, texWater, NULL, &dst);
                    SDL_SetTextureAlphaMod(texWater, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 20, 180, 255, (Uint8)(dp->alpha * 210));
                    SDL_RenderFillRect(ren, &dst);
                }
            }
        }
    }
}

static int playerSharesDogFloor(const Dog *d)
{
    float feet = player.y + PLAYER_H;
    if (fabsf(feet - d->floorY) <= (PLAYER_H * 0.65f)) {
        return 1;
    }
    return 0;
}

static void updateDogs(void)
{
    /* Freeze during freeze-dialogue sequences */
    if (gameState == GS_DIALOGUE && dlgFreezeGame) return;
    Uint32 now = SDL_GetTicks();
    int playerNearDogOnFloor = 0;

    for (int i = 0; i < dogCount; i++) {
        Dog *d = &dogs[i];
        if (!d->active) continue;
        if (worldYOutsideUpdateRange(d->floorY)) continue;

        if (!playerNearDogOnFloor && playerSharesDogFloor(d)) {
            playerNearDogOnFloor = 1;
        }

        /* animate */
        if (now - d->lastTick >= 1000u / DOG_ANIM_FPS) {
            d->frame = (d->frame + 1) % 36;
            d->lastTick = now;
        }

        /* move */
        d->x += DOG_SPEED * d->dir;

        /* bounce at slab bounds — no gap crossing */
        if (d->dir > 0 && d->x >= (float)d->rightBound) {
            d->x = (float)d->rightBound;
            d->dir = -1;
        } else if (d->dir < 0 && d->x <= (float)d->leftBound) {
            d->x = (float)d->leftBound;
            d->dir = 1;
        }

        /* player collision — skip if flicker (invincibility) active
           Hitbox is the BODY only (bottom 32px of the sprite) so a
           well-timed jump clears the top of the dog safely.          */
        if (!cheatInvincible && !flickerActive && now - dogHitTime >= (Uint32)DOG_HIT_COOLDOWN_MS) {
            float px1 = player.x,          px2 = player.x + PLAYER_W;
            float py1 = player.y,          py2 = player.y + PLAYER_H;
            /* body hitbox: horizontally inset 6px each side, vertically
               only the lower 32px (feet to mid-back), top 18px is safe */
            float dx1 = d->x + 6.0f,       dx2 = d->x + DOG_W - 6.0f;
            float dy2 = d->floorY;          /* floor surface             */
            float dy1 = dy2 - 32.0f;        /* only bottom 32px deadly   */
            if (px2 > dx1 && px1 < dx2 && py2 > dy1 && py1 < dy2) {
                /* HIT */
                dogHitTime = now;
                if (sfxDogBark) Mix_PlayChannel(-1, sfxDogBark, 0);
                {
                    int dmgIdx = rand() % 4;
                    if (sfxDamage[dmgIdx]) Mix_PlayChannel(-1, sfxDamage[dmgIdx], 0);
                }
                if (level1ApplyDamage(0, 0, NULL)) {
                    /* 1 second flicker = invincibility */
                    flickerActive = 1;
                    flickerStart  = now;
                }
            }
        }
    }

    if (level1ShouldUseDuoDeathRules() && !flickerActiveP2 &&
        now - dogHitTimeP2 >= (Uint32)DOG_HIT_COOLDOWN_MS) {
        for (int i = 0; i < dogCount; i++) {
            Dog *d = &dogs[i];
            float px1 = player2.x + 8.0f, px2 = player2.x + PLAYER_W - 8.0f;
            float py1 = player2.y + PLAYER_H - 24.0f;
            float py2 = player2.y + PLAYER_H;
            float dx1 = d->x + 6.0f, dx2 = d->x + DOG_W - 6.0f;
            float dy2 = d->floorY;
            float dy1 = dy2 - 32.0f;
            if (px2 > dx1 && px1 < dx2 && py2 > dy1 && py1 < dy2) {
                dogHitTimeP2 = now;
                if (level1ApplyDamage(1, 0, NULL)) {
                    flickerActiveP2 = 1;
                    flickerStartP2 = now;
                }
            }
        }
    }

    if (playerNearDogOnFloor && now - dogBarkNearTime >= (Uint32)DOG_BARK_NEAR_INTERVAL_MS) {
        if (sfxDogBark) {
            Mix_PlayChannel(-1, sfxDogBark, 0);
            dogBarkNearTime = now;
        }
    }
}

static void renderDogs(void)
{
    int tw = 0, th = 0;
    if (texDog && !getTextureSize(texDog, &tw, &th)) return;
    int fw = tw > 0 ? tw / 6 : 1;
    int fh = th > 0 ? th / 6 : 1;

    for (int i = 0; i < dogCount; i++) {
        Dog *d = &dogs[i];
        if (!d->active) continue;
        int sy = (int)(d->floorY - cameraY) - DOG_H;
        if (sy > SCREEN_H + DOG_H + (int)RENDER_PAD_Y ||
            sy + DOG_H < -DOG_H - (int)RENDER_PAD_Y) continue;

        int col  = d->frame % 6;
        int row2 = d->frame / 6;

        SDL_Rect dst = {(int)d->x + shakeOX, sy + shakeOY, DOG_W, DOG_H};

        if (texDog) {
            SDL_Rect src2 = {col * fw, row2 * fh, fw, fh};
            /* dog sheet faces right — flip when going left */
            SDL_RendererFlip flip = (d->dir < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(ren, texDog, &src2, &dst, 0, NULL, flip);
        } else {
            /* fallback */
            SDL_SetRenderDrawColor(ren, 180, 120, 40, 255);
            SDL_RenderFillRect(ren, &dst);
        }
    }
}

static void initRats(void)
{
    for (int i = 0; i < RAT_COUNT; i++) {
        Rat *r   = &rats[i];
        r->dir   = (rand() % 2) ? 1 : -1;
        r->speed = 1.6f + (float)(rand() % 30) * 0.12f;
        /* spread randomly across full screen width */
        r->x     = (float)(rand() % (SCREEN_W + RAT_W * 2) - RAT_W);
        /* row: vertical offset within the bottom strip (5 rows) */
        r->row   = rand() % 5;
        r->frame    = rand() % 36;
        r->lastTick = SDL_GetTicks() - (Uint32)(rand() % (1000 / RAT_ANIM_FPS));
    }
    ratsInited = 1;
}

static void renderDeathLine(void)
{
    if (!ratsVisible) return;   /* hidden until end of intro dialogue */
    if (finalDebrisStopped) return;  /* disappears on final floor */
    if (!ratsInited) initRats();

    /* Rats run AT the bottom of the screen, feet at screen bottom edge */
    /* ratSlideY > 0 means they're still sliding up from below — offset downward */
    int baseY = SCREEN_H - RAT_H + (int)ratSlideY;
    Uint32 now = SDL_GetTicks();

    int fw = 3636 / 6;   /* 606 */
    int fh = 2220 / 6;   /* 370 */

    for (int i = 0; i < RAT_COUNT; i++) {
        Rat *r = &rats[i];

        /* Advance animation */
        if (now - r->lastTick >= 1000u / RAT_ANIM_FPS) {
            r->frame = (r->frame + 1) % 36;
            r->lastTick = now;
        }

        /* Move */
        r->x += r->speed * r->dir;

        /* Bounce: cross border fully then reverse */
        if (r->dir > 0 && r->x > SCREEN_W + RAT_W * 0.5f) {
            r->x = SCREEN_W + RAT_W * 0.5f;
            r->dir = -1;
        }
        if (r->dir < 0 && r->x < -(float)RAT_W * 0.5f) {
            r->x = -(float)RAT_W * 0.5f;
            r->dir = 1;
        }

        /* Y: stagger rows slightly so they don't all sit at same Y */
        int ratY = baseY - r->row * 8 +20;

        if (texRat) {
            int col = r->frame % 6;
            int row2 = r->frame / 6;
            SDL_Rect src2 = {col * fw, row2 * fh, fw, fh};
            SDL_Rect dst  = {(int)r->x, ratY, RAT_W, RAT_H};
            SDL_RendererFlip flip = (r->dir > 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(ren, texRat, &src2, &dst, 0, NULL, flip);
        } else {
            SDL_SetRenderDrawColor(ren, 100, 60, 30, 255);
            SDL_Rect fb = {(int)r->x, ratY, RAT_W, RAT_H};
            SDL_RenderFillRect(ren, &fb);
        }
    }


}

static float clamp01f(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static int minimapAxisPosition(float value, float minValue, float maxValue, int startPx, int spanPx)
{
    float norm = 0.5f;
    if (maxValue - minValue > 0.0001f) {
        norm = (value - minValue) / (maxValue - minValue);
    }
    norm = clamp01f(norm);
    return startPx + (int)lroundf(norm * (float)(spanPx - 1));
}

static void drawMiniMapDot(int cx, int cy, int radius, Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(ren, r, g, b, 255);
    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            if (ox * ox + oy * oy > radius * radius) continue;
            SDL_RenderDrawPoint(ren, cx + ox, cy + oy);
        }
    }
}

static void renderMiniMap(void)
{
    if (floorCount <= 0) return;

    const int panelW = 72;
    const int panelH = 252;
    const int panelX = SCREEN_W - panelW - 14;
    const int panelY = 42;
    const int trackPad = 9;
    const int trackX = panelX + trackPad;
    const int trackY = panelY + 18;
    const int trackW = panelW - trackPad * 2;
    const int trackH = panelH - 30;

    const float p1cx = player.x + PLAYER_W * 0.5f;
    const float p1cy = player.y + PLAYER_H * 0.5f;
    float worldTop = floors[floorCount - 1].worldY - (float)PLAYER_H;
    float worldBottom = floors[0].worldY + (float)FLOOR_H;

    if (p1cy < worldTop) worldTop = p1cy;
    if (p1cy > worldBottom) worldBottom = p1cy;

    if (player2Enabled && level1DuoEnabled()) {
        const float p2cy = player2.y + PLAYER_H * 0.5f;
        if (p2cy < worldTop) worldTop = p2cy;
        if (p2cy > worldBottom) worldBottom = p2cy;
    }

    dr(panelX, panelY, panelW, panelH, 12, 18, 28, 190);
    dro(panelX, panelY, panelW, panelH, 126, 156, 196, 230);
    dr(trackX, trackY, trackW, trackH, 24, 34, 52, 225);
    dro(trackX, trackY, trackW, trackH, 86, 112, 146, 220);
    dr(trackX + trackW / 2, trackY, 1, trackH, 70, 96, 132, 180);

    {
        int p1x = minimapAxisPosition(p1cx, 0.0f, (float)SCREEN_W, trackX, trackW);
        int p1y = minimapAxisPosition(p1cy, worldTop, worldBottom, trackY, trackH);
        drawMiniMapDot(p1x, p1y, 3, 235, 60, 60);
    }

    if (player2Enabled && level1DuoEnabled()) {
        float p2cx = player2.x + PLAYER_W * 0.5f;
        float p2cy = player2.y + PLAYER_H * 0.5f;
        int p2x = minimapAxisPosition(p2cx, 0.0f, (float)SCREEN_W, trackX, trackW);
        int p2y = minimapAxisPosition(p2cy, worldTop, worldBottom, trackY, trackH);
        drawMiniMapDot(p2x, p2y, 3, 70, 150, 255);
    }

    if (font) dt(font, "MAP", panelX + 19, panelY + 2, 195, 215, 240);
}

/* ═══════════════════════════════════════════════════════════
   HUD (world overlay)
═══════════════════════════════════════════════════════════ */
static void renderHUD(void)
{
    Uint32 now = SDL_GetTicks();
    if (level1DuoEnabled()) {
        const int p1Y = 68;
        const int p2Y = 92;
        for (int i = 0; i < MAX_LIVES; i++) {
            if (i < lives) dr(14 + i * 24, p1Y, 18, 18, 220,  60,  60, 255);
            else           dr(14 + i * 24, p1Y, 18, 18,  80,  40,  40, 255);
        }
        for (int i = 0; i < player.extraHearts; i++)
            dr(14 + (MAX_LIVES + i) * 24, p1Y, 18, 18, 255, 130, 130, 255);

        for (int i = 0; i < MAX_LIVES; i++) {
            if (i < livesP2) dr(14 + i * 24, p2Y, 18, 18,  70, 150, 255, 255);
            else             dr(14 + i * 24, p2Y, 18, 18,  30,  55,  90, 255);
        }
        for (int i = 0; i < player2.extraHearts; i++)
            dr(14 + (MAX_LIVES + i) * 24, p2Y, 18, 18, 120, 220, 255, 255);
    } else {
        /* Solo health bar */
        for (int i = 0; i < MAX_LIVES; i++) {
            if (i < lives) dr(14 + i * 24, 68, 18, 18, 220,  60,  60, 255);
            else           dr(14 + i * 24, 68, 18, 18,  80,  40,  40, 255);
        }
        for (int i = 0; i < player.extraHearts; i++)
            dr(14 + (MAX_LIVES + i) * 24, 68, 18, 18, 60, 120, 255, 255);
    }

    char buf[80];

    /* Power-up icons */
    int px2 = 14;
    if (player.jetpackFloors > 0) {
        dr(px2, 66, 18, 18, 180, 80,  30, 255);
        dt(font, "J", px2 + 5, 67, 255, 255, 255); px2 += 26;
    }
    if (player.magnetTimer > 0 && now < player.magnetTimer) {
        Uint32 rem = (player.magnetTimer - now) / 1000 + 1;
        dr(px2, 66, 18, 18, 100, 100, 255, 255);
        char mb[16]; snprintf(mb, sizeof(mb), "%us", (unsigned)rem);
        dt(font, mb, px2 + 5, 67, 255, 255, 255); px2 += 26;
    }
    if (player.shoesTimer > 0 && now < player.shoesTimer) {
        Uint32 rem = (player.shoesTimer - now) / 1000 + 1;
        dr(px2, 66, 18, 18, 80, 220, 160, 255);
        char sb[16]; snprintf(sb, sizeof(sb), "%us", (unsigned)rem);
        dt(font, sb, px2 + 5, 67, 255, 255, 255); px2 += 26;
    }
    (void)px2;

    /* Key counter — centred top, big and clear */
    {
        char kbuf[32];
        snprintf(kbuf, sizeof(kbuf), "KEYS: %d / 10", keysHeld);
        dtc(bigFont, kbuf, 10, 255, 210, 50);
    }

    /* Height score */
    int h = (int)((float)(SCREEN_H - 80) - player.y);
    int hY = level1DuoEnabled() ? 114 : 90;
    if (h > score) score = h;
    snprintf(buf, sizeof(buf), "HEIGHT  %d", score);
    dt(font, buf, 14, hY, 255, 255, 255);

    /* State overlays */
    if (gameState == GS_SECOND_CHANCE)
        dtc(font, "SECOND CHANCE!", SCREEN_H / 2 - 14, 220, 220, 60);

    if (gameState == GS_COUNTDOWN) {
        char cb[4]; snprintf(cb, sizeof(cb), "%d", countdown);
        dtc(bigFont, cb, SCREEN_H / 2 - 34, 255, 220, 60);
    }

    renderMiniMap();
}

/* ═══════════════════════════════════════════════════════════
   DIALOGUE SYSTEM
═══════════════════════════════════════════════════════════ */

/* Wrap text to fit within maxW pixels — returns number of lines written.
   Lines are written into out[] array (each up to 256 chars). */
static int wrapText(TTF_Font *f, const char *text, int maxW,
                    char out[][256], int maxLines)
{
    if (!f || !text) return 0;
    int lineN = 0;
    char word[128]; char line[256] = "";
    const char *p = text;
    while (lineN < maxLines) {
        /* read next word */
        int wi = 0;
        while (*p && *p != ' ' && wi < 127) word[wi++] = *p++;
        word[wi] = '\0';
        if (*p == ' ') p++;
        if (wi == 0 && *p == '\0') break; /* done */
        /* test appending word to current line */
        char test[256];
        if (line[0]) {
            size_t pos = 0;
            while (line[pos] && pos < sizeof(test) - 1) {
                test[pos] = line[pos];
                pos++;
            }
            if (pos < sizeof(test) - 1) test[pos++] = ' ';
            for (int j = 0; word[j] && pos < sizeof(test) - 1; j++)
                test[pos++] = word[j];
            test[pos] = '\0';
        } else {
            snprintf(test, sizeof(test), "%s", word);
        }
        int tw, th;
        TTF_SizeText(f, test, &tw, &th);
        if (tw > maxW && line[0]) {
            /* flush current line */
            snprintf(out[lineN++], 256, "%s", line);
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        if (*p == '\0') { /* end of text */
            if (line[0]) snprintf(out[lineN++], 256, "%s", line);
            break;
        }
    }
    return lineN;
}

static const WrapCache *getDialogueWrapCache(TTF_Font *f, const char *text,
                                             int maxW, int maxLines)
{
    int cappedLines = maxLines;
    if (cappedLines > 8) cappedLines = 8;
    if (cappedLines < 0) cappedLines = 0;

    if (!dlgWrapCache.valid ||
        dlgWrapCache.font != f ||
        dlgWrapCache.maxW != maxW ||
        dlgWrapCache.maxLines != cappedLines ||
        strcmp(dlgWrapCache.text, text ? text : "") != 0) {
        dlgWrapCache.valid = 1;
        dlgWrapCache.font = f;
        dlgWrapCache.maxW = maxW;
        dlgWrapCache.maxLines = cappedLines;
        snprintf(dlgWrapCache.text, sizeof(dlgWrapCache.text), "%s", text ? text : "");
        dlgWrapCache.lineCount = wrapText(f, dlgWrapCache.text, maxW,
                                          dlgWrapCache.lines, cappedLines);
    }

    return &dlgWrapCache;
}

static void startDialogueSeq(DlgSeqID seq, const DlgLine *lines, int count)
{
    /* Only reset choice state when entering a genuinely new sequence */
    if (seq != dlgSeqActive)
        dlgChoiceMade = 0;

    dlgSeqActive  = seq;
    dlgCurLines   = lines;
    dlgCurCount   = count;
    dlgLineIdx    = 0;
    dlgCharShown  = 0;
    dlgNextCharMs = SDL_GetTicks() + dlgCharDelay(lines[0].text, 0);
    dlgFullyShown = 0;
    dlgShowChoice = 0;
    dlgChoiceSel  = 0;
    dlgAutoAdvanceMs = 0;
    dlgWrapCache.valid = 0;
    /* Non-freeze sequences: STREET, POSTFRIDGE, MICROWAVE overlay gameplay */
    dlgFreezeGame = (seq == DLGSEQ_STREET    ||
                     seq == DLGSEQ_POSTFRIDGE ||
                     seq == DLGSEQ_MICROWAVE  ||
                     seq == DLGSEQ_POSTMICRO) ? 0 : 1;
    /* Final sequences always freeze (player can't move during final scene) */
    if (seq == DLGSEQ_FINAL_PRE  || seq == DLGSEQ_FINAL_GUN ||
        seq == DLGSEQ_FINAL_FORG || seq == DLGSEQ_MARV_REACT)
        dlgFreezeGame = 1;
    gameState     = GS_DIALOGUE;
}

static void startDialogue(void)
{
    startDialogueSeq(DLGSEQ_INTRO, dlgLines, DLG_LINE_COUNT);
}

static void advanceDialogue(void)
{
    /* Confirm choice selection */
    if (dlgShowChoice) {
        dlgShowChoice = 0;
        dlgChoiceMade = 1;
        if (dlgSeqActive == DLGSEQ_SEWER) {
            if (dlgChoiceSel == 0)
                startDialogueSeq(DLGSEQ_SEWER, dlgSewerIgnore, DLG_SEWER_IGNORE_COUNT);
            else
                startDialogueSeq(DLGSEQ_SEWER, dlgSewerCrashout, DLG_SEWER_CRASHOUT_COUNT);
        } else if (dlgSeqActive == DLGSEQ_FINAL_PRE) {
            finalChoiceMade = dlgChoiceSel; /* 0=gun, 1=forget */
            /* Start consequence screen */
            gameState    = GS_CONSEQUENCE;
            conseqPhase  = 0;
            conseqFade   = 0.0f;
            conseqTimer  = SDL_GetTicks();
        }
        return;
    }

    if (!dlgFullyShown) {
        dlgCharShown  = (int)strlen(dlgCurLines[dlgLineIdx].text);
        dlgFullyShown = 1;
        return;
    }

    dlgLineIdx++;
    if (dlgLineIdx >= dlgCurCount) {
        if (dlgSeqActive == DLGSEQ_INTRO) {
            ratsVisible = 1;
            ratSlideY   = (float)(RAT_H + 60);
            startRatsLoopSfx();
            gameState   = GS_PLAYING;
        } else if (dlgSeqActive == DLGSEQ_SEWER && !dlgChoiceMade) {
            /* Pre-choice line done: show the choice menu */
            dlgShowChoice = 1;
            dlgChoiceSel  = 0;
        } else if (dlgSeqActive == DLGSEQ_STREET) {
            /* Street dialogue done: pause debris, begin fridge countdown */
            fridgePhase        = FRIDGE_COUNTDOWN;
            fridgeTimer        = SDL_GetTicks();
            fridgeDebrisPaused = 1;
            gameState          = GS_PLAYING;
        } else if (dlgSeqActive == DLGSEQ_POSTFRIDGE) {
            /* Post-fridge lines done: resume normally */
            gameState = GS_PLAYING;
        } else if (dlgSeqActive == DLGSEQ_MICROWAVE) {
            /* Microwave dialogue done: pause debris, start MW sequence */
            mwSequenceActive = 1;
            mwDebrisPaused   = 1;
            mwDropsDone      = 0;
            mwSimulStage     = 0;
            mwSimulCount     = 0;
            mwPhase          = MW_WARNING;
            mwTimer          = SDL_GetTicks();
            mwX = player.x + PLAYER_W/2.0f - MW_W/2.0f;
            if (mwX < 0) mwX = 0;
            if (mwX + MW_W > SCREEN_W) mwX = (float)(SCREEN_W - MW_W);
            mwWarnX = mwX;
            gameState = GS_PLAYING;
        } else if (dlgSeqActive == DLGSEQ_POSTMICRO) {
            /* Post-micro banter done: back to gameplay */
            gameState = GS_PLAYING;
        } else if (dlgSeqActive == DLGSEQ_FINAL_PRE && !dlgChoiceMade) {
            /* Pre-choice lines done: show the final choice */
            dlgShowChoice = 1;
            dlgChoiceSel  = 0;
        } else if (dlgSeqActive == DLGSEQ_FINAL_GUN ||
                   dlgSeqActive == DLGSEQ_FINAL_FORG) {
            /* Branch dialogue done: open door first, then Harry walks in */
            doorState     = DOOR_OPENING;
            doorAnimFrame = 0;
            doorAnimTick  = SDL_GetTicks();
            if (sfxDoorOpen) Mix_PlayChannel(-1, sfxDoorOpen, 0);
            gameState     = GS_DOOR_OPENING;
        } else if (dlgSeqActive == DLGSEQ_MARV_REACT) {
            /* Post-Harry one-liner done: give player control to enter door */
            gameState = GS_PLAYING;
        } else {
            /* Branch/sewer done: resume gameplay */
            gameState = GS_PLAYING;
        }
    } else {
        dlgCharShown  = 0;
        dlgNextCharMs = SDL_GetTicks() + dlgCharDelay(dlgCurLines[dlgLineIdx].text, 0);
        dlgFullyShown = 0;
    }
}

/* ── Fridge drop ── */
static void updateFridge(void)
{
    if (fridgePhase == FRIDGE_IDLE) return;
    Uint32 now     = SDL_GetTicks();
    Uint32 elapsed = now - fridgeTimer;

    if (fridgePhase == FRIDGE_COUNTDOWN) {
        if (elapsed >= FRIDGE_WAIT_MS) {
            fridgePhase = FRIDGE_SHAKING;
            fridgeTimer = now;
            shakeAmt    = 7.0f;
        }
        return;
    }

    if (fridgePhase == FRIDGE_SHAKING) {
        /* keep hammering shake */
        shakeAmt = 7.0f;
        shakeOX  = (rand() % 9) - 4;
        shakeOY  = (rand() % 9) - 4;
        if (elapsed >= FRIDGE_SHAKE_MS) {
            /* launch fridge from above screen in world-space */
            fridgeY     = cameraY - FRIDGE_H - 20.0f;
            fridgePhase = FRIDGE_DROPPING;
            fridgeTimer = now;
            fridgeKillTriggered = 0;
            if (sfxApplianceFall) Mix_PlayChannel(-1, sfxApplianceFall, 0);
        }
        return;
    }

    if (fridgePhase == FRIDGE_DROPPING) {
        fridgeY += FRIDGE_SPEED;

        /* Screen-space Y of fridge top */
        float screenTop = fridgeY - cameraY;
        float screenBot = screenTop + FRIDGE_H;

        /* Kill check — one shot, instant death in hit zone */
        if (!fridgeKillTriggered && screenBot >= 0 && screenTop <= SCREEN_H) {
            fridgeKillTriggered = 1;
            /* Convert player to screen-space for comparison */
            float px1 = player.x,              px2 = player.x + PLAYER_W;
            float py1 = player.y - cameraY,    py2 = py1 + PLAYER_H;
            /* hitbox: full left half of screen, full height of fridge */
            if (px1 < FRIDGE_X + FRIDGE_W && px2 > FRIDGE_X &&
                py2 > screenTop && py1 < screenBot) {
                if (!cheatInvincible) {
                    level1ApplyDamage(0, 1, "You've been crushed by a fridge twin");
                }
            }

            if (level1ShouldUseDuoDeathRules()) {
                float p2x1 = player2.x,           p2x2 = player2.x + PLAYER_W;
                float p2y1 = player2.y - cameraY, p2y2 = p2y1 + PLAYER_H;
                if (p2x1 < FRIDGE_X + FRIDGE_W && p2x2 > FRIDGE_X &&
                    p2y2 > screenTop && p2y1 < screenBot) {
                    level1ApplyDamage(1, 1, "You've been crushed by a fridge twin");
                }
            }
        }

        /* Fridge has fully passed below screen — start fade out */
        if (screenTop > SCREEN_H + 20) {
            fridgePhase  = FRIDGE_FADING;
            fridgeTimer  = now;
            fridgeAlpha  = 255.0f;
        }
        return;
    }

    if (fridgePhase == FRIDGE_FADING) {
        float t = (float)elapsed / (float)FRIDGE_FADE_MS;
        fridgeAlpha = 255.0f * (1.0f - t);
        if (t >= 1.0f) {
            fridgePhase        = FRIDGE_IDLE;
            fridgeAlpha        = 255.0f;
            fridgeDebrisPaused = 0;  /* debris resumes */
            /* reset debris timer so it doesn't fire instantly */
            debrisPhase = DEB_IDLE;
            debrisTimer = SDL_GetTicks();
            /* trigger post-fridge reaction dialogue */
            startDialogueSeq(DLGSEQ_POSTFRIDGE, dlgPostFridge, DLG_POSTFRIDGE_COUNT);
        }
        return;
    }
}

/* Blur helper: renders the fridge texture N times offset to fake motion blur */
static void renderFridgeBlur(int sx, int sy, int alpha)
{
    if (!texFridge) return;
    /* keep the blur readable without dragging the frame */
    int steps = 2;
    for (int i = steps; i >= 0; i--) {
        int   offsetY  = i * 18;
        Uint8 a        = (Uint8)(alpha * (steps - i) / (steps + 1));
        SDL_SetTextureAlphaMod(texFridge, a);
        SDL_Rect dst = { sx, sy - offsetY, FRIDGE_W, FRIDGE_H };
        SDL_RenderCopy(ren, texFridge, NULL, &dst);
    }
    /* solid front copy */
    SDL_SetTextureAlphaMod(texFridge, (Uint8)alpha);
    SDL_Rect front = { sx, sy, FRIDGE_W, FRIDGE_H };
    SDL_RenderCopy(ren, texFridge, NULL, &front);
    SDL_SetTextureAlphaMod(texFridge, 255);
}

static void renderFridge(void)
{
    if (fridgePhase == FRIDGE_IDLE || fridgePhase == FRIDGE_COUNTDOWN ||
        fridgePhase == FRIDGE_SHAKING) return;
    if (!texFridge) return;

    int sx  = FRIDGE_X + shakeOX;
    int sy  = (int)(fridgeY - cameraY) + shakeOY;
    int alpha = (fridgePhase == FRIDGE_FADING) ? (int)fridgeAlpha : 255;

    if (fridgePhase == FRIDGE_DROPPING)
        renderFridgeBlur(sx, sy, alpha);
    else {
        SDL_SetTextureAlphaMod(texFridge, (Uint8)alpha);
        SDL_Rect dst = { sx, sy, FRIDGE_W, FRIDGE_H };
        SDL_RenderCopy(ren, texFridge, NULL, &dst);
        SDL_SetTextureAlphaMod(texFridge, 255);
    }
}

/* ══════════════════════════════════════════════════════════════
   MICROWAVE DROP SYSTEM
══════════════════════════════════════════════════════════════ */
static void updateMicrowave(void)
{
    if (!mwSequenceActive) return;
    Uint32 now     = SDL_GetTicks();
    Uint32 elapsed = now - mwTimer;

    /* ── TRACKED DROPS (mwSimulStage == 0) ── */
    if (mwSimulStage == 0) {

        if (mwPhase == MW_WARNING) {
            if (elapsed >= MW_WARN_MS) {
                /* launch */
                mwY     = cameraY - MW_H - 10.0f;
                mwPhase = MW_DROPPING;
                mwTimer = now;
                if (sfxApplianceFall) Mix_PlayChannel(-1, sfxApplianceFall, 0);
            }
            return;
        }

        if (mwPhase == MW_DROPPING) {
            mwY += MW_SPEED;
            float screenTop = mwY - cameraY;
            float screenBot = screenTop + MW_H;

            /* Hit check */
            if (screenBot >= 0 && screenTop <= SCREEN_H && !cheatInvincible) {
                float px1 = player.x,           px2 = player.x + PLAYER_W;
                float py1 = player.y - cameraY, py2 = py1 + PLAYER_H;
                if (px2 > mwX && px1 < mwX + MW_W &&
                    py2 > screenTop && py1 < screenBot) {
                    if (!flickerActive) {
                        if (level1ApplyDamage(0, 0, "Microwaved.")) {
                            flickerActive=1; flickerStart=now;
                        } else {
                            return;
                        }
                    }
                }
            }

            if (level1ShouldUseDuoDeathRules()) {
                float p2x1 = player2.x,           p2x2 = player2.x + PLAYER_W;
                float p2y1 = player2.y - cameraY, p2y2 = p2y1 + PLAYER_H;
                if (p2x2 > mwX && p2x1 < mwX + MW_W &&
                    p2y2 > screenTop && p2y1 < screenBot && !flickerActiveP2) {
                    if (level1ApplyDamage(1, 0, "Microwaved.")) {
                        flickerActiveP2 = 1;
                        flickerStartP2 = now;
                    }
                }
            }

            if (screenTop > SCREEN_H + 20) {
                mwDropsDone++;
                if (mwDropsDone >= MW_DROPS_TRACKED) {
                    /* move to simultaneous phase */
                    mwSimulStage = 1;
                    mwPhase      = MW_WARNING;
                    mwTimer      = now;
                    /* stage 1: max-left and max-right */
                    mwSimulCount    = 2;
                    mwSimulX[0]     = 0.0f;
                    mwSimulX[1]     = (float)(SCREEN_W - MW_W);
                    mwSimulY[0]     = cameraY - MW_H - 10.0f;
                    mwSimulY[1]     = cameraY - MW_H - 10.0f;
                    mwWarnX         = 0.0f;   /* first warning column */
                } else {
                    /* next tracked drop */
                    mwPhase  = MW_FADING;
                    mwTimer  = now;
                }
            }
            return;
        }

        if (mwPhase == MW_FADING) {
            if (elapsed >= MW_INTERVAL_MS) {
                /* track player for next drop */
                mwX = player.x + PLAYER_W/2.0f - MW_W/2.0f;
                if (mwX < 0) mwX = 0;
                if (mwX + MW_W > SCREEN_W) mwX = (float)(SCREEN_W - MW_W);
                mwWarnX = mwX;
                mwPhase = MW_WARNING;
                mwTimer = now;
            }
            return;
        }
        return;
    }

    /* ── SIMULTANEOUS DROPS (mwSimulStage 1 and 2) ── */
    if (mwPhase == MW_WARNING) {
        if (elapsed >= MW_WARN_MS) {
            /* launch all simul microwaves */
            for (int i = 0; i < mwSimulCount; i++)
                mwSimulY[i] = cameraY - MW_H - 10.0f;
            mwPhase = MW_DROPPING;
            mwTimer = now;
            if (sfxApplianceFall) Mix_PlayChannel(-1, sfxApplianceFall, 0);
        }
        return;
    }

    if (mwPhase == MW_DROPPING) {
        int allGone = 1;
        for (int i = 0; i < mwSimulCount; i++) {
            mwSimulY[i] += MW_SPEED;
            float st = mwSimulY[i] - cameraY;
            float sb = st + MW_H;

            /* hit check */
            if (sb >= 0 && st <= SCREEN_H && !cheatInvincible) {
                float px1=player.x, px2=player.x+PLAYER_W;
                float py1=player.y-cameraY, py2=py1+PLAYER_H;
                if (px2>mwSimulX[i] && px1<mwSimulX[i]+MW_W &&
                    py2>st && py1<sb && !flickerActive) {
                    if (level1ApplyDamage(0, 0, "Microwaved.")) {
                        flickerActive=1; flickerStart=now;
                    } else {
                        return;
                    }
                }
            }
            if (level1ShouldUseDuoDeathRules()) {
                float p2x1 = player2.x, p2x2 = player2.x + PLAYER_W;
                float p2y1 = player2.y - cameraY, p2y2 = p2y1 + PLAYER_H;
                if (p2x2 > mwSimulX[i] && p2x1 < mwSimulX[i] + MW_W &&
                    p2y2 > st && p2y1 < sb && !flickerActiveP2) {
                    if (level1ApplyDamage(1, 0, "Microwaved.")) {
                        flickerActiveP2 = 1;
                        flickerStartP2 = now;
                    }
                }
            }
            if (st <= SCREEN_H + 20) allGone = 0;
        }

        if (allGone) {
            /* stage 1 (L+R) done — all done, no more stages */
            mwPhase          = MW_IDLE;
            mwSequenceActive = 0;
            mwDebrisPaused   = 0;
            debrisPhase      = DEB_IDLE;
            debrisTimer      = now;
            startDialogueSeq(DLGSEQ_POSTMICRO, dlgPostMicro, DLG_POSTMICRO_COUNT);
        }
        return;
    }
}

static void renderMicrowave(void)
{
    if (!mwSequenceActive) return;
    Uint32 now = SDL_GetTicks();

    /* ── Warning column(s) — same sinf pulse as debris, yellow ── */
    if (mwPhase == MW_WARNING) {
        float t     = (float)(now % 400) / 400.0f;
        float pulse = 0.40f + 0.20f * sinf(t * 6.2831f);
        Uint8 alpha = (Uint8)(pulse * 255);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 210, 0, alpha);

        if (mwSimulStage == 0) {
            /* single tracked column */
            SDL_Rect col = { (int)mwWarnX + shakeOX, shakeOY, MW_W, SCREEN_H };
            SDL_RenderFillRect(ren, &col);
        } else {
            /* all simul columns */
            for (int i = 0; i < mwSimulCount; i++) {
                SDL_Rect col = { (int)mwSimulX[i] + shakeOX, shakeOY, MW_W, SCREEN_H };
                SDL_RenderFillRect(ren, &col);
            }
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }

    if (mwPhase != MW_DROPPING || !texMicrowave) return;

    /* ── Render active microwaves with motion blur ── */
    /* ── draw one microwave with fridge-style motion blur ── */
    #define MW_BLUR_STEPS 2
    #define MW_BLUR_OFFSET 18

    if (mwSimulStage == 0) {
        float st = mwY - cameraY;
        if (st < SCREEN_H + MW_H) {
            int sx = (int)mwX + shakeOX;
            int sy = (int)st  + shakeOY;
            /* ghost trail */
            for (int i = MW_BLUR_STEPS; i >= 0; i--) {
                Uint8 a = (Uint8)(255 * (MW_BLUR_STEPS - i) / (MW_BLUR_STEPS + 1));
                SDL_SetTextureAlphaMod(texMicrowave, a);
                SDL_Rect g = { sx, sy - i * MW_BLUR_OFFSET, MW_W, MW_H };
                SDL_RenderCopy(ren, texMicrowave, NULL, &g);
            }
            /* solid front */
            SDL_SetTextureAlphaMod(texMicrowave, 255);
            SDL_Rect dst = { sx, sy, MW_W, MW_H };
            SDL_RenderCopy(ren, texMicrowave, NULL, &dst);
        }
    } else {
        for (int i = 0; i < mwSimulCount; i++) {
            float st = mwSimulY[i] - cameraY;
            if (st >= SCREEN_H + MW_H) continue;
            int sx = (int)mwSimulX[i] + shakeOX;
            int sy = (int)st           + shakeOY;
            for (int j = MW_BLUR_STEPS; j >= 0; j--) {
                Uint8 a = (Uint8)(255 * (MW_BLUR_STEPS - j) / (MW_BLUR_STEPS + 1));
                SDL_SetTextureAlphaMod(texMicrowave, a);
                SDL_Rect g = { sx, sy - j * MW_BLUR_OFFSET, MW_W, MW_H };
                SDL_RenderCopy(ren, texMicrowave, NULL, &g);
            }
            SDL_SetTextureAlphaMod(texMicrowave, 255);
            SDL_Rect dst = { sx, sy, MW_W, MW_H };
            SDL_RenderCopy(ren, texMicrowave, NULL, &dst);
        }
    }
    SDL_SetTextureAlphaMod(texMicrowave, 255);
}

/* ═══════════════════════════════════════════════════════════
   FINAL FLOOR SYSTEMS
═══════════════════════════════════════════════════════════ */

static void updateFinalCamPan(void)
{
    /* Slide death line off screen */
    if (finalDebrisStopped && deathLineY < cameraY + SCREEN_H + 200.0f)
        deathLineY += 4.0f;

    /* Player walk to door target */
    if (finalWalkPhase == FWALK_RUNNING) {
        float dist = finalWalkTargetX - player.x;
        if (fabsf(dist) > 3.0f) {
            player.x += (dist > 0 ? 1.0f : -1.0f) * 3.5f;
            player.facingRight = (dist > 0);
            animState = PANIM_RUN;
            tickAnim();
        } else {
            player.x = finalWalkTargetX;
            player.facingRight = 1;  /* face right toward door */
            animState = PANIM_IDLE;
            finalWalkPhase = FWALK_DONE;
        }
    }

    if (!camPanActive) return;
    float diff = camPanTargetY - cameraY;
    if (fabsf(diff) < 1.0f) {
        cameraY      = camPanTargetY;
        camPanActive = 0;
        /* Note: dialogue start handled in GS_FINAL_CAMPAN case once walk also done */
    } else {
        cameraY += diff * CAM_PAN_SPEED * 0.05f;
    }
}

static void updateDoorAnim(void)
{
    if (!doorSpawned) return;
    if (doorState == DOOR_CLOSED || doorState == DOOR_OPEN) return;
    Uint32 now = SDL_GetTicks();
    if (now - doorAnimTick >= (1000 / DOOR_FPS)) {
        doorAnimTick = now;
        doorAnimFrame++;
        int totalFrames = 36;
        if (doorAnimFrame >= totalFrames) {
            doorAnimFrame = 0;
            if (doorState == DOOR_OPENING) doorState = DOOR_OPEN;
            else if (doorState == DOOR_CLOSING) doorState = DOOR_CLOSED;
        }
    }
}

static void updateHarryIdle(void)
{
    if (!harryActive) return;
    Uint32 now = SDL_GetTicks();
    if (now - harryAnimTick >= (1000 / 8)) {
        harryAnimTick = now;
        harryAnimFrame = (harryAnimFrame + 1) % 36;
    }
}

static void updateHarryEnter(void)
{
    /* Harry slides toward door then fades out */
    if (gameState != GS_HARRY_ENTER) return;

    float doorCenterX = doorWorldX + FDOOR_W / 2.0f - PLAYER_W / 2.0f;
    float dist = doorCenterX - harryX;

    if (fabsf(dist) > 2.0f) {
        harryX += (dist > 0 ? 1.0f : -1.0f) * 3.5f;
        /* Keep Harry on floor during slide */
        harryWorldY = doorWorldY + FDOOR_H - PLAYER_H;
    } else {
        /* At door — fade out Harry */
        harryAlpha -= 6.0f;
        if (harryAlpha <= 0.0f) {
            harryAlpha       = 0.0f;
            harryActive      = 0;
            harryEnteredDoor = 1;
            /* Door is already open — just show Harry's last line based on choice, then give player control */
            if (finalChoiceMade == 0) {
                /* Take gun branch: Marv says "stupid fuck" */
                static const DlgLine postHarry0[] = {
                    { CHAR_MARV, "Stupid fuck." }
                };
                startDialogueSeq(DLGSEQ_MARV_REACT, postHarry0, 1);
            } else {
                /* Forget branch: Marv says "i'm killing that kid" */
                static const DlgLine postHarry1[] = {
                    { CHAR_MARV, "I'm killing that kid." }
                };
                startDialogueSeq(DLGSEQ_MARV_REACT, postHarry1, 1);
            }
            gameState = GS_FINAL_DIALOGUE;
        }
    }
}

static void updateConsequence(void)
{
    if (gameState != GS_CONSEQUENCE) return;
    Uint32 now     = SDL_GetTicks();
    Uint32 elapsed = now - conseqTimer;

    if (conseqPhase == 0) {
        /* Fade to black */
        conseqFade = (float)elapsed / CONSEQ_FADE_MS * 255.0f;
        if (conseqFade >= 255.0f) {
            conseqFade  = 255.0f;
            conseqPhase = 1;
            conseqTimer = now;
        }
    } else if (conseqPhase == 1) {
        /* Hold — text visible */
        if (elapsed >= CONSEQ_HOLD_MS) {
            conseqPhase = 2;
            conseqTimer = now;
        }
    } else if (conseqPhase == 2) {
        /* Fade back out */
        conseqFade = 255.0f - (float)elapsed / CONSEQ_FADE_MS * 255.0f;
        if (conseqFade <= 0.0f) {
            conseqFade  = 0.0f;
            /* Start branch dialogue */
            gameState = GS_FINAL_DIALOGUE;
            dlgChoiceMade = 1;
            if (finalChoiceMade == 0)
                startDialogueSeq(DLGSEQ_FINAL_GUN,  dlgFinalTakeGun,  DLG_FINAL_TAKEGUN_COUNT);
            else
                startDialogueSeq(DLGSEQ_FINAL_FORG, dlgFinalForget,   DLG_FINAL_FORGET_COUNT);
        }
    }
}

static void updatePlayerEnterDoor(void)
{
    if (!playerEnteringDoor) return;
    if (gameState != GS_FINAL_DIALOGUE && gameState != GS_PLAYING) return;
    float doorCenterX = doorWorldX + FDOOR_W / 2.0f - PLAYER_W / 2.0f;
    float dist = doorCenterX - player.x;
    if (fabsf(dist) > 2.0f) {
        player.x += (dist > 0 ? 1.0f : -1.0f) * 3.0f;
    } else {
        playerDoorAlpha -= 6.0f;
        if (playerDoorAlpha <= 0.0f) {
            playerDoorAlpha   = 0.0f;
            playerEnteredDoor = 1;
            playerEnteringDoor = 0;
            /* Both in — play door close animation then credits */
            doorState     = DOOR_CLOSING;
            doorAnimFrame = 0;
            doorAnimTick  = SDL_GetTicks();
        }
    }
}

static void renderDoor(void)
{
    if (!doorSpawned) return;
    int dx = (int)(doorWorldX);
    int dy = (int)(doorWorldY - cameraY) + shakeOY;
    if (dy > SCREEN_H || dy + FDOOR_H < 0) return;

    SDL_Texture *tex = NULL;
    switch (doorState) {
        case DOOR_CLOSED:  tex = texDoorClosed;  break;
        case DOOR_OPEN:    tex = texDoorOpen;     break;
        case DOOR_OPENING: tex = texDoorOpening;  break;
        case DOOR_CLOSING: tex = texDoorClosing;  break;
    }
    if (tex) {
        SDL_Rect dst = { dx + shakeOX, dy, FDOOR_W, FDOOR_H };
        if (doorState == DOOR_OPENING || doorState == DOOR_CLOSING) {
            int texW, texH;
            if (!getTextureSize(tex, &texW, &texH)) return;
            int fw = texW / FDOOR_COLS, fh = texH / FDOOR_ROWS;
            int col = doorAnimFrame % FDOOR_COLS, row = doorAnimFrame / FDOOR_ROWS;
            SDL_Rect src = { col*fw, row*fh, fw, fh };
            SDL_RenderCopy(ren, tex, &src, &dst);
        } else {
            SDL_RenderCopy(ren, tex, NULL, &dst);
        }
    } else {
        /* Fallback colored rect */
        SDL_SetRenderDrawColor(ren, 139, 90, 43, 255);
        SDL_Rect r = { dx + shakeOX, dy, FDOOR_W, FDOOR_H };
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawColor(ren, 200, 140, 60, 255);
        SDL_RenderDrawRect(ren, &r);
    }
}

static void renderHarryNPC(void)
{
    if (!harryActive) return;  /* only render when explicitly spawned */
    if (level1DuoEnabled()) return;  /* duo already has both players */
    /* Counterpart NPC: draw with correct frame size, feet anchored to floor */
    SDL_Texture *npcTex = partnerIdleTexture();
    int texW, texH;
    int harrySW = PLAYER_W, harrySH = PLAYER_H;
    if (npcTex) {
        if (!getTextureSize(npcTex, &texW, &texH)) return;
        int fh = texH / HARRY_ROWS;
        int fw = texW / HARRY_COLS;
        /* Scale to player height, preserve aspect */
        harrySH = PLAYER_H;
        harrySW = (int)((float)fw / fh * PLAYER_H);
    }
    int sx = (int)(harryX) + shakeOX;
    /* Anchor feet to harryWorldY */
    int sy = (int)(harryWorldY - cameraY) + shakeOY;
    if (sy > SCREEN_H || sy + harrySH < 0) return;

    if (npcTex) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(npcTex, (Uint8)harryAlpha);
        /* Manual frame draw — same as drawSprite but with HARRY cols/rows */
        int col = harryAnimFrame % HARRY_COLS;
        int row = harryAnimFrame / HARRY_COLS;
        int fw2 = texW / HARRY_COLS, fh2 = texH / HARRY_ROWS;
        SDL_Rect src = { col*fw2, row*fh2, fw2, fh2 };
        SDL_Rect dst = { sx, sy, harrySW, harrySH };
        SDL_RenderCopy(ren, npcTex, &src, &dst);
        SDL_SetTextureAlphaMod(npcTex, 255);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    } else {
        Uint8 fr = (partnerCharacterNumber() == 2) ? 255 : 50;
        Uint8 fg = (partnerCharacterNumber() == 2) ? 160 : 100;
        Uint8 fb = (partnerCharacterNumber() == 2) ? 60 : 180;
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, fr, fg, fb, (Uint8)harryAlpha);
        SDL_Rect r = { sx, sy, PLAYER_W, PLAYER_H };
        SDL_RenderFillRect(ren, &r);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    }
}

static void renderConsequence(void)
{
    if (gameState != GS_CONSEQUENCE) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)conseqFade);
    SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(ren, &full);

    if (conseqPhase == 1) {
        /* White text centered */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        dtc(font, "this action will have consequences :p",
            SCREEN_H / 2, 255, 255, 255);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void renderCredits(void)
{
    char line[128];
    int minutes;
    int seconds;

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    if (!activeSession || !level1ResultReady) {
        dtc(bigFont, "TO BE CONTINUED...", SCREEN_H / 2 - 40, 255, 230, 60);
        dtc(font, "Climb or Die", SCREEN_H / 2 + 20, 180, 180, 180);
        return;
    }

    minutes = activeSession->level1.time_taken_sec / 60;
    seconds = activeSession->level1.time_taken_sec % 60;

    dtc(bigFont, "LEVEL 1 COMPLETE", 120, 255, 230, 60);
    dtc(font, "Press Enter to continue to Level 2", 170, 180, 180, 180);

    snprintf(line, sizeof(line), "Keys Collected: %d / 10", activeSession->level1.keys_collected);
    dt(font, line, 300, 270, 255, 255, 255);
    snprintf(line, sizeof(line), "Height Reached: %d", activeSession->level1.height_reached);
    dt(font, line, 300, 320, 255, 255, 255);
    snprintf(line, sizeof(line), "Time: %dm %02ds", minutes, seconds);
    dt(font, line, 300, 370, 255, 255, 255);
    snprintf(line, sizeof(line), "Lives Remaining: %d / %d", activeSession->level1.lives_remaining, MAX_LIVES);
    dt(font, line, 300, 420, 255, 255, 255);
    snprintf(line, sizeof(line), "Level Score: %d / 1000", activeSession->level1.points);
    dt(font, line, 300, 500, 255, 230, 60);
    snprintf(line, sizeof(line), "Bonus Lives For Level 2: +%d", activeSession->bonus_lives_for_level2);
    dt(font, line, 300, 550, 120, 210, 255);
    snprintf(line, sizeof(line), "Total So Far: %d", activeSession->total_points);
    dt(font, line, 300, 600, 200, 200, 200);
}

static void checkFKeyDoor(void)
{
    /* Player presses F near door — enter it */
    if (!doorSpawned || doorState != DOOR_OPEN) return;
    if (playerEnteredDoor || playerEnteringDoor) return;
    float dx = doorWorldX - player.x;
    float dy = (doorWorldY + FDOOR_H) - (player.y + PLAYER_H);
    if (fabsf(dx) < 70.0f && fabsf(dy) < 50.0f) {
        playerEnteringDoor = 1;
        playerDoorAlpha    = 255.0f;
    }
}

static void launchNewShopLevel1FromCurrentStateForPlayer(int shopPlayerIndex)
{
    int shopRc;
    int cwdOk;
    int switchedToShopDir = 0;
    Uint32 shopPauseStartedAt;
    Uint32 shopPauseDeltaMs;
    char oldCwd[1024];
    NewShopState shopState;

    shopPlayerIndex = (player2Enabled && shopPlayerIndex == 1) ? 1 : 0;

    if (gameState == GS_CREDITS || gameState == GS_GAME_OVER) {
        return;
    }

    savedCamY = cameraY;
    savedPX   = player.x;
    savedPY   = player.y;
    savedPX2  = player2.x;
    savedPY2  = player2.y;
    pauseLevelTimer();
    stopRatsLoopSfx();
    runShopTransitionFadeOut();
    shopPauseStartedAt = SDL_GetTicks();

    memset(&shopState, 0, sizeof(shopState));
    shopState.control_scheme = level1ControlSchemeForPlayer(shopPlayerIndex) == CONTROL_SCHEME_WASD
        ? NEWSHOP_CONTROL_SCHEME_WASD
        : NEWSHOP_CONTROL_SCHEME_ARROWS;
    shopState.lives = lives;
    shopState.extra_hearts = player.extraHearts;
    shopState.duo_enabled = level1DuoEnabled() ? 1 : 0;
    shopState.online_hosted = online_client_is_host() ? 1 : 0;
    shopState.player_interact_bind[0] = level1PrimaryInteractBind();
    shopState.player_interact_bind[1] = level1SecondaryInteractBind();
    shopState.player_skin_number[0] = selectedCharacterNumber();
    shopState.player_skin_number[1] = partnerCharacterNumber();
    shopState.player_lives[0] = lives;
    shopState.player_lives[1] = player2Enabled ? livesP2 : lives;
    shopState.player_extra_hearts[0] = player.extraHearts;
    shopState.player_extra_hearts[1] = player2Enabled ? player2.extraHearts : 0;
    shopState.keys_held = keysHeld;
    shopState.active_player = shopPlayerIndex;
    shopState.player_buy_count_jetpack[0] = player.buyCountJetpack;
    shopState.player_buy_count_magnet[0] = player.buyCountMagnet;
    shopState.player_buy_count_shoes[0] = player.buyCountShoes;
    shopState.player_buy_count_jetpack[1] = player2Enabled ? player2.buyCountJetpack : 0;
    shopState.player_buy_count_magnet[1] = player2Enabled ? player2.buyCountMagnet : 0;
    shopState.player_buy_count_shoes[1] = player2Enabled ? player2.buyCountShoes : 0;
    shopState.buy_count_jetpack = shopState.player_buy_count_jetpack[shopPlayerIndex];
    shopState.buy_count_magnet = shopState.player_buy_count_magnet[shopPlayerIndex];
    shopState.buy_count_shoes = shopState.player_buy_count_shoes[shopPlayerIndex];
    shopState.purchased_jetpack = 0;
    shopState.purchased_magnet = 0;
    shopState.purchased_shoes = 0;

    cwdOk = (getcwd(oldCwd, sizeof(oldCwd)) != NULL);
    if (chdir("../newshoplvl1") == 0) {
        switchedToShopDir = 1;
        shopRc = runNewShopLevel1(win, ren, &shopState);
        if (shopRc != 0) {
            SDL_Log("newshoplvl1 exited with code %d", shopRc);
        }
    } else {
        shopRc = 1;
        SDL_Log("Failed to change directory to ../newshoplvl1");
    }

    if (switchedToShopDir && cwdOk) {
        if (chdir(oldCwd) != 0) {
            SDL_Log("Failed to restore working directory to %s", oldCwd);
        }
    }

    shopPauseDeltaMs = SDL_GetTicks() - shopPauseStartedAt;
    shiftWorldTimersAfterShop(shopPauseDeltaMs);
    logicClockNeedsReset = 1;

    /* Carry shop purchases and currency back into level 1 state. */
    lives = shopState.lives;
    player.extraHearts = shopState.extra_hearts;
    if (shopState.player_lives[0] >= 0) lives = shopState.player_lives[0];
    if (shopState.player_extra_hearts[0] >= 0)
        player.extraHearts = shopState.player_extra_hearts[0];
    if (player2Enabled) {
        if (shopState.player_lives[1] >= 0) livesP2 = shopState.player_lives[1];
        if (shopState.player_extra_hearts[1] >= 0)
            player2.extraHearts = shopState.player_extra_hearts[1];
    } else {
        livesP2 = lives;
    }
    keysHeld = shopState.keys_held;
    player.buyCountJetpack = shopState.player_buy_count_jetpack[0];
    player.buyCountMagnet = shopState.player_buy_count_magnet[0];
    player.buyCountShoes = shopState.player_buy_count_shoes[0];
    if (player2Enabled) {
        player2.buyCountJetpack = shopState.player_buy_count_jetpack[1];
        player2.buyCountMagnet = shopState.player_buy_count_magnet[1];
        player2.buyCountShoes = shopState.player_buy_count_shoes[1];
    }

    if (shopState.player_purchased_jetpack[0] > 0) {
        player.jetpackFloors = JETPACK_FLOORS;
    }
    if (player2Enabled && shopState.player_purchased_jetpack[1] > 0) {
        player2.jetpackFloors = JETPACK_FLOORS;
    }
    if (shopState.player_purchased_magnet[0] > 0) {
        player.magnetTimer = SDL_GetTicks() + MAGNET_DURATION_MS;
        magnetSlideIdx = -1;
    }
    if (player2Enabled && shopState.player_purchased_magnet[1] > 0) {
        player2.magnetTimer = SDL_GetTicks() + MAGNET_DURATION_MS;
        magnetSlideIdx = -1;
    }
    if (shopState.player_purchased_shoes[0] > 0) {
        player.shoesTimer = SDL_GetTicks() + SHOES_DURATION_MS;
    }
    if (player2Enabled && shopState.player_purchased_shoes[1] > 0) {
        player2.shoesTimer = SDL_GetTicks() + SHOES_DURATION_MS;
    }

    resumeLevelTimer();
    resumeWorldAudioAfterShop();
    shopTransitionFadeAlpha = 255.0f;

    /* Restore world — place player beside the house, not in the door. */
    cameraY = savedCamY - 140.0f;
    if (house.active) {
        float rightX = house.x + HOUSE_W + 10.0f;
        float leftX  = house.x - PLAYER_W - 10.0f;
        player.x = (rightX + PLAYER_W < SCREEN_W) ? rightX : leftX;
        player.y = floors[house.floorIdx].worldY - PLAYER_H;
        if (player2Enabled) {
            float p2RightX = rightX + 64.0f;
            float p2LeftX = leftX - 64.0f;
            player2.x = (p2RightX + PLAYER_W < SCREEN_W) ? p2RightX : p2LeftX;
            player2.y = floors[house.floorIdx].worldY - PLAYER_H;
        }
    } else {
        player.x = savedPX + 80.0f;
        player.y = savedPY;
        if (player2Enabled) {
            player2.x = savedPX2 + 80.0f;
            player2.y = savedPY2;
        }
    }
    player.vx = player.vy = 0;
    player.onGround = 1;
    if (player2Enabled) {
        player2.vx = player2.vy = 0;
        player2.onGround = 1;
    }
    countdown = 3;
    countdownStart = SDL_GetTicks();
    gameState = GS_COUNTDOWN;
    fadeAlpha = 255;
    house.active = 0;
    resetInputPipeline();
}

static void launchNewShopLevel1FromCurrentState(void)
{
    launchNewShopLevel1FromCurrentStateForPlayer(0);
}

static void tryEnterHouseFromWorld(int interactingPlayer)
{
    int nearDoorP1;
    int nearDoorP2;

    if (gameState != GS_PLAYING || !house.active) return;
    {
        int dx2 = (int)house.x + HOUSE_W / 2 - DOOR_W / 2;
        int dy2 = (int)house.y + HOUSE_H - DOOR_H;
        nearDoorP1 = (player.x + PLAYER_W > dx2 - 20 && player.x < dx2 + DOOR_W + 20 &&
                      player.y + PLAYER_H > dy2 - 20 && player.y < dy2 + DOOR_H + 40);
        nearDoorP2 = (player2Enabled &&
                      player2.x + PLAYER_W > dx2 - 20 && player2.x < dx2 + DOOR_W + 20 &&
                      player2.y + PLAYER_H > dy2 - 20 && player2.y < dy2 + DOOR_H + 40);
    }
    if (nearDoorP1 || nearDoorP2) {
        int shopPlayer = interactingPlayer;
        if (shopPlayer == 1 && !nearDoorP2) shopPlayer = 0;
        if (shopPlayer != 1 && !nearDoorP1 && nearDoorP2) shopPlayer = 1;
        launchNewShopLevel1FromCurrentStateForPlayer(shopPlayer);
    }
}

static void consumeBufferedActionsForTick(int *running)
{
    Uint32 now = SDL_GetTicks();
    int inDlgState = (gameState == GS_DIALOGUE || gameState == GS_FINAL_DIALOGUE);

    if (inDlgState && consumeBufferedAction(ACT_CONFIRM, now))
        advanceDialogue();

    if (inDlgState && dlgShowChoice &&
        consumeBufferedAction(ACT_CHOICE_TOGGLE, now))
        dlgChoiceSel = 1 - dlgChoiceSel;

    if (consumeBufferedAction(ACT_INTERACT, now)) {
        if (doorSpawned && doorState == DOOR_OPEN &&
            !playerEnteredDoor && !playerEnteringDoor &&
            (gameState == GS_PLAYING || gameState == GS_FINAL_DIALOGUE))
            checkFKeyDoor();
        tryEnterHouseFromWorld(bufferedInteractPlayer);
    }

    if (gameState == GS_CREDITS &&
        (consumeBufferedAction(ACT_CONFIRM, now) ||
         consumeBufferedAction(ACT_INTERACT, now))) {
        prepareLevel1Result(1);
        *running = 0;
    }

    if (gameState == GS_GAME_OVER && fadeAlpha >= 200 &&
        consumeBufferedAction(ACT_CONFIRM, now)) {
        prepareLevel1Result(0);
        *running = 0;
    }
}

static void renderDoorPrompt(void)
{
    char prompt[48];

    if (!doorSpawned || doorState != DOOR_OPEN) return;
    if (playerEnteredDoor || playerEnteringDoor) return;
    float dx = doorWorldX - player.x;
    float dy = (doorWorldY + FDOOR_H) - (player.y + PLAYER_H);
    if (fabsf(dx) < 70.0f && fabsf(dy) < 50.0f) {
        int px = (int)(doorWorldX) - 10;
        int py = (int)(doorWorldY - cameraY) - 28;
        char keyLabel[24];
        level1FormatInteractPrompt(keyLabel, sizeof(keyLabel));
        snprintf(prompt, sizeof(prompt), "[%s] Enter",
                 keyLabel);
        dt(font, prompt, px, py, 255, 255, 180);
    }
}

static void updateDialogue(void)
{
    if (dlgShowChoice) return;
    Uint32 now = SDL_GetTicks();

    /* Typewriter: only runs while line not fully shown */
    if (!dlgFullyShown) {
        if (!dlgCurLines || dlgLineIdx >= dlgCurCount) return;
        const char *text  = dlgCurLines[dlgLineIdx].text;
        int         total = (int)strlen(text);

        while (dlgCharShown < total && now >= dlgNextCharMs) {
            dlgCharShown++;
            if (sfxDlgLetter) Mix_PlayChannel(1, sfxDlgLetter, 0);
            if (dlgCharShown < total)
                dlgNextCharMs = now + dlgCharDelay(text, dlgCharShown);
            else {
                dlgFullyShown = 1;
                /* Schedule auto-advance for all non-freeze sequences */
                if (!dlgFreezeGame)
                    dlgAutoAdvanceMs = now + DLG_AUTO_ADVANCE_MS;
                Mix_HaltChannel(1);
            }
        }
    }

    /* Auto-advance timer — runs every frame even after line is fully shown */
    if (!dlgFreezeGame && dlgFullyShown && dlgAutoAdvanceMs &&
        now >= dlgAutoAdvanceMs) {
        dlgAutoAdvanceMs = 0;
        advanceDialogue();
    }
}

static void renderDialogueBox(void)
{
    if (gameState != GS_DIALOGUE && gameState != GS_FINAL_DIALOGUE &&
        gameState != GS_HARRY_ENTER  && gameState != GS_DOOR_OPENING) return;
    if (!dlgCurLines || dlgCurCount <= 0) return;

    /* Clamp index — advanceDialogue may have incremented past end this frame */
    int safeIdx = dlgLineIdx;
    if (safeIdx >= dlgCurCount) safeIdx = dlgCurCount - 1;
    const DlgLine *line = &dlgCurLines[safeIdx];

    /* ── Box background ── */
    SDL_SetRenderDrawColor(ren, 15, 15, 20, 230);
    SDL_Rect box = { DLGBOX_X, DLGBOX_Y, DLGBOX_W, DLGBOX_H };
    SDL_RenderFillRect(ren, &box);

    /* ── White border ── */
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_RenderDrawRect(ren, &box);
    /* double border (inner) */
    SDL_Rect inner = { DLGBOX_X + 3, DLGBOX_Y + 3, DLGBOX_W - 6, DLGBOX_H - 6 };
    SDL_RenderDrawRect(ren, &inner);

    /* ── Portrait ── */
    int px = DLGBOX_X + 12;
    int py = DLGBOX_Y + (DLGBOX_H - DLGBOX_PORTRAIT_SIZE) / 2;
    SDL_Rect portrait = { px, py, DLGBOX_PORTRAIT_SIZE, DLGBOX_PORTRAIT_SIZE };
    {
        int characterNumber = speakerCharacterNumber(line->speaker);
        SDL_Texture *portraitTex = characterPortraitTexture(characterNumber);
        if (portraitTex) {
            SDL_RenderCopy(ren, portraitTex, NULL, &portrait);
        } else {
            Uint8 fr = (characterNumber == 2) ? 200 : 50;
            Uint8 fg = (characterNumber == 2) ? 90  : 100;
            Uint8 fb = (characterNumber == 2) ? 40  : 180;
            Uint8 or = (characterNumber == 2) ? 240 : 100;
            Uint8 og = (characterNumber == 2) ? 130 : 160;
            Uint8 ob = (characterNumber == 2) ? 70  : 240;
            SDL_SetRenderDrawColor(ren, fr, fg, fb, 255);
            SDL_RenderFillRect(ren, &portrait);
            SDL_SetRenderDrawColor(ren, or, og, ob, 255);
            SDL_RenderDrawRect(ren, &portrait);
        }
    }

    /* ── Character name label above portrait ── */
    {
        int characterNumber = speakerCharacterNumber(line->speaker);
        const char *name = characterNameUpper(characterNumber);
        Uint8 nr = (characterNumber == 2) ? 240 : 100;
        Uint8 ng = (characterNumber == 2) ? 130 : 160;
        Uint8 nb = (characterNumber == 2) ? 70  : 240;
        if (font) {
            dt(font, name, px, DLGBOX_Y + 8, nr, ng, nb);
        }
    }

    /* ── Typewriter text ── */
    if (!font) return;

    if (dlgShowChoice) {
        /* ── Choice menu (Undertale-style * cursor) ── */
        /* Show the last spoken line still visible above choices */
        char visible[512] = "";
        int total2 = (int)strlen(line->text);
        memcpy(visible, line->text, (size_t)total2);
        visible[total2] = '\0';
        const WrapCache *wrap = getDialogueWrapCache(font, visible, DLGBOX_TEXT_W, 4);
        for (int i = 0; i < wrap->lineCount; i++)
            dt(font, wrap->lines[i], DLGBOX_TEXT_X, DLGBOX_TEXT_Y + i * 26, 200, 200, 200);

        /* Choice options */
        int choiceY = DLGBOX_Y + DLGBOX_H - 52;
        int choiceX = DLGBOX_TEXT_X;
        for (int i = 0; i < DLG_CHOICE_COUNT; i++) {
            int cx = choiceX + i * 220;
            int selected = (dlgChoiceSel == i);
            /* Glow box behind selected option */
            if (selected) {
                SDL_SetRenderDrawColor(ren, 255, 255, 80, 40);
                SDL_Rect glow = { cx - 14, choiceY - 4, 180, 34 };
                SDL_RenderFillRect(ren, &glow);
                SDL_SetRenderDrawColor(ren, 255, 255, 80, 200);
                SDL_RenderDrawRect(ren, &glow);
            }
            /* * cursor */
            if (selected)
                dt(font, "*", cx - 12, choiceY, 255, 255, 80);
            /* Label */
            Uint8 lr = selected ? 255 : 180;
            Uint8 lg = selected ? 255 : 180;
            Uint8 lb = selected ? 80  : 180;
            const char **activeLabels = (dlgSeqActive == DLGSEQ_FINAL_PRE)
                                       ? dlgFinalChoiceLabels
                                       : dlgChoiceLabels;
            dt(font, activeLabels[i], cx + 4, choiceY, lr, lg, lb);
        }
        return;
    }

    /* Build visible substring */
    char visible[512] = "";
    int total = (int)strlen(line->text);
    int shown = (dlgCharShown > total) ? total : dlgCharShown;
    memcpy(visible, line->text, (size_t)shown);
    visible[shown] = '\0';

    /* Word-wrap the visible portion */
    const WrapCache *wrap = getDialogueWrapCache(font, visible, DLGBOX_TEXT_W, 8);
    for (int i = 0; i < wrap->lineCount; i++) {
        int ty = DLGBOX_TEXT_Y + i * 26;
        dt(font, wrap->lines[i], DLGBOX_TEXT_X, ty, 255, 255, 255);
    }
}

/* ── White intro overlay ── */
static void renderIntroOverlay(void)
{
    if (introWhite <= 0) return;
    SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)introWhite);
    SDL_Rect full = { 0, 0, SCREEN_W, SCREEN_H };
    SDL_RenderFillRect(ren, &full);
}

/* ── Render intro title on top of white ── */
static void renderIntroTitle(void)
{
    if (introTitleAlpha <= 0 || !bigFont) return;
    SDL_Color black = {0, 0, 0, 255};
    int tw = 0, th = 0;
    SDL_Texture *tex = getCachedTextTexture(bigFont, "Chapter 1 : The Sneak", black, &tw, &th);
    if (!tex) return;
    SDL_Rect dst = {SCREEN_W / 2 - tw / 2, SCREEN_H / 2 - th / 2, tw, th};
    SDL_SetTextureAlphaMod(tex, (Uint8)introTitleAlpha);
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_SetTextureAlphaMod(tex, 255);
}

/* ═══════════════════════════════════════════════════════════
   FADE OVERLAY
═══════════════════════════════════════════════════════════ */
static void renderFade(void)
{
    if (fadeAlpha <= 0) return;
    SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)fadeAlpha);
    SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(ren, &full);
}

static void renderBlackOverlay(float alpha)
{
    if (alpha <= 0.0f) return;
    if (alpha > 255.0f) alpha = 255.0f;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)alpha);
    SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}

static void renderLevel1Frame(float extraBlackAlpha)
{
    int isIntro = (gameState == GS_INTRO_FADE_IN  ||
                   gameState == GS_INTRO_TITLE     ||
                   gameState == GS_INTRO_FADE_OUT);
    int inTrans = (gameState == GS_TRANSITION);

    if (shakeAmt > 0.5f) {
        SDL_Rect vp = {shakeOX, shakeOY, SCREEN_W, SCREEN_H};
        SDL_RenderSetViewport(ren, &vp);
    }
    dr(0, 0, SCREEN_W, SCREEN_H, BG);
    renderBackground();
    renderFloors();
    renderTopFloor();
    renderKeys();
    renderHouse();

    {
        int drawPlayer = 1;
        if (transPlayerHidden) drawPlayer = 0;
        if (isIntro) drawPlayer = 1;
        if (flickerActive) {
            Uint32 fe = SDL_GetTicks() - flickerStart;
            if (fe >= FLICKER_DURATION_MS) {
                flickerActive = 0;
            } else if ((fe / (FLICKER_DURATION_MS / 20)) % 2 == 1) {
                drawPlayer = 0;
            }
        }
        if (drawPlayer && !playerHurt) {
            SDL_Texture *idleTex = selectedIdleTexture();
            SDL_Texture *runTex = selectedRunTexture();
            SDL_Texture *jumpTex = selectedJumpTexture();
            if (playerEnteringDoor && idleTex) {
                Uint8 pa = (Uint8)playerDoorAlpha;
                if (idleTex) SDL_SetTextureAlphaMod(idleTex, pa);
                if (runTex) SDL_SetTextureAlphaMod(runTex, pa);
                if (jumpTex) SDL_SetTextureAlphaMod(jumpTex, pa);
            }
            renderPlayer();
            if (playerEnteringDoor) {
                if (idleTex) SDL_SetTextureAlphaMod(idleTex, 255);
                if (runTex) SDL_SetTextureAlphaMod(runTex, 255);
                if (jumpTex) SDL_SetTextureAlphaMod(jumpTex, 255);
            }
            renderPartnerPlayer();
        }
    }

    renderGauges();
    renderDogs();
    renderDeathLine();
    renderDebris();
    renderFridge();
    renderMicrowave();
    renderDoor();
    renderHarryNPC();
    renderDoorPrompt();
    if (playerHurt) renderHurtOverlay();

    SDL_RenderSetViewport(ren, NULL);

    if (!isIntro && (gameState != GS_DIALOGUE || !dlgFreezeGame)) {
        renderHUD();
    }
    if (cheatEverShown && font) {
        const char *invTxt = cheatInvincible ? "invi : on" : "invi : off";
        Uint8 ir = cheatInvincible ? 80  : 180;
        Uint8 ig = cheatInvincible ? 255 : 180;
        Uint8 ib = cheatInvincible ? 80  : 180;
        SDL_Color invColor = {ir, ig, ib, 255};
        int tw2 = 0, th2 = 0;
        SDL_Texture *invTex = getCachedTextTexture(font, invTxt, invColor, &tw2, &th2);
        if (invTex) {
            SDL_Rect dst = {SCREEN_W - tw2 - 12, 10, tw2, th2};
            SDL_RenderCopy(ren, invTex, NULL, &dst);
        }
    }
    if (isIntro) {
        renderIntroOverlay();
        renderIntroTitle();
    }
    if (inTrans) renderTransition();
    renderDialogueBox();
    renderConsequence();
    renderFade();
    renderBlackOverlay(shopTransitionFadeAlpha);
    renderBlackOverlay(extraBlackAlpha);

    if (gameState == GS_GAME_OVER && fadeAlpha >= 200) {
        dtc(bigFont ? bigFont : font,
            customDeathMsg ? customDeathMsg : "YOU DIED",
            SCREEN_H / 2 - 60, 220, 55, 55);
        {
            char gb[80];
            snprintf(gb, sizeof(gb), "Height: %d   Keys: %d", score, keysHeld);
            dtc(font, gb, SCREEN_H / 2 + 14, 180, 180, 180);
        }
        dtc(font, "Press Enter to continue", SCREEN_H / 2 + 50, 130, 130, 130);
    }
}

static void runShopTransitionFadeOut(void)
{
    Uint32 start = SDL_GetTicks();
    SDL_Event ev;

    while (1) {
        Uint32 now = SDL_GetTicks();
        float alpha = ((float)(now - start) / (float)SHOP_TRANSITION_MS) * 255.0f;
        if (alpha > 255.0f) alpha = 255.0f;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                if (activeSession) activeSession->quit_requested = 1;
                return;
            }
        }

        renderLevel1Frame(alpha);
        online_client_submit_frame(ren, 1);
        SDL_RenderPresent(ren);

        if (alpha >= 255.0f) {
            break;
        }

        SDL_Delay(1000 / TARGET_FPS);
    }
}

/* ═══════════════════════════════════════════════════════════
   MAIN
═══════════════════════════════════════════════════════════ */
int runLevel1(GameSession *session, SDL_Window *sharedWin, SDL_Renderer *sharedRen)
{
    int skipKeyPressCount = 0;
    int newShopShortcutPressCount = 0;
    int pauseMenuActive = 0;
    int pauseMenuReady = 0;

    activeSession = session;
    session_clear_level_life_carry();
    ownsSDL = (sharedWin == NULL || sharedRen == NULL);
    if (!ownsSDL) {
        win = sharedWin;
        ren = sharedRen;
    }
    resetLevel1RunState();
    levelStartTime = 0;
    timerPauseStart = 0;
    totalPausedMs = 0;
    levelTimerStarted = 0;
    levelTimerPaused = 0;
    level1ResultReady = 0;
    fpsDisplay = 0.0f;
    fpsWindowStart = SDL_GetTicks();
    fpsFrameCounter = 0;
    framePacerFreq = 0;
    snprintf(fpsLabel, sizeof(fpsLabel), "FPS %.1f", fpsDisplay);
    fpsLabelW = 0;
    fpsLabelH = 0;
    sharedRendererVSyncWasForced = 0;

    if (!initSDL()) return 1;
    pauseMenuReady = options_scene_init(win, ren);
    if (pauseMenuReady) options_scene_set_audio_enabled(0);
    refreshFpsLabel();
    prewarmStaticTextCache();
    initProceduralLevel();

    /* Pick a random floor that will appear in the upper portion of the screen
       (above the dialogue box which occupies the bottom ~200px).
       Floors are generated bottom-up: floor 0 = lowest, floor 7 = highest.
       We want the player visible above DLGBOX_Y (~520px from top), so pick
       from floors 3..7 (upper half of the initial 8). */
    int spawnFloor = 3 + rand() % 5;  /* floors 3,4,5,6,7 */
    if (spawnFloor >= floorCount) spawnFloor = floorCount - 1;
    Floor *f0 = &floors[spawnFloor];
    float spawnX = (f0->gapX >= PLAYER_W + 20)
                 ? (float)(f0->gapX / 2 - PLAYER_W / 2)
                 : (float)(f0->gapX + GAP_W + 10);
    initPlayer(spawnX, f0->worldY - PLAYER_H);
    player2Enabled = level1DuoEnabled();
    if (player2Enabled) {
        float spawnX2 = spawnX + 68.0f;
        if (spawnX2 + PLAYER_W > SCREEN_W)
            spawnX2 = spawnX - 68.0f;
        initPartnerPlayer(spawnX2, f0->worldY - PLAYER_H);
    }
    /* Set camera so the chosen floor appears in the upper half of the screen,
       leaving the bottom clear for the dialogue box */
    cameraY = player.y - SCREEN_H * 0.35f;
    /* NO flicker on first spawn — story intro handles the reveal */
    flickerActive = 0;
    debrisTimer   = SDL_GetTicks();
    debrisPhase   = DEB_IDLE;
    updateDeathLine();

    introWhite = 255.0f;
    introTitleAlpha = 0.0f;

    introTimer = SDL_GetTicks();

    int running = 1;
    SDL_Event ev;
    Uint64 logicPrevCounter = SDL_GetPerformanceCounter();
    Uint64 logicFreq = SDL_GetPerformanceFrequency();
    Uint32 lastAutosaveTick = SDL_GetTicks();
    double logicAccumulator = 0.0;
    const double logicStepSeconds = 1.0 / (double)LOGIC_TICK_HZ;
    const Uint32 autosaveIntervalMs = 5000;
    if (logicFreq == 0) logicFreq = 1;

    while (running)
    {
        Uint64 frameStartCounter = SDL_GetPerformanceCounter();

        if (logicClockNeedsReset) {
            logicPrevCounter = frameStartCounter;
            logicAccumulator = 0.0;
            logicClockNeedsReset = 0;
        }

        double frameSeconds = (double)(frameStartCounter - logicPrevCounter) / (double)logicFreq;
        logicPrevCounter = frameStartCounter;
        if (frameSeconds > 0.25) frameSeconds = 0.25;
        logicAccumulator += frameSeconds;
        if (shopTransitionFadeAlpha > 0.0f) {
            shopTransitionFadeAlpha -= (float)(255.0 * frameSeconds / ((double)SHOP_TRANSITION_MS / 1000.0));
            if (shopTransitionFadeAlpha < 0.0f) shopTransitionFadeAlpha = 0.0f;
        }
        clearFrameInputEdges();
        arcade_input_begin_frame();
        online_client_pump();
        if (online_client_should_abort_host_gameplay()) {
            if (activeSession) activeSession->quit_requested = 1;
            running = 0;
        }
        {
            int syncedPause = 0;
            while (online_client_consume_pause_state_change(&syncedPause)) {
                if (!pauseMenuReady) continue;
                if (syncedPause) {
                    if (!pauseMenuActive) {
                        pauseMenuActive = 1;
                        options_scene_enter();
                        pauseLevelTimer();
                        clearFrameInputEdges();
                        logicClockNeedsReset = 1;
                    }
                    online_client_send_pause_state(1);
                } else {
                    if (pauseMenuActive) {
                        pauseMenuActive = 0;
                        options_scene_leave();
                        resumeLevelTimer();
                        clearFrameInputEdges();
                        logicClockNeedsReset = 1;
                    }
                    online_client_send_pause_state(0);
                }
            }
        }

        /* ── Events ── */
        while (SDL_PollEvent(&ev)) {
            arcade_input_handle_event(&ev);
            if (ev.type == SDL_QUIT) {
                if (activeSession) activeSession->quit_requested = 1;
                running = 0;
            }
            if (pauseMenuActive) {
                OptionsSceneResult result = {0};
                options_scene_handle_event(&ev, &result);
                if (result.quit_to_menu) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    if (activeSession) activeSession->quit_requested = 1;
                    prepareLevel1Result(0);
                    running = 0;
                } else if (result.return_to_main) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    resumeLevelTimer();
                    logicClockNeedsReset = 1;
                    online_client_send_pause_state(0);
                }
                continue;
            }
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                int isRemoteEvent = (ev.key.windowID == LEVEL1_ONLINE_REMOTE_WINDOW_ID);
                SDL_Keymod mod = SDL_GetModState();

                if (k == SDLK_ESCAPE && pauseMenuReady) {
                    pauseMenuActive = 1;
                    options_scene_enter();
                    pauseLevelTimer();
                    clearFrameInputEdges();
                    logicClockNeedsReset = 1;
                    online_client_send_pause_state(1);
                    continue;
                }

                if (k == SDLK_a && !ev.key.repeat && (mod & KMOD_SHIFT)) {
                    SDL_Log("Level 1 Shift+A shortcut: skip to next level");
                    prepareLevel1Result(1);
                    running = 0;
                    continue;
                }

                if (k == SDLK_n) {
                    skipKeyPressCount++;
                    SDL_Log("Level 1 skip shortcut: %d/3", skipKeyPressCount);
                    if (skipKeyPressCount >= 3) {
                        /* Debug pass shortcut should still carry current hearts into level 2. */
                        prepareLevel1Result(1);
                        running = 0;
                    }
                    continue;
                }

                if ((k == SDLK_2 || k == SDLK_KP_2) && !ev.key.repeat &&
                    (mod & KMOD_ALT) &&
                    (mod & KMOD_SHIFT) &&
                    !(mod & KMOD_CTRL)) {
                    SDL_Log("Level 1 dev shortcut: jump to level 2");
                    if (activeSession) activeSession->dev_jump_to_level2 = 1;
                    prepareLevel1Result(1);
                    running = 0;
                    continue;
                }

                if (k == SDLK_p && !ev.key.repeat &&
                    (mod & KMOD_ALT) &&
                    (mod & KMOD_SHIFT) &&
                    !(mod & KMOD_CTRL)) {
                    SDL_Log("Level 1 dev shortcut: jump to final cutscene chase");
                    if (activeSession) activeSession->dev_jump_to_final_cutscene = 1;
                    prepareLevel1Result(1);
                    running = 0;
                    continue;
                }

                if (k == SDLK_p && !ev.key.repeat &&
                    (mod & KMOD_CTRL) &&
                    ((mod & KMOD_ALT) || (mod & KMOD_SHIFT))) {
                    SDL_Log("Level 1 dev shortcut: jump to final level");
                    if (activeSession) activeSession->dev_jump_to_final = 1;
                    prepareLevel1Result(1);
                    running = 0;
                    continue;
                }

                if (k == SDLK_p && !ev.key.repeat &&
                    !(mod & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT | KMOD_GUI))) {
                    newShopShortcutPressCount++;
                    SDL_Log("Level 1 newshop shortcut: %d/3", newShopShortcutPressCount);
                    if (newShopShortcutPressCount >= 3) {
                        newShopShortcutPressCount = 0;
                        launchNewShopLevel1FromCurrentState();
                    }
                    continue;
                }

                /* Cheat: RCTRL+SHIFT+D → toggle invincibility */
                if (k == SDLK_d) {
                    if ((mod & KMOD_RCTRL) && (mod & KMOD_SHIFT)) {
                        cheatInvincible = !cheatInvincible;
                        cheatEverShown  = 1;
                    }
                }
                /* Cheat: LCTRL+SHIFT+F → teleport to platform before final floor */
                if (k == SDLK_f) {
                    if ((mod & KMOD_LCTRL) && (mod & KMOD_SHIFT)) {
                        /* Force zone 2 and mark all zone-2 dialogues as seen */
                        bgZone = 2;
                        microwaveDlgTriggered = 1;
                        streetDlgTriggered    = 1;
                        sewerDlgTriggered     = 1;
                        /* Set bgCamAtZoneStart far enough back that travelled
                           never re-triggers the microwave threshold */
                        bgCamAtZoneStart = cameraY + TRANS_CAM_TRIGGER;

                        /* Ensure enough floors exist (extend if needed) */
                        while (floorCount < 4) extendFloors();

                        /* Place finish floor exactly 1 FLOOR_GAP above top floor */
                        if (finishFloorIdx < 0) {
                            float topY = floors[floorCount-1].worldY - FLOOR_GAP;
                            if (floorCount < MAX_FLOORS) {
                                Floor *ff  = &floors[floorCount];
                                ff->worldY = topY;
                                ff->gapX   = SCREEN_W; /* solid */
                                finishFloorIdx = floorCount;
                                floorCount++;
                                /* Spawn door+harry on freshly placed finish floor */
                                doorWorldX    = (float)(SCREEN_W / 2 - FDOOR_W / 2);
                                doorWorldY    = topY - FDOOR_H;
                                doorSpawned   = 1;
                                doorState     = DOOR_CLOSED;
                                doorAnimFrame = 0;
                                harryX        = doorWorldX + FDOOR_W + 20.0f;
                                harryWorldY   = topY - PLAYER_H;
                                harryActive   = 1;
                                harryAlpha    = 255.0f;
                                harryAnimFrame= 0;
                                harryAnimTick = SDL_GetTicks();
                            }
                        }

                        /* Teleport to one floor below finish */
                        if (finishFloorIdx >= 1 && finishFloorIdx < floorCount) {
                            int tIdx = finishFloorIdx - 1;
                            Floor *tfl = &floors[tIdx];
                            player.y  = tfl->worldY - PLAYER_H - 2.0f;
                            player.x  = (float)(SCREEN_W / 2 - PLAYER_W / 2);
                            player.vy = 0; player.vx = 0;
                            player.onGround = 0;
                            cameraY = player.y - SCREEN_H * 0.45f;
                            gameState = GS_PLAYING;
                            /* Reset final floor state so sequence plays fresh */
                            finalDlgTriggered = 0;
                            finalWalkPhase    = FWALK_NONE;
                            finalDebrisStopped= 0;
                            keysHeld          = KEYS_TO_WIN;
                            if (keysCollectedLifetime < keysHeld)
                                keysCollectedLifetime = keysHeld;
                            cheatInvincible   = 1;
                            cheatEverShown    = 1;
                            /* Re-spawn door+harry unconditionally (finish floor may already exist) */
                            if (finishFloorIdx >= 0 && finishFloorIdx < floorCount) {
                                float wy2 = floors[finishFloorIdx].worldY;
                                doorWorldX    = (float)(SCREEN_W / 2 - FDOOR_W / 2);
                                doorWorldY    = wy2 - FDOOR_H;
                                doorSpawned   = 1;
                                doorState     = DOOR_CLOSED;
                                doorAnimFrame = 0;
                                harryX        = doorWorldX + FDOOR_W + 20.0f;
                                harryWorldY   = wy2 - PLAYER_H;
                                harryActive   = 1;
                                harryAlpha    = 255.0f;
                                harryAnimFrame= 0;
                                harryAnimTick = SDL_GetTicks();
                            }
                        }
                    }
                }

                if (!ev.key.repeat) {
                    Uint32 now = SDL_GetTicks();
                    int inDlgState = (gameState == GS_DIALOGUE ||
                                      gameState == GS_FINAL_DIALOGUE);
                    if (!isRemoteEvent && isLevel1JumpKey(k)) {
                        inputSnap.jumpPressed = 1;
                        queueBufferedAction(ACT_JUMP, now);
                    }
                    if (level1DuoEnabled() &&
                        ((isRemoteEvent && isLevel1RemoteJumpKey(k)) ||
                         (!isRemoteEvent && !online_client_is_connected() &&
                          isLevel1JumpKeyForPlayer(k, 1)))) {
                        inputSnap.jumpPressedP2 = 1;
                    }
                    if (isLevel1InteractKey(k)) {
                        bufferedInteractPlayer =
                            isRemoteEvent
                                ? 1
                                : level1InteractPlayerForKey(k);
                        inputSnap.interactPressed = 1;
                        queueBufferedAction(ACT_INTERACT, now);
                    }
                    if (isConfirmKey(k)) {
                        inputSnap.confirmPressed = 1;
                        queueBufferedAction(ACT_CONFIRM, now);
                    }
                    if (inDlgState && dlgShowChoice &&
                        (k == SDLK_LEFT || k == SDLK_RIGHT))
                        queueBufferedAction(ACT_CHOICE_TOGGLE, now);
                }
            } else if (ev.type == SDL_KEYUP) {
                SDL_Keycode k = ev.key.keysym.sym;
                int isRemoteEvent = (ev.key.windowID == LEVEL1_ONLINE_REMOTE_WINDOW_ID);
                if (!isRemoteEvent && isLevel1JumpKey(k)) inputSnap.jumpReleased = 1;
                if (level1DuoEnabled() &&
                    ((isRemoteEvent && isLevel1RemoteJumpKey(k)) ||
                     (!isRemoteEvent && !online_client_is_connected() &&
                      isLevel1JumpKeyForPlayer(k, 1))))
                    inputSnap.jumpReleasedP2 = 1;
                if (isLevel1InteractKey(k)) inputSnap.interactReleased = 1;
                if (isConfirmKey(k)) inputSnap.confirmReleased = 1;
            }
        }

        if (pauseMenuActive) {
            options_scene_update((float)frameSeconds);
            renderLevel1Frame(0.0f);
            renderFpsCounter();
            online_client_submit_frame(ren, 1);
            options_scene_render();
            SDL_RenderPresent(ren);
            capFrameRate(frameStartCounter);
            continue;
        }

        captureHeldInputSnapshot();

        int catchupSteps = 0;
        while (logicAccumulator >= logicStepSeconds &&
               catchupSteps < FIXED_MAX_CATCHUP_STEPS &&
               running) {
            consumeBufferedActionsForTick(&running);
            if (!running) break;

            /* ── Fixed Update ── */
            switch (gameState)
            {
        case GS_INTRO_FADE_IN:
            introWhite      = 255.0f;
            introTitleAlpha = 0.0f;
            if (SDL_GetTicks() - introTimer >= (Uint32)INTRO_FADE_IN_MS) {
                introTimer = SDL_GetTicks();
                gameState  = GS_INTRO_TITLE;
            }
        break;

        case GS_INTRO_TITLE:
        {
            Uint32 el = SDL_GetTicks() - introTimer;
            if (el < (Uint32)INTRO_TITLE_IN_MS) {
                introTitleAlpha = (float)el / INTRO_TITLE_IN_MS * 255.0f;
                introWhite      = 255.0f;
            } else if (el < (Uint32)(INTRO_TITLE_IN_MS + INTRO_TITLE_HOLD_MS)) {
                introTitleAlpha = 255.0f;
                introWhite      = 255.0f;
            } else if (el < (Uint32)(INTRO_TITLE_IN_MS + INTRO_TITLE_HOLD_MS + INTRO_FADE_OUT_MS)) {
                float t2 = (float)(el - INTRO_TITLE_IN_MS - INTRO_TITLE_HOLD_MS) / INTRO_FADE_OUT_MS;
                introTitleAlpha = 255.0f;
                introWhite      = (1.0f - t2) * 255.0f;
            } else {
                introTitleAlpha = 255.0f;
                introWhite      = 0.0f;
                introTimer = SDL_GetTicks();
                gameState  = GS_INTRO_FADE_OUT;
            }
        }
        break;

        case GS_INTRO_FADE_OUT:
            introWhite      = 0.0f;
            introTitleAlpha = 0.0f;
            startDialogue();
        break;

        case GS_DIALOGUE:
            updateDialogue();
            /* also slide rats if already visible (shouldn't be, but safe) */
            if (ratsVisible && ratSlideY > 0) {
                ratSlideY -= RAT_SLIDE_SPEED;
                if (ratSlideY < 0) ratSlideY = 0;
            }
            /* Non-freeze dialogue: run full gameplay underneath */
            if (!dlgFreezeGame) {
                updateDebris();
                updateFridge();
                updateMicrowave();
                updateGauges();
                updateDogs();
                if (!playerHurt) {
                    updatePlayer();
                    updatePartnerPlayer();
                }
                else {
                    player.x  += player.vx;
                    player.vx *= 0.97f;
                    if (player.x + PLAYER_W < 0) player.x = (float)SCREEN_W;
                    if (player.x > SCREEN_W)     player.x = -(float)PLAYER_W;
                }
                updateCamera();
                updateDeathLine();
                if ((!playerHurt || level1DuoEnabled()) && !finalDlgTriggered) checkDeathLine();
                extendFloors();
                updateBgTileCounter();
                if (ratsVisible && ratSlideY > 0) {
                    ratSlideY -= RAT_SLIDE_SPEED;
                    if (ratSlideY < 0) ratSlideY = 0;
                }
            }
            break;

        case GS_PLAYING:
            startLevelTimer();
            resumeLevelTimer();
            updateDebris();
            updateFridge();
            updateMicrowave();
            updateGauges();
            updateDogs();
            /* Keep door animation running during free play after finale */
            if (finalDlgTriggered) { updateDoorAnim(); updateHarryIdle(); updatePlayerEnterDoor(); }
            /* Credits if both entered */
            if (finalDlgTriggered && playerEnteredDoor && doorState == DOOR_CLOSED)
                gameState = GS_CREDITS;
            if (!playerHurt) {
                updatePlayer();
                updatePartnerPlayer();
            }
            else {
                /* during hurt: pure horizontal slide, no gravity, no vertical movement */
                player.x  += player.vx;
                player.vx *= 0.97f;
                if (player.x + PLAYER_W < 0) player.x = (float)SCREEN_W;
                if (player.x > SCREEN_W)     player.x = -(float)PLAYER_W;
            }
            updateCamera();
            updateDeathLine();
            if (!playerHurt || level1DuoEnabled()) checkDeathLine();  /* allow duo partner death checks */
            extendFloors();
            updateBgTileCounter();
            /* slide rats up if still entering */
            if (ratsVisible && ratSlideY > 0) {
                ratSlideY -= RAT_SLIDE_SPEED;
                if (ratSlideY < 0) ratSlideY = 0;
            }
            break;

        case GS_TRANSITION:
            updateTransition();
            updateDebris();
            updateFridge();
            updateMicrowave();
            updateGauges();
            updateDogs();
            break;

        case GS_SECOND_CHANCE:
            updateCamera(); updateDeathLine();
            if (SDL_GetTicks() - scTimer > 900) {
                doRespawn(); gameState = GS_PLAYING;
            }
            break;

        case GS_COUNTDOWN:
            pauseLevelTimer();
            /* Fade in while counting 3-2-1 then resume */
            fadeAlpha -= 5.0f; if (fadeAlpha < 0) fadeAlpha = 0;
            {
                Uint32 el = SDL_GetTicks() - countdownStart;
                countdown = 3 - (int)(el / 1000);
                if (countdown < 1) countdown = 1;
                if (el >= 3000) { gameState = GS_PLAYING; fadeAlpha = 0; }
            }
            updateDeathLine();
            break;

        case GS_GAME_OVER:
            stopRatsLoopSfx();
            fadeAlpha += 2.5f;
            if (fadeAlpha > 255) fadeAlpha = 255;
            break;

        case GS_FINAL_CAMPAN:
            updateFinalCamPan();
            updateHarryIdle();
            updateDoorAnim();
            /* Both cam settled AND walk done → start dialogue */
            if (!camPanActive && finalWalkPhase == FWALK_DONE &&
                gameState == GS_FINAL_CAMPAN) {
                gameState = GS_FINAL_DIALOGUE;
                startDialogueSeq(DLGSEQ_FINAL_PRE, dlgFinalPre, DLG_FINALPRE_COUNT);
            }
            break;

        case GS_FINAL_DIALOGUE:
            updateDialogue();
            updateHarryIdle();
            updateDoorAnim();
            updateHarryEnter();
            updatePlayerEnterDoor();
            /* When door close animation finishes → credits */
            if (playerEnteredDoor && doorState == DOOR_CLOSED)
                gameState = GS_CREDITS;
            break;

        case GS_CONSEQUENCE:
            updateConsequence();
            break;

        case GS_DOOR_OPENING:
            updateDoorAnim();
            updateHarryIdle();
            /* Once door fully open, start Harry walking in */
            if (doorState == DOOR_OPEN)
                gameState = GS_HARRY_ENTER;
            break;

        case GS_HARRY_ENTER:
            updateHarryEnter();
            updateHarryIdle();
            updateDoorAnim();
            break;

        case GS_CREDITS:
            prepareLevel1Result(1);
            break;
        }

            if (gameState != GS_CREDITS &&
                gameState != GS_GAME_OVER)
                cleanupWorldBelowCamera();

            logicAccumulator -= logicStepSeconds;
            catchupSteps++;
        }

        if (catchupSteps == FIXED_MAX_CATCHUP_STEPS &&
            logicAccumulator >= logicStepSeconds)
            logicAccumulator = fmod(logicAccumulator, logicStepSeconds);

        updateFpsCounter(SDL_GetTicks());

        if (activeSession && activeSession->save_enabled && running &&
            !level1ResultReady &&
            gameState != GS_CREDITS &&
            gameState != GS_GAME_OVER) {
            Uint32 autosaveNow = SDL_GetTicks();
            if (autosaveNow - lastAutosaveTick >= autosaveIntervalMs) {
                activeSession->level1.completed = 0;
                activeSession->level1.lives_remaining = lives;
                session_autosave_progress(activeSession, 1, lives, level1DuoEnabled() ? livesP2 : 0);
                lastAutosaveTick = autosaveNow;
            }
        }

        /* ── Render ── */
        /* Credits get their own full-screen render */
        if (gameState == GS_CREDITS) {
            renderCredits();
            renderFpsCounter();
            options_scene_render_global_brightness_overlay(ren);
            online_client_submit_frame(ren, 1);
            SDL_RenderPresent(ren);
            capFrameRate(frameStartCounter);
            continue;
        }

        renderLevel1Frame(0.0f);
        renderFpsCounter();
        options_scene_render_global_brightness_overlay(ren);
        online_client_submit_frame(ren, 1);
        SDL_RenderPresent(ren);
        capFrameRate(frameStartCounter);
    }

    if (pauseMenuReady) options_scene_cleanup();
    closeSDL();
    activeSession = NULL;
    return 0;
}
