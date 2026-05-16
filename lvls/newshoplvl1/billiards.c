#include "billiards.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265358979323846f

static const MathQuestion MATH_QUESTIONS[12] = {
    {"2 + 3 = ?", 5, "Add 2 and 3."},
    {"4 + 1 = ?", 5, "Add the two numbers."},
    {"6 - 2 = ?", 4, "Take 2 away from 6."},
    {"7 - 3 = ?", 4, "Take 3 away from 7."},
    {"3 x 2 = ?", 6, "Multiply 3 by 2."},
    {"4 x 2 = ?", 8, "Multiply 4 by 2."},
    {"8 / 2 = ?", 4, "Split 8 into 2 groups."},
    {"9 / 3 = ?", 3, "Split 9 into 3 groups."},
    {"10 - 6 = ?", 4, "Subtract 6 from 10."},
    {"5 + 4 = ?", 9, "Add 5 and 4."},
    {"12 / 4 = ?", 3, "12 divided by 4."},
    {"2 x 5 = ?", 10, "Multiply 2 by 5."}
};

static const char *POCKET_LABELS[BILLIARDS_POCKET_COUNT] = {
    "top-left",
    "top-middle",
    "top-right",
    "bottom-left",
    "bottom-middle",
    "bottom-right"
};

static float deg_to_rad(float deg) {
    return deg * PI_F / 180.0f;
}

static float vec_dist(Vec2 a, Vec2 b) {
    return hypotf(b.x - a.x, b.y - a.y);
}

static Vec2 vec_normalize(Vec2 v) {
    const float len = hypotf(v.x, v.y);
    if (len <= 0.0001f) {
        return (Vec2){1.0f, 0.0f};
    }
    return (Vec2){v.x / len, v.y / len};
}

static Vec2 vec_rotate(Vec2 v, float angle_rad) {
    const float c = cosf(angle_rad);
    const float s = sinf(angle_rad);
    return (Vec2){
        v.x * c - v.y * s,
        v.x * s + v.y * c
    };
}

static Vec2 vec_lerp(Vec2 a, Vec2 b, float t) {
    return (Vec2){
        lerpf(a.x, b.x, t),
        lerpf(a.y, b.y, t)
    };
}

static Vec2 vec_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

static Vec2 vec_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

static Vec2 vec_scale(Vec2 v, float scale) {
    return (Vec2){v.x * scale, v.y * scale};
}

static float vec_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

static bool point_in_rect(Rect rect, int x, int y) {
    return (
        x >= (int)rect.x &&
        x < (int)(rect.x + rect.w) &&
        y >= (int)rect.y &&
        y < (int)(rect.y + rect.h)
    );
}

static Vec2 clamp_point(Vec2 point, float popup_w, float popup_h, float padding) {
    return (Vec2){
        .x = clampf(point.x, padding, fmaxf(padding, popup_w - padding)),
        .y = clampf(point.y, padding, fmaxf(padding, popup_h - padding))
    };
}

static void draw_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y,
    SDL_Color color,
    bool centered
) {
    if (!font || !text || !text[0]) {
        return;
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst = {x, y, surface->w, surface->h};
    if (centered) {
        dst.x -= surface->w / 2;
        dst.y -= surface->h / 2;
    }

    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void draw_filled_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            }
        }
    }
}

static void session_reset(QuizSession *session) {
    memset(session, 0, sizeof(*session));
    session->total_questions = BILLIARDS_QUIZ_TOTAL_QUESTIONS;
    session->required_wins = BILLIARDS_QUIZ_REQUIRED_WINS;
}

static int pick_math_question(QuizSession *session, MathQuestion *out_question) {
    int available_indices[12];
    int available_count = 0;

    for (int i = 0; i < 12; ++i) {
        if (!session->used_question[i]) {
            available_indices[available_count++] = i;
        }
    }

    if (available_count == 0) {
        memset(session->used_question, 0, sizeof(session->used_question));
        for (int i = 0; i < 12; ++i) {
            available_indices[available_count++] = i;
        }
    }

    const int pick_index = available_indices[rand() % available_count];
    session->used_question[pick_index] = true;
    *out_question = MATH_QUESTIONS[pick_index];
    return pick_index;
}

static int distinct_wrong_answer(int correct) {
    const int offsets[] = {1, -1, 2, -2, 3, -3};
    const int count = (int)(sizeof(offsets) / sizeof(offsets[0]));
    for (int i = 0; i < count; ++i) {
        const int candidate = correct + offsets[i];
        if (candidate > 0 && candidate != correct) {
            return candidate;
        }
    }
    return correct + 1;
}

static void sync_popup_phase(BilliardsState *state, uint32_t now_ms) {
    const uint32_t elapsed = now_ms - state->popup_phase_started_at;

    if (state->popup_phase == POPUP_OPENING && elapsed >= BILLIARDS_POPUP_ANIMATION_DURATION_MS) {
        state->popup_phase = POPUP_OPEN;
        state->popup_phase_started_at = now_ms;
    }

    if (state->popup_phase == POPUP_CLOSING && elapsed >= BILLIARDS_POPUP_ANIMATION_DURATION_MS) {
        state->popup_phase = POPUP_HIDDEN;
        state->popup_phase_started_at = now_ms;
    }
}

bool billiards_visible(const BilliardsState *state) {
    return state->popup_phase != POPUP_HIDDEN;
}

