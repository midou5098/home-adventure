/*  Level 2 – The Chase
    SDL2 + SDL2_ttf skeleton, rectangles only (no textures).
    Build:  gcc main.c -o prot -lSDL2 -lSDL2_ttf -lSDL2_image -lm
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "../shared/arcade_input.h"
#include "../shared/session.h"
#include "../../src/options/options_scene.h"
#include "online_client.h"

/* ═══════════════════════════════════════════════════════════
   CONSTANTS
═══════════════════════════════════════════════════════════ */
#define SCREEN_W      1280
#define SCREEN_H      720
#define GROUND_Y      640
#define PLAYER_W      50
#define PLAYER_H      72
#define PLAYER_SPEED  4.5f
#define GRAVITY       0.55f
#define JUMP_VY      -14.0f
#define MAX_FALL      18.0f
#define MAX_LIVES     9
#define TARGET_FPS    30
#define LEVEL2_ONLINE_REMOTE_WINDOW_ID 0xFFFFFFFFu
#define ANIM_COLS     6
#define ANIM_ROWS     6
#define ANIM_FPS      28
#define MARV_JUMP_SCALE_X  1.14f
#define MARV_JUMP_SCALE_Y  1.09f
#define HARRY_JUMP_SCALE_X 1.42f
#define HARRY_JUMP_SCALE_Y 1.16f

#define MAX_OBJECTS   20
#define MAX_PLATFORMS 40
#define CARDBOX_SPAWN_INTERVAL_MS 1400u
#define ONLINE_CARDBOX_SPAWN_INTERVAL_MS 3000u

#define PLAYER_MAX_X  (SCREEN_W / 2)

/* Shared level 2 type + default layout declarations. */
#include "level2_types.h"

/* Sky punishment block */
#define MAX_SKY_BOMBS 5
static SkyBomb skyBombs[MAX_SKY_BOMBS];

/* Dialogue box */
#define DLGBOX_X              60
#define DLGBOX_Y              (SCREEN_H - 130)
#define DLGBOX_W              (SCREEN_W - 120)
#define DLGBOX_H              120
#define DLGBOX_PORTRAIT_SIZE  60

/* End room doors */
#define DOOR_BOTTOM_X  1190
#define DOOR_BOTTOM_Y  (GROUND_Y - 80)
#define DOOR_BOTTOM_W  60
#define DOOR_BOTTOM_H  80
#define DOOR_TOP_X     1190
#define DOOR_TOP_Y     120
#define DOOR_TOP_W     60
#define DOOR_TOP_H     80

/* ═══════════════════════════════════════════════════════════
   RENDERER GLOBAL + dr() MACRO
═══════════════════════════════════════════════════════════ */
static SDL_Renderer *ren = NULL;

#define dr(x,y,w,h,r,g,b,a) do { \
    SDL_SetRenderDrawColor(ren,(r),(g),(b),(a)); \
    SDL_Rect _r={(x),(y),(w),(h)}; \
    SDL_RenderFillRect(ren,&_r); \
} while(0)

/* ═══════════════════════════════════════════════════════════
   GLOBALS
═══════════════════════════════════════════════════════════ */
static SDL_Window   *win       = NULL;
static TTF_Font     *font      = NULL;
static TTF_Font     *bigFont   = NULL;
static GameSession  *activeSession = NULL;
static int           ownsSDL = 1;
static Mix_Music    *bgMusic = NULL;
static Mix_Chunk    *sfxJump[2] = {NULL, NULL};
static Mix_Chunk    *sfxDamage[5] = {NULL, NULL, NULL, NULL, NULL};
static Mix_Chunk    *sfxLand = NULL;
static Mix_Chunk    *sfxTrapCrack = NULL;
static Mix_Chunk    *sfxDialogueLetter = NULL;
static Mix_Chunk    *sfxSlamImpact = NULL;
static Mix_Chunk    *sfxFakeExit = NULL;
static Mix_Chunk    *sfxGravityFlip = NULL;
static Mix_Chunk    *sfxKeyPickup = NULL;
static Mix_Chunk    *sfxDoorOpen = NULL;
static Mix_Chunk    *sfxRatSqueak = NULL;
static Mix_Chunk    *sfxCardboardRoll = NULL;
static int           mixerOwned = 0;
static int           audioOwned = 0;
static SDL_Texture  *texBgFar    = NULL;
static SDL_Texture  *texBgNear   = NULL;
static SDL_Texture  *texGround   = NULL;
static SDL_Texture  *texPlatform = NULL;
static SDL_Texture  *texMarvRun  = NULL;  /* assets/animations/marv_run.png  */
static SDL_Texture  *texMarvIdle = NULL;  /* assets/animations/marv_idle.png */
static SDL_Texture  *texMarvJump = NULL;  /* assets/animations/marv_jump.png */
static SDL_Texture  *texMarvJetpack = NULL; /* assets/animations/skin1_jetpack.png */
static SDL_Texture  *texHarryIdle = NULL; /* assets/animations/harry_idle.png */
static SDL_Texture  *texHarryRun = NULL;  /* assets/animations/harry_run.png */
static SDL_Texture  *texHarryJump = NULL; /* assets/animations/harry_jump.png */
static SDL_Texture  *texHarryJetpack = NULL; /* assets/animations/skin2_jetpack.png */
static SDL_Texture  *texCardbox   = NULL; /* assets/animations/cardboard_box.png */
static SDL_Texture  *texMicrowave  = NULL; /* assets/static/microwave.png */
static SDL_Texture  *texKid       = NULL; /* assets/animations/kid_idle.png */
static SDL_Texture *texStaticKid   = NULL;  /* assets/static/kid_idle.png   */
static SDL_Texture *texStaticMarv  = NULL;  /* assets/static/skin1_idle.png */
static SDL_Texture *texStaticHarry = NULL;  /* assets/static/harry_idle.png */
static SDL_Texture  *texCans      = NULL; /* assets/items/cans.png */
static SDL_Texture  *texFridge    = NULL; /* assets/items/fridge.png */
static SDL_Texture  *texRat       = NULL; /* assets/animations/rat_idle.png */
static SDL_Texture *texPortraitHarry = NULL;
static SDL_Texture *texPortraitMarv  = NULL;
static int texBgFarW = 0, texBgFarH = 0;
static int texBgNearW = 0, texBgNearH = 0;
static int texGroundW = 0, texGroundH = 0;
static int texPlatformW = 0, texPlatformH = 0;
static int texMarvRunW = 0, texMarvRunH = 0;
static int texMarvIdleW = 0, texMarvIdleH = 0;
static int texMarvJumpW = 0, texMarvJumpH = 0;
static int texMarvJetpackW = 0, texMarvJetpackH = 0;
static int texHarryIdleW = 0, texHarryIdleH = 0;
static int texHarryRunW = 0, texHarryRunH = 0;
static int texHarryJumpW = 0, texHarryJumpH = 0;
static int texHarryJetpackW = 0, texHarryJetpackH = 0;
static int texCardboxW = 0, texCardboxH = 0;
static int texMicrowaveW = 0, texMicrowaveH = 0;
static int texKidW = 0, texKidH = 0;
static int texStaticKidW = 0, texStaticKidH = 0;
static int texStaticMarvW = 0, texStaticMarvH = 0;
static int texStaticHarryW = 0, texStaticHarryH = 0;
static int texCansW = 0, texCansH = 0;
static int texFridgeW = 0, texFridgeH = 0;
static int texRatW = 0, texRatH = 0;
static int texPortraitHarryW = 0, texPortraitHarryH = 0;
static int texPortraitMarvW = 0, texPortraitMarvH = 0;
static int level2StartLives = MAX_LIVES;
static int level2ResultReady = 0;

static int    cardboxFrame    = 0;
static Uint32 cardboxLastTick = 0;
static int    kevinAnimFrame   = 0;
static Uint32 kevinAnimLastTick = 0;

#define RAT_COUNT    20
#define RAT_RESPAWN_X (SCREEN_W + 80)     /* rats re-enter from just off the right edge */
#define SFX_CH_RAT_SQUEAK     6
#define SFX_CH_CARDBOARD_ROLL 7
static Rat    rats[RAT_COUNT];
static int    ratsActive = 0;
static int    launchJumpActive = 0;
static int    ratsRetreating = 0;   /* 1 = rats are fleeing left */
static Uint32 ratsRetreatStart = 0; /* when retreat began */
static Uint32 ratSqueakNextAt = 0;

static GameState  gameState  = GS_INTRO_FADE_IN;
static ChasePhase chasePhase = PHASE_OBJECTS;

static Player player;
static Marv   marv;

/* Kevin */
static float kevinX = 1200.0f;
static float kevinY = (float)(GROUND_Y - PLAYER_H);
static int   kevinVisible = 1;

/* Objects */
static Obj objects[MAX_OBJECTS];
static int objCount       = 0;   /* total spawned so far */
static int slamSpawned    = 0;
static int slamDlgStarted = 0;  /* 1 = fridge warning dialogue triggered */
static Uint32 objLastSpawn= 0;

/* Platforms (chase) */
static Platform platforms[MAX_PLATFORMS];
static Uint32   platLastSpawn = 0;
static int      patternCycle  = 0;   /* 0=A 1=B 2=C */
static Uint32   patternStart  = 0;
static int      patternPlatsThisCycle = 0;

/* End-room static platforms are declared in level2_types.h. */

/* Background parallax */
static float bgOff[3] = {0.0f, 0.0f, 0.0f};
static float bgScrollSpeed = 1.0f;

/* Intro */
static float  introWhite      = 0.0f;
static float  introTitleAlpha = 0.0f;
static Uint32 introTimer      = 0;
#define INTRO_FADE_IN_MS    800
#define INTRO_TITLE_IN_MS   600
#define INTRO_TITLE_HOLD_MS 1000
#define INTRO_TITLE_OUT_MS  600
#define INTRO_FADE_OUT_MS   1000

/* Dialogue */
static const DlgLine *dlgCurLines  = NULL;
static int            dlgCurCount  = 0;
static int            dlgLineIdx   = 0;
static int            dlgCharShown = 0;
static Uint32         dlgNextCharMs= 0;
static int            dlgFullyShown= 0;
static int            dlgFreezeGame= 1;
static int            dlgActive    = 0;
static Uint32         dlgLineFullAt= 0;  /* timestamp when current line finished typing */
/* which sequence is queued */
static int            dlgSeqPending = -1; /* 0=intro 1=kevin escape */
static int            kevinEscapeDlgDone = 0;
#define DLG_WRAP_MAX_LINES 8
#define DLG_WRAP_MAX_CHARS 256
static char           dlgWrappedLines[DLG_WRAP_MAX_LINES][DLG_WRAP_MAX_CHARS];
static int            dlgWrappedConsumedEnds[DLG_WRAP_MAX_LINES];
static int            dlgWrappedCount = 0;

#define TEXT_CACHE_CAPACITY 256
static TextCacheEntry textCache[TEXT_CACHE_CAPACITY];
static Uint32         textCacheStamp = 0;

/* Slam push state (Bug 3) */
static int   slamPushingPlayer = 0;  /* 1 = player locked to slam object */
static int   slamPushingMarv   = 0;  /* 1 = marv locked to slam object */
static int   slamReEntryActive    = 0;  /* 1 = re-entry phase after slam before platforms */
static int   slamReEntryDlgStarted = 0;

/* Sky hazard random spawner (Bug 2) */
static Uint32 hazardLastSpawn = 0;
static Uint32 hazardNextSpawnAt = 0;

/* Pushback slide (Bug 3) */
static int   pushBackActive = 0;
static float pushBackVx     = 0.0f;

/* Debug invincibility (Bug 4) */
static int debugInvincible = 0;
static int groundAdminMode = 0;
static int groundDrawHeight = SCREEN_H - GROUND_Y;
static int groundRepeatStartX = 0;
static float fpsDisplay = 0.0f;
static Uint32 fpsWindowStart = 0;
static int fpsFrameCounter = 0;

/* Slam wait (Bug 3) */
static int    slamWaiting   = 0;
static Uint32 slamWaitStart = 0;

/* Dialogue line arrays */
static const DlgLine dlgFridgeWarning[] = {
    { CHAR_HARRY, "MARV THERE IS A FUCKING FRIDGE COMING TOWARD US" },
};
#define DLG_FRIDGE_WARNING_COUNT 1

static const DlgLine dlgRats[] = {
    { CHAR_MARV,  "harry why do i hear rats?" },
    { CHAR_HARRY, "bruh..." },
};
#define DLG_RATS_COUNT 2

static const DlgLine dlgIntro[] = {
    { CHAR_HARRY, "There he is! Kevin!! Get back here!!" },
    { CHAR_MARV,  "I swear when I catch him I'm breaking every bone in his body" },
};
#define DLG_INTRO_COUNT 2

static const DlgLine dlgKevinEscape[] = {
    { CHAR_MARV,  "HE'S GETTING AWAY!!" },
    { CHAR_HARRY, "He had turbo shoes Merv, turbo shoes" },
    { CHAR_MARV,  "TURBO SHOES??? WHERE DID HE GET TURBO SHOES" },
    { CHAR_HARRY, "I don't know... but we'll find him" },
};
#define DLG_KEVIN_ESCAPE_COUNT 4


/* ── Microwave phase ── */
#define MW_SPEED            55.0f
#define MW_W                80
#define MW_H                70
#define MW_SLAM_COUNT       7

static int    mwRound         = 0;
static float  mwX             = 0.0f;
static int    mwActive        = 0;
static int    mwWaiting       = 0;
static Uint32 mwWaitStart     = 0;
static int    mwBothOut       = 0;
static int    mwReentering    = 0;
static int    dlgAtTop        = 0;
static Uint32 mwAutoAdvanceAt = 0;   /* when to auto-advance to next dlg line */

static const DlgLine dlgMwIntro[] = {
    { CHAR_HARRY, "marv..." },
    { CHAR_MARV,  "what now harry" },
    { CHAR_HARRY, "umm u remember that fridge?..." },
    { CHAR_MARV,  "what abo" },
};
#define DLG_MW_INTRO_COUNT 4

static const DlgLine dlgMwHit1[] = {
    { CHAR_HARRY, "marv are you okay?" },
    { CHAR_MARV,  "FUCK NO MY FACE IS FRIED WHERE DID THAT COME FRO" },
};
#define DLG_MW_HIT1_COUNT 2
static const DlgLine dlgMwHit2[] = { { CHAR_MARV,  "THIS IS RIDICULOU" } };
static const DlgLine dlgMwHit3[] = { { CHAR_HARRY, "where does he" } };
static const DlgLine dlgMwHit4[] = { { CHAR_HARRY, "even come with" } };
static const DlgLine dlgMwHit5[] = { { CHAR_MARV,  "these microwaves" } };
static const DlgLine dlgMwHit6[] = { { CHAR_MARV,  "STOP IT YOU FUCKING CUNT" } };
/* Phase timers */
static Uint32 phaseStart    = 0;
static int    phasePlatsDone= 0;  /* seconds elapsed in PHASE_PLATFORMS */
static int    lastPlatWasTrap = 0;
static int    marvReEntryStarted = 0;

/* Transition (black fade between rooms) */
static float  transAlpha    = 0.0f;
static int    transDir      = 0;   /* +1 fade in, -1 fade out */
static Uint32 transStart    = 0;
static GameState transTargetState = GS_MINI_LEVEL;
#define TRANS_FADE_MS 800
#define TRANS_HOLD_MS 500

/* End room */
static float  marvEndVx = 0.0f;
static float  marvEndAlpha = 255.0f;
static int    endFadeActive = 0;
static float  endFadeAlpha  = 0.0f;
static int    topDoorClosed      = 0;
static Uint32 topDoorClosedTimer = 0;
static int    topDoorUsed        = 0;
static int    needKeyShow        = 0;
static Uint32 needKeyTimer       = 0;

/* Mini level */
static int   miniPlat0Touched   = 0;
static Uint32 miniPlat0FallStart= 0;
static float  miniPlat0Y        = 550.0f;
static int   miniGravFlipped    = 0;
static Uint32 miniGravFlipStart = 0;
static int   miniPlat2Bounced   = 0;
static int   miniFakeExitUsed   = 0;
static int   miniKeySpawned     = 0;
static float  miniKeyX          = 620.0f;
static float  miniKeyY          = 284.0f;
static int   miniKeyCollected   = 0;
static int   miniTrapDoorOpen   = 0;
static float  miniTrapDoorX     = 580.0f;
static int   miniTrapDoorW      = 100;

/* Mini text overlays */
static int    miniShowWTF     = 0;
static Uint32 miniWTFStart    = 0;
static int    miniShowBonk    = 0;
static Uint32 miniBonkStart   = 0;
static int    miniShowPsych   = 0;
static Uint32 miniPsychStart  = 0;
#define MINI_TEXT_DURATION_MS 1500

/* New mini level state */
static float  miniPlat4Y         = 0.0f;   /* trap plat - y changes on fall */
static Uint32 miniPlat4FallStart = 0;
static int    miniPlat4Touched   = 0;
static float  miniPlat5Y         = 0.0f;   /* second trap plat */
static Uint32 miniPlat5FallStart = 0;
static int    miniPlat5Touched   = 0;
static int    miniFakeExit2Used  = 0;      /* second fake exit */
static int    miniShowPsych2     = 0;
static Uint32 miniPsych2Start    = 0;
static int    miniFakeKey3Taken  = 0;      /* fake key on platform 3 */
static int    miniShowGotcha     = 0;
static Uint32 miniGotchaStart    = 0;
/* Spike zones (3 danger areas on the ground - player dies if touching) */
/* Each spike zone: x, width. y is always GROUND_Y - 24 (render) */
#define MINI_NUM_SPIKES 3
static int miniSpikeX[MINI_NUM_SPIKES] = {280, 560, 850};
static int miniSpikeW[MINI_NUM_SPIKES] = {60,  50,  70};

/* Screen shake */
static int    shakeActive = 0;
static Uint32 shakeStart  = 0;
static int    shakeOX     = 0;
static int    shakeOY     = 0;
#define SHAKE_DURATION_MS 500

/* KEEP GOING anim */
#define KEEP_GOING_MS 1000

/* Slowdown */
static float  slowdownSpeed = 1.0f; /* multiplier on bgScrollSpeed */

/* Key state */
static int keyLeft = 0, keyRight = 0, keyUp = 0, keyE = 0;
static int keyEPressed = 0; /* edge detect */

/* Multiplayer */
static int multiplayerMode = 0;   /* 1 if ./prot 2 was passed */
static int p1UsesWASD      = 1;   /* 1=P1 uses WASD+Space, 0=P1 uses Arrows */
static int p1IsHarry       = 0;   /* 1 = P1 plays Harry (marv struct), 0 = P1 plays Marv (player struct) */
static char p1Name[64] = "Player 1";
static char p2Name[64] = "Player 2";
static int key2Left  = 0, key2Right  = 0, key2Up  = 0;  /* P2 input */
static int onlineRemoteInteractDown = 0;

