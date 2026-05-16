#ifndef SDLVERSIONSHOP_BILLIARDS_H
#define SDLVERSIONSHOP_BILLIARDS_H

#include "game.h"
#include "input.h"
#include "map.h"

#include <stdbool.h>

#define BILLIARDS_POCKET_COUNT 6

typedef struct {
    char prompt[32];
    int answer;
    char hint[48];
} MathQuestion;

typedef struct {
    char id;
    char label[16];
    char detail[24];
    float angle_rad;
    Vec2 contact_point;
    Vec2 eight_end;
    Vec2 guide_end;
} QuizOption;

typedef struct {
    int total_questions;
    int required_wins;
    int completed_questions;
    int wins;
    bool is_complete;
    bool did_pass;
    bool used_question[12];
} QuizSession;

typedef struct BilliardsState {
    PopupPhase popup_phase;
    uint32_t popup_phase_started_at;

    bool round_ready;
    int round_id;

    QuizPhase phase;
    uint32_t phase_started_at;

    float ball_radius;
    float ball_diameter;
    float cue_length;
    float cue_width;
    float table_min_x;
    float table_max_x;
    float table_min_y;
    float table_max_y;

    Vec2 pockets[BILLIARDS_POCKET_COUNT];
    const char *pocket_labels[BILLIARDS_POCKET_COUNT];
    int target_pocket;

    Vec2 white_ball_start;
    Vec2 eight_ball_start;
    Vec2 decor_ball1;

    QuizOption options[2];
    int correct_option;
    int selected_option;
    bool did_answer_correctly;

    char math_prompt[96];
    char math_hint[64];

    QuizSession session;
} BilliardsState;

typedef struct {
    Rect popup;
    Rect close_button;
    Rect option_buttons[2];
    Rect card;
} BilliardsLayout;

void billiards_init(BilliardsState *state);
void billiards_open(BilliardsState *state, const MapState *map, uint32_t now_ms);
void billiards_close(BilliardsState *state, uint32_t now_ms);

bool billiards_visible(const BilliardsState *state);
float billiards_popup_progress(BilliardsState *state, uint32_t now_ms);

void billiards_update(
    BilliardsState *state,
    const MapState *map,
    const InputState *input,
    uint32_t now_ms
);

void billiards_render(
    BilliardsState *state,
    const MapState *map,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const TextureAsset *popup_texture,
    const TextureAsset *close_texture,
    const TextureAsset *cue_texture,
    const TextureAsset *white_ball_texture,
    const TextureAsset *eight_ball_texture,
    const TextureAsset *ball1_texture,
    const TextureAsset *correct_overlay_texture,
    uint32_t now_ms
);

void billiards_compute_layout(
    BilliardsState *state,
    const MapState *map,
    uint32_t now_ms,
    BilliardsLayout *out_layout
);

float billiards_get_phase_progress(
    const BilliardsState *state,
    uint32_t now_ms,
    uint32_t duration_ms
);

#endif