float billiards_popup_progress(BilliardsState *state, uint32_t now_ms) {
    sync_popup_phase(state, now_ms);

    if (state->popup_phase == POPUP_HIDDEN) {
        return 0.0f;
    }
    if (state->popup_phase == POPUP_OPEN) {
        return 1.0f;
    }

    const float elapsed = (float)(now_ms - state->popup_phase_started_at);
    const float progress = clampf(
        elapsed / (float)BILLIARDS_POPUP_ANIMATION_DURATION_MS,
        0.0f,
        1.0f
    );

    if (state->popup_phase == POPUP_CLOSING) {
        return 1.0f - progress;
    }

    return progress;
}

float billiards_get_phase_progress(
    const BilliardsState *state,
    uint32_t now_ms,
    uint32_t duration_ms
) {
    if (!duration_ms) {
        return 1.0f;
    }

    const float elapsed = (float)(now_ms - state->phase_started_at);
    return clampf(elapsed / (float)duration_ms, 0.0f, 1.0f);
}

void billiards_compute_layout(
    BilliardsState *state,
    const MapState *map,
    uint32_t now_ms,
    BilliardsLayout *out_layout
) {
    const float progress = billiards_popup_progress(state, now_ms);

    const Rect popup_world = map->billiards_popup;
    const float screen_x = map->offset_x + popup_world.x;
    const float final_screen_y = map->offset_y + popup_world.y;
    const float start_screen_y = WINDOW_H + fmaxf(36.0f, popup_world.h * 0.12f);
    const float animated_y = lerpf(
        start_screen_y,
        final_screen_y,
        ease_out_back(progress, 1.08f)
    );

    out_layout->popup = (Rect){
        .x = roundf(screen_x),
        .y = roundf(animated_y),
        .w = popup_world.w,
        .h = popup_world.h
    };

    const float close_size = fminf(
        BILLIARDS_POPUP_CLOSE_BUTTON_SIZE,
        fmaxf(28.0f, floorf(fminf(popup_world.w, popup_world.h) * 0.12f))
    );
    const float close_padding = fmaxf(
        BILLIARDS_POPUP_CLOSE_BUTTON_PADDING,
        floorf(close_size * 0.35f)
    );

    out_layout->close_button = (Rect){
        .x = out_layout->popup.x + close_padding,
        .y = out_layout->popup.y + close_padding,
        .w = close_size,
        .h = close_size
    };

    out_layout->card = (Rect){
        .x = out_layout->popup.x + out_layout->popup.w * 0.14f,
        .y = out_layout->popup.y + out_layout->popup.h * 0.018f,
        .w = out_layout->popup.w * 0.72f,
        .h = out_layout->popup.h * 0.215f
    };

    const float gap = 14.0f;
    const float option_w = (out_layout->card.w - gap - 36.0f) * 0.5f;
    const float button_y = out_layout->card.y + out_layout->card.h - 52.0f;

    out_layout->option_buttons[0] = (Rect){
        .x = out_layout->card.x + 18.0f,
        .y = button_y,
        .w = option_w,
        .h = 46.0f
    };
    out_layout->option_buttons[1] = (Rect){
        .x = out_layout->card.x + 18.0f + option_w + gap,
        .y = button_y,
        .w = option_w,
        .h = 46.0f
    };
}

static void build_pockets(BilliardsState *state, const MapState *map) {
    const float popup_w = map->billiards_popup.w;
    const float popup_h = map->billiards_popup.h;
    const float padding = ceilf(state->ball_radius + 8.0f);

    const float ratio_x[BILLIARDS_POCKET_COUNT] = {0.107f, 0.5f, 0.893f, 0.107f, 0.5f, 0.893f};
    const float ratio_y[BILLIARDS_POCKET_COUNT] = {0.132f, 0.104f, 0.132f, 0.868f, 0.896f, 0.868f};

    const int offset_x[BILLIARDS_POCKET_COUNT] = {
        map->config.hole_top_left_x,
        map->config.hole_top_middle_x,
        map->config.hole_top_right_x,
        map->config.hole_bottom_left_x,
        map->config.hole_bottom_middle_x,
        map->config.hole_bottom_right_x
    };

    const int offset_y[BILLIARDS_POCKET_COUNT] = {
        map->config.hole_top_left_y,
        map->config.hole_top_middle_y,
        map->config.hole_top_right_y,
        map->config.hole_bottom_left_y,
        map->config.hole_bottom_middle_y,
        map->config.hole_bottom_right_y
    };

    for (int i = 0; i < BILLIARDS_POCKET_COUNT; ++i) {
        state->pocket_labels[i] = POCKET_LABELS[i];
        state->pockets[i] = clamp_point(
            (Vec2){
                popup_w * ratio_x[i] + (float)offset_x[i],
                popup_h * ratio_y[i] + (float)offset_y[i]
            },
            popup_w,
            popup_h,
            padding
        );
    }
}

static Vec2 ghost_contact_point(Vec2 eight_start, Vec2 target, float ball_radius) {
    const Vec2 direction = vec_normalize((Vec2){target.x - eight_start.x, target.y - eight_start.y});
    return (Vec2){
        eight_start.x - direction.x * ball_radius * 2.0f,
        eight_start.y - direction.y * ball_radius * 2.0f
    };
}

static Vec2 clamp_to_table(const BilliardsState *state, Vec2 point) {
    return (Vec2){
        clampf(point.x, state->table_min_x, state->table_max_x),
        clampf(point.y, state->table_min_y, state->table_max_y)
    };
}

