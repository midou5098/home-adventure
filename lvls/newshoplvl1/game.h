#ifndef SDLVERSIONSHOP_GAME_H
#define SDLVERSIONSHOP_GAME_H

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

#define WINDOW_W 800
#define WINDOW_H 600
#define TARGET_FPS 60

#define TILE_SIZE 32
#define FLOOR_TILE_SIZE 16
#define PLAYER_W 24
#define PLAYER_H 24
#define PLAYER_SPEED 180.0f
#define PLAYER_SPRITE_DRAW_HEIGHT 56
#define MAX_DELTA_SECONDS 0.05f

#define WALL_COVER_HEIGHT 56
#define MAX_DECOR_TABLES 12

#define BILLIARDS_POPUP_CLOSE_BUTTON_SIZE 42
#define BILLIARDS_POPUP_CLOSE_BUTTON_PADDING 26
#define BILLIARDS_POPUP_ANIMATION_DURATION_MS 520
#define BILLIARDS_POPUP_OVERLAY_ALPHA 0.68f

#define BILLIARDS_QUIZ_CUE_DURATION_MS 260
#define BILLIARDS_QUIZ_WHITE_BALL_DURATION_MS 360
#define BILLIARDS_QUIZ_EIGHT_BALL_DURATION_MS 1200
#define BILLIARDS_QUIZ_RESULT_DURATION_MS 420
#define BILLIARDS_QUIZ_TOTAL_QUESTIONS 3
#define BILLIARDS_QUIZ_REQUIRED_WINS 2

typedef struct {
    float x;
    float y;
    float w;
    float h;
} Rect;

typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;
} Seg;

typedef struct {
    float min;
    float max;
} Range;

typedef struct {
    float x;
    float y;
} Vec2;

typedef enum {
    MAP_SHAPE_RECT = 0,
    MAP_SHAPE_L,
    MAP_SHAPE_T,
    MAP_SHAPE_U
} MapShape;

typedef enum {
    FACING_DOWN = 0,
    FACING_UP,
    FACING_LEFT,
    FACING_RIGHT
} Facing;

typedef enum {
    TARGET_NONE = 0,
    TARGET_BILLIARDS,
    TARGET_BARISTA,
    TARGET_ARCADE,
    TARGET_EXIT_DOOR
} InteractionTarget;

typedef enum {
    POPUP_HIDDEN = 0,
    POPUP_OPENING,
    POPUP_OPEN,
    POPUP_CLOSING
} PopupPhase;

typedef enum {
    QUIZ_PHASE_QUESTION = 0,
    QUIZ_PHASE_CUE,
    QUIZ_PHASE_WHITE_BALL,
    QUIZ_PHASE_EIGHT_BALL,
    QUIZ_PHASE_RESULT
} QuizPhase;

typedef struct {
    SDL_Texture *texture;
    int w;
    int h;
    bool loaded;
} TextureAsset;

typedef struct {
    TextureAsset base;
    int cols;
    int rows;
    int frame_count;
    int frame_w;
    int frame_h;
} SpriteSheet;

float clampf(float value, float min_value, float max_value);
float lerpf(float a, float b, float t);
float ease_out_cubic(float value);
float ease_in_out_cubic(float value);
float ease_out_back(float value, float overshoot);

bool rect_overlaps(Rect a, Rect b);
bool rect_touch_or_overlap(Rect a, Rect b);

int game_run(void);

#endif