/* ═══════════════════════════════════════════════════════════
   FORWARD DECLARATIONS
═══════════════════════════════════════════════════════════ */
static void startDialogue(const DlgLine *lines, int count, int freeze);
static void updateDialogue(Uint32 now);
static void renderDialogue(Uint32 now);
static void renderText(TTF_Font *f, const char *s, int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
static void renderTextCentered(TTF_Font *f, const char *s, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
static void renderTextRight(TTF_Font *f, const char *s, int x, int y, Uint8 r, Uint8 g, Uint8 b);
static void closeSDL(void);
static void initLevel(void);
static void update(Uint32 now);
static void render(Uint32 now, int present);
static void prepareLevel2Result(int completed);
static void playSfxChunk(Mix_Chunk *chunk);
static void playJumpSfx(void);
static void playDamageSfx(void);

/* ═══════════════════════════════════════════════════════════
   DIALOGUE HELPERS
═══════════════════════════════════════════════════════════ */
static Uint32 dlgCharDelay(const char *text, int idx)
{
    char c  = text[idx];
    int  len = (int)strlen(text);
    char c1 = (idx + 1 < len) ? text[idx + 1] : '\0';
    char c2 = (idx + 2 < len) ? text[idx + 2] : '\0';
    if (c == '.' && c1 == '.' && c2 == '.') return 300;
    if (c == '!' && c1 == '!')              return 120;
    if (c == '?' && c1 == '?')              return 110;
    if (c == '.' || c == '!' || c == '?')   return 280;
    if (c == ',')                            return 160;
    if (c == ' ')                            return 30;
    return 25; /* ~40 chars/sec */
}

static void clearWrappedDialogue(void)
{
    dlgWrappedCount = 0;
    for (int i = 0; i < DLG_WRAP_MAX_LINES; i++) {
        dlgWrappedLines[i][0] = '\0';
        dlgWrappedConsumedEnds[i] = 0;
    }
}

static void pushWrappedDialogueLine(const char *line, int consumedEnd)
{
    if (dlgWrappedCount >= DLG_WRAP_MAX_LINES) return;
    snprintf(dlgWrappedLines[dlgWrappedCount],
             sizeof(dlgWrappedLines[dlgWrappedCount]),
             "%s", line ? line : "");
    dlgWrappedConsumedEnds[dlgWrappedCount] = consumedEnd;
    dlgWrappedCount++;
}

static void prepareDialogueWrappedLine(void)
{
    clearWrappedDialogue();
    if (!dlgCurLines || dlgLineIdx < 0 || dlgLineIdx >= dlgCurCount) return;

    const char *text = dlgCurLines[dlgLineIdx].text;
    if (!text || !text[0]) return;

    if (!font) {
        pushWrappedDialogueLine(text, (int)strlen(text));
        return;
    }

    const int maxW = DLGBOX_W - DLGBOX_PORTRAIT_SIZE - 30;
    char line[DLG_WRAP_MAX_CHARS] = "";
    int lineSourceLen = 0;
    int totalConsumed = 0;
    const char *p = text;

    while (*p) {
        if (*p == '\n') {
            totalConsumed += lineSourceLen + 1;
            pushWrappedDialogueLine(line, totalConsumed);
            line[0] = '\0';
            lineSourceLen = 0;
            p++;
            continue;
        }

        int spaceCount = 0;
        while (*p == ' ') {
            spaceCount++;
            p++;
        }

        if (*p == '\0') {
            lineSourceLen += spaceCount;
            break;
        }
        if (*p == '\n') {
            totalConsumed += lineSourceLen + spaceCount + 1;
            pushWrappedDialogueLine(line, totalConsumed);
            line[0] = '\0';
            lineSourceLen = 0;
            p++;
            continue;
        }

        char word[DLG_WRAP_MAX_CHARS];
        int wi = 0;
        while (*p && *p != ' ' && *p != '\n') {
            if (wi < DLG_WRAP_MAX_CHARS - 1) word[wi++] = *p;
            p++;
        }
        word[wi] = '\0';

        char testLine[DLG_WRAP_MAX_CHARS];
        if (line[0]) {
            SDL_strlcpy(testLine, line, sizeof(testLine));
            SDL_strlcat(testLine, " ", sizeof(testLine));
            SDL_strlcat(testLine, word, sizeof(testLine));
        } else {
            SDL_strlcpy(testLine, word, sizeof(testLine));
        }

        int tw = 0;
        int th = 0;
        TTF_SizeText(font, testLine, &tw, &th);
        if (tw > maxW && line[0]) {
            totalConsumed += lineSourceLen + spaceCount;
            pushWrappedDialogueLine(line, totalConsumed);
            SDL_strlcpy(line, word, sizeof(line));
            lineSourceLen = wi;
        } else {
            SDL_strlcpy(line, testLine, sizeof(line));
            lineSourceLen += spaceCount + wi;
        }
    }

    if (line[0] || dlgWrappedCount == 0) {
        totalConsumed += lineSourceLen;
        pushWrappedDialogueLine(line, totalConsumed);
    }
}

static void startDialogue(const DlgLine *lines, int count, int freeze)
{
    dlgCurLines   = lines;
    dlgCurCount   = count;
    dlgLineIdx    = 0;
    dlgCharShown  = 0;
    dlgFullyShown = 0;
    dlgFreezeGame = freeze;
    dlgActive     = 1;
    dlgLineFullAt = 0;
    dlgNextCharMs = SDL_GetTicks() + 25;
    prepareDialogueWrappedLine();
}

static void advanceDlgLine(void)
{
    dlgLineIdx++;
    if (dlgLineIdx >= dlgCurCount) {
        dlgActive = 0;
        clearWrappedDialogue();
        return;
    }
    dlgCharShown  = 0;
    dlgFullyShown = 0;
    dlgLineFullAt = 0;
    dlgNextCharMs = SDL_GetTicks() + 25;
    prepareDialogueWrappedLine();
}

static void updateDialogue(Uint32 now)
{
    if (!dlgActive) return;
    const char *txt = dlgCurLines[dlgLineIdx].text;
    int len = (int)strlen(txt);
    if (!dlgFullyShown) {
        if (now >= dlgNextCharMs) {
            if (dlgCharShown < len) {
                dlgNextCharMs = now + dlgCharDelay(txt, dlgCharShown);
                if (txt[dlgCharShown] != ' ' && txt[dlgCharShown] != '\n')
                    playSfxChunk(sfxDialogueLetter);
                dlgCharShown++;
            }
            if (dlgCharShown >= len) {
                dlgFullyShown = 1;
                dlgLineFullAt = now;
            }
        }
    } else {
        /* Auto-advance after 1.5s pause */
        if (now - dlgLineFullAt >= 1500) {
            advanceDlgLine();
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   TEXT RENDERING
═══════════════════════════════════════════════════════════ */
static char *dupText(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (!copy) return NULL;
    memcpy(copy, s, len);
    return copy;
}

static void freeTextCacheEntry(TextCacheEntry *entry)
{
    if (!entry->used) return;
    if (entry->texture) SDL_DestroyTexture(entry->texture);
    free(entry->text);
    memset(entry, 0, sizeof(*entry));
}

static void clearTextCache(void)
{
    for (int i = 0; i < TEXT_CACHE_CAPACITY; i++) {
        freeTextCacheEntry(&textCache[i]);
    }
    textCacheStamp = 0;
}

static TextCacheEntry *getTextCacheEntry(TTF_Font *f, const char *s,
                                         Uint8 r, Uint8 g, Uint8 b)
{
    if (!f || !s || !s[0]) return NULL;

    int freeIdx = -1;
    int lruIdx = 0;
    Uint32 oldestStamp = 0xFFFFFFFFu;

    for (int i = 0; i < TEXT_CACHE_CAPACITY; i++) {
        TextCacheEntry *entry = &textCache[i];
        if (!entry->used) {
            if (freeIdx < 0) freeIdx = i;
            continue;
        }
        if (entry->font == f &&
            entry->r == r && entry->g == g && entry->b == b &&
            entry->text && strcmp(entry->text, s) == 0)
        {
            entry->lastUsed = ++textCacheStamp;
            return entry;
        }
        if (entry->lastUsed < oldestStamp) {
            oldestStamp = entry->lastUsed;
            lruIdx = i;
        }
    }

    int idx = (freeIdx >= 0) ? freeIdx : lruIdx;
    TextCacheEntry *entry = &textCache[idx];
    if (entry->used) freeTextCacheEntry(entry);

    SDL_Color col = {r, g, b, 255};
    SDL_Surface *sf = TTF_RenderText_Blended(f, s, col);
    if (!sf) return NULL;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(ren, sf);
    int textW = sf->w;
    int textH = sf->h;
    SDL_FreeSurface(sf);
    if (!texture) return NULL;

    char *copy = dupText(s);
    if (!copy) {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    entry->used = 1;
    entry->font = f;
    entry->r = r;
    entry->g = g;
    entry->b = b;
    entry->text = copy;
    entry->texture = texture;
    entry->w = textW;
    entry->h = textH;
    entry->lastUsed = ++textCacheStamp;
    return entry;
}

static void renderText(TTF_Font *f, const char *s, int x, int y,
                       Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    TextCacheEntry *entry = getTextCacheEntry(f, s, r, g, b);
    if (!entry) return;
    SDL_SetTextureAlphaMod(entry->texture, a);
    SDL_Rect dst = {x, y, entry->w, entry->h};
    SDL_RenderCopy(ren, entry->texture, NULL, &dst);
}

static void renderTextCentered(TTF_Font *f, const char *s, int y,
                                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    TextCacheEntry *entry = getTextCacheEntry(f, s, r, g, b);
    if (!entry) return;
    SDL_SetTextureAlphaMod(entry->texture, a);
    SDL_Rect dst = {SCREEN_W / 2 - entry->w / 2, y, entry->w, entry->h};
    SDL_RenderCopy(ren, entry->texture, NULL, &dst);
}

static void renderTextRight(TTF_Font *f, const char *s, int x, int y,
                             Uint8 r, Uint8 g, Uint8 b)
{
    TextCacheEntry *entry = getTextCacheEntry(f, s, r, g, b);
    if (!entry) return;
    SDL_SetTextureAlphaMod(entry->texture, 255);
    SDL_Rect dst = {x - entry->w, y, entry->w, entry->h};
    SDL_RenderCopy(ren, entry->texture, NULL, &dst);
}

static int selectedCharacterNumber(void)
{
    if (!activeSession) return 1;
    if (activeSession->player_skin_number[0] == 2) return 2;
    if (activeSession->player_skin_number[0] == 1) return 1;
    return (activeSession->skin_number == 2) ? 2 : 1;
}

static InteractBind level2InteractBindForPlayer(int player_index)
{
    InteractBind bind;

    if (!activeSession || player_index < 0 || player_index > 1)
        return INTERACT_BIND_E;

    bind = activeSession->player_interact_bind[player_index];
    if (bind == INTERACT_BIND_E || bind == INTERACT_BIND_F || bind == INTERACT_BIND_0)
        return bind;
    return INTERACT_BIND_E;
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

static int isLevel2InteractKey(SDL_Keycode key)
{
    if (!activeSession)
        return key == SDLK_e;

    if (keyMatchesInteractBind(key, level2InteractBindForPlayer(0)))
        return 1;
    if (activeSession->mode == GAME_MODE_DUO
        && keyMatchesInteractBind(key, level2InteractBindForPlayer(1)))
        return 1;

    /* Backward compatibility. */
    return key == SDLK_e;
}

static int companionCharacterNumber(void)
{
    return (selectedCharacterNumber() == 1) ? 2 : 1;
}

static int hasLevel2Companion(void)
{
    return 1;
}

static int level2OnlineHostActive(void)
{
    return online_client_is_connected() && online_client_is_host();
}

static Uint32 cardboxSpawnIntervalMs(void)
{
    return (multiplayerMode && level2OnlineHostActive())
        ? ONLINE_CARDBOX_SPAWN_INTERVAL_MS
        : CARDBOX_SPAWN_INTERVAL_MS;
}

static void applyOnlineRemoteDuoInput(void)
{
    int remoteLeft;
    int remoteRight;
    int remoteUp;
    int remoteInteractNow;

    if (!multiplayerMode || !level2OnlineHostActive()) {
        onlineRemoteInteractDown = 0;
        return;
    }

    remoteLeft = online_client_remote_scancode_down(SDL_SCANCODE_A) ? 1 : 0;
    remoteRight = online_client_remote_scancode_down(SDL_SCANCODE_D) ? 1 : 0;
    remoteUp = (online_client_remote_scancode_down(SDL_SCANCODE_W) ||
                online_client_remote_scancode_down(SDL_SCANCODE_SPACE)) ? 1 : 0;

    if (p1IsHarry) {
        keyLeft = remoteLeft;
        keyRight = remoteRight;
        keyUp = remoteUp;
    } else {
        key2Left = remoteLeft;
        key2Right = remoteRight;
        key2Up = remoteUp;
    }

    remoteInteractNow =
        online_client_remote_scancode_down(SDL_SCANCODE_F) ||
        online_client_remote_scancode_down(SDL_SCANCODE_0);
    if (remoteInteractNow && !onlineRemoteInteractDown) {
        keyE = 1;
        keyEPressed = 1;
    }
    onlineRemoteInteractDown = remoteInteractNow;
}

static int dialogueCharacterNumber(DlgCharacter speaker)
{
    return (speaker == CHAR_MARV) ? 1 : 2;
}

static const char *characterName(int characterNumber)
{
    return (characterNumber == 2) ? "Harry" : "Marv";
}

static SDL_Texture *characterRunTexture(int characterNumber)
{
    return (characterNumber == 2) ? texHarryRun : texMarvRun;
}

static SDL_Texture *characterIdleTexture(int characterNumber)
{
    return (characterNumber == 2) ? texHarryIdle : texMarvIdle;
}

static SDL_Texture *characterJumpTexture(int characterNumber)
{
    return (characterNumber == 2) ? texHarryJump : texMarvJump;
}

static SDL_Texture *characterJetpackTexture(int characterNumber)
{
    return (characterNumber == 2) ? texHarryJetpack : texMarvJetpack;
}

static void characterTextureSize(SDL_Texture *texture, int characterNumber, int *w, int *h)
{
    if (w) *w = 0;
    if (h) *h = 0;
    if (texture == characterIdleTexture(characterNumber)) {
        if (characterNumber == 2) {
            if (w) *w = texHarryIdleW;
            if (h) *h = texHarryIdleH;
        } else {
            if (w) *w = texMarvIdleW;
            if (h) *h = texMarvIdleH;
        }
    } else if (texture == characterJumpTexture(characterNumber)) {
        if (characterNumber == 2) {
            if (w) *w = texHarryJumpW;
            if (h) *h = texHarryJumpH;
        } else {
            if (w) *w = texMarvJumpW;
            if (h) *h = texMarvJumpH;
        }
    } else if (texture == characterJetpackTexture(characterNumber)) {
        if (characterNumber == 2) {
            if (w) *w = texHarryJetpackW;
            if (h) *h = texHarryJetpackH;
        } else {
            if (w) *w = texMarvJetpackW;
            if (h) *h = texMarvJetpackH;
        }
    } else {
        if (characterNumber == 2) {
            if (w) *w = texHarryRunW;
            if (h) *h = texHarryRunH;
        } else {
            if (w) *w = texMarvRunW;
            if (h) *h = texMarvRunH;
        }
    }
}

static SDL_Texture *characterPortraitTexture(int characterNumber)
{
    return (characterNumber == 2) ? texPortraitHarry : texPortraitMarv;
}

static SDL_Texture *characterStaticIdleTexture(int characterNumber)
{
    return (characterNumber == 2) ? texStaticHarry : texStaticMarv;
}

static void characterStaticIdleSize(int characterNumber, int *w, int *h)
{
    if (w) *w = (characterNumber == 2) ? texStaticHarryW : texStaticMarvW;
    if (h) *h = (characterNumber == 2) ? texStaticHarryH : texStaticMarvH;
}

static void prepareLevel2Result(int completed)
{
    int player_lost;
    int marv_lost;

    if (!activeSession || level2ResultReady) return;

    player_lost = level2StartLives - player.lives;
    if (player_lost < 0) player_lost = 0;

    marv_lost = level2StartLives - marv.lives;
    if (marv_lost < 0) marv_lost = 0;

    activeSession->level2.starting_lives = level2StartLives;
    activeSession->level2.player_lives_lost = player_lost;
    activeSession->level2.marv_lives_lost = multiplayerMode ? marv_lost : -1;
    activeSession->level2.mini_key_found = miniKeyCollected ? 1 : 0;
    activeSession->level2.multiplayer = multiplayerMode ? 1 : 0;
    activeSession->level2.completed = completed ? 1 : 0;
    session_calculate_level2_points(activeSession);
    if (!activeSession->level2.completed)
        session_clear_level_life_carry();
    session_calculate_total_points(activeSession);
    level2ResultReady = 1;
}

/* ═══════════════════════════════════════════════════════════
   INIT / CLEANUP
═══════════════════════════════════════════════════════════ */
static SDL_Texture *loadTextureWithSize(const char *path, const char *label,
                                        int *w, int *h)
{
    if (w) *w = 0;
    if (h) *h = 0;

    SDL_Texture *texture = IMG_LoadTexture(ren, path);
    if (!texture) {
        SDL_Log("WARNING: could not load %s — %s", label, IMG_GetError());
        return NULL;
    }

    SDL_QueryTexture(texture, NULL, NULL, w, h);
    return texture;
}

static Mix_Chunk *loadSfxChunkWithFallbacks(const char *fileName, const char *label)
{
    char pathBuf[3][PATH_MAX];
    const char *paths[4];
    Mix_Chunk *chunk = NULL;
    snprintf(pathBuf[0], sizeof(pathBuf[0]), "assets/sounds/%s", fileName);
    snprintf(pathBuf[1], sizeof(pathBuf[1]), "level2-chase/assets/sounds/%s", fileName);
    snprintf(pathBuf[2], sizeof(pathBuf[2]), "../level2-chase/assets/sounds/%s", fileName);
    paths[0] = pathBuf[0];
    paths[1] = pathBuf[1];
    paths[2] = pathBuf[2];
    paths[3] = NULL;

    for (int i = 0; paths[i] && !chunk; i++) {
        chunk = Mix_LoadWAV(paths[i]);
    }

    if (!chunk) {
        SDL_Log("WARNING: Level 2 could not load SFX %s", label);
    }
    return chunk;
}

static void loadLevel2Sfx(void)
{
    sfxJump[0] = loadSfxChunkWithFallbacks("jump_1.wav", "jump_1.wav");
    sfxJump[1] = loadSfxChunkWithFallbacks("jump_2.wav", "jump_2.wav");
    sfxLand = loadSfxChunkWithFallbacks("land.wav", "land.wav");
    sfxTrapCrack = loadSfxChunkWithFallbacks("wood_crack.wav", "wood_crack.wav");
    sfxDialogueLetter = loadSfxChunkWithFallbacks("dialogue_letter_appear.wav",
                                                  "dialogue_letter_appear.wav");
    sfxDamage[0] = loadSfxChunkWithFallbacks("taking_damage.mp3", "taking_damage.mp3");
    sfxDamage[1] = loadSfxChunkWithFallbacks("taking_damage.mp3", "taking_damage.mp3");
    sfxDamage[2] = loadSfxChunkWithFallbacks("taking_damage.mp3", "taking_damage.mp3");
    sfxDamage[3] = loadSfxChunkWithFallbacks("taking_damage.mp3", "taking_damage.mp3");
    sfxSlamImpact = loadSfxChunkWithFallbacks("slam_impact_appliances.mp3",
                                              "slam_impact_appliances.mp3");
    sfxFakeExit = loadSfxChunkWithFallbacks("fake_exit_key.mp3", "fake_exit_key.mp3");
    sfxGravityFlip = loadSfxChunkWithFallbacks("gravity_flip.mp3", "gravity_flip.mp3");
    sfxKeyPickup = loadSfxChunkWithFallbacks("key_pickup.mp3", "key_pickup.mp3");
    sfxDoorOpen = loadSfxChunkWithFallbacks("door_open.mp3", "door_open.mp3");
    sfxRatSqueak = loadSfxChunkWithFallbacks("rats.mp3", "rats.mp3");
    sfxCardboardRoll = loadSfxChunkWithFallbacks("cardboard_roll.mp3", "cardboard_roll.mp3");
}

static void freeLevel2Sfx(void)
{
    for (int i = 0; i < 2; i++) {
        if (sfxJump[i]) {
            Mix_FreeChunk(sfxJump[i]);
            sfxJump[i] = NULL;
        }
    }
    for (int i = 0; i < 5; i++) {
        if (sfxDamage[i]) {
            Mix_FreeChunk(sfxDamage[i]);
            sfxDamage[i] = NULL;
        }
    }
    if (sfxLand) {
        Mix_FreeChunk(sfxLand);
        sfxLand = NULL;
    }
    if (sfxTrapCrack) {
        Mix_FreeChunk(sfxTrapCrack);
        sfxTrapCrack = NULL;
    }
    if (sfxDialogueLetter) {
        Mix_FreeChunk(sfxDialogueLetter);
        sfxDialogueLetter = NULL;
    }
    if (sfxSlamImpact) {
        Mix_FreeChunk(sfxSlamImpact);
        sfxSlamImpact = NULL;
    }
    if (sfxFakeExit) {
        Mix_FreeChunk(sfxFakeExit);
        sfxFakeExit = NULL;
    }
    if (sfxGravityFlip) {
        Mix_FreeChunk(sfxGravityFlip);
        sfxGravityFlip = NULL;
    }
    if (sfxKeyPickup) {
        Mix_FreeChunk(sfxKeyPickup);
        sfxKeyPickup = NULL;
    }
    if (sfxDoorOpen) {
        Mix_FreeChunk(sfxDoorOpen);
        sfxDoorOpen = NULL;
    }
    if (sfxRatSqueak) {
        Mix_FreeChunk(sfxRatSqueak);
        sfxRatSqueak = NULL;
    }
    if (sfxCardboardRoll) {
        Mix_FreeChunk(sfxCardboardRoll);
        sfxCardboardRoll = NULL;
    }
    Mix_HaltChannel(SFX_CH_RAT_SQUEAK);
    Mix_HaltChannel(SFX_CH_CARDBOARD_ROLL);
}

static void playSfxChunk(Mix_Chunk *chunk)
{
    if (!chunk) return;
    Mix_PlayChannel(-1, chunk, 0);
}

static void playJumpSfx(void)
{
    Mix_Chunk *pick = NULL;
    if (sfxJump[0] && sfxJump[1]) pick = sfxJump[rand() % 2];
    else if (sfxJump[0]) pick = sfxJump[0];
    else if (sfxJump[1]) pick = sfxJump[1];
    playSfxChunk(pick);
}

static void playDamageSfx(void)
{
    Mix_Chunk *available[5];
    int count = 0;
    for (int i = 0; i < 5; i++) {
        if (sfxDamage[i]) available[count++] = sfxDamage[i];
    }
    if (count > 0)
        playSfxChunk(available[rand() % count]);
}

static void startLevel2Music(void)
{
    const char *musicPaths[] = {
        "assets/sounds/bg_music.mp3",
        "level2-chase/assets/sounds/bg_music.mp3",
        "../level2-chase/assets/sounds/bg_music.mp3",
        NULL
    };
    int mixerFlags;

    mixerFlags = Mix_Init(0);
    if ((mixerFlags & MIX_INIT_MP3) == 0) {
        if ((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) == 0) {
            SDL_Log("WARNING: Level 2 MP3 mixer init failed: %s", Mix_GetError());
            return;
        }
        mixerOwned = 1;
    }

    if (!Mix_QuerySpec(NULL, NULL, NULL)) {
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
            SDL_Log("WARNING: Level 2 audio open failed: %s", Mix_GetError());
            if (mixerOwned) {
                Mix_Quit();
                mixerOwned = 0;
            }
            return;
        }
        audioOwned = 1;
    }
    Mix_AllocateChannels(16);

    loadLevel2Sfx();

    for (int i = 0; musicPaths[i] && !bgMusic; i++)
        bgMusic = Mix_LoadMUS(musicPaths[i]);

    if (!bgMusic) {
        SDL_Log("WARNING: Level 2 could not load bg_music.mp3");
        return;
    }

    if (Mix_PlayingMusic()) Mix_HaltMusic();
    Mix_VolumeMusic(options_scene_get_music_volume_sdl());
    if (Mix_PlayMusic(bgMusic, -1) != 0)
        SDL_Log("WARNING: Level 2 music playback failed: %s", Mix_GetError());
}

static void stopLevel2Music(void)
{
    if (bgMusic) {
        if (Mix_PlayingMusic()) Mix_HaltMusic();
        Mix_FreeMusic(bgMusic);
        bgMusic = NULL;
    }
    freeLevel2Sfx();
    if (audioOwned) {
        Mix_CloseAudio();
        audioOwned = 0;
    }
    if (mixerOwned) {
        Mix_Quit();
        mixerOwned = 0;
    }
}

static int initSDL(void)
{
    srand((unsigned)time(NULL));
    if (ownsSDL) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            SDL_Log("SDL_Init error: %s", SDL_GetError());
            return 0;
        }
        if (TTF_Init() != 0) {
            SDL_Log("TTF_Init error: %s", TTF_GetError());
            return 0;
        }
        IMG_Init(IMG_INIT_PNG);

        win = SDL_CreateWindow("Chapter 2 : The Chase",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
        if (!win) { SDL_Log("Window error: %s", SDL_GetError()); return 0; }

        ren = SDL_CreateRenderer(win, -1,
                                 SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!ren) { SDL_Log("Renderer error: %s", SDL_GetError()); return 0; }
    } else if (!win || !ren) {
        return 0;
    }

    arcade_input_init();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    /* Load fonts — try local then system fallbacks */
    const char *fontPaths[] = {
        "font.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        NULL
    };
    for (int i = 0; fontPaths[i] && !font; i++)
        font = TTF_OpenFont(fontPaths[i], 20);
    for (int i = 0; fontPaths[i] && !bigFont; i++)
        bigFont = TTF_OpenFont(fontPaths[i], 36);
    if (!font)    SDL_Log("WARNING: no font loaded — text will be invisible");
    if (!bigFont) SDL_Log("WARNING: no bigFont loaded");

    texMarvRun = loadTextureWithSize("assets/animations/skin1_run.png",
                                     "skin1_run.png", &texMarvRunW, &texMarvRunH);
    texMarvIdle = loadTextureWithSize("assets/animations/skin1_idle.png",
                                      "skin1_idle.png", &texMarvIdleW, &texMarvIdleH);
    texMarvJump = loadTextureWithSize("assets/animations/skin1_jump.png",
                                      "skin1_jump.png", &texMarvJumpW, &texMarvJumpH);
    texMarvJetpack = loadTextureWithSize("assets/animations/skin1_jetpack.png",
                                         "skin1_jetpack.png",
                                         &texMarvJetpackW, &texMarvJetpackH);
    texHarryIdle = loadTextureWithSize("assets/animations/skin2_idle.png",
                                       "skin2_idle.png", &texHarryIdleW, &texHarryIdleH);
    texHarryRun = loadTextureWithSize("assets/animations/skin2_run.png",
                                      "skin2_run.png", &texHarryRunW, &texHarryRunH);
    texHarryJump = loadTextureWithSize("assets/animations/skin2_jump.png",
                                       "skin2_jump.png", &texHarryJumpW, &texHarryJumpH);
    texHarryJetpack = loadTextureWithSize("assets/animations/skin2_jetpack.png",
                                          "skin2_jetpack.png",
                                          &texHarryJetpackW, &texHarryJetpackH);
    texCardbox = loadTextureWithSize("assets/animations/cardboard_box.png",
                                     "cardboard_box.png", &texCardboxW, &texCardboxH);
    texKid = loadTextureWithSize("assets/animations/kid_idle.png",
                                 "kid_idle.png", &texKidW, &texKidH);
    texStaticKid = loadTextureWithSize("assets/static/kid_idle.png",
                                       "static/kid_idle.png", &texStaticKidW, &texStaticKidH);
    texStaticMarv = loadTextureWithSize("assets/static/skin1_idle.png",
                                        "static/skin1_idle.png",
                                        &texStaticMarvW, &texStaticMarvH);
    texStaticHarry = loadTextureWithSize("assets/static/skin2_idle.png",
                                         "static/skin2_idle.png",
                                         &texStaticHarryW, &texStaticHarryH);
    texCans = loadTextureWithSize("assets/items/cans.png",
                                  "cans.png", &texCansW, &texCansH);
    texFridge = loadTextureWithSize("assets/items/fridge.png",
                                    "fridge.png", &texFridgeW, &texFridgeH);
    texMicrowave = loadTextureWithSize("assets/static/microwave.png",
                                       "microwave.png", &texMicrowaveW, &texMicrowaveH);
    texRat = loadTextureWithSize("assets/animations/rat_idle.png",
                                 "rat_idle.png", &texRatW, &texRatH);
    texPortraitHarry = loadTextureWithSize("assets/portraits/skin2_portrait.png",
                                           "portraits/skin2_portrait.png",
                                           &texPortraitHarryW, &texPortraitHarryH);
    texPortraitMarv = loadTextureWithSize("assets/portraits/skin1_portrait.png",
                                          "portraits/skin1_portrait.png",
                                          &texPortraitMarvW, &texPortraitMarvH);
    if (texPortraitHarry) SDL_SetTextureBlendMode(texPortraitHarry, SDL_BLENDMODE_BLEND);
    if (texPortraitMarv) SDL_SetTextureBlendMode(texPortraitMarv, SDL_BLENDMODE_BLEND);

    texBgFar = loadTextureWithSize("assets/backgrounds/bg_street_far.png",
                                   "bg_street_far.png", &texBgFarW, &texBgFarH);
    texBgNear = loadTextureWithSize("assets/backgrounds/bg_street_near.png",
                                    "bg_street_near.png", &texBgNearW, &texBgNearH);
    texGround = loadTextureWithSize("assets/backgrounds/bg_ground_v2.jpg",
                                    "bg_ground_v2.jpg", &texGroundW, &texGroundH);
    if (!texGround) {
        texGround = loadTextureWithSize("assets/backgrounds/bg_ground.png",
                                        "bg_ground.png", &texGroundW, &texGroundH);
    }
    texPlatform = loadTextureWithSize("assets/items/platform_house.png",
                                      "platform_house.png",
                                      &texPlatformW, &texPlatformH);

    startLevel2Music();

    return 1;
}

static void closeSDL(void)
{
    stopLevel2Music();
    clearTextCache();
    if (texMarvRun)  SDL_DestroyTexture(texMarvRun);
    if (texMarvIdle) SDL_DestroyTexture(texMarvIdle);
    if (texMarvJump)  SDL_DestroyTexture(texMarvJump);
    if (texMarvJetpack) SDL_DestroyTexture(texMarvJetpack);
    if (texHarryIdle) SDL_DestroyTexture(texHarryIdle);
    if (texHarryRun)  SDL_DestroyTexture(texHarryRun);
    if (texHarryJump) SDL_DestroyTexture(texHarryJump);
    if (texHarryJetpack) SDL_DestroyTexture(texHarryJetpack);
    if (texCardbox)   SDL_DestroyTexture(texCardbox);
    if (texKid)       SDL_DestroyTexture(texKid);
    if (texStaticKid)   SDL_DestroyTexture(texStaticKid);
    if (texStaticMarv)  SDL_DestroyTexture(texStaticMarv);
    if (texStaticHarry) SDL_DestroyTexture(texStaticHarry);
    if (texCans)      SDL_DestroyTexture(texCans);
    if (texFridge)    SDL_DestroyTexture(texFridge);
    if (texMicrowave) SDL_DestroyTexture(texMicrowave);
    if (texRat)       SDL_DestroyTexture(texRat);
    if (texPortraitHarry) SDL_DestroyTexture(texPortraitHarry);
    if (texPortraitMarv)  SDL_DestroyTexture(texPortraitMarv);
    if (texBgFar)  SDL_DestroyTexture(texBgFar);
    if (texBgNear) SDL_DestroyTexture(texBgNear);
    if (texGround)    SDL_DestroyTexture(texGround);
    if (texPlatform)  SDL_DestroyTexture(texPlatform);
    if (font)    TTF_CloseFont(font);
    if (bigFont) TTF_CloseFont(bigFont);
    if (ownsSDL) {
        TTF_Quit();
        IMG_Quit();
        if (ren) SDL_DestroyRenderer(ren);
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        ren = NULL;
        win = NULL;
    } else if (ren) {
        SDL_SetRenderTarget(ren, NULL);
        SDL_RenderSetViewport(ren, NULL);
        SDL_RenderSetClipRect(ren, NULL);
        SDL_RenderSetLogicalSize(ren, 0, 0);
    }
    arcade_input_shutdown();
    font = NULL;
    bigFont = NULL;
}

/* ═══════════════════════════════════════════════════════════
   LEVEL INIT
═══════════════════════════════════════════════════════════ */
static void initLevel(void)
{
    const int maxBonusLives = 99;
    int carryStartLives = 0;
    int remainingLives = MAX_LIVES;
    int bonusLives = 0;

    if (session_load_level_life_carry(&carryStartLives, NULL, NULL)) {
        level2StartLives = carryStartLives;
    } else if (activeSession) {
        if (activeSession->level1.completed) {
            remainingLives = activeSession->level1.lives_remaining;
            if (remainingLives < 1) remainingLives = 1;
            if (remainingLives > MAX_LIVES) remainingLives = MAX_LIVES;
        }
        bonusLives = activeSession->bonus_lives_for_level2;
        if (bonusLives < 0) bonusLives = 0;
        if (bonusLives > maxBonusLives) bonusLives = maxBonusLives;
        level2StartLives = remainingLives + bonusLives;
    } else {
        level2StartLives = MAX_LIVES;
    }

    if (level2StartLives < 1) level2StartLives = 1;
    if (level2StartLives > (MAX_LIVES + maxBonusLives)) level2StartLives = MAX_LIVES + maxBonusLives;
    level2ResultReady = 0;

    /* Player */
    player.x            = 200.0f;
    player.y            = (float)(GROUND_Y - PLAYER_H);
    player.vx           = 0.0f;
    player.vy           = 0.0f;
    player.onGround     = 1;
    player.facingRight  = 1;
    player.animFrame    = 0;
    player.animLastTick = SDL_GetTicks();
    player.lives        = level2StartLives;
    player.invincible   = 0;
    player.invincStart  = 0;
    player.keepGoingShow= 0;
    player.keepGoingStart=0;
    player.dead = 0;
    player.deadSince = 0;
    player.hasKey       = 0;

    /* Marv */
    marv.x          = 120.0f;
    marv.y          = (float)(GROUND_Y - PLAYER_H);
    marv.vx         = 0.0f;
    marv.vy         = 0.0f;
    marv.onGround   = 1;
    marv.animFrame  = 0;
    marv.animLastTick = SDL_GetTicks();
    marv.animFps    = ANIM_FPS;
    marv.targetX    = 120.0f;
    marv.reEntryDone= 1;
    marv.lives        = level2StartLives;
    marv.invincible   = 0;
    marv.keepGoingShow = 0;
    marv.dead = 0;
    marv.deadSince = 0;

    /* Kevin */
    kevinX = 1200.0f;
    kevinY = (float)(GROUND_Y - PLAYER_H);
    kevinVisible = 1;

    /* Objects */
    memset(objects, 0, sizeof(objects));
    objCount     = 0;
    slamSpawned  = 0;
    slamDlgStarted = 0;
    objLastSpawn = SDL_GetTicks();
    cardboxFrame = 0;
    cardboxLastTick = SDL_GetTicks();
    kevinAnimFrame = 0;
    kevinAnimLastTick = SDL_GetTicks();

    /* Platforms */
    memset(platforms, 0, sizeof(platforms));
    lastPlatWasTrap = 0;
    platLastSpawn = SDL_GetTicks();
    patternCycle  = 0;
    patternStart  = SDL_GetTicks();
    patternPlatsThisCycle = 0;

    /* Background */
    bgOff[0] = bgOff[1] = bgOff[2] = 0.0f;
    bgScrollSpeed = 1.0f;

    /* State */
    gameState  = GS_INTRO_FADE_IN;
    chasePhase = PHASE_OBJECTS;
    phaseStart = SDL_GetTicks();
    phasePlatsDone = 0;
    marvReEntryStarted = 0;
    kevinEscapeDlgDone = 0;
    slowdownSpeed = 1.0f;

    /* Intro */
    introWhite      = 255.0f;
    introTitleAlpha = 0.0f;
    introTimer      = SDL_GetTicks();

    /* Dialogue */
    dlgActive     = 0;
    dlgCurLines   = NULL;
    dlgCurCount   = 0;
    dlgLineIdx    = 0;
    dlgCharShown  = 0;
    dlgFullyShown = 0;
    dlgSeqPending = -1;

    /* Transition */
    transAlpha = 0.0f;
    transDir   = 0;

    /* End room */
    marvEndVx   = 0.0f;
    marvEndAlpha= 255.0f;
    endFadeActive = 0;
    endFadeAlpha  = 0.0f;
    topDoorClosed = 0;
    topDoorUsed   = 0;
    needKeyShow = 0;

    /* Mini level */
    miniPlat0Touched  = 0;
    miniPlat0FallStart= 0;
    miniPlat0Y        = 555.0f;
    miniGravFlipped   = 0;
    miniGravFlipStart = 0;
    miniPlat2Bounced  = 0;
    miniFakeExitUsed  = 0;
    miniKeySpawned    = 0;
    miniKeyX          = 620.0f;
    miniKeyY          = 284.0f;
    miniKeyCollected  = 0;
    miniTrapDoorOpen  = 0;
    miniShowWTF       = 0;
    miniShowBonk      = 0;
    miniShowPsych     = 0;
    miniPlat4Touched  = 0;
    miniPlat4FallStart= 0;
    miniPlat4Y        = 460.0f;
    miniPlat5Touched  = 0;
    miniPlat5FallStart= 0;
    miniPlat5Y        = 340.0f;
    miniFakeExit2Used = 0;
    miniShowPsych2    = 0;
    miniPsych2Start   = 0;

    mwRound = 0; mwActive = 0; mwWaiting = 0; mwBothOut = 0; mwReentering = 0; dlgAtTop = 0; mwAutoAdvanceAt = 0;

    ratsActive        = 0;
    launchJumpActive  = 0;
    ratsRetreating    = 0;
    ratsRetreatStart  = 0;
    ratSqueakNextAt   = 0;
    slamReEntryActive = 0;
    slamReEntryDlgStarted = 0;
    for (int ri = 0; ri < RAT_COUNT; ri++) rats[ri].active = 0;
    Mix_HaltChannel(SFX_CH_RAT_SQUEAK);
    Mix_HaltChannel(SFX_CH_CARDBOARD_ROLL);

    shakeActive = 0;
    keyLeft = keyRight = keyUp = keyE = keyEPressed = 0;
    key2Left = key2Right = key2Up = 0;
    onlineRemoteInteractDown = 0;

    slamPushingPlayer = 0;
    slamPushingMarv   = 0;
    memset(skyBombs, 0, sizeof(skyBombs));
    hazardLastSpawn = 0;
    hazardNextSpawnAt = 0;
    slamWaiting   = 0;
    slamWaitStart = 0;

    pushBackActive = 0;
    pushBackVx     = 0.0f;

    debugInvincible = 0;
    groundAdminMode = 0;
    groundDrawHeight = SCREEN_H - GROUND_Y;
    groundRepeatStartX = 0;
    fpsDisplay = 0.0f;
    fpsWindowStart = SDL_GetTicks();
    fpsFrameCounter = 0;
    clearWrappedDialogue();
}

/* ═══════════════════════════════════════════════════════════
   PHYSICS HELPERS
═══════════════════════════════════════════════════════════ */
/* Apply gravity and clamp fall speed */
static void applyGravity(float *vy)
{
    *vy += GRAVITY;
    if (*vy > MAX_FALL) *vy = MAX_FALL;
}

/* Platform collision (chase platforms, returns 1 if landed) */
static int landOnPlatforms(float *px, float *py, float *pvy, int *onGround,
                           float pw, float ph, int triggerTrap)
{
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!platforms[i].active) continue;
        Platform *p = &platforms[i];
        float impactVy = *pvy;
        /* Check top collision only (falling onto platform) */
        if (*pvy >= 0.0f
            && *px + pw > p->x
            && *px      < p->x + p->w
            && *py + ph <= p->y + 8.0f
            && *py + ph + *pvy >= p->y)
        {
            *py       = p->y - ph;
            *pvy      = 0.0f;
            *onGround = 1;
            if (triggerTrap && p->trap && !p->trapTriggered) {
                p->trapTriggered = 1;
                *pvy = 20.0f;
                *onGround = 0;
                playSfxChunk(sfxTrapCrack);
            } else if (impactVy > 2.0f) {
                playSfxChunk(sfxLand);
            }
            return 1;
        }
    }
    return 0;
}

/* Static platform collision for end room */
static int landOnStaticPlats(float *px, float *py, float *pvy, int *onGround, float pw, float ph)
{
    for (int i = 0; i < 3; i++) {
        StaticPlat *p = &endPlats[i];
        float impactVy = *pvy;
        if (*pvy >= 0.0f
            && *px + pw > (float)p->x
            && *px      < (float)(p->x + p->w)
            && *py + ph <= (float)p->y + 8.0f
            && *py + ph + *pvy >= (float)p->y)
        {
            *py       = (float)p->y - ph;
            *pvy      = 0.0f;
            *onGround = 1;
            if (impactVy > 2.0f) playSfxChunk(sfxLand);
            return 1;
        }
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
   OBJECT SPAWN
═══════════════════════════════════════════════════════════ */
static void spawnObject(int big)
{
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) {
            objects[i].active = 1;
            objects[i].big    = big;
            if (big) {
                objects[i].x  = (float)SCREEN_W + 50.0f;
                objects[i].y  = (float)(GROUND_Y - 500);
                objects[i].vx = -6.5f;
                objects[i].vy = 0.0f;
            } else {
                objects[i].x  = 1280.0f;
                objects[i].y  = (float)(GROUND_Y - 150 - (rand() % 80));
                objects[i].vx = -10.0f;
                objects[i].vy = -8.0f;
            }
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   PLATFORM SPAWN (CHASE)
═══════════════════════════════════════════════════════════ */
static void spawnPlatform(float y, int w);

static void spawnPlatformNoTrap(float y, int w)
{
    spawnPlatform(y, w);
    /* Clear trap flag on the most recently spawned platform */
    for (int i = MAX_PLATFORMS - 1; i >= 0; i--) {
        if (platforms[i].active) {
            platforms[i].trap = 0;
            platforms[i].trapTriggered = 0;
            break;
        }
    }
}

static void spawnPlatform(float y, int w)
{
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!platforms[i].active) {
            platforms[i].active = 1;
            platforms[i].x = (float)SCREEN_W;
            platforms[i].y = y;
            platforms[i].w = w;
            platforms[i].h = 16;
            platforms[i].trap = 0;
            platforms[i].trapTriggered = 0;
            platforms[i].vy = 0.0f;
            /* 10% trap chance, never two in a row */
            if (!lastPlatWasTrap && (rand() % 10) == 0) {
                platforms[i].trap = 1;
                lastPlatWasTrap = 1;
            } else {
                lastPlatWasTrap = 0;
            }
            return;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   PLAYER HIT / LIVES
═══════════════════════════════════════════════════════════ */
static void playerHit(void)
{
    if (debugInvincible) return;
    if (player.invincible) return;
    playDamageSfx();
    player.lives--;
    if (player.lives <= 0) {
        player.lives = 0;
        if (multiplayerMode && !marv.dead) {
            /* Other player alive — start respawn timer */
            player.dead     = 1;
            player.deadSince = SDL_GetTicks();
            player.x        = -500.0f;   /* move off-screen */
            player.vx       = 0.0f;
            player.vy       = 0.0f;
        } else {
            gameState = GS_GAME_OVER;
        }
        return;
    }
    player.vx = -12.0f;
    player.vy = -5.0f;
    player.onGround = 0;
    player.invincible   = 1;
    player.invincStart  = SDL_GetTicks();
    player.keepGoingShow= 1;
    player.keepGoingStart = SDL_GetTicks();
}

/* ═══════════════════════════════════════════════════════════
   MARV (P2) HIT / LIVES
═══════════════════════════════════════════════════════════ */
static void marvHit(void)
{
    if (!multiplayerMode) return;
    if (debugInvincible) return;
    if (marv.invincible) return;
    playDamageSfx();
    marv.lives--;
    if (marv.lives <= 0) {
        marv.lives = 0;
        if (!player.dead) {
            /* P1 alive — start P2 respawn timer */
            marv.dead     = 1;
            marv.deadSince = SDL_GetTicks();
            marv.x        = -500.0f;
            marv.vx       = 0.0f;
            marv.vy       = 0.0f;
        } else {
            gameState = GS_GAME_OVER;
        }
        return;
    }
    marv.vx = -12.0f;
    marv.vy = -5.0f;
    marv.onGround = 0;
    marv.invincible    = 1;
    marv.invincStart   = SDL_GetTicks();
    marv.keepGoingShow = 1;
    marv.keepGoingStart = SDL_GetTicks();
}

/* ═══════════════════════════════════════════════════════════
   SKY BOMBS
═══════════════════════════════════════════════════════════ */
static void updateSkyBombs(Uint32 now)
{
    (void)now;
    for (int i = 0; i < MAX_SKY_BOMBS; i++) {
        if (!skyBombs[i].active) continue;
        skyBombs[i].y += 10.0f; /* drop */
        skyBombs[i].angle += 6.0f;
        if (skyBombs[i].angle >= 360.0f) skyBombs[i].angle -= 360.0f;
        /* Hit player */
        if (!player.invincible &&
            player.x + PLAYER_W > skyBombs[i].x &&
            player.x            < skyBombs[i].x + 128 &&
            player.y + PLAYER_H > skyBombs[i].y &&
            player.y            < skyBombs[i].y + 128)
        {
            playerHit();
            skyBombs[i].active = 0;
        }
        /* Hit P2 */
        if (multiplayerMode && !marv.invincible &&
            marv.x + PLAYER_W > skyBombs[i].x &&
            marv.x            < skyBombs[i].x + 128 &&
            marv.y + PLAYER_H > skyBombs[i].y &&
            marv.y            < skyBombs[i].y + 128)
        {
            marvHit();
            skyBombs[i].active = 0;
        }
        /* Remove when off screen */
        if (skyBombs[i].y > SCREEN_H + 40) skyBombs[i].active = 0;
    }
}

/* ═══════════════════════════════════════════════════════════
   UPDATE CHASE PHASES
═══════════════════════════════════════════════════════════ */
static void updateChaseObjects(Uint32 now)
{
    int anyCardboxActive = 0;

    if (now - cardboxLastTick >= 4) {
        cardboxFrame += 2;
        if (cardboxFrame >= 36) cardboxFrame = 0;
        cardboxLastTick = now;
    }

    /* Spawn logic */
    if (!slamSpawned && objCount < 15) {
        if (now - objLastSpawn >= cardboxSpawnIntervalMs()) {
            spawnObject(0);
            objCount++;
            objLastSpawn = now;
        }
    } else if (!slamSpawned && objCount >= 15) {
        /* First: trigger warning dialogue, keep playing normally */
        if (!slamDlgStarted) {
            startDialogue(dlgFridgeWarning, DLG_FRIDGE_WARNING_COUNT, 0);
            slamDlgStarted = 1;
        }
        /* Once dialogue is done, spawn the fridge */
        if (slamDlgStarted && !dlgActive) {
            spawnObject(1);
            playSfxChunk(sfxSlamImpact);
            slamSpawned = 1;
            chasePhase  = PHASE_SLAM;
            phaseStart  = now;
        }
    }

    /* Update each object */
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        if (!objects[i].big) {
            anyCardboxActive = 1;
            /* Arc object */
            float groundLevel = (float)(GROUND_Y - 40);
            if (objects[i].y < groundLevel) {
                objects[i].vy += GRAVITY;
                objects[i].y  += objects[i].vy;
                objects[i].x  += objects[i].vx;
                if (objects[i].y >= groundLevel) {
                    objects[i].y  = groundLevel;
                    objects[i].vy = 0.0f;
                }
            } else {
                /* Slide at ground level */
                objects[i].x += objects[i].vx;
            }
        } else {
            /* Big slam object: straight */
            objects[i].x += objects[i].vx;
        }
        /* Remove if off-screen left */
        if (objects[i].x < -200.0f) {
            objects[i].active = 0;
        }
    }

    if (sfxCardboardRoll) {
        if (anyCardboxActive) {
            if (!Mix_Playing(SFX_CH_CARDBOARD_ROLL))
                Mix_PlayChannel(SFX_CH_CARDBOARD_ROLL, sfxCardboardRoll, -1);
        } else if (Mix_Playing(SFX_CH_CARDBOARD_ROLL)) {
            Mix_HaltChannel(SFX_CH_CARDBOARD_ROLL);
        }
    }

    /* Collision: player vs small objects only */
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active || objects[i].big) continue;
        float ox = objects[i].x;
        float oy = objects[i].y;
        int   ow = 40;
        int   oh = 40;
        if (player.x + PLAYER_W > ox && player.x < ox + ow &&
            player.y + PLAYER_H > oy && player.y < oy + oh)
        {
            playerHit();
        }
        /* P2 hit */
        if (multiplayerMode && !marv.invincible &&
            marv.x + PLAYER_W > ox && marv.x < ox + ow &&
            marv.y + PLAYER_H > oy && marv.y < oy + oh)
        {
            marvHit();
        }
    }

    /* Collision: player vs slam object — PUSH, don't hit */
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active || !objects[i].big) continue;
        float ox = objects[i].x;
        float oy = objects[i].y;
        int ow = 350, oh = 500;

        /* Check if slam object has reached player — predictive: trigger 20px early */
        if (!slamPushingPlayer) {
            if (player.x + PLAYER_W > ox - 20.0f && player.x < ox + ow &&
                player.y + PLAYER_H > oy && player.y < oy + oh)
            {
                slamPushingPlayer = 1;
            }
        }
        /* Hard push: even without slamPushingPlayer, if player is inside fridge, eject left */
        if (!debugInvincible) {
            if (player.x + PLAYER_W > ox && player.x < ox + ow &&
                player.y + PLAYER_H > oy && player.y < oy + oh)
            {
                slamPushingPlayer = 1;
                player.x = ox - (float)PLAYER_W - 2.0f;
            }
        }
        /* Lock pushed characters to front face of slam object */
        if (slamPushingPlayer && !debugInvincible) {
            player.x = ox - (float)PLAYER_W - 2.0f;
            /* Don't force Y — let jump/fall animation continue naturally */
        }
        if (!slamPushingMarv) {
            if (marv.x + PLAYER_W > ox && marv.x < ox + ow &&
                marv.y + PLAYER_H > oy && marv.y < oy + oh)
            {
                slamPushingMarv = 1;
            }
        }
        if (slamPushingMarv) {
            marv.x = ox - (float)PLAYER_W - 2.0f;
            /* Don't force Y — let jump/fall animation continue naturally */
        }
    }

    /* Random sky hazards between obstacles — 2-5 second random interval */
    if (chasePhase == PHASE_OBJECTS && now - phaseStart >= 4000) {
        if (hazardNextSpawnAt == 0) {
            /* First time: set initial delay */
            hazardNextSpawnAt = now + 2000 + (Uint32)(rand() % 3000);
        }
        if (now >= hazardNextSpawnAt) {
            for (int sb = 0; sb < MAX_SKY_BOMBS; sb++) {
                if (!skyBombs[sb].active) {
                    skyBombs[sb].active = 1;
                    skyBombs[sb].x = player.x + (float)((rand() % 400) - 200);
                    skyBombs[sb].y = -40.0f;
                    skyBombs[sb].angle = 0.0f;
                    break;
                }
            }
            /* Pick next random spawn time: 2000-5000ms from now */
            hazardNextSpawnAt = now + 2000 + (Uint32)(rand() % 3000);
        }
    }

    /* Check for slam phase completion: both characters pushed off screen */
    if (slamSpawned && chasePhase == PHASE_SLAM) {
        if (slamPushingPlayer && player.x + PLAYER_W < 0.0f &&
            slamPushingMarv   && marv.x  + PLAYER_W < 0.0f)
        {
            if (!slamWaiting) {
                slamWaiting   = 1;
                slamWaitStart = now;
            }
            if (slamWaiting && now - slamWaitStart >= 2000) {
                /* Start re-entry phase: run back in, dialogue, then jump+platforms */
                slamWaiting           = 0;
                slamReEntryActive     = 1;
                slamReEntryDlgStarted = 0;
                memset(objects, 0, sizeof(objects));
                memset(skyBombs, 0, sizeof(skyBombs));
                slamPushingPlayer = 0;
                slamPushingMarv   = 0;
                memset(platforms, 0, sizeof(platforms));
                /* Place both off-screen left running right at ground level */
                player.x = -90.0f; player.y = (float)(GROUND_Y - PLAYER_H);
                player.vx = PLAYER_SPEED; player.vy = 0.0f; player.onGround = 1;
                player.invincible = 0; player.facingRight = 1;
                marv.x   = -180.0f; marv.y = (float)(GROUND_Y - PLAYER_H);
                marv.vx  = PLAYER_SPEED; marv.vy = 0.0f; marv.onGround = 1;
                marv.reEntryDone = 1;
                marvReEntryStarted = 0;
            }
        }
    }

    /* Fallback: if slam phase stuck >10s, trigger re-entry */
    if (chasePhase == PHASE_SLAM && !slamReEntryActive && now - phaseStart >= 10000) {
        slamWaiting           = 0;
        slamReEntryActive     = 1;
        slamReEntryDlgStarted = 0;
        memset(objects, 0, sizeof(objects));
        memset(skyBombs, 0, sizeof(skyBombs));
        slamPushingPlayer = 0;
        slamPushingMarv   = 0;
        memset(platforms, 0, sizeof(platforms));
        player.x = -90.0f; player.y = (float)(GROUND_Y - PLAYER_H);
        player.vx = PLAYER_SPEED; player.vy = 0.0f; player.onGround = 1;
        player.invincible = 0; player.facingRight = 1;
        marv.x   = -180.0f; marv.y = (float)(GROUND_Y - PLAYER_H);
        marv.vx  = PLAYER_SPEED; marv.vy = 0.0f; marv.onGround = 1;
        marv.reEntryDone = 1; marvReEntryStarted = 0;
    }

    /* Re-entry phase: run in, play rats dialogue, then jump + spawn platforms + rats */
    if (slamReEntryActive) {
        /* Advance marv toward his resting x */
        marv.x += marv.vx;
        if (marv.x > 120.0f) marv.vx = 0.0f;

        /* Start dialogue once player is on screen */
        if (!slamReEntryDlgStarted && player.x > 10.0f) {
            slamReEntryDlgStarted = 1;
            startDialogue(dlgRats, DLG_RATS_COUNT, 0);
        }

        /* When dialogue done → big jump + spawn platforms + rats */
        if (slamReEntryDlgStarted && !dlgActive) {
            slamReEntryActive = 0;
            /* Snap to ground then launch instantly */
            player.y      = (float)(GROUND_Y - PLAYER_H);
            player.vx     = 0.0f;
            player.vy     = -16.0f;   /* gentle arc — gravity reduced below */
            player.onGround = 0;
            launchJumpActive  = 1;
            /* Marv enters floating mode (AI) or gets launch jump (P2 multiplayer) */
            if (multiplayerMode) {
                marv.y       = (float)(GROUND_Y - PLAYER_H);
                marv.vx      = 0.0f;
                marv.vy      = -16.0f;
                marv.onGround = 0;
            } else {
                marv.vx = 0.0f; marv.vy = 0.0f;
            }
            /* Spawn initial platforms — some on-screen for player to land on after jump,
               rest off-screen right scrolling in */
            platLastSpawn = now;
            /* 2 platforms on-screen at reachable heights for landing */
            float onScreenYs[2] = {
                (float)(GROUND_Y - 220),
                (float)(GROUND_Y - 220),
            };
            float onScreenXs[2] = { 350.0f, 600.0f };
            for (int p = 0; p < 2; p++) {
                spawnPlatformNoTrap(onScreenYs[p], 480);
                for (int q = MAX_PLATFORMS - 1; q >= 0; q--) {
                    if (platforms[q].active && platforms[q].x == (float)SCREEN_W) {
                        platforms[q].x = onScreenXs[p];
                        break;
                    }
                }
            }
            /* 4 more platforms coming in from off-screen right */
            for (int p = 0; p < 4; p++) {
                float py = (float)(GROUND_Y - 220);
                spawnPlatformNoTrap(py, 480);
                for (int q = MAX_PLATFORMS - 1; q >= 0; q--) {
                    if (platforms[q].active && platforms[q].x == (float)SCREEN_W) {
                        platforms[q].x = (float)SCREEN_W + p * 220.0f;
                        break;
                    }
                }
            }
            /* Activate rats — spread across screen for immediate visual effect */
            ratsActive = 1;
            for (int ri = 0; ri < RAT_COUNT; ri++) {
                rats[ri].active    = 1;
                /* First half: already on the right side of the screen; second half: coming from off-screen right */
                if (ri < RAT_COUNT / 2) {
                    rats[ri].x = (float)(SCREEN_W / 2 + rand() % (SCREEN_W / 2));
                } else {
                    rats[ri].x = (float)(RAT_RESPAWN_X + rand() % (SCREEN_W / 3));
                }
                rats[ri].vx        = -(2.0f + (float)(rand() % 35) * 0.1f);
                rats[ri].animFrame = rand() % 36;
                rats[ri].animLastTick = now;
            }
            /* Transition to platform phase */
            chasePhase    = PHASE_PLATFORMS;
            phaseStart    = now;
            marvReEntryStarted  = 0;
            marv.reEntryDone    = 1;
        }
        return;
    }
}

static void updateChasePlatforms(Uint32 now)
{
    /* Scroll platforms left */
    float scrollSpd = 6.0f * bgScrollSpeed * slowdownSpeed;
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!platforms[i].active) continue;
        platforms[i].x -= scrollSpd;
        if (platforms[i].x < -200.0f) platforms[i].active = 0;

        /* Trap platform: fall when triggered */
        if (platforms[i].trap && platforms[i].trapTriggered) {
            platforms[i].vy += 2.0f;
            platforms[i].y  += platforms[i].vy;
            if (platforms[i].y > (float)(SCREEN_H + 60)) platforms[i].active = 0;
        }
    }

    if (chasePhase == PHASE_PLATFORMS) {
        if (now - phaseStart < 6000) {
            /* First 6s: straight connected highway at fixed height (placeholder for dialogue) */
            if (now - platLastSpawn >= 400) {
                spawnPlatformNoTrap((float)(GROUND_Y - 220), 480);
                platLastSpawn = now;
            }
        } else {
            /* After 6s: random platform spawning */
            if (now - platLastSpawn >= 700) {
                float py = (float)(GROUND_Y - 80 - rand() % 280);
                spawnPlatform(py, 160);
                platLastSpawn = now;
            }
        }
        if (launchJumpActive && now - phaseStart >= 3000) launchJumpActive = 0;
        /* After 10 seconds → PHASE_PATTERN_PLATFORMS */
        if (now - phaseStart >= 10000) {
            chasePhase    = PHASE_PATTERN_PLATFORMS;
            phaseStart    = now;
            patternStart  = now;
            patternCycle  = 0;
            patternPlatsThisCycle = 0;
            platLastSpawn = now;
        }
    } else if (chasePhase == PHASE_PATTERN_PLATFORMS) {
        /* Spawn platforms every 700ms at random heights with varying widths */
        if (now - platLastSpawn >= 700) {
            float py = (float)(GROUND_Y - 80 - rand() % 280);
            int pw = 140 + rand() % 80; /* width varies 140-220 */
            spawnPlatform(py, pw);
            platLastSpawn = now;
        }
        /* Start rat retreat 8 seconds before end of phase (at 52s mark) */
        if (!ratsRetreating && now - phaseStart >= 52000) {
            ratsRetreating   = 1;
            ratsRetreatStart = now;
            /* Reverse all rats: negative vx so they flee left */
            for (int ri = 0; ri < RAT_COUNT; ri++) {
                rats[ri].vx = -(3.0f + (float)(rand() % 30) * 0.1f);
            }
        }
        /* After 60s → PHASE_RETURN_GROUND */
        if (now - phaseStart >= 60000) {
            chasePhase = PHASE_RETURN_GROUND;
            phaseStart = now;
        }
    } else if (chasePhase == PHASE_RETURN_GROUND) {
        /* Don't spawn new platforms, wait then go to microwave phase */
        if (now - phaseStart >= 4000) {
            /* PHASE_MICROWAVE takes over character motion, so force both
               characters back onto the ground before the handoff. */
            player.y = (float)(GROUND_Y - PLAYER_H);
            player.vy = 0.0f;
            player.onGround = 1;
            if (multiplayerMode) {
                marv.y = (float)(GROUND_Y - PLAYER_H);
                marv.vy = 0.0f;
                marv.onGround = 1;
            }
            chasePhase  = PHASE_MICROWAVE;
            phaseStart  = now;
            mwRound     = 0;
            mwActive    = 0;
            mwWaiting   = 0;
            mwBothOut   = 0;
            mwReentering= 0;
            dlgAtTop    = 1;
            startDialogue(dlgMwIntro, DLG_MW_INTRO_COUNT, 0);
        }
    } else if (chasePhase == PHASE_MICROWAVE) {
        /* ── Auto-advance: 0.5s after last char, no input needed ── */
        if (dlgActive && dlgFullyShown && mwAutoAdvanceAt == 0)
            mwAutoAdvanceAt = now + 150;

        if (mwAutoAdvanceAt != 0 && now >= mwAutoAdvanceAt) {
            mwAutoAdvanceAt = 0;
            /* Check if this is the last line of the current sequence */
            int isLastLine = (dlgLineIdx >= dlgCurCount - 1);
            advanceDlgLine();          /* closes dlgActive if last line */
            if (isLastLine) {
                /* Last line done → fire the microwave immediately */
                mwX      = (float)(SCREEN_W + 20);
                mwActive = 1;
                player.vx = 0.0f;
                marv.vx   = 0.0f;
            }
        }

        /* ── Microwave in flight ── */
        if (mwActive) {
            int mwImpact = 0;
            mwX -= MW_SPEED;

            /* Hit player */
            if (!player.invincible &&
                mwX < player.x + PLAYER_W && mwX + MW_W > player.x) {
                player.vx         = -50.0f;
                player.invincible = 1;
                player.invincStart= now;
                mwImpact = 1;
            }
            /* Hit marv */
            if (marv.vx == 0.0f &&
                mwX < marv.x + PLAYER_W && mwX + MW_W > marv.x) {
                marv.vx = -50.0f;
                mwImpact = 1;
            }
            if (mwImpact) playSfxChunk(sfxSlamImpact);

            /* Slide both off screen — no deceleration */
            if (player.vx < 0.0f) player.x += player.vx;
            if (marv.vx   < 0.0f) marv.x   += marv.vx;

            /* When microwave exits left → snap both off and start wait */
            if (mwX + MW_W < -10.0f) {
                player.x    = -200.0f;
                marv.x      = -200.0f;
                player.vx   = 0.0f;
                marv.vx     = 0.0f;
                mwActive    = 0;
                mwBothOut   = 1;
                mwWaitStart = now;
            }
        }

        /* ── 2s pause then re-enter ── */
        if (mwBothOut && now - mwWaitStart >= 2000) {
            mwBothOut      = 0;
            mwReentering   = 1;
            player.x = -100.0f; player.y = (float)(GROUND_Y - PLAYER_H);
            player.vx = 10.0f;  player.vy = 0.0f;
            player.onGround = 1; player.invincible = 0; player.facingRight = 1;
            marv.x   = -180.0f; marv.y = (float)(GROUND_Y - PLAYER_H);
            marv.vx  = 10.0f;   marv.vy = 0.0f; marv.onGround = 1;
        }

        /* ── Players running back in ── */
        if (mwReentering) {
            player.x += player.vx;
            marv.x   += marv.vx;
            if (player.x >= 200.0f) { player.x = 200.0f; player.vx = 0.0f; }
            if (marv.x   >= 120.0f) { marv.x   = 120.0f; marv.vx  = 0.0f; }

            /* Both settled → show next round dialogue */
            if (player.vx == 0.0f && marv.vx == 0.0f) {
                mwReentering = 0;
                mwRound++;
                if (mwRound >= MW_SLAM_COUNT) {
                    /* All done — wait 2s then continue */
                    mwWaiting = 1; mwWaitStart = now;
                } else {
                    const DlgLine *lines = NULL; int count = 1;
                    switch (mwRound) {
                        case 1: lines = dlgMwHit1; count = DLG_MW_HIT1_COUNT; break;
                        case 2: lines = dlgMwHit2; count = 1; break;
                        case 3: lines = dlgMwHit3; count = 1; break;
                        case 4: lines = dlgMwHit4; count = 1; break;
                        case 5: lines = dlgMwHit5; count = 1; break;
                        case 6: lines = dlgMwHit6; count = 1; break;
                        default: break;
                    }
                    if (lines) { startDialogue(lines, count, 0); mwAutoAdvanceAt = 0; }
                }
            }
        }

        /* ── Final 2s wait then continue game ── */
        if (mwWaiting && now - mwWaitStart >= 2000) {
            mwWaiting  = 0;
            dlgAtTop   = 0;
            chasePhase = PHASE_KEVIN_ESCAPE;
            phaseStart = now;
            kevinX     = 900.0f; kevinVisible = 1;
            startDialogue(dlgKevinEscape, DLG_KEVIN_ESCAPE_COUNT, 0);
        }

        return;
    } else if (chasePhase == PHASE_KEVIN_ESCAPE) {
        /* Kevin runs away */
        kevinX += 8.0f;
        if (kevinX > 1300.0f) kevinVisible = 0;

        /* When dialogue done → PHASE_SLOWDOWN */
        if (!dlgActive && !kevinEscapeDlgDone) {
            kevinEscapeDlgDone = 1;
            chasePhase = PHASE_SLOWDOWN;
            phaseStart = now;
        }
    } else if (chasePhase == PHASE_SLOWDOWN) {
        /* Lerp scroll speed and anim to 0 over 3s */
        float t = (float)(now - phaseStart) / 3000.0f;
        if (t > 1.0f) t = 1.0f;
        slowdownSpeed = 1.0f - t;
        if (t >= 1.0f) {
            /* Transition straight to the level summary. */
            prepareLevel2Result(1);
            transAlpha  = 0.0f;
            transDir    = 1;
            transStart  = now;
            transTargetState = GS_END_FADE;
            gameState   = GS_MINI_TRANSITION_IN;
        }
    }
}