static float distance_to_wall_along_dir(const BilliardsState *state, Vec2 origin, Vec2 dir) {
    float distance = INFINITY;

    if (fabsf(dir.x) > 0.0001f) {
        const float target_x = dir.x > 0.0f ? state->table_max_x : state->table_min_x;
        const float candidate = (target_x - origin.x) / dir.x;
        if (candidate > 0.0f) {
            distance = fminf(distance, candidate);
        }
    }

    if (fabsf(dir.y) > 0.0001f) {
        const float target_y = dir.y > 0.0f ? state->table_max_y : state->table_min_y;
        const float candidate = (target_y - origin.y) / dir.y;
        if (candidate > 0.0f) {
            distance = fminf(distance, candidate);
        }
    }

    if (!isfinite(distance)) {
        return 0.0f;
    }
    return fmaxf(distance, 0.0f);
}

static Vec2 wall_normal_at_point(const BilliardsState *state, Vec2 point) {
    const float epsilon = state->ball_radius * 0.26f + 1.0f;
    Vec2 normal = {0.0f, 0.0f};

    if (point.x <= state->table_min_x + epsilon) {
        normal.x += 1.0f;
    }
    if (point.x >= state->table_max_x - epsilon) {
        normal.x -= 1.0f;
    }
    if (point.y <= state->table_min_y + epsilon) {
        normal.y += 1.0f;
    }
    if (point.y >= state->table_max_y - epsilon) {
        normal.y -= 1.0f;
    }

    if (fabsf(normal.x) > 0.0001f || fabsf(normal.y) > 0.0001f) {
        return vec_normalize(normal);
    }

    {
        const float dist_left = fabsf(point.x - state->table_min_x);
        const float dist_right = fabsf(state->table_max_x - point.x);
        const float dist_top = fabsf(point.y - state->table_min_y);
        const float dist_bottom = fabsf(state->table_max_y - point.y);
        const float smallest = fminf(fminf(dist_left, dist_right), fminf(dist_top, dist_bottom));

        if (smallest == dist_left) return (Vec2){1.0f, 0.0f};
        if (smallest == dist_right) return (Vec2){-1.0f, 0.0f};
        if (smallest == dist_top) return (Vec2){0.0f, 1.0f};
        return (Vec2){0.0f, -1.0f};
    }
}

static void resolve_ball_spawns(BilliardsState *state, const MapState *map) {
    const float popup_w = map->billiards_popup.w;
    const float popup_h = map->billiards_popup.h;
    const float padding = ceilf(state->ball_radius + 8.0f);

    state->white_ball_start = clamp_point(
        (Vec2){(float)map->config.cue_ball_x, (float)map->config.cue_ball_y},
        popup_w,
        popup_h,
        padding
    );

    state->eight_ball_start = clamp_point(
        (Vec2){(float)map->config.eight_ball_x, (float)map->config.eight_ball_y},
        popup_w,
        popup_h,
        padding
    );

    const float min_sep = state->ball_radius * 5.2f;
    if (vec_dist(state->white_ball_start, state->eight_ball_start) < min_sep) {
        Vec2 moved = {
            state->white_ball_start.x + min_sep,
            state->white_ball_start.y
        };
        state->eight_ball_start = clamp_point(moved, popup_w, popup_h, padding);
    }
}

static void make_round(BilliardsState *state, const MapState *map, uint32_t now_ms) {
    if (state->session.completed_questions >= state->session.total_questions) {
        return;
    }

    state->round_ready = true;
    state->round_id += 1;
    state->phase = QUIZ_PHASE_QUESTION;
    state->phase_started_at = now_ms;
    state->selected_option = -1;
    state->did_answer_correctly = false;

    state->ball_diameter = fmaxf(
        26.0f,
        roundf(fminf(map->billiards_popup.w, map->billiards_popup.h) * 0.075f)
    );
    state->ball_radius = state->ball_diameter * 0.5f;
    state->cue_length = fmaxf(150.0f, map->billiards_popup.h * 0.42f);
    state->cue_width = fmaxf(18.0f, map->billiards_popup.w * 0.032f);
    {
        const float table_padding = ceilf(state->ball_radius + 8.0f);
        state->table_min_x = table_padding;
        state->table_max_x = fmaxf(table_padding, map->billiards_popup.w - table_padding);
        state->table_min_y = table_padding;
        state->table_max_y = fmaxf(table_padding, map->billiards_popup.h - table_padding);
    }

    build_pockets(state, map);
    resolve_ball_spawns(state, map);
    state->decor_ball1 = (Vec2){map->billiards_popup.w * 0.19f, map->billiards_popup.h * 0.3f};

    state->target_pocket = rand() % BILLIARDS_POCKET_COUNT;

    const Vec2 target = state->pockets[state->target_pocket];
    Vec2 correct_contact = ghost_contact_point(state->eight_ball_start, target, state->ball_radius);
    correct_contact = clamp_point(
        correct_contact,
        map->billiards_popup.w,
        map->billiards_popup.h,
        ceilf(state->ball_radius + 8.0f)
    );

    const float correct_angle = atan2f(
        correct_contact.y - state->white_ball_start.y,
        correct_contact.x - state->white_ball_start.x
    );

    Vec2 target_dir = vec_normalize((Vec2){target.x - state->eight_ball_start.x, target.y - state->eight_ball_start.y});
    Vec2 wrong_dir = vec_rotate(target_dir, deg_to_rad(22.0f));
    Vec2 wrong_contact = {
        state->eight_ball_start.x - wrong_dir.x * state->ball_radius * 2.0f,
        state->eight_ball_start.y - wrong_dir.y * state->ball_radius * 2.0f
    };
    wrong_contact = clamp_point(
        wrong_contact,
        map->billiards_popup.w,
        map->billiards_popup.h,
        ceilf(state->ball_radius + 8.0f)
    );

    Vec2 wrong_end = state->eight_ball_start;
    {
        float shot_distance = distance_to_wall_along_dir(state, state->eight_ball_start, wrong_dir);
        shot_distance = fmaxf(shot_distance - 1.0f, state->ball_radius * 2.0f);
        wrong_end = vec_add(state->eight_ball_start, vec_scale(wrong_dir, shot_distance));
        wrong_end = clamp_to_table(state, wrong_end);
    }

    const float wrong_angle = atan2f(
        wrong_contact.y - state->white_ball_start.y,
        wrong_contact.x - state->white_ball_start.x
    );

    MathQuestion question = MATH_QUESTIONS[0];
    pick_math_question(&state->session, &question);
    const int wrong_answer = distinct_wrong_answer(question.answer);

    const int correct_index = rand() % 2;
    const int wrong_index = correct_index == 0 ? 1 : 0;
    state->correct_option = correct_index;

    state->options[0] = (QuizOption){.id = 'a'};
    state->options[1] = (QuizOption){.id = 'b'};

    const float guide_length = map->billiards_popup.w * 0.3f;

    state->options[correct_index].angle_rad = correct_angle;
    state->options[correct_index].contact_point = correct_contact;
    state->options[correct_index].eight_end = target;
    state->options[correct_index].guide_end = (Vec2){
        state->white_ball_start.x + cosf(correct_angle) * guide_length,
        state->white_ball_start.y + sinf(correct_angle) * guide_length
    };
    snprintf(state->options[correct_index].label, sizeof(state->options[correct_index].label), "%d", question.answer);
    snprintf(state->options[correct_index].detail, sizeof(state->options[correct_index].detail), "%s", "Solve the math");

    state->options[wrong_index].angle_rad = wrong_angle;
    state->options[wrong_index].contact_point = wrong_contact;
    state->options[wrong_index].eight_end = wrong_end;
    state->options[wrong_index].guide_end = (Vec2){
        state->white_ball_start.x + cosf(wrong_angle) * guide_length,
        state->white_ball_start.y + sinf(wrong_angle) * guide_length
    };
    snprintf(state->options[wrong_index].label, sizeof(state->options[wrong_index].label), "%d", wrong_answer);
    snprintf(state->options[wrong_index].detail, sizeof(state->options[wrong_index].detail), "%s", "Solve the math");

    snprintf(
        state->math_prompt,
        sizeof(state->math_prompt),
        "Pocket %s: %s",
        state->pocket_labels[state->target_pocket],
        question.prompt
    );
    snprintf(state->math_hint, sizeof(state->math_hint), "%s", question.hint);
}

static void record_round_result(BilliardsState *state) {
    state->session.completed_questions += 1;
    if (state->session.completed_questions > state->session.total_questions) {
        state->session.completed_questions = state->session.total_questions;
    }

    if (state->did_answer_correctly) {
        state->session.wins += 1;
        if (state->session.wins > state->session.total_questions) {
            state->session.wins = state->session.total_questions;
        }
    }

    if (state->session.completed_questions >= state->session.total_questions) {
        state->session.is_complete = true;
        state->session.did_pass = state->session.wins >= state->session.required_wins;
    }
}

static void sync_quiz_phase(BilliardsState *state, const MapState *map, uint32_t now_ms) {
    if (!state->round_ready) {
        return;
    }

    bool keep_syncing = true;

    while (keep_syncing) {
        keep_syncing = false;
        const uint32_t elapsed = now_ms - state->phase_started_at;

        if (state->phase == QUIZ_PHASE_CUE && elapsed >= BILLIARDS_QUIZ_CUE_DURATION_MS) {
            state->phase = QUIZ_PHASE_WHITE_BALL;
            state->phase_started_at += BILLIARDS_QUIZ_CUE_DURATION_MS;
            keep_syncing = true;
            continue;
        }

        if (state->phase == QUIZ_PHASE_WHITE_BALL && elapsed >= BILLIARDS_QUIZ_WHITE_BALL_DURATION_MS) {
            state->phase = QUIZ_PHASE_EIGHT_BALL;
            state->phase_started_at += BILLIARDS_QUIZ_WHITE_BALL_DURATION_MS;
            keep_syncing = true;
            continue;
        }

        if (state->phase == QUIZ_PHASE_EIGHT_BALL && elapsed >= BILLIARDS_QUIZ_EIGHT_BALL_DURATION_MS) {
            state->phase = QUIZ_PHASE_RESULT;
            state->phase_started_at += BILLIARDS_QUIZ_EIGHT_BALL_DURATION_MS;
            record_round_result(state);
            keep_syncing = true;
            continue;
        }

        if (state->phase == QUIZ_PHASE_RESULT && elapsed >= BILLIARDS_QUIZ_RESULT_DURATION_MS) {
            if (state->session.is_complete) {
                billiards_close(state, now_ms);
                return;
            }
            make_round(state, map, now_ms);
        }
    }
}

static void submit_answer(BilliardsState *state, int option_index, uint32_t now_ms) {
    if (state->phase != QUIZ_PHASE_QUESTION) {
        return;
    }
    if (option_index < 0 || option_index > 1) {
        return;
    }

    state->selected_option = option_index;
    state->did_answer_correctly = option_index == state->correct_option;
    state->phase = QUIZ_PHASE_CUE;
    state->phase_started_at = now_ms;
}

void billiards_init(BilliardsState *state) {
    memset(state, 0, sizeof(*state));
    state->popup_phase = POPUP_HIDDEN;
    state->selected_option = -1;
    session_reset(&state->session);
}