/* ═══════════════════════════════════════════════════════════
   UPDATE RATS
═══════════════════════════════════════════════════════════ */
static void updateRats(Uint32 now)
{
    if (!ratsActive) {
        ratSqueakNextAt = 0;
        Mix_HaltChannel(SFX_CH_RAT_SQUEAK);
        return;
    }
    int anyActive = 0;
    for (int ri = 0; ri < RAT_COUNT; ri++) {
        if (!rats[ri].active) continue;
        rats[ri].x += rats[ri].vx;
        if (ratsRetreating) {
            /* Deactivate once fully off the left edge */
            if (rats[ri].x < -60.0f) {
                rats[ri].active = 0;
                continue;
            }
        } else {
            /* Normal loop: once a rat exits left, bring it back from the right */
            if (rats[ri].x < -60.0f) {
                rats[ri].x  = (float)(RAT_RESPAWN_X + rand() % 80);
                rats[ri].vx = -(1.5f + (float)(rand() % 30) * 0.1f);
            }
        }
        /* Animate at ~10 fps */
        if (now - rats[ri].animLastTick >= 100) {
            rats[ri].animFrame = (rats[ri].animFrame + 1) % 36;
            rats[ri].animLastTick = now;
        }
        anyActive = 1;
    }
    if (anyActive && sfxRatSqueak) {
        if (ratSqueakNextAt == 0) ratSqueakNextAt = now + (Uint32)(rand() % 180);
        if (now >= ratSqueakNextAt) {
            if (!Mix_Playing(SFX_CH_RAT_SQUEAK))
                Mix_PlayChannel(SFX_CH_RAT_SQUEAK, sfxRatSqueak, 0);
            ratSqueakNextAt = now + 160 + (Uint32)(rand() % 80);
        }
    } else {
        ratSqueakNextAt = 0;
        Mix_HaltChannel(SFX_CH_RAT_SQUEAK);
    }
    /* Once all rats have fled, deactivate the system */
    if (ratsRetreating && !anyActive) {
        ratsActive     = 0;
        ratsRetreating = 0;
        ratSqueakNextAt = 0;
        Mix_HaltChannel(SFX_CH_RAT_SQUEAK);
    }
}