void billiards_open(BilliardsState *state, const MapState *map, uint32_t now_ms) {
    session_reset(&state->session);
    state->round_ready = false;
    state->round_id = 0;
    make_round(state, map, now_ms);

    state->popup_phase = POPUP_OPENING;
    state->popup_phase_started_at = now_ms;
}

void billiards_close(BilliardsState *state, uint32_t now_ms) {
    if (state->popup_phase == POPUP_HIDDEN || state->popup_phase == POPUP_CLOSING) {
        return;
    }

    state->popup_phase = POPUP_CLOSING;
    state->popup_phase_started_at = now_ms;
}

void billiards_update(
    BilliardsState *state,
    const MapState *map,
    const InputState *input,
    uint32_t now_ms
) {
    sync_popup_phase(state, now_ms);

    if (!billiards_visible(state)) {
        return;
    }

    if (!state->round_ready && (state->popup_phase == POPUP_OPENING || state->popup_phase == POPUP_OPEN)) {
        make_round(state, map, now_ms);
    }

    BilliardsLayout layout;
    billiards_compute_layout(state, map, now_ms, &layout);

    if (input->mouse_clicked) {
        if (point_in_rect(layout.close_button, input->mouse_x, input->mouse_y)) {
            billiards_close(state, now_ms);
            return;
        }

        if (state->phase == QUIZ_PHASE_QUESTION) {
            for (int i = 0; i < 2; ++i) {
                if (point_in_rect(layout.option_buttons[i], input->mouse_x, input->mouse_y)) {
                    submit_answer(state, i, now_ms);
                    break;
                }
            }
        }
    }

    if (input->just_pressed[SDL_SCANCODE_A]) {
        submit_answer(state, 0, now_ms);
    }
    if (input->just_pressed[SDL_SCANCODE_B]) {
        submit_answer(state, 1, now_ms);
    }

    sync_quiz_phase(state, map, now_ms);
}

static void compute_visual_state(
    const BilliardsState *state,
    uint32_t now_ms,
    Vec2 *cue_ball,
    Vec2 *eight_ball,
    float *cue_angle,
    float *cue_distance,
    bool *show_cue,
    float *eight_scale,
    float *eight_alpha
) {
    static const float WRONG_SHOT_BOUNCE_START = 0.58f;
    static const float CUE_POST_HIT_ROLL_END = 0.88f;
    const float base_cue_distance = state->ball_radius * 4.6f;
    const int picked = state->selected_option >= 0 ? state->selected_option : state->correct_option;

    *cue_ball = state->white_ball_start;
    *eight_ball = state->eight_ball_start;
    *cue_angle = state->options[picked].angle_rad;
    *cue_distance = base_cue_distance;
    *show_cue = true;
    *eight_scale = 1.0f;
    *eight_alpha = 1.0f;

    if (state->phase == QUIZ_PHASE_CUE && state->selected_option >= 0) {
        const float progress = billiards_get_phase_progress(state, now_ms, BILLIARDS_QUIZ_CUE_DURATION_MS);
        const float swing =
            progress < 0.45f
                ? lerpf(0.0f, state->ball_radius * 0.95f, progress / 0.45f)
                : lerpf(state->ball_radius * 0.95f, -state->ball_radius * 0.35f, (progress - 0.45f) / 0.55f);
        *cue_distance = base_cue_distance + swing;
        return;
    }

    if (state->phase == QUIZ_PHASE_WHITE_BALL && state->selected_option >= 0) {
        *show_cue = false;
        const float progress = ease_in_out_cubic(
            billiards_get_phase_progress(state, now_ms, BILLIARDS_QUIZ_WHITE_BALL_DURATION_MS)
        );
        *cue_ball = vec_lerp(state->white_ball_start, state->options[state->selected_option].contact_point, progress);
        *cue_distance = base_cue_distance - state->ball_radius * 0.4f;
        return;
    }

    if (state->phase == QUIZ_PHASE_EIGHT_BALL && state->selected_option >= 0) {
        const QuizOption *selected = &state->options[state->selected_option];
        *show_cue = false;
        const float progress = billiards_get_phase_progress(state, now_ms, BILLIARDS_QUIZ_EIGHT_BALL_DURATION_MS);
        const Vec2 shot_dir = vec_normalize(vec_sub(selected->contact_point, state->white_ball_start));
        const float cue_roll_distance = state->ball_radius * 4.2f;
        const float roll_t = clampf(progress / CUE_POST_HIT_ROLL_END, 0.0f, 1.0f);
        const float roll_decay = (1.0f - expf(-4.2f * roll_t)) / (1.0f - expf(-4.2f));
        *cue_ball = clamp_to_table(
            state,
            vec_add(selected->contact_point, vec_scale(shot_dir, cue_roll_distance * roll_decay))
        );

        *cue_distance = base_cue_distance - state->ball_radius * 0.4f;

        if (state->did_answer_correctly) {
            const float ball_progress = fminf(progress / 0.72f, 1.0f);
            *eight_ball = vec_lerp(
                state->eight_ball_start,
                selected->eight_end,
                ease_in_out_cubic(ball_progress)
            );

            if (progress >= 0.72f) {
                const float fade = ease_out_cubic((progress - 0.72f) / 0.28f);
                *eight_ball = selected->eight_end;
                *eight_scale = lerpf(1.0f, 0.24f, fade);
                *eight_alpha = lerpf(1.0f, 0.0f, fade);
            }
            return;
        }

        {
            const Vec2 wall_hit = selected->eight_end;
            const Vec2 incoming = vec_normalize(vec_sub(wall_hit, state->eight_ball_start));
            const Vec2 normal = wall_normal_at_point(state, wall_hit);
            const float reflect_amount = 2.0f * vec_dot(incoming, normal);
            Vec2 reflected = vec_sub(incoming, vec_scale(normal, reflect_amount));
            const float reflected_len = hypotf(reflected.x, reflected.y);
            const float first_leg_distance = fmaxf(
                vec_dist(state->eight_ball_start, wall_hit),
                state->ball_radius * 4.8f
            );
            const float rebound_distance = fmaxf(
                state->ball_radius * 2.2f,
                first_leg_distance * 0.34f
            );

            if (reflected_len <= 0.0001f) {
                reflected = vec_scale(incoming, -1.0f);
            } else {
                reflected = vec_scale(reflected, 1.0f / reflected_len);
            }

            const Vec2 rebound_end = clamp_to_table(
                state,
                vec_add(wall_hit, vec_scale(reflected, rebound_distance))
            );

            if (progress <= WRONG_SHOT_BOUNCE_START) {
                const float leg_t = ease_in_out_cubic(progress / WRONG_SHOT_BOUNCE_START);
                *eight_ball = vec_lerp(state->eight_ball_start, wall_hit, leg_t);
            } else {
                const float rebound_t = clampf(
                    (progress - WRONG_SHOT_BOUNCE_START) / (1.0f - WRONG_SHOT_BOUNCE_START),
                    0.0f,
                    1.0f
                );
                const float damping = (1.0f - expf(-3.4f * rebound_t)) / (1.0f - expf(-3.4f));
                *eight_ball = vec_lerp(wall_hit, rebound_end, damping);
            }
        }
        return;
    }

    if (state->phase == QUIZ_PHASE_RESULT && state->selected_option >= 0) {
        const QuizOption *selected = &state->options[state->selected_option];
        const Vec2 shot_dir = vec_normalize(vec_sub(selected->contact_point, state->white_ball_start));
        const float cue_roll_distance = state->ball_radius * 4.2f;
        *show_cue = false;
        *cue_ball = clamp_to_table(
            state,
            vec_add(selected->contact_point, vec_scale(shot_dir, cue_roll_distance))
        );
        *cue_distance = base_cue_distance - state->ball_radius * 0.4f;

        if (state->did_answer_correctly) {
            *eight_ball = selected->eight_end;
            *eight_scale = 0.24f;
            *eight_alpha = 0.0f;
        } else {
            const Vec2 wall_hit = selected->eight_end;
            const Vec2 incoming = vec_normalize(vec_sub(wall_hit, state->eight_ball_start));
            const Vec2 normal = wall_normal_at_point(state, wall_hit);
            const float reflect_amount = 2.0f * vec_dot(incoming, normal);
            Vec2 reflected = vec_sub(incoming, vec_scale(normal, reflect_amount));
            const float reflected_len = hypotf(reflected.x, reflected.y);
            const float first_leg_distance = fmaxf(
                vec_dist(state->eight_ball_start, wall_hit),
                state->ball_radius * 4.8f
            );
            const float rebound_distance = fmaxf(
                state->ball_radius * 2.2f,
                first_leg_distance * 0.34f
            );

            if (reflected_len <= 0.0001f) {
                reflected = vec_scale(incoming, -1.0f);
            } else {
                reflected = vec_scale(reflected, 1.0f / reflected_len);
            }

            *eight_ball = clamp_to_table(
                state,
                vec_add(wall_hit, vec_scale(reflected, rebound_distance))
            );
        }
    }
}