static void updateMarvP2(Uint32 now)
{
    /* Scripted: Re-entry slide after slam — same as AI */
    if (marvReEntryStarted && !marv.reEntryDone) {
        marv.vx += 0.15f;
        if (marv.vx > 8.0f) marv.vx = 8.0f;
        marv.x += marv.vx;
        if (marv.x >= 120.0f) {
            marv.x = 120.0f;
            marv.reEntryDone = 1;
            marv.vx = 0.0f;
        }
        goto p2_anim_tick;
    }

    /* Slam re-entry: scripted movement handled entirely by updateChaseObjects — skip here */
    if (slamReEntryActive) {
        goto p2_anim_tick;
    }

    /* Platform phases: P2 plays normally — jump on platforms, fall = lose life */
    if (chasePhase == PHASE_PLATFORMS || chasePhase == PHASE_PATTERN_PLATFORMS) {
        /* Horizontal input */
        if (key2Left)        marv.vx = -PLAYER_SPEED;
        else if (key2Right)  marv.vx =  PLAYER_SPEED;
        else                 marv.vx =  0.0f;
        /* Jump */
        if (key2Up && marv.onGround) {
            marv.vy      = JUMP_VY;
            playJumpSfx();
            marv.onGround = 0;
        }
        /* Gravity */
        if (!marv.onGround) {
            applyGravity(&marv.vy);
            marv.y += marv.vy;
        }
        /* Platform landing */
        marv.onGround = 0;
        if (!landOnPlatforms(&marv.x, &marv.y, &marv.vy,
                             &marv.onGround, (float)PLAYER_W, (float)PLAYER_H, 0))
        {
            if (marv.y + PLAYER_H >= (float)GROUND_Y) {
                /* Fell to ground — lose a life, respawn to best platform */
                if (now - phaseStart >= 3000) {
                    marvHit();
                }
                float bestY = (float)(GROUND_Y - 200);
                for (int pi = 0; pi < MAX_PLATFORMS; pi++) {
                    if (platforms[pi].active && platforms[pi].x > 50.0f && platforms[pi].x < SCREEN_W - 50) {
                        if (platforms[pi].y < bestY || bestY == (float)(GROUND_Y - 200))
                            bestY = platforms[pi].y;
                    }
                }
                marv.x  = 300.0f;
                marv.y  = bestY - (float)PLAYER_H - 2.0f;
                marv.vy = -8.0f;
                marv.onGround = 0;
            }
        }
        /* Apply horizontal movement + clamp */
        marv.x += marv.vx;
        if (marv.x < 0.0f)                        marv.x = 0.0f;
        if (marv.x + PLAYER_W > (float)SCREEN_W)  marv.x = (float)(SCREEN_W - PLAYER_W);
        /* Fall off bottom */
        if (marv.y > (float)(SCREEN_H + 50)) {
            marvHit();
            marv.x = 300.0f;
            marv.y = (float)(GROUND_Y - PLAYER_H);
            marv.vx = 0.0f; marv.vy = 0.0f; marv.onGround = 1;
        }
        goto p2_anim_tick;
    }

    /* P2 horizontal input */
    if (key2Left)        marv.vx = -PLAYER_SPEED;
    else if (key2Right)  marv.vx =  PLAYER_SPEED;
    else                 marv.vx =  0.0f;

    /* P2 jump */
    if (key2Up && marv.onGround) {
        marv.vy      = JUMP_VY;
        playJumpSfx();
        marv.onGround = 0;
    }

    /* Apply gravity */
    if (!marv.onGround) {
        applyGravity(&marv.vy);
        marv.y += marv.vy;
        if (marv.y + PLAYER_H >= (float)GROUND_Y) {
            marv.y      = (float)(GROUND_Y - PLAYER_H);
            marv.vy     = 0.0f;
            marv.onGround = 1;
        }
    }

    /* Apply horizontal movement */
    marv.x += marv.vx;
    if (marv.x < 0.0f)                        marv.x = 0.0f;
    if (marv.x + PLAYER_W > (float)SCREEN_W)  marv.x = (float)(SCREEN_W - PLAYER_W);

p2_anim_tick:;
    Uint32 marvAnimDelay = (Uint32)(1000 / (marv.animFps > 0 ? marv.animFps : 1));
    if (now - marv.animLastTick > marvAnimDelay) {
        marv.animFrame = (marv.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
        marv.animLastTick = now;
    }
}

static void updateMarvAI(Uint32 now)
{
    /* Re-entry slide after slam */
    if (marvReEntryStarted && !marv.reEntryDone) {
        marv.vx += 0.15f;
        if (marv.vx > 8.0f) marv.vx = 8.0f;
        marv.x += marv.vx;
        if (marv.x >= 120.0f) {
            marv.x = 120.0f;
            marv.reEntryDone = 1;
            marv.vx = 0.0f;
        }
        return;
    }

    /* Check Marv is still on something — if platform scrolled away, start falling */
    if (marv.onGround) {
        int stillOn = 0;
        /* Check ground (only valid outside platform phases) */
        if (chasePhase != PHASE_PLATFORMS && chasePhase != PHASE_PATTERN_PLATFORMS) {
            if (marv.y + PLAYER_H >= (float)GROUND_Y - 2.0f) stillOn = 1;
        }
        /* Check platforms */
        for (int i = 0; i < MAX_PLATFORMS; i++) {
            if (!platforms[i].active) continue;
            if (marv.x + PLAYER_W > platforms[i].x &&
                marv.x            < platforms[i].x + platforms[i].w &&
                fabsf(marv.y + PLAYER_H - platforms[i].y) < 8.0f)
            {
                stillOn = 1;
                break;
            }
        }
        if (!stillOn) marv.onGround = 0;
    }

    /* Jump over objects */
    if (chasePhase == PHASE_OBJECTS || chasePhase == PHASE_SLAM) {
        if (marv.onGround) {
            for (int i = 0; i < MAX_OBJECTS; i++) {
                if (!objects[i].active) continue;
                float dist = objects[i].x - marv.x;
                if (dist > 0.0f && dist < 160.0f) {
                    marv.vy = JUMP_VY;
                    playJumpSfx();
                    marv.onGround = 0;
                    break;
                }
            }
        }
    }

    /* During platform phases: Marv floats at y=50 (menacing hover, no jumping) */
    if (chasePhase == PHASE_PLATFORMS || chasePhase == PHASE_PATTERN_PLATFORMS) {
        marv.y      = 50.0f;
        marv.vy     = 0.0f;
        marv.onGround = 0;
        return;
    }

    /* Apply gravity (for PHASE_RETURN_GROUND onwards — Marv falls from float position) */
    if (!marv.onGround) {
        applyGravity(&marv.vy);
        marv.y += marv.vy;
        if (marv.y + PLAYER_H >= (float)GROUND_Y) {
            marv.y = (float)(GROUND_Y - PLAYER_H);
            marv.vy = 0.0f;
            marv.onGround = 1;
        }
    }

    /* Marv anim */
    Uint32 marvAnimDelay = (Uint32)(1000 / (marv.animFps > 0 ? marv.animFps : 1));
    if (now - marv.animLastTick > marvAnimDelay) {
        marv.animFrame = (marv.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
        marv.animLastTick = now;
    }
    (void)now;
}

/* ═══════════════════════════════════════════════════════════
   UPDATE END ROOM
═══════════════════════════════════════════════════════════ */
static void updateEndRoom(Uint32 now)
{
    /* Player physics */
    if (!player.keepGoingShow) {
        if (keyLeft)  { player.vx = -PLAYER_SPEED; player.facingRight = 0; }
        else if (keyRight) { player.vx = PLAYER_SPEED; player.facingRight = 1; }
        else           player.vx = 0.0f;
        if (keyUp && player.onGround) {
            player.vy = JUMP_VY;
            playJumpSfx();
            player.onGround = 0;
        }
    } else {
        player.vx = PLAYER_SPEED;
        /* Still allow jump during respawn run */
        if (keyUp && player.onGround) {
            player.vy = JUMP_VY;
            playJumpSfx();
            player.onGround = 0;
        }
        if (now - player.keepGoingStart >= KEEP_GOING_MS)
            player.keepGoingShow = 0;
    }

    player.x += player.vx;
    if (player.x < 20.0f) player.x = 20.0f;

    if (!player.onGround) {
        applyGravity(&player.vy);
        player.y += player.vy;

        player.onGround = 0;
        if (!landOnStaticPlats(&player.x, &player.y, &player.vy, &player.onGround,
                               (float)PLAYER_W, (float)PLAYER_H))
        {
            if (player.y + PLAYER_H >= (float)GROUND_Y) {
                player.y = (float)(GROUND_Y - PLAYER_H);
                player.vy = 0.0f;
                player.onGround = 1;
            }
        }
    } else {
        player.vy = 0.0f;
        /* Check still on something */
        int stillOn = 0;
        if (player.y + PLAYER_H >= (float)GROUND_Y - 2.0f) stillOn = 1;
        for (int i = 0; i < 3; i++) {
            StaticPlat *p = &endPlats[i];
            if (player.x + PLAYER_W > (float)p->x && player.x < (float)(p->x + p->w) &&
                (int)(player.y + PLAYER_H) == p->y) stillOn = 1;
        }
        if (!stillOn) { player.onGround = 0; }
    }

    /* Invincible timer */
    if (player.invincible && now - player.invincStart >= 1000)
        player.invincible = 0;

    /* Marv slide in end fade */
    if (endFadeActive) {
        marv.x += marvEndVx;
        marvEndVx += 0.2f;
        if (marvEndAlpha > 0.0f) {
            marvEndAlpha -= 2.0f;
            if (marvEndAlpha < 0.0f) marvEndAlpha = 0.0f;
        }
        endFadeAlpha += 1.5f;
        if (endFadeAlpha >= 255.0f) {
            endFadeAlpha = 255.0f;
            endFadeActive = 0;
            prepareLevel2Result(1);
            gameState = GS_END_FADE;
        }
    }

    /* Door interactions */
    if (keyEPressed && !endFadeActive) {
        /* DOOR_TOP → mini level */
        {
            float cx = (float)DOOR_TOP_X + DOOR_TOP_W / 2.0f;
            float cy = (float)DOOR_TOP_Y + DOOR_TOP_H / 2.0f;
            float dx = player.x + PLAYER_W / 2.0f - cx;
            float dy = player.y + PLAYER_H / 2.0f - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 80.0f) {
                if (topDoorUsed) {
                    topDoorClosed = 1;
                    topDoorClosedTimer = now;
                } else {
                    playSfxChunk(sfxDoorOpen);
                    topDoorUsed = 1;
                    transAlpha = 0.0f;
                    transDir = 1;
                    transStart = now;
                    transTargetState = GS_MINI_LEVEL;
                    gameState = GS_MINI_TRANSITION_IN;
                    player.x = 100.0f;
                    player.y = (float)(GROUND_Y - PLAYER_H);
                    player.vx = 0.0f;
                    player.vy = 0.0f;
                    player.onGround = 1;
                }
            }
        }

        /* DOOR_BOTTOM → need key, or end game */
        {
            float cx = (float)DOOR_BOTTOM_X + DOOR_BOTTOM_W / 2.0f;
            float cy = (float)DOOR_BOTTOM_Y + DOOR_BOTTOM_H / 2.0f;
            float dx = player.x + PLAYER_W / 2.0f - cx;
            float dy = player.y + PLAYER_H / 2.0f - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 80.0f) {
                if (!player.hasKey) {
                    needKeyShow = 1;
                    needKeyTimer = now;
                } else {
                    endFadeActive = 1;
                    marvEndVx = 5.0f;
                }
            }
        }
    }

    /* Player anim */
    if (now - player.animLastTick > (Uint32)(1000 / ANIM_FPS)) {
        player.animFrame = (player.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
        player.animLastTick = now;
    }
}

/* ═══════════════════════════════════════════════════════════
   UPDATE MINI LEVEL
═══════════════════════════════════════════════════════════ */
/* Mini level platform defaults are declared in level2_types.h. */

static int landOnMiniPlats(float *px, float *py, float *pvy, int *onGround, float pw, float ph, Uint32 now)
{
    for (int i = 0; i < 7; i++) {
        float py_plat;
        if      (i == 0) py_plat = miniPlat0Y;
        else if (i == 4) py_plat = miniPlat4Y;
        else if (i == 5) py_plat = miniPlat5Y;
        else             py_plat = miniPlats[i].y;
        float px_plat = miniPlats[i].x;
        int   pw_plat = miniPlats[i].w;

        /* Skip fallen platforms */
        if (i == 0 && miniPlat0Touched == 1 && miniPlat0Y > (float)GROUND_Y + 10.0f) continue;
        if (i == 4 && miniPlat4Touched == 1 && miniPlat4Y > (float)GROUND_Y + 10.0f) continue;
        if (i == 5 && miniPlat5Touched == 1 && miniPlat5Y > (float)GROUND_Y + 10.0f) continue;

        int downward = (!miniGravFlipped) ? (*pvy >= 0.0f) : (*pvy <= 0.0f);
        float impactSpeed = fabsf(*pvy);
        float feet   = !miniGravFlipped ? (*py + ph) : *py;
        float platTop= !miniGravFlipped ? py_plat    : (py_plat + miniPlats[i].h);

        if (downward
            && *px + pw > px_plat
            && *px      < px_plat + pw_plat
            && fabsf(feet - platTop) <= 12.0f)
        {
            if (!miniGravFlipped) {
                *py = py_plat - ph;
                if (*pvy > 0.0f) *pvy = 0.0f;
            } else {
                *py = py_plat + miniPlats[i].h;
                if (*pvy < 0.0f) *pvy = 0.0f;
            }
            *onGround = 1;
            if (impactSpeed > 2.0f) playSfxChunk(sfxLand);

            /* Trap: platform 0 falls after 0.3s contact */
            if (i == 0 && miniPlat0Touched == 0) {
                miniPlat0Touched  = 1;
                miniPlat0FallStart= now;
            }
            /* Gravity flip: platform 1 orange pad */
            if (i == 1 && !miniGravFlipped) {
                float padX = miniPlats[1].x + 10.0f;
                if (*px + pw > padX && *px < padX + 30.0f) {
                    miniGravFlipped   = 1;
                    miniGravFlipStart = now;
                    miniShowWTF       = 1;
                    miniWTFStart      = now;
                    playSfxChunk(sfxGravityFlip);
                    *pvy = JUMP_VY; /* launch */
                    playJumpSfx();
                    *onGround = 0;
                }
            }
            /* Bouncy: platform 2 first touch */
            if (i == 2 && !miniPlat2Bounced) {
                miniPlat2Bounced = 1;
                *pvy      = -22.0f;
                *onGround = 0;
                miniShowBonk = 1;
                miniBonkStart = now;
                /* Key spawns after bounce and relanding */
            }
            /* After plat2 bounce, if bounced and landing again: spawn key */
            if (i == 2 && miniPlat2Bounced && !miniKeySpawned) {
                miniKeySpawned = 1;
                miniKeyX = miniPlats[2].x + 20.0f;
                miniKeyY = miniPlats[2].y - 20.0f;
            }
            /* Trap: platform 4 falls after 0.2s contact */
            if (i == 4 && miniPlat4Touched == 0) {
                miniPlat4Touched  = 1;
                miniPlat4FallStart= now;
            }
            /* Trap: platform 5 falls after 0.15s contact (faster) */
            if (i == 5 && miniPlat5Touched == 0) {
                miniPlat5Touched  = 1;
                miniPlat5FallStart= now;
            }
            /* Fake exit 2: platform 6 */
            if (i == 6 && !miniFakeExit2Used) {
                /* nothing on land — interaction is via E key in updateMiniLevel */
            }
            return i;
        }
    }
    return -1;
}

static void updateMiniLevel(Uint32 now)
{
    float gravDir = miniGravFlipped ? -1.0f : 1.0f;

    /* Restore gravity after 1.5s */
    if (miniGravFlipped && now - miniGravFlipStart >= 1500) {
        miniGravFlipped = 0;
    }

    /* Platform 0 fall */
    if (miniPlat0Touched == 1) {
        if (now - miniPlat0FallStart >= 100) {
            miniPlat0Y += 8.0f; /* fall faster */
        }
    }

    /* Platform 4 fall */
    if (miniPlat4Touched == 1) {
        if (now - miniPlat4FallStart >= 200)
            miniPlat4Y += 7.0f;
    }
    /* Platform 5 fall */
    if (miniPlat5Touched == 1) {
        if (now - miniPlat5FallStart >= 150)
            miniPlat5Y += 8.0f;
    }

    /* Player movement */
    if (!player.keepGoingShow) {
        if (keyLeft)  { player.vx = -PLAYER_SPEED; player.facingRight = 0; }
        else if (keyRight) { player.vx = PLAYER_SPEED; player.facingRight = 1; }
        else           player.vx = 0.0f;
        if (keyUp && player.onGround) {
            player.vy = JUMP_VY * gravDir;
            playJumpSfx();
            player.onGround = 0;
        }
    } else {
        player.vx = PLAYER_SPEED;
        /* Still allow jump during respawn run */
        if (keyUp && player.onGround) {
            player.vy = JUMP_VY;
            playJumpSfx();
            player.onGround = 0;
        }
        if (now - player.keepGoingStart >= KEEP_GOING_MS)
            player.keepGoingShow = 0;
    }

    player.x += player.vx;
    /* Screen bounds */
    if (player.x < 0.0f) player.x = 0.0f;
    if (player.x + PLAYER_W > (float)SCREEN_W) player.x = (float)(SCREEN_W - PLAYER_W);

    /* Spike zone death */
    if (!player.invincible) {
        for (int s = 0; s < MINI_NUM_SPIKES; s++) {
            if (player.x + PLAYER_W > (float)miniSpikeX[s]
                && player.x < (float)(miniSpikeX[s] + miniSpikeW[s])
                && player.y + PLAYER_H >= (float)(GROUND_Y - 28)) {
                playerHit();
                player.x = 100.0f;
                player.y = (float)(GROUND_Y - PLAYER_H);
                player.vx = 0.0f;
                player.vy = 0.0f;
                player.onGround = 1;
                if (gameState == GS_GAME_OVER) return;
                break;
            }
        }
    }

    /* Gravity */
    player.vy += GRAVITY * gravDir;
    if (!miniGravFlipped) {
        if (player.vy >  MAX_FALL) player.vy =  MAX_FALL;
    } else {
        if (player.vy < -MAX_FALL) player.vy = -MAX_FALL;
    }
    player.y += player.vy;

    player.onGround = 0;

    /* Ground or ceiling depending on gravity */
    if (!miniGravFlipped) {
        if (player.y + PLAYER_H >= (float)GROUND_Y) {
            player.y = (float)(GROUND_Y - PLAYER_H);
            player.vy = 0.0f;
            player.onGround = 1;
        }
        /* Ceiling kill */
        if (player.y < 10.0f && player.vy < 0.0f) {
            player.vy = fabsf(player.vy) * 0.8f;
            player.y = 10.0f;
        }
    } else {
        /* Flipped: ceiling is ground */
        if (player.y <= 0.0f) {
            player.y = 0.0f;
            player.vy = 0.0f;
            player.onGround = 1;
        }
        if (player.y + PLAYER_H > (float)GROUND_Y && player.vy > 0.0f) {
            player.vy = -fabsf(player.vy) * 0.8f;
        }
    }

    /* Platform landing */
    landOnMiniPlats(&player.x, &player.y, &player.vy, &player.onGround, (float)PLAYER_W, (float)PLAYER_H, now);

    /* Key collection */
    if (miniKeySpawned && !miniKeyCollected) {
        float dx = player.x + PLAYER_W / 2.0f - (miniKeyX + 10.0f);
        float dy = player.y + PLAYER_H / 2.0f - (miniKeyY + 10.0f);
        if (fabsf(dx) < 30.0f && fabsf(dy) < 30.0f) {
            miniKeyCollected = 1;
            player.hasKey    = 1;
            miniTrapDoorOpen = 1;
            playSfxChunk(sfxKeyPickup);
            playSfxChunk(sfxDoorOpen);
        }
    }

    /* Fake exit interaction */
    if (keyEPressed && !miniFakeExitUsed) {
        float cx = miniPlats[3].x + miniPlats[3].w / 2.0f;
        float cy = miniPlats[3].y;
        float dx = player.x + PLAYER_W / 2.0f - cx;
        float dy = player.y + PLAYER_H / 2.0f - cy;
        if (fabsf(dx) < 60.0f && fabsf(dy) < 60.0f) {
            miniFakeExitUsed = 1;
            miniShowPsych    = 1;
            miniPsychStart   = now;
            playSfxChunk(sfxFakeExit);
        }
    }

    /* Fake exit 2: platform 6 */
    if (keyEPressed && !miniFakeExit2Used) {
        float cx = miniPlats[6].x + miniPlats[6].w / 2.0f;
        float cy = miniPlats[6].y;
        float dx = player.x + PLAYER_W / 2.0f - cx;
        float dy = player.y + PLAYER_H / 2.0f - cy;
        if (fabsf(dx) < 60.0f && fabsf(dy) < 60.0f) {
            miniFakeExit2Used = 1;
            miniShowPsych2    = 1;
            miniPsych2Start   = now;
            playSfxChunk(sfxFakeExit);
        }
    }

    /* Fake key on platform 3 — collected on proximity, no E press needed */
    if (!miniFakeKey3Taken) {
        float fkx = miniPlats[3].x + 20.0f;
        float fky = miniPlats[3].y - 22.0f;
        float dx  = player.x + PLAYER_W / 2.0f - (fkx + 10.0f);
        float dy  = player.y + PLAYER_H / 2.0f - (fky + 10.0f);
        if (fabsf(dx) < 28.0f && fabsf(dy) < 28.0f) {
            miniFakeKey3Taken = 1;
            miniShowGotcha    = 1;
            miniGotchaStart   = now;
            playSfxChunk(sfxFakeExit);
        }
    }

    /* Exit trapdoor */
    if (miniTrapDoorOpen) {
        if (player.x + PLAYER_W > miniTrapDoorX
            && player.x < miniTrapDoorX + miniTrapDoorW
            && player.y + PLAYER_H >= (float)GROUND_Y - 5.0f)
        {
            /* Trigger transition out straight to the summary. */
            prepareLevel2Result(1);
            shakeActive = 1;
            shakeStart  = now;
            transAlpha  = 0.0f;
            transDir    = 1;
            transStart  = now;
            transTargetState = GS_END_FADE;
            gameState   = GS_MINI_TRANSITION_OUT;
        }
    }

    /* Fall off screen */
    if (player.y > (float)(SCREEN_H + 50)) {
        playerHit();
        player.x = 100.0f;
        player.y = (float)(GROUND_Y - PLAYER_H);
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.onGround = 1;
        if (gameState == GS_GAME_OVER) return;
    }

    /* Player anim */
    if (now - player.animLastTick > (Uint32)(1000 / ANIM_FPS)) {
        player.animFrame = (player.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
        player.animLastTick = now;
    }

    /* Invincible timer */
    if (player.invincible && now - player.invincStart >= 1000)
        player.invincible = 0;

    keyEPressed = 0; /* reset edge detect after all interaction checks */
}

/* ═══════════════════════════════════════════════════════════
   UPDATE TRANSITION
═══════════════════════════════════════════════════════════ */
static void updateTransition(Uint32 now)
{
    if (transDir == 1) {
        /* Fade to black */
        float t = (float)(now - transStart) / (float)TRANS_FADE_MS;
        transAlpha = t * 255.0f;
        if (transAlpha >= 255.0f) {
            transAlpha = 255.0f;
            /* Switch state and start fade out */
            gameState  = transTargetState;
            transDir   = -1;
            transStart = now;
            /* If going to mini level, reset player pos */
            if (transTargetState == GS_MINI_LEVEL) {
                player.x = 100.0f;
                player.y = (float)(GROUND_Y - PLAYER_H);
                player.vx = 0.0f;
                player.vy = 0.0f;
                player.onGround = 1;
                /* Reset mini state */
                miniPlat0Touched  = 0;
                miniPlat0FallStart= 0;
                miniPlat0Y        = 555.0f;
                miniGravFlipped   = 0;
                miniGravFlipStart = 0;
                miniPlat2Bounced  = 0;
                miniFakeExitUsed  = 0;
                miniKeySpawned    = 0;
                miniKeyCollected  = 0;
                miniTrapDoorOpen  = 0;
                miniShowWTF = miniShowBonk = miniShowPsych = 0;
                miniFakeKey3Taken = 0;
                miniShowGotcha    = 0;
                miniGotchaStart   = 0;
                miniPlat4Touched  = 0;
                miniPlat4FallStart= 0;
                miniPlat4Y        = 460.0f;
                miniPlat5Touched  = 0;
                miniPlat5FallStart= 0;
                miniPlat5Y        = 340.0f;
                miniFakeExit2Used = 0;
                miniShowPsych2    = 0;
                miniPsych2Start   = 0;
            }
        }
    } else if (transDir == -1) {
        /* Fade from black */
        float t = (float)(now - transStart) / (float)TRANS_FADE_MS;
        transAlpha = 255.0f - t * 255.0f;
        if (transAlpha <= 0.0f) {
            transAlpha = 0.0f;
            transDir   = 0;
        }
    }

    /* Shake */
    if (shakeActive) {
        if (now - shakeStart < SHAKE_DURATION_MS) {
            int shakeAmp = 3;
            shakeOX = (rand() % (shakeAmp * 2 + 1)) - shakeAmp;
            shakeOY = (rand() % (shakeAmp * 2 + 1)) - shakeAmp;
        } else {
            shakeActive = 0;
            shakeOX = 0;
            shakeOY = 0;
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   MAIN UPDATE
═══════════════════════════════════════════════════════════ */
static void update(Uint32 now)
{
    if (player.keepGoingShow && now - player.keepGoingStart >= KEEP_GOING_MS)
        player.keepGoingShow = 0;
    if (multiplayerMode && marv.keepGoingShow && now - marv.keepGoingStart >= KEEP_GOING_MS)
        marv.keepGoingShow = 0;

    switch (gameState) {

    case GS_KEY_SELECT:
        break;

    case GS_INTRO_FADE_IN: {
        introWhite = 255.0f;
        if (now - introTimer >= 300) {
            gameState  = GS_INTRO_TITLE;
            introTimer = now;
            introTitleAlpha = 0.0f;
        }
        break;
    }
    case GS_INTRO_TITLE: {
        /* Sequence:
         *  0..INTRO_TITLE_IN_MS       : title fades in, white stays 255
         *  ..+INTRO_TITLE_HOLD_MS     : title fully visible, white stays 255
         *  ..+800ms                   : white fades 255→0 to reveal level
         *  done → GS_INTRO_FADE_OUT   */
        Uint32 elapsed = now - introTimer;
        Uint32 fadeWhiteMs = 800;
        if (elapsed < (Uint32)INTRO_TITLE_IN_MS) {
            introTitleAlpha = (float)elapsed / INTRO_TITLE_IN_MS * 255.0f;
            introWhite      = 255.0f;
        } else if (elapsed < (Uint32)(INTRO_TITLE_IN_MS + INTRO_TITLE_HOLD_MS)) {
            introTitleAlpha = 255.0f;
            introWhite      = 255.0f;
        } else if (elapsed < (Uint32)(INTRO_TITLE_IN_MS + INTRO_TITLE_HOLD_MS + fadeWhiteMs)) {
            float t = (float)(elapsed - INTRO_TITLE_IN_MS - INTRO_TITLE_HOLD_MS) / (float)fadeWhiteMs;
            introTitleAlpha = 255.0f;
            introWhite      = 255.0f - t * 255.0f;
        } else {
            introTitleAlpha = 255.0f;
            introWhite      = 0.0f;
            gameState  = GS_INTRO_FADE_OUT;
            introTimer = now;
        }
        break;
    }
    case GS_INTRO_FADE_OUT: {
        introWhite = 0.0f;
        gameState  = GS_DIALOGUE;
        startDialogue(dlgIntro, DLG_INTRO_COUNT, 1);
        break;
    }
    case GS_DIALOGUE: {
        updateDialogue(now);
        if (kevinVisible && now - kevinAnimLastTick > (Uint32)(1000 / ANIM_FPS)) {
            kevinAnimFrame = (kevinAnimFrame + 1) % 36;
            kevinAnimLastTick = now;
        }
        if (!dlgActive) {
            gameState  = GS_CHASE_RUN;
            chasePhase = PHASE_OBJECTS;
            phaseStart = now;
            objLastSpawn = now;
        }
        break;
    }
    case GS_CHASE_RUN: {
        /* Background scroll */
        float spd = bgScrollSpeed * slowdownSpeed;
        bgOff[0] += 5.0f  * spd;
        bgOff[2] += 20.0f * spd;

        /* Moving platforms can scroll away while the player is stationary.
           Drop the player immediately instead of leaving onGround latched on. */
        if (player.onGround && chasePhase != PHASE_MICROWAVE) {
            int stillOn = 0;

            if (player.y + PLAYER_H >= (float)GROUND_Y - 2.0f)
                stillOn = 1;

            for (int i = 0; i < MAX_PLATFORMS && !stillOn; i++) {
                if (!platforms[i].active) continue;
                if (player.x + PLAYER_W > platforms[i].x &&
                    player.x < platforms[i].x + platforms[i].w &&
                    fabsf(player.y + PLAYER_H - platforms[i].y) < 8.0f)
                {
                    stillOn = 1;
                }
            }

            if (!stillOn)
                player.onGround = 0;
        }

        /* Player – auto run right during chase */
        if (chasePhase == PHASE_MICROWAVE) {
            /* Microwave phase owns player movement entirely — skip normal input */
        } else if (slamReEntryActive) {
            /* Force run right during re-entry; allow jump */
            player.vx = PLAYER_SPEED;
            player.facingRight = 1;
            if (keyUp && player.onGround) {
                player.vy = JUMP_VY;
                playJumpSfx();
                player.onGround = 0;
            }
        } else if (player.keepGoingShow) {
            /* During platform/object phases: preserve knockback direction */
            if (chasePhase == PHASE_OBJECTS ||
                chasePhase == PHASE_PLATFORMS ||
                chasePhase == PHASE_PATTERN_PLATFORMS) {
                player.vx = 0.0f;
            } else {
                player.vx = PLAYER_SPEED;  /* run right, away from Marv */
            }
            /* Still allow jump during respawn run */
            if (keyUp && player.onGround) {
                player.vy = JUMP_VY;
                playJumpSfx();
                player.onGround = 0;
            }
            if (now - player.keepGoingStart >= KEEP_GOING_MS)
                player.keepGoingShow = 0;
        } else {
            /* Free left/right movement in left half */
            if (!pushBackActive) {
                if (keyLeft)       { player.vx = -PLAYER_SPEED; player.facingRight = 0; }
                else if (keyRight) { player.vx =  PLAYER_SPEED; player.facingRight = 1; }
                else               { player.vx = 0.0f; }
                if (keyUp && player.onGround) {
                    player.vy = JUMP_VY;
                    playJumpSfx();
                    player.onGround = 0;
                }
            }
        }

        if (chasePhase != PHASE_MICROWAVE) player.x += player.vx;
        /* Left border clamp — disabled during PHASE_SLAM and PHASE_MICROWAVE so off-screen can trigger */
        if (player.x < 0.0f && chasePhase != PHASE_SLAM && chasePhase != PHASE_MICROWAVE) player.x = 0.0f;
        /* Right border punishment — violent pushback slide */
        if (player.x > (float)SCREEN_W * 0.5f) {
            player.x       = (float)SCREEN_W * 0.5f;
            player.vx      = 0.0f;
            pushBackActive = 1;
            pushBackVx     = -28.0f;  /* violent leftward slide */
        }

        /* Pushback slide — disabled during PHASE_SLAM so slam can push player off-screen */
        if (pushBackActive && chasePhase != PHASE_SLAM) {
            player.x  += pushBackVx;
            pushBackVx *= 0.88f;          /* decelerate */
            if (player.x < 0.0f) { player.x = 0.0f; }
            if (fabsf(pushBackVx) < 1.0f) {
                pushBackActive = 0;
                pushBackVx     = 0.0f;
            }
            /* Block normal input during pushback */
            player.vx = pushBackVx;
        }
        /* Final left-border safety clamp (covers keepGoing, pushback, and normal movement) */
        if (player.x < 0.0f && chasePhase != PHASE_SLAM && chasePhase != PHASE_MICROWAVE)
            player.x = 0.0f;

        if (chasePhase != PHASE_MICROWAVE) {
            player.y += player.vy;
            if (!player.onGround) {
                if (launchJumpActive) {
                    player.vy += 0.28f;   /* slow floaty arc instead of normal 0.55 gravity */
                    if (player.onGround || player.vy > 0.0f) {
                        /* Still rising or just peaked — keep reduced gravity */
                    }
                } else {
                    applyGravity(&player.vy);
                }
            }

            /* Platform landing */
            player.onGround = 0;
            if (!landOnPlatforms(&player.x, &player.y, &player.vy,
                                 &player.onGround, (float)PLAYER_W, (float)PLAYER_H, 1))
            {
                if (player.y + PLAYER_H >= (float)GROUND_Y) {
                    if (chasePhase == PHASE_PLATFORMS || chasePhase == PHASE_PATTERN_PLATFORMS) {
                        /* 3-second grace period after phase start so the launch jump can land safely */
                        if (now - phaseStart >= 3000) {
                            /* Falling to ground during platform phases = lose a life */
                            playerHit();
                        }
                        float bestY = (float)(GROUND_Y - 200);
                        for (int pi = 0; pi < MAX_PLATFORMS; pi++) {
                            if (platforms[pi].active && platforms[pi].x > 50.0f && platforms[pi].x < SCREEN_W - 50) {
                                if (platforms[pi].y < bestY || bestY == (float)(GROUND_Y - 200))
                                    bestY = platforms[pi].y;
                            }
                        }
                        player.x  = 200.0f;
                        player.y  = bestY - (float)PLAYER_H - 2.0f;
                        player.vy = -8.0f;   /* small bounce upward so player lands cleanly */
                        player.onGround = 0;
                    } else {
                        player.y = (float)(GROUND_Y - PLAYER_H);
                        player.vy = 0.0f;
                        player.onGround = 1;
                        launchJumpActive = 0;
                    }
                }
            }

            /* Fall off bottom */
            if (player.y > (float)(SCREEN_H + 50)) {
                playerHit();
                player.x = 200.0f;
                player.y = (float)(GROUND_Y - PLAYER_H);
                player.vx = 0.0f;
                player.vy = 0.0f;
                player.onGround = 1;
                if (gameState == GS_GAME_OVER) break;
            }
        } /* end !PHASE_MICROWAVE physics */

        /* Invincible timer */
        if (player.invincible && now - player.invincStart >= 1000)
            player.invincible = 0;
        if (multiplayerMode && marv.invincible && now - marv.invincStart >= 1000)
            marv.invincible = 0;

        /* Respawn timers (multiplayer) */
        if (multiplayerMode) {
            Uint32 nowTick = SDL_GetTicks();
            if (player.dead && nowTick - player.deadSince >= 15000) {
                player.dead       = 0;
                player.lives      = 1;
                player.invincible = 1;
                player.invincStart = nowTick;
                player.x          = 250.0f;
                player.y          = (float)(GROUND_Y - PLAYER_H);
                player.vx         = 0.0f;
                player.vy         = 0.0f;
                player.onGround   = 1;
            }
            if (marv.dead && nowTick - marv.deadSince >= 15000) {
                marv.dead       = 0;
                marv.lives      = 1;
                marv.invincible = 1;
                marv.invincStart = nowTick;
                marv.x          = 200.0f;
                marv.y          = (float)(GROUND_Y - PLAYER_H);
                marv.vx         = 0.0f;
                marv.vy         = 0.0f;
                marv.onGround   = 1;
            }
        }

        /* Phase updates */
        if (chasePhase == PHASE_OBJECTS || chasePhase == PHASE_SLAM)
            updateChaseObjects(now);
        else {
            Mix_HaltChannel(SFX_CH_CARDBOARD_ROLL);
            updateChasePlatforms(now);
        }

        if (chasePhase != PHASE_MICROWAVE) {
            if (multiplayerMode)
                updateMarvP2(now);
            else
                updateMarvAI(now);
        } else {
            /* Still advance Marv's run animation during microwave phase */
            Uint32 marvAnimDelay = (Uint32)(1000 / (marv.animFps > 0 ? marv.animFps : 1));
            if (now - marv.animLastTick > marvAnimDelay) {
                marv.animFrame = (marv.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
                marv.animLastTick = now;
            }
        }
        updateSkyBombs(now);

        /* Kevin animation — advance whenever visible */
        if (kevinVisible && now - kevinAnimLastTick > (Uint32)(1000 / ANIM_FPS)) {
            kevinAnimFrame = (kevinAnimFrame + 1) % 36;
            kevinAnimLastTick = now;
        }

        if (dlgActive) updateDialogue(now);
        updateRats(now);

        /* Player anim */
        if (now - player.animLastTick > (Uint32)(1000 / ANIM_FPS)) {
            player.animFrame = (player.animFrame + 1) % (ANIM_COLS * ANIM_ROWS);
            player.animLastTick = now;
        }
        break;
    }
    case GS_END_ROOM: {
        if (transDir != 0) updateTransition(now);
        updateEndRoom(now);
        /* Respawn timers (multiplayer) */
        if (multiplayerMode) {
            Uint32 nowTick = SDL_GetTicks();
            if (player.dead && nowTick - player.deadSince >= 15000) {
                player.dead       = 0;
                player.lives      = 1;
                player.invincible = 1;
                player.invincStart = nowTick;
                player.x          = 250.0f;
                player.y          = (float)(GROUND_Y - PLAYER_H);
                player.vx         = 0.0f;
                player.vy         = 0.0f;
                player.onGround   = 1;
            }
            if (marv.dead && nowTick - marv.deadSince >= 15000) {
                marv.dead       = 0;
                marv.lives      = 1;
                marv.invincible = 1;
                marv.invincStart = nowTick;
                marv.x          = 200.0f;
                marv.y          = (float)(GROUND_Y - PLAYER_H);
                marv.vx         = 0.0f;
                marv.vy         = 0.0f;
                marv.onGround   = 1;
            }
        }
        break;
    }
    case GS_MINI_TRANSITION_IN:
    case GS_MINI_TRANSITION_OUT: {
        updateTransition(now);
        break;
    }
    case GS_MINI_LEVEL: {
        updateMiniLevel(now);
        if (transDir != 0) updateTransition(now);
        /* Respawn timers (multiplayer) */
        if (multiplayerMode) {
            Uint32 nowTick = SDL_GetTicks();
            if (player.dead && nowTick - player.deadSince >= 15000) {
                player.dead       = 0;
                player.lives      = 1;
                player.invincible = 1;
                player.invincStart = nowTick;
                player.x          = 250.0f;
                player.y          = (float)(GROUND_Y - PLAYER_H);
                player.vx         = 0.0f;
                player.vy         = 0.0f;
                player.onGround   = 1;
            }
            if (marv.dead && nowTick - marv.deadSince >= 15000) {
                marv.dead       = 0;
                marv.lives      = 1;
                marv.invincible = 1;
                marv.invincStart = nowTick;
                marv.x          = 200.0f;
                marv.y          = (float)(GROUND_Y - PLAYER_H);
                marv.vx         = 0.0f;
                marv.vy         = 0.0f;
                marv.onGround   = 1;
            }
        }
        break;
    }
    case GS_END_FADE:
    case GS_GAME_OVER:
        break;
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER RATS
═══════════════════════════════════════════════════════════ */
static void renderRats(int ox, int oy)
{
    if (!ratsActive) return;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    for (int ri = 0; ri < RAT_COUNT; ri++) {
        if (!rats[ri].active) continue;
        int rx = (int)rats[ri].x + ox;
        int ry = GROUND_Y - 28 + oy;
        if (texRat && texRatW > 0 && texRatH > 0) {
            /* rat.png is a 6-col × 6-row animation sheet, same as all other chars */
            int cols = 6, rows = 6;
            int fw = texRatW / cols;
            int fh = texRatH / rows;
            int frameIdx = rats[ri].animFrame % (cols * rows);
            int col = frameIdx % cols;
            int row = frameIdx / cols;
            SDL_Rect src = { col * fw, row * fh, fw, fh };
            SDL_Rect dst = { rx, GROUND_Y - 32 + oy, 32, 32 };
            SDL_RenderCopy(ren, texRat, &src, &dst);
        } else {
            dr(rx, ry, 24, 16, 160, 120, 80, 255);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER GROUND (tiled texture or fallback rect)
═══════════════════════════════════════════════════════════ */
static void renderGround(int ox, int oy)
{
    int groundH = groundDrawHeight;
    if (groundH < 1) groundH = 1;
    if (groundH > SCREEN_H) groundH = SCREEN_H;
    int groundY = SCREEN_H - groundH + oy;

    dr(0 + ox, groundY, SCREEN_W, groundH, 80, 60, 40, 255);

    if (texGround && texGroundW > 0 && texGroundH > 0) {
        int scrollX = ((int)bgOff[2] + groundRepeatStartX) % texGroundW;
        if (scrollX < 0) scrollX += texGroundW;
        int startX = -scrollX;
        for (int x = startX; x < SCREEN_W; x += texGroundW) {
            SDL_Rect dst = { x + ox, groundY, texGroundW, groundH };
            SDL_RenderCopy(ren, texGround, NULL, &dst);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER BACKGROUND PARALLAX
═══════════════════════════════════════════════════════════ */
static void renderBackground(int ox, int oy)
{
    /* Helper: draw one layer with infinite horizontal tiling */
    /* Layer 0 — far (slowest) */
    if (texBgFar && texBgFarW > 0) {
        int startX = -((int)bgOff[0] % texBgFarW);
        for (int x = startX; x < SCREEN_W; x += texBgFarW)
        {
            SDL_Rect dst = { x + ox, 0 + oy, texBgFarW, SCREEN_H };
            SDL_RenderCopy(ren, texBgFar, NULL, &dst);
        }
    } else {
        /* fallback: dark blue bands */
        int bandW = 80;
        float off = bgOff[0];
        for (int x = -(int)off; x < SCREEN_W + bandW; x += bandW * 2)
            dr(x + ox, 0 + oy, bandW, SCREEN_H, 15, 20, 55, 255);
    }

    /* Layer 2 — near (fastest) */
    if (texBgNear && texBgNearW > 0 && texBgNearH > 0) {
        int startX = -((int)bgOff[2] % texBgNearW);
        for (int x = startX; x < SCREEN_W; x += texBgNearW)
        {
            SDL_Rect dst = { x + ox, GROUND_Y - texBgNearH + 10 + oy,
                             texBgNearW, (int)(texBgNearH * 1.37f) };
            SDL_RenderCopy(ren, texBgNear, NULL, &dst);
        }
    } else {
        int sW = 200;
        float off = bgOff[2];
        for (int x = -(int)off; x < SCREEN_W + sW; x += sW + 80)
            dr(x + ox, GROUND_Y + oy, sW, 20, 30, 25, 20, 255);
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER CHARACTER (colored rect with tint anim)
═══════════════════════════════════════════════════════════ */
static void renderCharacter(int x, int y, Uint8 r, Uint8 g, Uint8 b, int skip)
{
    if (skip) return;
    dr(x, y, PLAYER_W, PLAYER_H, r, g, b, 255);
    dr(x + 10, y + 15, 8, 8, 255, 255, 255, 200);
    dr(x + PLAYER_W - 18, y + 15, 8, 8, 255, 255, 255, 200);
}

static SDL_Rect makeSpriteDstRect(int x, int y, float scaleX, float scaleY)
{
    int drawW = (int)lroundf((float)PLAYER_W * scaleX);
    int drawH = (int)lroundf((float)PLAYER_H * scaleY);
    SDL_Rect dst = {
        x - (drawW - PLAYER_W) / 2,
        y - (drawH - PLAYER_H),
        drawW,
        drawH
    };
    return dst;
}

/* ═══════════════════════════════════════════════════════════
   RENDER PLAYER SPRITE (run / idle sheet)
═══════════════════════════════════════════════════════════ */
static void renderPlayerSprite(int x, int y, int skip)
{
    int characterNumber = selectedCharacterNumber();
    if (skip) return;
    SDL_Texture *tex;
    SDL_Texture *idleTex = characterIdleTexture(characterNumber);
    SDL_Texture *runTex = characterRunTexture(characterNumber);
    SDL_Texture *jumpTex = characterJumpTexture(characterNumber);
    int frameOverride = -1; /* -1 = use player.animFrame */
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (gameState == GS_DIALOGUE) {
        tex = idleTex;
        frameOverride = 0;  /* freeze on first frame */
    } else if (gameState == GS_END_ROOM && player.vx == 0.0f) {
        tex = idleTex;
    } else {
        tex = (player.onGround == 0 && jumpTex) ? jumpTex : runTex;
        if (tex == jumpTex) {
            scaleX = (characterNumber == 2) ? HARRY_JUMP_SCALE_X : MARV_JUMP_SCALE_X;
            scaleY = (characterNumber == 2) ? HARRY_JUMP_SCALE_Y : MARV_JUMP_SCALE_Y;
        }
    }
    if (!tex) {
        dr(x, y, PLAYER_W, PLAYER_H, 0, 120, 255, 255);
        return;
    }
    int texW = 0;
    int texH = 0;
    characterTextureSize(tex, characterNumber, &texW, &texH);
    if (texW <= 0 || texH <= 0) {
        dr(x, y, PLAYER_W, PLAYER_H, 0, 120, 255, 255);
        return;
    }
    int fw = texW / 6;
    int fh = texH / 6;
    int frameIdx = (frameOverride >= 0) ? frameOverride : player.animFrame;
    int col = frameIdx % 6;
    int row = frameIdx / 6;
    SDL_Rect src = { col * fw, row * fh, fw, fh };
    SDL_Rect dst = makeSpriteDstRect(x, y, scaleX, scaleY);
    SDL_RendererFlip flip = (player.facingRight == 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(ren, tex, &src, &dst, 0.0, NULL, flip);
}

/* ═══════════════════════════════════════════════════════════
   RENDER HARRY SPRITE (AI companion)
═══════════════════════════════════════════════════════════ */
static void renderHarrySprite(int x, int y, int skip)
{
    int characterNumber = companionCharacterNumber();
    if (skip) return;
    SDL_Texture *tex;
    int frameIdx;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    if (!multiplayerMode
        && (chasePhase == PHASE_PLATFORMS || chasePhase == PHASE_PATTERN_PLATFORMS)
        && characterJetpackTexture(characterNumber)) {
        tex = characterJetpackTexture(characterNumber);
        frameIdx = marv.animFrame;
    } else if (marv.onGround == 0 && characterJumpTexture(characterNumber)) {
        tex = characterJumpTexture(characterNumber);
        frameIdx = marv.animFrame;
        if (frameIdx > 18) frameIdx = 18;
        scaleX = (characterNumber == 2) ? HARRY_JUMP_SCALE_X : MARV_JUMP_SCALE_X;
        scaleY = (characterNumber == 2) ? HARRY_JUMP_SCALE_Y : MARV_JUMP_SCALE_Y;
    } else {
        tex = characterRunTexture(characterNumber);
        frameIdx = marv.animFrame;
    }
    if (!tex) { dr(x, y, PLAYER_W, PLAYER_H, 220, 50, 50, 255); return; }
    int texW = 0;
    int texH = 0;
    characterTextureSize(tex, characterNumber, &texW, &texH);
    if (texW <= 0 || texH <= 0) {
        dr(x, y, PLAYER_W, PLAYER_H, 220, 50, 50, 255);
        return;
    }
    int fw = texW / 6;
    int fh = texH / 6;
    int col = frameIdx % 6;
    int row = frameIdx / 6;
    SDL_Rect src = { col * fw, row * fh, fw, fh };
    SDL_Rect dst = makeSpriteDstRect(x, y, scaleX, scaleY);
    SDL_RendererFlip flip = (marv.vx > 0.0f) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(ren, tex, &src, &dst, 0.0, NULL, flip);
}

/* ═══════════════════════════════════════════════════════════
   RENDER DIALOGUE
═══════════════════════════════════════════════════════════ */
static void renderDialogue(Uint32 now)
{
    if (!dlgActive) return;

    /* Position: top of screen during microwave phase, bottom otherwise */
    int boxY = dlgAtTop ? 10 : DLGBOX_Y;

    /* Box background */
    SDL_SetRenderDrawColor(ren, 10, 10, 20, 220);
    SDL_Rect box = {DLGBOX_X, boxY, DLGBOX_W, DLGBOX_H};
    SDL_RenderFillRect(ren, &box);
    SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
    SDL_RenderDrawRect(ren, &box);

    DlgCharacter spk = dlgCurLines[dlgLineIdx].speaker;
    /* Portrait */
    {
        int characterNumber = dialogueCharacterNumber(spk);
        SDL_Rect portrait = { DLGBOX_X + 8, boxY + 8, DLGBOX_PORTRAIT_SIZE, DLGBOX_PORTRAIT_SIZE };
        SDL_Texture *ptex = characterPortraitTexture(characterNumber);
        if (ptex) {
            SDL_RenderCopy(ren, ptex, NULL, &portrait);
        } else {
            Uint8 pr = (characterNumber == 2) ? 0   : 220;
            Uint8 pg = (characterNumber == 2) ? 120 : 50;
            Uint8 pb = (characterNumber == 2) ? 255 : 50;
            dr(portrait.x, portrait.y, DLGBOX_PORTRAIT_SIZE, DLGBOX_PORTRAIT_SIZE, pr, pg, pb, 255);
        }
    }

    /* Speaker name */
    const char *name = characterName(dialogueCharacterNumber(spk));
    renderText(font, name,
               DLGBOX_X + 8,
               boxY + DLGBOX_PORTRAIT_SIZE + 10,
               200, 200, 200, 255);

    int textX = DLGBOX_X + DLGBOX_PORTRAIT_SIZE + 20;
    int textY = boxY + 20;

    if (font) {
        int lineH = TTF_FontLineSkip(font);
        int curY = textY;
        int prevConsumed = 0;
        for (int i = 0; i < dlgWrappedCount; i++) {
            int sourceVisible = dlgCharShown - prevConsumed;
            if (sourceVisible < 0) sourceVisible = 0;

            int visibleLen = (int)strlen(dlgWrappedLines[i]);
            int charsToRender = sourceVisible;
            if (charsToRender > visibleLen) charsToRender = visibleLen;
            if (charsToRender > 0) {
                char visibleLine[DLG_WRAP_MAX_CHARS];
                memcpy(visibleLine, dlgWrappedLines[i], (size_t)charsToRender);
                visibleLine[charsToRender] = '\0';
                renderText(font, visibleLine, textX, curY, 255, 255, 255, 255);
            }
            if (dlgFullyShown || dlgCharShown > prevConsumed) {
                curY += lineH;
            } else {
                break;
            }
            prevConsumed = dlgWrappedConsumedEnds[i];
            if (!dlgFullyShown && prevConsumed >= dlgCharShown) break;
        }
    }

    /* Advance hint */
    if (dlgFullyShown) {
        Uint32 blink = (now / 400) % 2;
        if (blink) renderText(font, "[ Space / Enter ]",
                              DLGBOX_X + DLGBOX_W - 180,
                              boxY + DLGBOX_H - 22,
                              180, 180, 180, 255);
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER HUD
═══════════════════════════════════════════════════════════ */
static void updateFpsCounter(Uint32 now)
{
    if (fpsWindowStart == 0) fpsWindowStart = now;
    fpsFrameCounter++;
    Uint32 elapsed = now - fpsWindowStart;
    if (elapsed >= 500) {
        fpsDisplay = (elapsed > 0) ? ((float)fpsFrameCounter * 1000.0f / (float)elapsed) : 0.0f;
        fpsFrameCounter = 0;
        fpsWindowStart = now;
    }
}

static void renderFpsCounter(void)
{
    char line[32];
    snprintf(line, sizeof(line), "FPS %.1f", fpsDisplay);
    renderTextRight(font, line, SCREEN_W - 10, 50, 160, 230, 255);
}

static float clamp01f(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void drawRoundedRectFilled(int x, int y, int w, int h, int radius,
                                  Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (w <= 0 || h <= 0) return;
    if (radius < 0) radius = 0;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    SDL_SetRenderDrawColor(ren, r, g, b, a);
    if (radius <= 0) {
        SDL_Rect rc = {x, y, w, h};
        SDL_RenderFillRect(ren, &rc);
        return;
    }

    for (int yy = 0; yy < h; yy++) {
        int inset = 0;
        if (yy < radius) {
            int dy = radius - 1 - yy;
            inset = radius - (int)sqrtf((float)(radius * radius - dy * dy));
        } else if (yy >= h - radius) {
            int dy = yy - (h - radius);
            inset = radius - (int)sqrtf((float)(radius * radius - dy * dy));
        }
        SDL_RenderDrawLine(ren, x + inset, y + yy, x + w - 1 - inset, y + yy);
    }
}

static void drawFilledCircle(int cx, int cy, int radius,
                             Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    if (radius <= 0) return;
    SDL_SetRenderDrawColor(ren, r, g, b, a);
    for (int yy = -radius; yy <= radius; yy++) {
        int dx = (int)sqrtf((float)(radius * radius - yy * yy));
        SDL_RenderDrawLine(ren, cx - dx, cy + yy, cx + dx, cy + yy);
    }
}

static float getLevel2ProgressPct(Uint32 now)
{
    float p = 0.0f;

    if (gameState == GS_END_FADE) return 1.0f;
    if (gameState == GS_END_ROOM) {
        if (endFadeActive) return 0.995f;
        return player.hasKey ? 0.985f : 0.955f;
    }
    if (gameState == GS_MINI_LEVEL) {
        if (miniTrapDoorOpen) return 0.98f;
        return miniKeyCollected ? 0.975f : 0.965f;
    }
    if (gameState == GS_MINI_TRANSITION_OUT) {
        return (transTargetState == GS_END_FADE) ? 0.995f : 0.985f;
    }
    if (gameState == GS_MINI_TRANSITION_IN) {
        if (transTargetState == GS_END_FADE) return 0.995f;
        return (transTargetState == GS_END_ROOM) ? 0.985f : 0.96f;
    }

    switch (chasePhase) {
        case PHASE_OBJECTS: {
            float t = clamp01f((float)objCount / 15.0f);
            if (slamSpawned || slamDlgStarted) t = 1.0f;
            p = 0.02f + 0.14f * t;
            break;
        }
        case PHASE_SLAM: {
            float t = clamp01f((float)(now - phaseStart) / 10000.0f);
            p = 0.16f + 0.10f * t;
            break;
        }
        case PHASE_PLATFORMS: {
            float t = clamp01f((float)(now - phaseStart) / 10000.0f);
            p = 0.26f + 0.14f * t;
            break;
        }
        case PHASE_PATTERN_PLATFORMS: {
            float t = clamp01f((float)(now - phaseStart) / 60000.0f);
            p = 0.40f + 0.32f * t;
            break;
        }
        case PHASE_RETURN_GROUND: {
            float t = clamp01f((float)(now - phaseStart) / 4000.0f);
            p = 0.72f + 0.08f * t;
            break;
        }
        case PHASE_MICROWAVE: {
            float t = 0.0f;
            if (MW_SLAM_COUNT > 0) t = (float)mwRound / (float)MW_SLAM_COUNT;
            if (mwActive) t += 0.12f;
            t = clamp01f(t);
            p = 0.80f + 0.10f * t;
            break;
        }
        case PHASE_KEVIN_ESCAPE: {
            float t = clamp01f((float)(now - phaseStart) / 3500.0f);
            p = 0.90f + 0.04f * t;
            break;
        }
        case PHASE_SLOWDOWN: {
            float t = clamp01f((float)(now - phaseStart) / 3000.0f);
            p = 0.94f + 0.04f * t;
            break;
        }
    }

    if (p < 0.0f) p = 0.0f;
    if (p > 0.999f) p = 0.999f;
    return p;
}

static void renderLevelProgressMiniMap(Uint32 now)
{
    int mapW = 184;
    int mapH = 40;
    int mapX = SCREEN_W - mapW - 16;
    int mapY = SCREEN_H - mapH - 16;
    int radius = 10;
    int trackPad = 14;
    int trackX = mapX + trackPad;
    int trackY = mapY + mapH / 2;
    int trackW = mapW - trackPad * 2;
    float progress = getLevel2ProgressPct(now);
    int dotX = trackX + (int)(progress * (float)(trackW - 1));
    char pct[24];

    drawRoundedRectFilled(mapX, mapY, mapW, mapH, radius, 12, 18, 30, 150);
    drawRoundedRectFilled(trackX, trackY - 2, trackW, 4, 2, 170, 190, 220, 140);
    drawFilledCircle(dotX, trackY, 5, 255, 190, 70, 240);

    snprintf(pct, sizeof(pct), "%d%%", (int)(progress * 100.0f + 0.5f));
    renderText(font, pct, mapX + mapW - 40, mapY + 10, 225, 235, 245, 230);
}

static void renderHUD(Uint32 now)
{
    /* Lives hearts */
    for (int i = 0; i < player.lives; i++) {
        dr(10 + i * 20, 10, 15, 15, 220, 50, 50, 255);
    }
    /* P1 name under hearts */
    renderText(font, p1Name, 10, 28, 220, 220, 220, 255);

    /* P1 respawn countdown */
    if (multiplayerMode && player.dead) {
        Uint32 elapsed = now - player.deadSince;
        int secsLeft = (int)((15000 - (int)elapsed) / 1000) + 1;
        if (secsLeft < 1) secsLeft = 1;
        char cntBuf[32];
        snprintf(cntBuf, sizeof(cntBuf), "RESPAWN %ds", secsLeft);
        renderText(font, cntBuf, 10, 10, 255, 100, 100, 255);
    }

    /* P2 lives — top right (only in multiplayer) */
    if (multiplayerMode) {
        for (int i = 0; i < marv.lives; i++) {
            dr(SCREEN_W - 10 - (i + 1) * 20, 10, 15, 15, 50, 150, 220, 255);
        }
        /* P2 name under hearts — right-aligned */
        renderTextRight(font, p2Name, SCREEN_W - 10, 28, 150, 200, 255);
        /* P2 respawn countdown */
        if (marv.dead) {
            Uint32 elapsed2 = now - marv.deadSince;
            int secsLeft2 = (int)((15000 - (int)elapsed2) / 1000) + 1;
            if (secsLeft2 < 1) secsLeft2 = 1;
            char cntBuf2[32];
            snprintf(cntBuf2, sizeof(cntBuf2), "RESPAWN %ds", secsLeft2);
            renderTextRight(font, cntBuf2, SCREEN_W - 10, 10, 100, 180, 255);
        }
    }

    /* P2 KEEP GOING */
    if (multiplayerMode && marv.keepGoingShow) {
        Uint32 elapsed2 = now - marv.keepGoingStart;
        if (elapsed2 < (Uint32)KEEP_GOING_MS) {
            Uint8 alpha2 = (Uint8)(255 - (elapsed2 * 255 / KEEP_GOING_MS));
            renderTextCentered(bigFont, "P2 KEEP GOING!", SCREEN_H / 2 + 20,
                               100, 180, 255, alpha2);
        }
    }

    /* Key icon if held */
    if (player.hasKey) {
        dr(10, 30, 18, 10, 255, 220, 0, 255);
        renderText(font, "KEY", 30, 28, 255, 220, 0, 255);
    }

    /* Debug invincible indicator */
    if (debugInvincible) {
        renderText(font, "[INVINCIBLE]", SCREEN_W/2 - 60, 10, 255, 255, 0, 255);
    }

    if (groundAdminMode) {
        char line1[96];
        char line2[96];
        snprintf(line1, sizeof(line1), "GROUND ADMIN [F1] h=%d", groundDrawHeight);
        snprintf(line2, sizeof(line2), "repeatX=%d  PgUp/PgDn Home/End Backspace=reset", groundRepeatStartX);
        renderText(font, line1, 10, SCREEN_H - 40, 255, 230, 120, 255);
        renderText(font, line2, 10, SCREEN_H - 22, 255, 230, 120, 255);
    }

    renderLevelProgressMiniMap(now);

    /* KEEP GOING */
    if (player.keepGoingShow) {
        Uint32 elapsed = now - player.keepGoingStart;
        if (elapsed < (Uint32)KEEP_GOING_MS) {
            Uint8 alpha = (Uint8)(255 - (elapsed * 255 / KEEP_GOING_MS));
            renderTextCentered(bigFont, "KEEP GOING!", SCREEN_H / 2 - 40,
                               255, 220, 50, alpha);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER CHASE OBJECTS
═══════════════════════════════════════════════════════════ */
static void renderObjects(int ox, int oy)
{
    for (int i = 0; i < MAX_OBJECTS; i++) {
        if (!objects[i].active) continue;
        if (objects[i].big) {
            if (texFridge && texFridgeW > 0 && texFridgeH > 0) {
                SDL_Rect dst = { (int)objects[i].x + ox,
                                 GROUND_Y - texFridgeH + 80 + oy,
                                 texFridgeW, texFridgeH };
                SDL_RenderCopy(ren, texFridge, NULL, &dst);
            } else {
                dr((int)objects[i].x + ox, (int)objects[i].y + oy, 350, 500, 180, 30, 20, 255);
                dr((int)objects[i].x + 60 + ox, (int)objects[i].y + 60 + oy, 50, 40, 255,255,255,200);
                dr((int)objects[i].x + 240 + ox, (int)objects[i].y + 60 + oy, 50, 40, 255,255,255,200);
            }
        } else {
            /* rolling cardbox */
            if (texCardbox && texCardboxW > 0 && texCardboxH > 0) {
                int fw = texCardboxW / 6;
                int fh = texCardboxH / 6;
                int col = cardboxFrame % 6, row = cardboxFrame / 6;
                SDL_Rect src = { col * fw, row * fh, fw, fh };
                SDL_Rect dst = { (int)objects[i].x + ox, (int)objects[i].y + oy, 64, 64 };
                SDL_RenderCopy(ren, texCardbox, &src, &dst);
            } else {
                dr((int)objects[i].x + ox, (int)objects[i].y + oy, 64, 64, 160, 120, 40, 255);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════
   RENDER PLATFORMS (CHASE)
═══════════════════════════════════════════════════════════ */
static void renderPlatforms(int ox, int oy)
{
    int colorState = -1;
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        if (!platforms[i].active) continue;
        if (texPlatform && texPlatformW > 0 && texPlatformH > 0) {
            int nextColorState = platforms[i].trap ? 1 : 0;
            if (nextColorState != colorState) {
                if (nextColorState) SDL_SetTextureColorMod(texPlatform, 255, 55, 55);
                else SDL_SetTextureColorMod(texPlatform, 255, 255, 255);
                colorState = nextColorState;
            }
            /* Tile horizontally to fill platform width */
            for (int x = 0; x < platforms[i].w; x += texPlatformW) {
                int drawW = texPlatformW;
                if (x + drawW > platforms[i].w) drawW = platforms[i].w - x;
                SDL_Rect src = { 0, 0, drawW, texPlatformH };
                SDL_Rect dst = { (int)platforms[i].x + x + ox,
                                 (int)platforms[i].y + oy,
                                 drawW, platforms[i].h };
                SDL_RenderCopy(ren, texPlatform, &src, &dst);
            }
        } else {
            dr((int)platforms[i].x + ox, (int)platforms[i].y + oy,
               platforms[i].w, platforms[i].h, 100, 100, 80, 255);
        }
    }
    if (colorState != -1) SDL_SetTextureColorMod(texPlatform, 255, 255, 255);
}

/* ═══════════════════════════════════════════════════════════
   RENDER END ROOM
═══════════════════════════════════════════════════════════ */
static void renderEndRoom(int ox, int oy, Uint32 now)
{
    /* Ground */
    renderGround(ox, oy);
    /* Left wall */
    dr(0 + ox, 0 + oy, 20, SCREEN_H, 60, 40, 30, 255);
    /* Platforms */
    for (int i = 0; i < 3; i++) {
        dr(endPlats[i].x + ox, endPlats[i].y + oy, endPlats[i].w, endPlats[i].h,
           120, 100, 70, 255);
    }
    /* Door bottom (brown) */
    dr(DOOR_BOTTOM_X + ox, DOOR_BOTTOM_Y + oy, DOOR_BOTTOM_W, DOOR_BOTTOM_H,
       139, 69, 19, 255);
    /* Door top (blue) */
    dr(DOOR_TOP_X + ox, DOOR_TOP_Y + oy, DOOR_TOP_W, DOOR_TOP_H,
       30, 100, 200, 255);
    /* Door labels */
    /* TOP door → mini level */
    renderText(font, "E", DOOR_TOP_X + 20 + ox, DOOR_TOP_Y - 20 + oy, 255,255,255,255);
    /* BOTTOM door → needs key */
    if (player.hasKey)
        renderText(font, "E", DOOR_BOTTOM_X + 20 + ox, DOOR_BOTTOM_Y - 20 + oy, 255,255,100,255);
    else
        renderText(font, "KEY?", DOOR_BOTTOM_X - 10 + ox, DOOR_BOTTOM_Y - 20 + oy, 180,100,100,255);

    /* Companion (static in end room) */
    if (hasLevel2Companion()) {
        int companionNumber = companionCharacterNumber();
        SDL_Texture *staticTex = characterStaticIdleTexture(companionNumber);
        int staticW = 0;
        int staticH = 0;
        characterStaticIdleSize(companionNumber, &staticW, &staticH);
        Uint8 mAlpha = (Uint8)SDL_clamp((int)marvEndAlpha, 0, 255);

        if (staticTex && staticW > 0 && staticH > 0) {
            int drawH = PLAYER_H * 2;
            int drawW = (staticH > 0) ? (staticW * drawH / staticH) : PLAYER_W * 2;
            SDL_Rect dst = { (int)marv.x + ox, GROUND_Y - drawH + oy, drawW, drawH };
            SDL_SetTextureAlphaMod(staticTex, mAlpha);
            SDL_RenderCopy(ren, staticTex, NULL, &dst);
            SDL_SetTextureAlphaMod(staticTex, 255);
        } else {
            SDL_SetRenderDrawColor(ren, 220, 50, 50, mAlpha);
            SDL_Rect mRect = {(int)marv.x + ox, (int)marv.y + oy, PLAYER_W, PLAYER_H};
            SDL_RenderFillRect(ren, &mRect);
        }
    }

    /* Door feedback text */
    if (needKeyShow && now - needKeyTimer < 2000) {
        renderTextCentered(font, "Need a key...", DOOR_BOTTOM_Y - 40, 255, 100, 100, 255);
    }
    if (topDoorClosed && now - topDoorClosedTimer < 2000) {
        renderTextCentered(font, "Room closed.", DOOR_TOP_Y - 30, 180, 180, 180, 255);
    }

    /* End fade overlay */
    if (endFadeActive || gameState == GS_END_FADE) {
        Uint8 fa = (Uint8)SDL_clamp((int)endFadeAlpha, 0, 255);
        dr(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, fa);
    }
    (void)now;
}

/* ═══════════════════════════════════════════════════════════
   RENDER MINI LEVEL
═══════════════════════════════════════════════════════════ */
static void renderMiniLevel(int ox, int oy, Uint32 now)
{
    /* Background tint */
    dr(0 + ox, 0 + oy, SCREEN_W, SCREEN_H, 10, 15, 30, 255);
    /* Ground */
    renderGround(ox, oy);

    /* Trap door hole in ground */
    if (miniTrapDoorOpen) {
        dr((int)miniTrapDoorX + ox, GROUND_Y + oy, miniTrapDoorW, 80, 5, 5, 10, 255);
        renderText(font, "EXIT v", (int)miniTrapDoorX + 5 + ox, GROUND_Y - 20 + oy,
                   255, 255, 100, 255);
    }

    /* Spike zones */
    for (int s = 0; s < MINI_NUM_SPIKES; s++) {
        dr(miniSpikeX[s] + ox, GROUND_Y - 24 + oy, miniSpikeW[s], 24, 200, 40, 40, 255);
        renderText(font, "^^^", miniSpikeX[s] + 2 + ox, GROUND_Y - 22 + oy, 255, 80, 80, 220);
    }

    /* Platform 0 — looks safe (green), no warning label */
    int p0show = !(miniPlat0Touched == 1 && miniPlat0Y > (float)GROUND_Y + 10.0f);
    if (p0show) {
        dr((int)miniPlats[0].x + ox, (int)miniPlat0Y + oy, miniPlats[0].w, miniPlats[0].h,
           60, 200, 80, 255);
    }

    /* Platform 1 with orange gravity pad */
    dr((int)miniPlats[1].x + ox, (int)miniPlats[1].y + oy, miniPlats[1].w, miniPlats[1].h,
       100, 100, 80, 255);
    dr((int)miniPlats[1].x + 10 + ox, (int)miniPlats[1].y - 8 + oy, 30, 8, 255, 140, 0, 255);

    /* Platform 2 */
    dr((int)miniPlats[2].x + ox, (int)miniPlats[2].y + oy, miniPlats[2].w, miniPlats[2].h,
       80, 160, 80, 255);

    /* Platform 3 (fake exit) — looks very legit */
    dr((int)miniPlats[3].x + ox, (int)miniPlats[3].y + oy, miniPlats[3].w, miniPlats[3].h,
       60, 120, 220, 255);
    renderText(font, "EXIT!", (int)miniPlats[3].x + 5 + ox, (int)miniPlats[3].y - 20 + oy,
               100, 220, 255, 240);

    /* Platform 4 — looks safe (teal), no warning */
    {
        int p4show = !(miniPlat4Touched == 1 && miniPlat4Y > (float)GROUND_Y + 10.0f);
        if (p4show) {
            dr((int)miniPlats[4].x + ox, (int)miniPlat4Y + oy, miniPlats[4].w, miniPlats[4].h,
               40, 180, 160, 255);
        }
    }

    /* Platform 5 — looks safe (cyan), no warning */
    {
        int p5show = !(miniPlat5Touched == 1 && miniPlat5Y > (float)GROUND_Y + 10.0f);
        if (p5show) {
            dr((int)miniPlats[5].x + ox, (int)miniPlat5Y + oy, miniPlats[5].w, miniPlats[5].h,
               50, 200, 220, 255);
        }
    }

    /* Platform 6 (fake exit #2) — looks like the real deal */
    dr((int)miniPlats[6].x + ox, (int)miniPlats[6].y + oy, miniPlats[6].w, miniPlats[6].h,
       60, 120, 220, 255);
    renderText(font, "EXIT!", (int)miniPlats[6].x + 5 + ox, (int)miniPlats[6].y - 20 + oy,
               100, 220, 255, 240);

    /* Fake key on platform 3 — golden, looks identical to real key */
    if (!miniFakeKey3Taken) {
        float fkx = miniPlats[3].x + 20.0f;
        float fky = miniPlats[3].y - 22.0f;
        dr((int)fkx + ox, (int)fky + oy, 20, 20, 255, 220, 0, 255);
        renderText(font, "KEY!", (int)fkx + 22 + ox, (int)fky + oy, 255, 220, 0, 200);
    }

    /* Real key */
    if (miniKeySpawned && !miniKeyCollected) {
        dr((int)miniKeyX + ox, (int)miniKeyY + oy, 20, 20, 255, 220, 0, 255);
        renderText(font, "KEY!", (int)miniKeyX + 22 + ox, (int)miniKeyY + oy,
                   255, 220, 0, 200);
    }

    /* Gravity flipped indicator */
    if (miniGravFlipped) {
        renderTextCentered(bigFont, "GRAVITY FLIPPED", 60, 255, 140, 0, 220);
    }

    /* Text overlays */
    if (miniShowWTF && now - miniWTFStart < MINI_TEXT_DURATION_MS) {
        Uint8 a = (Uint8)(255 - (now - miniWTFStart) * 255 / MINI_TEXT_DURATION_MS);
        renderTextCentered(bigFont, "WTF!?", SCREEN_H / 2 - 40, 255, 220, 50, a);
    } else if (miniShowWTF && now - miniWTFStart >= MINI_TEXT_DURATION_MS) {
        miniShowWTF = 0;
    }
    if (miniShowBonk && now - miniBonkStart < MINI_TEXT_DURATION_MS) {
        Uint8 a = (Uint8)(255 - (now - miniBonkStart) * 255 / MINI_TEXT_DURATION_MS);
        renderTextCentered(bigFont, "BONK!", SCREEN_H / 2 - 40, 255, 255, 255, a);
    } else if (miniShowBonk && now - miniBonkStart >= MINI_TEXT_DURATION_MS) {
        miniShowBonk = 0;
    }
    if (miniShowPsych && now - miniPsychStart < MINI_TEXT_DURATION_MS) {
        Uint8 a = (Uint8)(255 - (now - miniPsychStart) * 255 / MINI_TEXT_DURATION_MS);
        renderTextCentered(bigFont, "nice try lol", SCREEN_H / 2 - 40, 255, 80, 80, a);
    } else if (miniShowPsych && now - miniPsychStart >= MINI_TEXT_DURATION_MS) {
        miniShowPsych = 0;
    }
    if (miniShowPsych2 && now - miniPsych2Start < MINI_TEXT_DURATION_MS) {
        Uint8 a = (Uint8)(255 - (now - miniPsych2Start) * 255 / MINI_TEXT_DURATION_MS);
        renderTextCentered(bigFont, "bro really thought", SCREEN_H / 2 - 40, 255, 100, 255, a);
    } else if (miniShowPsych2 && now - miniPsych2Start >= MINI_TEXT_DURATION_MS) {
        miniShowPsych2 = 0;
    }
    if (miniShowGotcha && now - miniGotchaStart < MINI_TEXT_DURATION_MS) {
        Uint8 a = (Uint8)(255 - (now - miniGotchaStart) * 255 / MINI_TEXT_DURATION_MS);
        renderTextCentered(bigFont, "ITS FAKE LMAOOO", SCREEN_H / 2 - 40, 255, 200, 0, a);
    } else if (miniShowGotcha && now - miniGotchaStart >= MINI_TEXT_DURATION_MS) {
        miniShowGotcha = 0;
    }

}

/* ═══════════════════════════════════════════════════════════
   KEY SELECT SCREEN
═══════════════════════════════════════════════════════════ */
static void renderKeySelect(void)
{
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    if (!font) return;

    int cx = SCREEN_W / 2;

    renderText(font, "MULTIPLAYER - PLAYER 1: CHOOSE YOUR CONTROLS",
               cx - 340, 200, 255, 255, 255, 255);

    renderText(font, "Press  W A S D + SPACE  to use WASD",
               cx - 220, 310, 180, 220, 255, 255);

    renderText(font, "Press  ARROW KEYS  to use Arrows",
               cx - 200, 380, 255, 220, 180, 255);

    renderText(font, "Player 2 will get the remaining controls.",
               cx - 240, 490, 160, 160, 160, 255);

    char assignLine[192];
    if (p1IsHarry)
        snprintf(assignLine, sizeof(assignLine), "%s plays Harry  |  %s plays Marv", p1Name, p2Name);
    else
        snprintf(assignLine, sizeof(assignLine), "%s plays Marv   |  %s plays Harry", p1Name, p2Name);
    renderText(font, assignLine, cx - 280, 570, 255, 255, 100, 255);
}

/* ═══════════════════════════════════════════════════════════
   MAIN RENDER
═══════════════════════════════════════════════════════════ */
static void render(Uint32 now, int present)
{
    updateFpsCounter(now);

    if (gameState == GS_KEY_SELECT) {
        renderKeySelect();
        renderFpsCounter();
        if (present) SDL_RenderPresent(ren);
        return;
    }

    int ox = shakeActive ? shakeOX : 0;
    int oy = shakeActive ? shakeOY : 0;

    /* 1. Clear */
    SDL_SetRenderDrawColor(ren, 15, 15, 25, 255);
    SDL_RenderClear(ren);

    if (gameState == GS_GAME_OVER) {
        dr(0, 0, SCREEN_W, SCREEN_H, 20, 0, 0, 255);
        renderTextCentered(bigFont ? bigFont : font, "YOU DIED",
                           SCREEN_H/2 - 80, 255, 60, 60, 255);
        renderTextCentered(font, "Press Enter to continue",
                           SCREEN_H/2 + 40, 160, 160, 160, 255);
        options_scene_render_global_brightness_overlay(ren);
        renderFpsCounter();
        if (present) SDL_RenderPresent(ren);
        return;
    }

    if (gameState == GS_END_FADE) {
        dr(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 255);
        if (activeSession && level2ResultReady) {
            char line[160];
            int total_possible = 2000;

            renderTextCentered(bigFont, "LEVEL 2 SUMMARY", 90, 255, 220, 80, 255);
            snprintf(line, sizeof(line), "Lives Lost: %d / %d",
                     activeSession->level2.player_lives_lost,
                     activeSession->level2.starting_lives);
            renderText(font, line, 280, 220, 255, 255, 255, 255);

            snprintf(line, sizeof(line), "Secret Key: %s",
                     activeSession->level2.mini_key_found ? "YES" : "NO");
            renderText(font, line, 280, 270, 255, 255, 255, 255);

            snprintf(line, sizeof(line), "Level 2 Score: %d / 1000", activeSession->level2.points);
            renderText(font, line, 280, 330, 255, 220, 80, 255);

            snprintf(line, sizeof(line), "Level 1 Score: %d / 1000", activeSession->level1.points);
            renderText(font, line, 280, 430, 180, 220, 255, 255);
            snprintf(line, sizeof(line), "Total Score: %d / %d", activeSession->total_points, total_possible);
            renderText(font, line, 280, 480, 255, 255, 255, 255);
            snprintf(line, sizeof(line), "Grade: %c", activeSession->grade);
            renderText(font, line, 280, 530, 255, 220, 80, 255);
            renderText(font, session_grade_flavor(activeSession->grade), 280, 580, 190, 190, 190, 255);
            renderTextCentered(font, "Press Enter to finish", 650, 180, 180, 180, 255);
        }
        options_scene_render_global_brightness_overlay(ren);
        renderFpsCounter();
        if (present) SDL_RenderPresent(ren);
        return;
    }

    /* Intro states: only draw white rect and title, then return */
    if (gameState == GS_INTRO_FADE_IN || gameState == GS_INTRO_TITLE || gameState == GS_INTRO_FADE_OUT) {
        if (introWhite > 0.0f) {
            Uint8 wa = (Uint8)SDL_clamp((int)introWhite, 0, 255);
            dr(0, 0, SCREEN_W, SCREEN_H, 255, 255, 255, wa);
        }
        if (introTitleAlpha > 0.0f) {
            Uint8 ta = (Uint8)SDL_clamp((int)introTitleAlpha, 0, 255);
            renderTextCentered(bigFont, "Chapter 2 : The Chase",
                               SCREEN_H / 2 - 30, 30, 30, 60, ta);
        }
        options_scene_render_global_brightness_overlay(ren);
        renderFpsCounter();
        if (present) SDL_RenderPresent(ren);
        return;
    }

    /* 2. Background */
    if (gameState == GS_CHASE_RUN || gameState == GS_DIALOGUE) {
        renderBackground(ox, oy);
    } else if (gameState == GS_END_ROOM) {
        dr(0 + ox, 0 + oy, SCREEN_W, SCREEN_H, 25, 18, 12, 255);
    } else if (gameState == GS_MINI_LEVEL
               || gameState == GS_MINI_TRANSITION_IN
               || gameState == GS_MINI_TRANSITION_OUT) {
        /* handled in renderMiniLevel */
    }

    /* 3 + 4. Ground + Platforms */
    if (gameState == GS_CHASE_RUN || gameState == GS_DIALOGUE) {
        renderGround(ox, oy);
        renderPlatforms(ox, oy);
    }

    /* Danger line */
    if (gameState == GS_CHASE_RUN) {
        SDL_SetRenderDrawColor(ren, 220, 30, 30, 180);
        SDL_RenderDrawLine(ren, PLAYER_MAX_X + ox, 0 + oy, PLAYER_MAX_X + ox, SCREEN_H + oy);
    }

    /* 5. Objects */
    if (gameState == GS_CHASE_RUN &&
        (chasePhase == PHASE_OBJECTS || chasePhase == PHASE_SLAM)) {
        renderObjects(ox, oy);
    }

    /* 5c. Rats */
    if (gameState == GS_CHASE_RUN) renderRats(ox, oy);

    /* 5b. Sky bombs */
    if (gameState == GS_CHASE_RUN) {
        for (int i = 0; i < MAX_SKY_BOMBS; i++) {
            if (!skyBombs[i].active) continue;
            SDL_Rect dst = { (int)skyBombs[i].x + ox, (int)skyBombs[i].y + oy, 128, 128 };
            if (texCans) {
                SDL_RenderCopyEx(ren, texCans, NULL, &dst,
                                 (double)skyBombs[i].angle, NULL, SDL_FLIP_NONE);
            } else {
                dr(dst.x, dst.y, 128, 128, 255, 50, 200, 255);
            }
        }
    }

    /* 6. Kevin */
    if ((gameState == GS_CHASE_RUN || gameState == GS_DIALOGUE) && kevinVisible) {
        if (gameState == GS_DIALOGUE && texStaticKid && texStaticKidW > 0 && texStaticKidH > 0) {
            SDL_Rect dst = { (int)kevinX + ox, GROUND_Y - texStaticKidH + oy,
                             texStaticKidW, texStaticKidH };
            SDL_RenderCopy(ren, texStaticKid, NULL, &dst);
        } else if (texKid && texKidW > 0 && texKidH > 0) {
            int fw = texKidW / 6, fh = texKidH / 6;
            int col = kevinAnimFrame % 6, row = kevinAnimFrame / 6;
            SDL_Rect src = { col * fw, row * fh, fw, fh };
            SDL_Rect dst = { (int)kevinX + ox, (int)kevinY + oy, PLAYER_W, PLAYER_H };
            SDL_RenderCopy(ren, texKid, &src, &dst);
        } else {
            renderCharacter((int)kevinX + ox, (int)kevinY + oy, 50, 200, 50, 0);
        }
    }

    /* 7. Harry (AI companion) */
    if (hasLevel2Companion() && (gameState == GS_CHASE_RUN || gameState == GS_DIALOGUE)) {
        int companionNumber = companionCharacterNumber();
        SDL_Texture *staticTex = characterStaticIdleTexture(companionNumber);
        int staticW = 0;
        int staticH = 0;
        characterStaticIdleSize(companionNumber, &staticW, &staticH);
        if (gameState == GS_DIALOGUE && staticTex && staticW > 0 && staticH > 0) {
            int drawH = PLAYER_H * 2;
            int drawW = (staticH > 0) ? (staticW * drawH / staticH) : PLAYER_W * 2;
            SDL_Rect dst = { (int)marv.x + ox, GROUND_Y - drawH + oy, drawW, drawH };
            SDL_RenderCopy(ren, staticTex, NULL, &dst);
        } else {
            int marvSkip = (marv.dead) ? 1 :
                           ((multiplayerMode && marv.invincible) ? (int)(((now - marv.invincStart) / 80) % 2) : 0);
            renderHarrySprite((int)marv.x + ox, (int)marv.y + oy, marvSkip);
        }
    }

    /* 8. Harry (player) */
    {
        int skipDraw = player.dead ? 1 : 0;
        if (!player.dead && player.invincible) {
            Uint32 elapsed = now - player.invincStart;
            skipDraw = (elapsed / 80) % 2;
        }
        if (gameState != GS_END_ROOM && gameState != GS_MINI_LEVEL
            && gameState != GS_MINI_TRANSITION_IN && gameState != GS_MINI_TRANSITION_OUT)
        {
            renderPlayerSprite((int)player.x + ox, (int)player.y + oy, skipDraw);
        } else if (gameState == GS_MINI_LEVEL
                   || gameState == GS_MINI_TRANSITION_IN
                   || gameState == GS_MINI_TRANSITION_OUT) {
            /* drawn inside mini level render */
        } else {
            /* end room - draw player */
            renderPlayerSprite((int)player.x + ox, (int)player.y + oy, skipDraw);
        }
    }

    /* 8b. Microwave */
    if (gameState == GS_CHASE_RUN && chasePhase == PHASE_MICROWAVE && mwActive) {
        int mx = (int)mwX + ox;
        int my = (int)(GROUND_Y - MW_H) + oy;
        if (texMicrowave && texMicrowaveW > 0 && texMicrowaveH > 0) {
            SDL_Rect dst = { mx, my, MW_W, MW_H };
            SDL_RenderCopy(ren, texMicrowave, NULL, &dst);
        } else {
            /* Fallback placeholder */
            dr(mx, my, MW_W, MW_H, 180, 180, 180, 255);
            dr(mx + 8, my + 8, MW_W - 20, MW_H - 16, 30, 30, 30, 255);
            dr(mx + MW_W - 12, my + MW_H/2 - 8, 6, 16, 80, 80, 80, 255);
        }
    }

    /* 9. End room */
    if (gameState == GS_END_ROOM) {
        renderEndRoom(ox, oy, now);
        /* Player over end room */
        {
            int skipDraw = player.dead ? 1 : 0;
            if (!player.dead && player.invincible) skipDraw = ((now - player.invincStart) / 80) % 2;
            renderPlayerSprite((int)player.x + ox, (int)player.y + oy, skipDraw);
        }
    }

    /* 10. Mini level */
    if (gameState == GS_MINI_LEVEL
        || (gameState == GS_MINI_TRANSITION_IN && transTargetState == GS_MINI_LEVEL)
        || gameState == GS_MINI_TRANSITION_OUT)
    {
        renderMiniLevel(ox, oy, now);
        /* Player in mini */
        int skipDraw = player.dead ? 1 : 0;
        if (!player.dead && player.invincible) skipDraw = ((now - player.invincStart) / 80) % 2;
        renderPlayerSprite((int)player.x + ox, (int)player.y + oy, skipDraw);
    }

    /* 10b. End room during transition from chase to end room */
    if (gameState == GS_MINI_TRANSITION_IN && transTargetState == GS_END_ROOM) {
        dr(0 + ox, 0 + oy, SCREEN_W, SCREEN_H, 25, 18, 12, 255);
        renderEndRoom(ox, oy, now);
        int skipDraw = player.dead ? 1 : (player.invincible ? (((now - player.invincStart) / 80) % 2) : 0);
        renderPlayerSprite((int)player.x + ox, (int)player.y + oy, skipDraw);
    }

    /* 11. HUD */
    renderHUD(now);

    /* 12. Dialogue */
    renderDialogue(now);

    /* 13-14. KEEP GOING rendered inside renderHUD */

    /* 15. Transition overlay */
    if (transAlpha > 0.0f || gameState == GS_MINI_TRANSITION_IN || gameState == GS_MINI_TRANSITION_OUT) {
        Uint8 ta = (Uint8)SDL_clamp((int)transAlpha, 0, 255);
        dr(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, ta);
    }

    /* 17. Present */
    options_scene_render_global_brightness_overlay(ren);
    renderFpsCounter();
    online_client_submit_frame(ren, 2);
    if (present) SDL_RenderPresent(ren);
}

static void capLevel2FrameRate(Uint64 frameStartCounter)
{
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 elapsedCounter;
    Uint32 elapsedMs;
    Uint32 targetMs = 1000u / TARGET_FPS;

    if (freq == 0 || frameStartCounter == 0) {
        SDL_Delay(targetMs);
        return;
    }

    elapsedCounter = SDL_GetPerformanceCounter() - frameStartCounter;
    elapsedMs = (Uint32)((elapsedCounter * 1000u) / freq);
    if (elapsedMs < targetMs) {
        SDL_Delay(targetMs - elapsedMs);
    }
}

/* ═══════════════════════════════════════════════════════════
   MAIN
═══════════════════════════════════════════════════════════ */
int runLevel2(GameSession *session, SDL_Window *sharedWin, SDL_Renderer *sharedRen)
{
    ControlScheme p1ControlScheme = CONTROL_SCHEME_WASD;
    int pauseMenuActive = 0;
    int pauseMenuReady = 0;

    activeSession = session;
    ownsSDL = (sharedWin == NULL || sharedRen == NULL);
    if (!ownsSDL) {
        win = sharedWin;
        ren = sharedRen;
    }

    multiplayerMode = (activeSession && activeSession->mode == GAME_MODE_DUO) ? 1 : 0;
    if (activeSession) {
        p1ControlScheme = activeSession->player_control_scheme[0];
        if (p1ControlScheme != CONTROL_SCHEME_WASD
            && p1ControlScheme != CONTROL_SCHEME_ARROWS
            && p1ControlScheme != CONTROL_SCHEME_CONTROLLER) {
            p1ControlScheme = (activeSession->control_scheme == CONTROL_SCHEME_ARROWS)
                ? CONTROL_SCHEME_ARROWS
                : CONTROL_SCHEME_WASD;
        }
        p1UsesWASD = (p1ControlScheme == CONTROL_SCHEME_WASD) ? 1 : 0;
        if (activeSession->player_skin_number[0] == 2)
            p1IsHarry = 1;
        else if (activeSession->player_skin_number[0] == 1)
            p1IsHarry = 0;
        else
            p1IsHarry = (activeSession->skin_number == 2) ? 1 : 0;
    } else {
        p1UsesWASD = 1;
        p1IsHarry = 0;
    }
    if (multiplayerMode)
        p1UsesWASD = 0;

    if (!initSDL()) {
        SDL_Log("Init failed, exiting.");
        return 1;
    }
    pauseMenuReady = options_scene_init(win, ren);
    if (pauseMenuReady) options_scene_set_audio_enabled(0);
    initLevel();
    if (multiplayerMode && gameState == GS_KEY_SELECT)
        gameState = GS_INTRO_FADE_IN;

    int running = 1;
    int skipKeyPressCount = 0;
    Uint32 lastAutosaveTick = SDL_GetTicks();
    const Uint32 autosaveIntervalMs = 5000;
    SDL_Event ev;

    while (running) {
        Uint64 frameStartCounter = SDL_GetPerformanceCounter();

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
                        keyLeft = 0;
                        keyRight = 0;
                        keyUp = 0;
                        keyE = 0;
                        keyEPressed = 0;
                        key2Left = 0;
                        key2Right = 0;
                        key2Up = 0;
                    }
                    online_client_send_pause_state(1);
                } else {
                    if (pauseMenuActive) {
                        pauseMenuActive = 0;
                        options_scene_leave();
                        keyLeft = 0;
                        keyRight = 0;
                        keyUp = 0;
                        keyE = 0;
                        keyEPressed = 0;
                        key2Left = 0;
                        key2Right = 0;
                        key2Up = 0;
                    }
                    online_client_send_pause_state(0);
                }
            }
        }
        while (SDL_PollEvent(&ev)) {
            arcade_input_handle_event(&ev);
            if (ev.type == SDL_QUIT) {
                if (activeSession) activeSession->quit_requested = 1;
                running = 0;
                break;
            }
            if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) &&
                level2OnlineHostActive() &&
                ev.key.windowID == LEVEL2_ONLINE_REMOTE_WINDOW_ID) {
                continue;
            }
            if (pauseMenuActive) {
                OptionsSceneResult result = {0};
                options_scene_handle_event(&ev, &result);
                if (result.quit_to_menu) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    if (activeSession) activeSession->quit_requested = 1;
                    prepareLevel2Result(0);
                    running = 0;
                } else if (result.return_to_main) {
                    pauseMenuActive = 0;
                    options_scene_leave();
                    online_client_send_pause_state(0);
                }
                continue;
            }
            if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                SDL_Keymod mod = SDL_GetModState();
                if (k == SDLK_ESCAPE && pauseMenuReady) {
                    pauseMenuActive = 1;
                    options_scene_enter();
                    keyLeft = 0;
                    keyRight = 0;
                    keyUp = 0;
                    keyE = 0;
                    keyEPressed = 0;
                    key2Left = 0;
                    key2Right = 0;
                    key2Up = 0;
                    online_client_send_pause_state(1);
                    continue;
                }
                if (k == SDLK_p && !ev.key.repeat &&
                    (mod & KMOD_ALT) &&
                    (mod & KMOD_SHIFT) &&
                    !(mod & KMOD_CTRL)) {
                    SDL_Log("Level 2 dev shortcut: jump to final cutscene chase");
                    if (activeSession) activeSession->dev_jump_to_final_cutscene = 1;
                    prepareLevel2Result(1);
                    running = 0;
                    break;
                }
                if (k == SDLK_p && !ev.key.repeat &&
                    (mod & KMOD_CTRL) &&
                    ((mod & KMOD_ALT) || (mod & KMOD_SHIFT))) {
                    SDL_Log("Level 2 dev shortcut: jump to final level");
                    if (activeSession) activeSession->dev_jump_to_final = 1;
                    prepareLevel2Result(1);
                    running = 0;
                    break;
                }
                if (k == SDLK_a && !ev.key.repeat && (mod & KMOD_SHIFT)) {
                    SDL_Log("Level 2 Shift+A shortcut: skip to next level");
                    prepareLevel2Result(1);
                    running = 0;
                    break;
                }
                if (k == SDLK_n) {
                    skipKeyPressCount++;
                    SDL_Log("Level 2 skip shortcut: %d/3", skipKeyPressCount);
                    if (skipKeyPressCount >= 3) {
                        prepareLevel2Result(1);
                        running = 0;
                    }
                    break;
                }
                if (k == SDLK_F1) {
                    groundAdminMode = !groundAdminMode;
                    SDL_Log("Level 2 ground admin mode: %s", groundAdminMode ? "ON" : "OFF");
                    break;
                }
                if (groundAdminMode &&
                    (k == SDLK_PAGEUP || k == SDLK_PAGEDOWN ||
                     k == SDLK_HOME   || k == SDLK_END      ||
                     k == SDLK_BACKSPACE)) {
                    if (k == SDLK_PAGEUP) {
                        groundDrawHeight += 8;
                    } else if (k == SDLK_PAGEDOWN) {
                        groundDrawHeight -= 8;
                    } else if (k == SDLK_HOME) {
                        groundRepeatStartX -= 16;
                    } else if (k == SDLK_END) {
                        groundRepeatStartX += 16;
                    } else if (k == SDLK_BACKSPACE) {
                        groundDrawHeight = SCREEN_H - GROUND_Y;
                        groundRepeatStartX = 0;
                    }
                    if (groundDrawHeight < 1) groundDrawHeight = 1;
                    if (groundDrawHeight > SCREEN_H) groundDrawHeight = SCREEN_H;
                    if (groundRepeatStartX < -8192) groundRepeatStartX = -8192;
                    if (groundRepeatStartX > 8192) groundRepeatStartX = 8192;
                    SDL_Log("Ground admin: height=%d repeatStartX=%d",
                            groundDrawHeight, groundRepeatStartX);
                    break;
                }
                if (gameState == GS_END_FADE &&
                    (k == SDLK_RETURN || k == SDLK_SPACE || isLevel2InteractKey(k))) {
                    running = 0;
                    break;
                }
                if (k == SDLK_d && (SDL_GetModState() & KMOD_CTRL)) {
                    debugInvincible = !debugInvincible;
                } else if (k == SDLK_m && (SDL_GetModState() & KMOD_CTRL) && (SDL_GetModState() & KMOD_SHIFT)) {
                    /* Debug: Ctrl+Shift+M — jump to microwave slam scene as if played there */
                    gameState         = GS_CHASE_RUN;
                    chasePhase        = PHASE_MICROWAVE;
                    phaseStart        = SDL_GetTicks();
                    memset(objects,  0, sizeof(objects));
                    memset(skyBombs, 0, sizeof(skyBombs));
                    slamSpawned       = 1;
                    slamPushingPlayer = 0;
                    slamPushingMarv   = 0;
                    slamWaiting       = 0;
                    slamWaitStart     = 0;
                    pushBackActive    = 0;
                    pushBackVx        = 0.0f;
                    player.x          = 200.0f; player.y = (float)(GROUND_Y - PLAYER_H);
                    player.vx         = 0.0f;   player.vy = 0.0f;
                    player.onGround   = 1;       player.invincible = 0;
                    player.lives      = level2StartLives;
                    marv.lives        = level2StartLives;
                    marv.invincible   = 0;
                    marv.x            = 120.0f; marv.y = (float)(GROUND_Y - PLAYER_H);
                    marv.vx           = 0.0f;   marv.vy = 0.0f; marv.onGround = 1;
                    dlgActive = 0;
                    kevinEscapeDlgDone = 0;      kevinX = 900.0f; kevinVisible = 1;
                    mwRound = 0; mwActive = 0; mwWaiting = 0;
                    mwBothOut = 0; mwReentering = 0; dlgAtTop = 1; mwAutoAdvanceAt = 0;
                    startDialogue(dlgMwIntro, DLG_MW_INTRO_COUNT, 0);
                } else {
                    /* Key selection screen — handle before all other key logic */
                    if (gameState == GS_KEY_SELECT) {
                        int chosen = 0;
                        if (k == SDLK_w || k == SDLK_a || k == SDLK_s || k == SDLK_d || k == SDLK_SPACE) {
                            p1UsesWASD = 1;
                            chosen = 1;
                        } else if (k == SDLK_LEFT || k == SDLK_RIGHT || k == SDLK_UP || k == SDLK_DOWN) {
                            p1UsesWASD = 0;
                            chosen = 1;
                        }
                        if (chosen) {
                            gameState = GS_INTRO_FADE_IN;
                        }
                        goto skip_game_keys;
                    }

                    /* Movement key routing — split in multiplayer */
                    if (!multiplayerMode) {
                        if ((p1UsesWASD && k == SDLK_d) ||
                            (!p1UsesWASD && k == SDLK_RIGHT))
                            keyRight = 1;
                    } else {
                        /* Determine physical key sets for P1 and P2 */
                        int p1Right = p1UsesWASD ? (k == SDLK_d)     : (k == SDLK_RIGHT);
                        int p2Right = p1UsesWASD ? (k == SDLK_RIGHT) : (k == SDLK_d);
                        /* Route to game vars based on character assignment */
                        if (p1Right) { if (p1IsHarry) key2Right = 1; else keyRight = 1; }
                        if (p2Right) { if (p1IsHarry) keyRight  = 1; else key2Right = 1; }
                    }
                }
                if (gameState != GS_KEY_SELECT) {
                    if (!multiplayerMode) {
                        if ((p1UsesWASD && k == SDLK_a) ||
                            (!p1UsesWASD && k == SDLK_LEFT))
                            keyLeft = 1;
                        if ((p1UsesWASD && (k == SDLK_w || k == SDLK_SPACE)) ||
                            (!p1UsesWASD && k == SDLK_UP))
                            keyUp = 1;
                    } else {
                        int p1Left  = p1UsesWASD ? (k == SDLK_a)                          : (k == SDLK_LEFT);
                        int p2Left  = p1UsesWASD ? (k == SDLK_LEFT)                       : (k == SDLK_a);
                        int p1Up    = p1UsesWASD ? (k == SDLK_w || k == SDLK_SPACE)       : (k == SDLK_UP);
                        int p2Up    = p1UsesWASD ? (k == SDLK_UP)                         : (k == SDLK_w || k == SDLK_SPACE);
                        if (p1Left) { if (p1IsHarry) key2Left = 1; else keyLeft = 1; }
                        if (p2Left) { if (p1IsHarry) keyLeft  = 1; else key2Left = 1; }
                        if (p1Up)   { if (p1IsHarry) key2Up   = 1; else keyUp   = 1; }
                        if (p2Up)   { if (p1IsHarry) keyUp    = 1; else key2Up  = 1; }
                    }
                    if (isLevel2InteractKey(k)) { keyE = 1; keyEPressed = 1; }
                    if (gameState == GS_GAME_OVER) {
                        if (k == SDLK_RETURN || k == SDLK_SPACE || isLevel2InteractKey(k)) {
                            prepareLevel2Result(0);
                            running = 0;
                            break;
                        }
                    }
                    /* Dialogue advance */
                    if ((k == SDLK_SPACE || k == SDLK_RETURN) && dlgActive && dlgFreezeGame) {
                        if (dlgFullyShown) {
                            advanceDlgLine();
                        } else {
                            /* Show all immediately */
                            dlgCharShown = (int)strlen(dlgCurLines[dlgLineIdx].text);
                            dlgFullyShown = 1;
                            dlgLineFullAt = SDL_GetTicks();
                        }
                    } else if ((k == SDLK_SPACE || k == SDLK_RETURN) && dlgActive && !dlgFreezeGame) {
                        if (dlgFullyShown) {
                            advanceDlgLine();
                        } else {
                            dlgCharShown = (int)strlen(dlgCurLines[dlgLineIdx].text);
                            dlgFullyShown = 1;
                            dlgLineFullAt = SDL_GetTicks();
                        }
                    }
                }
                skip_game_keys:;
            }
            if (ev.type == SDL_KEYUP) {
                SDL_Keycode k = ev.key.keysym.sym;
                if (!multiplayerMode) {
                    if ((p1UsesWASD && k == SDLK_a) ||
                        (!p1UsesWASD && k == SDLK_LEFT))
                        keyLeft = 0;
                    if ((p1UsesWASD && k == SDLK_d) ||
                        (!p1UsesWASD && k == SDLK_RIGHT))
                        keyRight = 0;
                    if ((p1UsesWASD && (k == SDLK_w || k == SDLK_SPACE)) ||
                        (!p1UsesWASD && k == SDLK_UP))
                        keyUp = 0;
                } else {
                    int p1Left  = p1UsesWASD ? (k == SDLK_a)                          : (k == SDLK_LEFT);
                    int p2Left  = p1UsesWASD ? (k == SDLK_LEFT)                       : (k == SDLK_a);
                    int p1Right = p1UsesWASD ? (k == SDLK_d)                          : (k == SDLK_RIGHT);
                    int p2Right = p1UsesWASD ? (k == SDLK_RIGHT)                      : (k == SDLK_d);
                    int p1Up    = p1UsesWASD ? (k == SDLK_w || k == SDLK_SPACE)       : (k == SDLK_UP);
                    int p2Up    = p1UsesWASD ? (k == SDLK_UP)                         : (k == SDLK_w || k == SDLK_SPACE);
                    if (p1Left)  { if (p1IsHarry) key2Left  = 0; else keyLeft  = 0; }
                    if (p2Left)  { if (p1IsHarry) keyLeft   = 0; else key2Left  = 0; }
                    if (p1Right) { if (p1IsHarry) key2Right = 0; else keyRight = 0; }
                    if (p2Right) { if (p1IsHarry) keyRight  = 0; else key2Right = 0; }
                    if (p1Up)    { if (p1IsHarry) key2Up    = 0; else keyUp    = 0; }
                    if (p2Up)    { if (p1IsHarry) keyUp     = 0; else key2Up   = 0; }
                }
                if (isLevel2InteractKey(k)) keyE = 0;
            }
        }
        applyOnlineRemoteDuoInput();
        if (!running) break;

        Uint32 now = SDL_GetTicks();
        if (activeSession && activeSession->save_enabled && running &&
            !level2ResultReady &&
            gameState != GS_END_FADE &&
            gameState != GS_GAME_OVER) {
            if (now - lastAutosaveTick >= autosaveIntervalMs) {
                activeSession->level2.completed = 0;
                activeSession->level2.starting_lives = level2StartLives;
                activeSession->level2.player_lives_lost = (level2StartLives > player.lives) ? (level2StartLives - player.lives) : 0;
                activeSession->level2.marv_lives_lost =
                    multiplayerMode ? ((level2StartLives > marv.lives) ? (level2StartLives - marv.lives) : 0) : -1;
                activeSession->level2.multiplayer = multiplayerMode ? 1 : 0;
                activeSession->level2.mini_key_found = miniKeyCollected ? 1 : 0;
                session_autosave_progress(activeSession, 2, player.lives, multiplayerMode ? marv.lives : 0);
                lastAutosaveTick = now;
            }
        }
        if (pauseMenuActive) {
            options_scene_update(1.0f / 60.0f);
            render(now, 0);
            online_client_submit_frame(ren, 2);
            options_scene_render();
            SDL_RenderPresent(ren);
        } else {
            update(now);
            render(now, 1);
        }
        capLevel2FrameRate(frameStartCounter);
    }

    if (pauseMenuReady) options_scene_cleanup();
    closeSDL();
    activeSession = NULL;
    return 0;
}