static void draw_ball(
    SDL_Renderer *renderer,
    const TextureAsset *texture,
    Vec2 center,
    float diameter,
    float scale,
    SDL_Color fallback,
    float alpha
) {
    const float size = diameter * scale;
    if (size <= 0.0f || alpha <= 0.0f) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect dst = {
        .x = (int)lroundf(center.x - size * 0.5f),
        .y = (int)lroundf(center.y - size * 0.5f),
        .w = (int)lroundf(size),
        .h = (int)lroundf(size)
    };

    if (texture && texture->loaded) {
        SDL_SetTextureAlphaMod(texture->texture, (Uint8)lroundf(alpha * 255.0f));
        SDL_RenderCopy(renderer, texture->texture, NULL, &dst);
        SDL_SetTextureAlphaMod(texture->texture, 255);
        return;
    }

    SDL_SetRenderDrawColor(
        renderer,
        fallback.r,
        fallback.g,
        fallback.b,
        (Uint8)lroundf(alpha * 255.0f)
    );
    draw_filled_circle(renderer, (int)lroundf(center.x), (int)lroundf(center.y), (int)lroundf(size * 0.5f));
}

static const char *question_text(const BilliardsState *state) {
    static char text[128];

    if (state->phase == QUIZ_PHASE_RESULT) {
        if (state->session.is_complete) {
            snprintf(
                text,
                sizeof(text),
                "%s: %d/%d correct.",
                state->session.did_pass ? "Run clear" : "Run over",
                state->session.wins,
                state->session.total_questions
            );
            return text;
        }

        return state->did_answer_correctly
            ? "Correct shot. The 8-ball drops in."
            : "Wrong shot. The 8-ball misses.";
    }

    if (state->phase != QUIZ_PHASE_QUESTION && state->selected_option >= 0) {
        snprintf(
            text,
            sizeof(text),
            "Locked in: %c - %s",
            state->options[state->selected_option].id,
            state->options[state->selected_option].label
        );
        return text;
    }

    return state->math_prompt;
}

static const char *question_subtext(const BilliardsState *state) {
    static char text[160];

    int current_question = state->session.completed_questions + 1;
    if (state->phase == QUIZ_PHASE_RESULT) {
        current_question = state->session.completed_questions;
    }
    if (current_question < 1) {
        current_question = 1;
    }
    if (current_question > state->session.total_questions) {
        current_question = state->session.total_questions;
    }

    if (state->phase == QUIZ_PHASE_QUESTION) {
        snprintf(
            text,
            sizeof(text),
            "Question %d/%d. Need %d wins. %s",
            current_question,
            state->session.total_questions,
            state->session.required_wins,
            state->math_hint
        );
        return text;
    }

    if (state->phase == QUIZ_PHASE_RESULT) {
        if (state->session.is_complete) {
            snprintf(
                text,
                sizeof(text),
                "%s",
                state->session.did_pass
                    ? "You reached the win goal. Closing popup."
                    : "Not enough wins. Closing popup."
            );
            return text;
        }

        snprintf(
            text,
            sizeof(text),
            "Score: %d/%d. Next question loads automatically.",
            state->session.wins,
            state->session.completed_questions
        );
        return text;
    }

    snprintf(
        text,
        sizeof(text),
        "Question %d/%d. Current wins: %d.",
        current_question,
        state->session.total_questions,
        state->session.wins
    );
    return text;
}

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
) {
    if (!billiards_visible(state)) {
        return;
    }

    const float progress = billiards_popup_progress(state, now_ms);
    if (progress <= 0.0f) {
        return;
    }

    const float opacity = ease_out_cubic(progress);

    BilliardsLayout layout;
    billiards_compute_layout(state, map, now_ms, &layout);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x07, 0x0a, 0x12, (Uint8)lroundf(opacity * BILLIARDS_POPUP_OVERLAY_ALPHA * 255.0f));
    SDL_Rect full = {0, 0, WINDOW_W, WINDOW_H};
    SDL_RenderFillRect(renderer, &full);

    SDL_Rect popup_rect = {
        (int)layout.popup.x,
        (int)layout.popup.y,
        (int)layout.popup.w,
        (int)layout.popup.h
    };

    if (popup_texture && popup_texture->loaded) {
        SDL_SetTextureAlphaMod(popup_texture->texture, (Uint8)lroundf(opacity * 255.0f));
        SDL_RenderCopy(renderer, popup_texture->texture, NULL, &popup_rect);
        SDL_SetTextureAlphaMod(popup_texture->texture, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 0x2f, 0x6b, 0x3f, (Uint8)lroundf(opacity * 255.0f));
        SDL_RenderFillRect(renderer, &popup_rect);
    }

    Vec2 cue_ball = state->white_ball_start;
    Vec2 eight_ball = state->eight_ball_start;
    float cue_angle = 0.0f;
    float cue_distance = state->ball_radius * 4.6f;
    bool show_cue = true;
    float eight_scale = 1.0f;
    float eight_alpha = 1.0f;

    compute_visual_state(
        state,
        now_ms,
        &cue_ball,
        &eight_ball,
        &cue_angle,
        &cue_distance,
        &show_cue,
        &eight_scale,
        &eight_alpha
    );

    Vec2 screen_pockets[BILLIARDS_POCKET_COUNT];
    for (int i = 0; i < BILLIARDS_POCKET_COUNT; ++i) {
        screen_pockets[i] = (Vec2){layout.popup.x + state->pockets[i].x, layout.popup.y + state->pockets[i].y};
    }

    for (int i = 0; i < BILLIARDS_POCKET_COUNT; ++i) {
        const float radius = i == state->target_pocket ? state->ball_radius * 0.98f : state->ball_radius * 0.82f;
        SDL_SetRenderDrawColor(renderer, 10, 14, 18, i == state->target_pocket ? 220 : 170);
        draw_filled_circle(
            renderer,
            (int)lroundf(screen_pockets[i].x),
            (int)lroundf(screen_pockets[i].y),
            (int)lroundf(radius)
        );
    }

    const float pulse = 0.72f + sinf((float)now_ms / 180.0f) * 0.12f;
    const Vec2 target_screen = screen_pockets[state->target_pocket];

    SDL_SetRenderDrawColor(renderer, 255, 228, 110, (Uint8)lroundf(opacity * pulse * 80.0f));
    draw_filled_circle(
        renderer,
        (int)lroundf(target_screen.x),
        (int)lroundf(target_screen.y),
        (int)lroundf(state->ball_radius * 1.55f)
    );

    if (state->phase == QUIZ_PHASE_QUESTION) {
        SDL_SetRenderDrawColor(renderer, 247, 212, 99, 220);
        SDL_RenderDrawLine(
            renderer,
            (int)lroundf(layout.popup.x + state->white_ball_start.x),
            (int)lroundf(layout.popup.y + state->white_ball_start.y),
            (int)lroundf(layout.popup.x + state->options[0].guide_end.x),
            (int)lroundf(layout.popup.y + state->options[0].guide_end.y)
        );

        SDL_SetRenderDrawColor(renderer, 132, 226, 255, 220);
        SDL_RenderDrawLine(
            renderer,
            (int)lroundf(layout.popup.x + state->white_ball_start.x),
            (int)lroundf(layout.popup.y + state->white_ball_start.y),
            (int)lroundf(layout.popup.x + state->options[1].guide_end.x),
            (int)lroundf(layout.popup.y + state->options[1].guide_end.y)
        );
    }

    draw_ball(
        renderer,
        ball1_texture,
        (Vec2){layout.popup.x + state->decor_ball1.x, layout.popup.y + state->decor_ball1.y},
        state->ball_diameter,
        1.0f,
        (SDL_Color){241, 197, 59, 255},
        1.0f
    );

    if (show_cue) {
        Vec2 cue_center = {
            layout.popup.x + cue_ball.x - cosf(cue_angle) * cue_distance,
            layout.popup.y + cue_ball.y - sinf(cue_angle) * cue_distance
        };

        SDL_Rect cue_dst = {
            .x = (int)lroundf(cue_center.x - state->cue_width * 0.5f),
            .y = (int)lroundf(cue_center.y - state->cue_length * 0.5f),
            .w = (int)lroundf(state->cue_width),
            .h = (int)lroundf(state->cue_length)
        };

        if (cue_texture && cue_texture->loaded) {
            SDL_RenderCopyEx(
                renderer,
                cue_texture->texture,
                NULL,
                &cue_dst,
                (cue_angle * 180.0f / PI_F) + 90.0f,
                NULL,
                SDL_FLIP_NONE
            );
        } else {
            SDL_SetRenderDrawColor(renderer, 210, 176, 123, 255);
            SDL_RenderFillRect(renderer, &cue_dst);
        }
    }

    draw_ball(
        renderer,
        white_ball_texture,
        (Vec2){layout.popup.x + cue_ball.x, layout.popup.y + cue_ball.y},
        state->ball_diameter,
        1.0f,
        (SDL_Color){245, 245, 245, 255},
        1.0f
    );

    draw_ball(
        renderer,
        eight_ball_texture,
        (Vec2){layout.popup.x + eight_ball.x, layout.popup.y + eight_ball.y},
        state->ball_diameter,
        eight_scale,
        (SDL_Color){31, 31, 37, 255},
        eight_alpha
    );

    if (state->phase == QUIZ_PHASE_RESULT && state->did_answer_correctly && correct_overlay_texture && correct_overlay_texture->loaded) {
        SDL_SetTextureAlphaMod(correct_overlay_texture->texture, (Uint8)lroundf(opacity * 220.0f));
        SDL_RenderCopy(renderer, correct_overlay_texture->texture, NULL, &popup_rect);
        SDL_SetTextureAlphaMod(correct_overlay_texture->texture, 255);
    }

    SDL_Color card_color = {7, 12, 20, 220};
    if (state->phase == QUIZ_PHASE_RESULT && state->did_answer_correctly) {
        card_color = (SDL_Color){8, 43, 28, 225};
    } else if (state->phase == QUIZ_PHASE_RESULT && !state->did_answer_correctly) {
        card_color = (SDL_Color){56, 18, 18, 230};
    }

    SDL_Rect card_rect = {
        (int)layout.card.x,
        (int)layout.card.y,
        (int)layout.card.w,
        (int)layout.card.h
    };

    SDL_SetRenderDrawColor(renderer, card_color.r, card_color.g, card_color.b, card_color.a);
    SDL_RenderFillRect(renderer, &card_rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 46);
    SDL_RenderDrawRect(renderer, &card_rect);

    draw_text(
        renderer,
        font,
        question_text(state),
        (int)layout.card.x + 18,
        (int)layout.card.y + 12,
        (SDL_Color){246, 241, 221, 255},
        false
    );

    draw_text(
        renderer,
        font,
        question_subtext(state),
        (int)layout.card.x + 18,
        (int)layout.card.y + (int)layout.card.h - 42,
        (SDL_Color){199, 211, 223, 255},
        false
    );

    for (int i = 0; i < 2; ++i) {
        SDL_Color fill = i == 0
            ? (SDL_Color){34, 42, 56, 240}
            : (SDL_Color){25, 46, 58, 240};

        const bool is_selected = state->selected_option == i;
        const bool is_correct = state->correct_option == i;

        if (state->phase != QUIZ_PHASE_QUESTION) {
            if (is_correct) {
                fill = (SDL_Color){101, 202, 145, 245};
            } else if (is_selected) {
                fill = (SDL_Color){235, 108, 108, 245};
            }
        } else if (is_selected) {
            fill = (SDL_Color){245, 215, 113, 245};
        }

        SDL_Rect option_rect = {
            (int)layout.option_buttons[i].x,
            (int)layout.option_buttons[i].y,
            (int)layout.option_buttons[i].w,
            (int)layout.option_buttons[i].h
        };

        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &option_rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
        SDL_RenderDrawRect(renderer, &option_rect);

        char option_letter[2] = {state->options[i].id, '\0'};
        draw_text(
            renderer,
            font,
            option_letter,
            option_rect.x + 16,
            option_rect.y + 13,
            (SDL_Color){16, 33, 49, 255},
            false
        );

        draw_text(
            renderer,
            font,
            state->options[i].label,
            option_rect.x + 48,
            option_rect.y + 11,
            (SDL_Color){246, 241, 221, 255},
            false
        );
    }

    SDL_Rect close_rect = {
        (int)layout.close_button.x,
        (int)layout.close_button.y,
        (int)layout.close_button.w,
        (int)layout.close_button.h
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 240);
    SDL_RenderFillRect(renderer, &close_rect);
    SDL_SetRenderDrawColor(renderer, 22, 28, 39, 180);
    SDL_RenderDrawRect(renderer, &close_rect);

    if (close_texture && close_texture->loaded) {
        SDL_RenderCopy(renderer, close_texture->texture, NULL, &close_rect);
    } else {
        SDL_SetRenderDrawColor(renderer, 28, 36, 48, 255);
        SDL_RenderDrawLine(renderer, close_rect.x + 8, close_rect.y + 8, close_rect.x + close_rect.w - 8, close_rect.y + close_rect.h - 8);
        SDL_RenderDrawLine(renderer, close_rect.x + close_rect.w - 8, close_rect.y + 8, close_rect.x + 8, close_rect.y + close_rect.h - 8);
    }
}
