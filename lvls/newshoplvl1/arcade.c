#include "arcade.h"

#include <SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ARCADE_MENU_COUNT ARCADE_RESULT_COUNT
#define TETRIS_COLS 10
#define TETRIS_ROWS 20
#define TETRIS_FALL_MS 500u
#define TETRIS_PIECE_LIMIT 20
#define TETRIS_WIN_SCORE 2000u
#define MINES_SIZE 10
#define MINES_COUNT 15
#define SPACE_COLS 6
#define SPACE_ROWS 3
#define SPACE_ENEMY_COUNT (SPACE_COLS * SPACE_ROWS)
#define MAZE_MAP_WIDTH 16
#define MAZE_MAP_HEIGHT 16
#define MAZE_MOVE_SPEED 3.0f
#define MAZE_TURN_SPEED 2.2f
#define MAZE_MAX_DT 0.05f
#define MAZE_PLAYER_RADIUS 0.18f
#define MAZE_TILE_FINISH 2
#define MAZE_TILE_SPAWN 3
#define MAZE_TILE_FAN 4
#define MAZE_TILE_SWALL 5
#define MAZE_FINISH_TRIGGER_DISTANCE 0.30f
#define MAZE_FAN_FRAME_TIME_MS 110
#define MAZE_FAN_LAYOUT_MAX_DIVISOR 12
#define MAZE_FAN_OVERLAY_WIDTH_FRACTION 0.55f
#define MAZE_FAN_OVERLAY_HEIGHT_FRACTION 0.62f
#define MAZE_PI2 6.28318530718f
#define PUZZLE_ROUNDS_TO_PLAY 5
#define PUZZLE_REQUIRED_SCORE 3
#define PUZZLE_ROUND_TIME_MS 12000u
#define PUZZLE_FEEDBACK_MS 700u
#define PUZZLE_MIN_PHOTO_CHOICES 3
#define PUZZLE_PERMIL 1000
#define PUZZLE_LOGICAL_W 1280.0f
#define PUZZLE_LOGICAL_H 720.0f

typedef struct {
    int x;
    int y;
} CellPoint;

static const CellPoint TETRIS_SHAPES[7][4][4] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}, {{2, 0}, {2, 1}, {2, 2}, {2, 3}}, {{0, 2}, {1, 2}, {2, 2}, {3, 2}}, {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {1, 1}, {1, 2}}, {{0, 1}, {1, 1}, {2, 1}, {2, 2}}, {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {1, 1}, {1, 2}, {2, 2}}, {{0, 1}, {1, 1}, {2, 1}, {0, 2}}, {{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {1, 1}, {2, 1}}, {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}, {{1, 0}, {1, 1}, {2, 1}, {2, 2}}, {{1, 1}, {2, 1}, {0, 2}, {1, 2}}, {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}, {{1, 0}, {1, 1}, {2, 1}, {1, 2}}, {{0, 1}, {1, 1}, {2, 1}, {1, 2}}, {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}, {{2, 0}, {1, 1}, {2, 1}, {1, 2}}, {{0, 1}, {1, 1}, {1, 2}, {2, 2}}, {{1, 0}, {0, 1}, {1, 1}, {0, 2}}}
};

static const SDL_Color TETRIS_COLORS[8] = {
    {26, 24, 34, 255},
    {0, 240, 240, 255},
    {0, 102, 204, 255},
    {255, 140, 0, 255},
    {240, 220, 70, 255},
    {80, 220, 120, 255},
    {170, 80, 220, 255},
    {220, 70, 70, 255}
};

static const int MAZE_MAP[MAZE_MAP_HEIGHT][MAZE_MAP_WIDTH] = {
    {1, 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 3, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 4, 1},
    {1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 4, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1},
    {1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1},
    {1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    {1, 4, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 2, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
};

static const char *const ARCADE_MENU_LABELS[ARCADE_MENU_COUNT] = {
    "TETRIS",
    "MINESWEEPER",
    "SPACE INVADERS",
    "CAR MAZE",
    "PICTURE PUZZLE"
};

static void arcade_set_result(ArcadeState *state, ArcadeScreen screen, ArcadeResult result) {
    if (screen < ARCADE_SCREEN_TETRIS || screen > ARCADE_SCREEN_PUZZLE) {
        return;
    }
    state->results[screen - 1] = result;
}

static void arcade_award_keys_for_result(ArcadeState *state, ArcadeResult result) {
    if (result == ARCADE_RESULT_WIN) {
        state->pending_key_reward += 2;
    }
}

static bool arcade_activate_pressed(const InputState *input) {
    return input->controller_interact_pressed ||
        input_pressed(input, SDL_SCANCODE_RETURN) ||
        input_pressed(input, SDL_SCANCODE_KP_ENTER) ||
        input_pressed(input, SDL_SCANCODE_SPACE);
}

static bool arcade_activate_down(const InputState *input) {
    return input->controller_interact_down ||
        input_down(input, SDL_SCANCODE_RETURN) ||
        input_down(input, SDL_SCANCODE_KP_ENTER) ||
        input_down(input, SDL_SCANCODE_SPACE);
}

static void arcade_reset_finish_input_guard(ArcadeState *state) {
    state->finish_input_ready = false;
    state->finish_input_screen = state->screen;
}

static bool arcade_finished_overlay_accept(ArcadeState *state, const InputState *input) {
    if (state->finish_input_screen != state->screen) {
        arcade_reset_finish_input_guard(state);
    }

    if (!state->finish_input_ready) {
        if (!arcade_activate_down(input)) {
            state->finish_input_ready = true;
        }
        return false;
    }

    return arcade_activate_pressed(input);
}

static void arcade_draw_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y,
    SDL_Color color
) {
    if (!renderer || !font || !text || !text[0]) {
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
    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void arcade_draw_text_centered(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    const SDL_Rect *rect,
    int y,
    SDL_Color color
) {
    int text_w = 0;
    int text_h = 0;
    (void)text_h;

    if (!renderer || !font || !text || !text[0] || !rect) {
        return;
    }
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0) {
        return;
    }

    arcade_draw_text(renderer, font, text, rect->x + (rect->w - text_w) / 2, y, color);
}

static void fill_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, rect);
}

static void stroke_rect(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, rect);
}

static bool maze_tile_is_solid(int tile_value) {
    return tile_value != 0 && tile_value != MAZE_TILE_SPAWN;
}

static bool maze_position_walkable(float x, float y, float radius) {
    const int min_x = (int)floorf(x - radius);
    const int max_x = (int)floorf(x + radius);
    const int min_y = (int)floorf(y - radius);
    const int max_y = (int)floorf(y + radius);

    for (int cell_y = min_y; cell_y <= max_y; ++cell_y) {
        for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
            if (cell_x < 0 || cell_x >= MAZE_MAP_WIDTH || cell_y < 0 || cell_y >= MAZE_MAP_HEIGHT) {
                return false;
            }
            if (!maze_tile_is_solid(MAZE_MAP[cell_y][cell_x])) {
                continue;
            }

            const float nearest_x = fmaxf((float)cell_x, fminf(x, (float)cell_x + 1.0f));
            const float nearest_y = fmaxf((float)cell_y, fminf(y, (float)cell_y + 1.0f));
            const float dx = x - nearest_x;
            const float dy = y - nearest_y;
            if ((dx * dx + dy * dy) < (radius * radius)) {
                return false;
            }
        }
    }

    return true;
}

static bool maze_find_spawn_point(float *spawn_x, float *spawn_y) {
    if (!spawn_x || !spawn_y) {
        return false;
    }

    for (int cell_y = 0; cell_y < MAZE_MAP_HEIGHT; ++cell_y) {
        for (int cell_x = 0; cell_x < MAZE_MAP_WIDTH; ++cell_x) {
            if (MAZE_MAP[cell_y][cell_x] == MAZE_TILE_SPAWN) {
                *spawn_x = (float)cell_x + 0.5f;
                *spawn_y = (float)cell_y + 0.5f;
                return true;
            }
        }
    }

    return false;
}

static bool maze_is_near_tile_type(float x, float y, int tile_type, float distance) {
    const float distance_sq = distance * distance;

    for (int cell_y = 0; cell_y < MAZE_MAP_HEIGHT; ++cell_y) {
        for (int cell_x = 0; cell_x < MAZE_MAP_WIDTH; ++cell_x) {
            if (MAZE_MAP[cell_y][cell_x] != tile_type) {
                continue;
            }

            const float nearest_x = fmaxf((float)cell_x, fminf(x, (float)cell_x + 1.0f));
            const float nearest_y = fmaxf((float)cell_y, fminf(y, (float)cell_y + 1.0f));
            const float dx = x - nearest_x;
            const float dy = y - nearest_y;
            if ((dx * dx + dy * dy) <= distance_sq) {
                return true;
            }
        }
    }

    return false;
}

static void maze_detect_fan_sheet_layout(int width, int height, int *out_cols, int *out_rows) {
    if (!out_cols || !out_rows) {
        return;
    }

    *out_cols = 1;
    *out_rows = 1;

    if (width <= 0 || height <= 0) {
        return;
    }

    if (width >= height * 2 && (width % height) == 0) {
        *out_cols = width / height;
        return;
    }

    if (height >= width * 2 && (height % width) == 0) {
        *out_rows = height / width;
        return;
    }

    {
        const float aspect = (width > height)
            ? ((float)width / (float)height)
            : ((float)height / (float)width);
        if (aspect <= 1.35f) {
            int best_divisor = 1;
            for (int divisor = 2; divisor <= MAZE_FAN_LAYOUT_MAX_DIVISOR; ++divisor) {
                if ((width % divisor) != 0 || (height % divisor) != 0) {
                    continue;
                }

                if ((width / divisor) < 48 || (height / divisor) < 48) {
                    continue;
                }

                best_divisor = divisor;
            }

            if (best_divisor > 1) {
                *out_cols = best_divisor;
                *out_rows = best_divisor;
                return;
            }
        }
    }

    if ((width % height) == 0) {
        *out_cols = width / height;
        return;
    }

    if ((height % width) == 0) {
        *out_rows = height / width;
    }
}

static void maze_reset(ArcadeMazeState *state, uint32_t now_ms) {
    memset(state, 0, sizeof(*state));
    state->overlay_selection = 0;
    state->last_updated_at = now_ms;
    state->x = 1.5f;
    state->y = 1.5f;
    (void)maze_find_spawn_point(&state->x, &state->y);
}

static void maze_update(ArcadeMazeState *state, const InputState *input, uint32_t now_ms) {
    float dt = 0.0f;
    if (now_ms > state->last_updated_at) {
        dt = (float)(now_ms - state->last_updated_at) / 1000.0f;
    }
    state->last_updated_at = now_ms;
    if (dt > MAZE_MAX_DT) {
        dt = MAZE_MAX_DT;
    }

    if (state->won) {
        return;
    }

    if (state->paused) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->overlay_selection = 0;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->overlay_selection = 1;
        }
        return;
    }

    if (input_down(input, SDL_SCANCODE_LEFT) || input_down(input, SDL_SCANCODE_A)) {
        state->angle -= MAZE_TURN_SPEED * dt;
    }
    if (input_down(input, SDL_SCANCODE_RIGHT) || input_down(input, SDL_SCANCODE_D)) {
        state->angle += MAZE_TURN_SPEED * dt;
    }
    if (state->angle < 0.0f) {
        state->angle += MAZE_PI2;
    } else if (state->angle >= MAZE_PI2) {
        state->angle -= MAZE_PI2;
    }

    {
        float move_step = 0.0f;
        if (input_down(input, SDL_SCANCODE_UP) || input_down(input, SDL_SCANCODE_W)) {
            move_step += MAZE_MOVE_SPEED * dt;
        }
        if (input_down(input, SDL_SCANCODE_DOWN) || input_down(input, SDL_SCANCODE_S)) {
            move_step -= MAZE_MOVE_SPEED * dt;
        }

        if (move_step != 0.0f) {
            const float next_x = state->x + cosf(state->angle) * move_step;
            const float next_y = state->y + sinf(state->angle) * move_step;

            if (maze_position_walkable(next_x, state->y, MAZE_PLAYER_RADIUS)) {
                state->x = next_x;
            }
            if (maze_position_walkable(state->x, next_y, MAZE_PLAYER_RADIUS)) {
                state->y = next_y;
            }
        }
    }

    if (maze_is_near_tile_type(state->x, state->y, MAZE_TILE_FINISH, MAZE_FINISH_TRIGGER_DISTANCE)) {
        state->won = true;
    }
}

static int rand_piece_type(void) {
    return rand() % 7;
}

static void tetris_spawn_piece(ArcadeTetrisState *state, uint32_t now_ms) {
    state->current = state->next;
    state->current.x = 3;
    state->current.y = 0;
    state->current.rotation = 0;
    state->next.type = rand_piece_type();
    state->next.rotation = 0;
    state->next.x = 0;
    state->next.y = 0;
    state->fall_started_at = now_ms;
    state->pieces_used++;
}

static bool tetris_piece_fits(const ArcadeTetrisState *state, ArcadeTetrisPiece piece) {
    for (int i = 0; i < 4; ++i) {
        const CellPoint block = TETRIS_SHAPES[piece.type][piece.rotation][i];
        const int x = piece.x + block.x;
        const int y = piece.y + block.y;

        if (x < 0 || x >= TETRIS_COLS || y >= TETRIS_ROWS) {
            return false;
        }
        if (y >= 0 && state->cells[y][x] != 0) {
            return false;
        }
    }
    return true;
}

static void tetris_reset(ArcadeTetrisState *state, uint32_t now_ms) {
    memset(state, 0, sizeof(*state));
    state->current.type = rand_piece_type();
    state->next.type = rand_piece_type();
    state->overlay_selection = 0;
    tetris_spawn_piece(state, now_ms);
    if (!tetris_piece_fits(state, state->current)) {
        state->game_over = true;
        state->won = false;
    }
}

static bool tetris_try_move(ArcadeTetrisState *state, int dx, int dy) {
    ArcadeTetrisPiece moved = state->current;
    moved.x += dx;
    moved.y += dy;
    if (!tetris_piece_fits(state, moved)) {
        return false;
    }
    state->current = moved;
    return true;
}

static void tetris_try_rotate(ArcadeTetrisState *state) {
    ArcadeTetrisPiece rotated = state->current;
    rotated.rotation = (rotated.rotation + 1) % 4;
    if (tetris_piece_fits(state, rotated)) {
        state->current = rotated;
        return;
    }

    rotated.x -= 1;
    if (tetris_piece_fits(state, rotated)) {
        state->current = rotated;
        return;
    }

    rotated.x += 2;
    if (tetris_piece_fits(state, rotated)) {
        state->current = rotated;
    }
}

static void tetris_lock_piece(ArcadeTetrisState *state, uint32_t now_ms) {
    for (int i = 0; i < 4; ++i) {
        const CellPoint block = TETRIS_SHAPES[state->current.type][state->current.rotation][i];
        const int x = state->current.x + block.x;
        const int y = state->current.y + block.y;
        if (y >= 0 && y < TETRIS_ROWS && x >= 0 && x < TETRIS_COLS) {
            state->cells[y][x] = state->current.type + 1;
        }
    }

    int cleared = 0;
    for (int y = TETRIS_ROWS - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < TETRIS_COLS; ++x) {
            if (state->cells[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (!full) {
            continue;
        }

        cleared++;
        for (int row = y; row > 0; --row) {
            memcpy(state->cells[row], state->cells[row - 1], sizeof(state->cells[row]));
        }
        memset(state->cells[0], 0, sizeof(state->cells[0]));
        y++;
    }

    state->lines += (uint32_t)cleared;
    state->score += (uint32_t)(100 * (cleared > 0 ? cleared : 1));

    if (state->pieces_used >= TETRIS_PIECE_LIMIT) {
        state->game_over = true;
        state->won = state->score > TETRIS_WIN_SCORE;
        return;
    }

    tetris_spawn_piece(state, now_ms);
    if (!tetris_piece_fits(state, state->current)) {
        state->game_over = true;
        state->won = false;
    }
}

static void tetris_update(ArcadeTetrisState *state, const InputState *input, uint32_t now_ms) {
    if (state->game_over) {
        return;
    }

    if (state->paused) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->overlay_selection = 0;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->overlay_selection = 1;
        }
        return;
    }

    if (input_pressed(input, SDL_SCANCODE_LEFT) || input_pressed(input, SDL_SCANCODE_A)) {
        tetris_try_move(state, -1, 0);
    }
    if (input_pressed(input, SDL_SCANCODE_RIGHT) || input_pressed(input, SDL_SCANCODE_D)) {
        tetris_try_move(state, 1, 0);
    }
    if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
        tetris_try_rotate(state);
    }
    if (input_pressed(input, SDL_SCANCODE_SPACE)) {
        while (tetris_try_move(state, 0, 1)) {
        }
        tetris_lock_piece(state, now_ms);
        return;
    }

    const uint32_t fall_delay = input_down(input, SDL_SCANCODE_DOWN) || input_down(input, SDL_SCANCODE_S)
        ? (TETRIS_FALL_MS / 6u)
        : TETRIS_FALL_MS;
    if (now_ms - state->fall_started_at < fall_delay) {
        return;
    }

    state->fall_started_at = now_ms;
    if (!tetris_try_move(state, 0, 1)) {
        tetris_lock_piece(state, now_ms);
    }
}

static bool mines_in_bounds(int x, int y) {
    return x >= 0 && x < MINES_SIZE && y >= 0 && y < MINES_SIZE;
}

static void mines_place_bombs(ArcadeMinesState *state, int safe_x, int safe_y) {
    int placed = 0;
    while (placed < MINES_COUNT) {
        const int x = rand() % MINES_SIZE;
        const int y = rand() % MINES_SIZE;
        if (state->cells[y][x].has_mine) {
            continue;
        }
        if (abs(x - safe_x) <= 1 && abs(y - safe_y) <= 1) {
            continue;
        }
        state->cells[y][x].has_mine = true;
        placed++;
    }

    for (int y = 0; y < MINES_SIZE; ++y) {
        for (int x = 0; x < MINES_SIZE; ++x) {
            if (state->cells[y][x].has_mine) {
                continue;
            }
            uint8_t count = 0;
            for (int ny = y - 1; ny <= y + 1; ++ny) {
                for (int nx = x - 1; nx <= x + 1; ++nx) {
                    if ((nx != x || ny != y) && mines_in_bounds(nx, ny) && state->cells[ny][nx].has_mine) {
                        count++;
                    }
                }
            }
            state->cells[y][x].adjacent = count;
        }
    }
}

static void mines_reset(ArcadeMinesState *state) {
    memset(state, 0, sizeof(*state));
    state->first_click = true;
    state->overlay_selection = 0;
}

static void mines_reveal_all(ArcadeMinesState *state) {
    for (int y = 0; y < MINES_SIZE; ++y) {
        for (int x = 0; x < MINES_SIZE; ++x) {
            if (state->cells[y][x].has_mine) {
                state->cells[y][x].revealed = true;
            }
        }
    }
}

static void mines_reveal(ArcadeMinesState *state, int start_x, int start_y) {
    if (!mines_in_bounds(start_x, start_y) || state->won || state->lost) {
        return;
    }

    ArcadeMineCell *start = &state->cells[start_y][start_x];
    if (start->flagged || start->revealed) {
        return;
    }

    if (state->first_click) {
        mines_place_bombs(state, start_x, start_y);
        state->first_click = false;
    }

    if (start->has_mine) {
        state->lost = true;
        start->revealed = true;
        mines_reveal_all(state);
        return;
    }

    int queue_x[MINES_SIZE * MINES_SIZE];
    int queue_y[MINES_SIZE * MINES_SIZE];
    bool queued[MINES_SIZE][MINES_SIZE] = {{false}};
    int head = 0;
    int tail = 0;
    queue_x[tail] = start_x;
    queue_y[tail] = start_y;
    queued[start_y][start_x] = true;
    tail++;

    while (head < tail) {
        const int x = queue_x[head];
        const int y = queue_y[head];
        head++;

        ArcadeMineCell *cell = &state->cells[y][x];
        if (cell->revealed || cell->flagged || cell->has_mine) {
            continue;
        }

        cell->revealed = true;
        state->revealed_safe_cells++;

        if (cell->adjacent != 0) {
            continue;
        }

        for (int ny = y - 1; ny <= y + 1; ++ny) {
            for (int nx = x - 1; nx <= x + 1; ++nx) {
                if (mines_in_bounds(nx, ny) && !queued[ny][nx]) {
                    queued[ny][nx] = true;
                    queue_x[tail] = nx;
                    queue_y[tail] = ny;
                    tail++;
                }
            }
        }
    }

    if (state->revealed_safe_cells >= (MINES_SIZE * MINES_SIZE - MINES_COUNT)) {
        state->won = true;
    }
}

static void mines_toggle_flag(ArcadeMinesState *state, int x, int y) {
    if (!mines_in_bounds(x, y) || state->won || state->lost) {
        return;
    }

    ArcadeMineCell *cell = &state->cells[y][x];
    if (cell->revealed) {
        return;
    }

    cell->flagged = !cell->flagged;
    state->flags_used += cell->flagged ? 1 : -1;
}

static void mines_update(ArcadeMinesState *state, const InputState *input) {
    if (state->won || state->lost) {
        return;
    }

    if (state->paused) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->overlay_selection = 0;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->overlay_selection = 1;
        }
        return;
    }

    if (input_pressed(input, SDL_SCANCODE_LEFT) || input_pressed(input, SDL_SCANCODE_A)) {
        state->cursor_x = state->cursor_x > 0 ? state->cursor_x - 1 : 0;
    }
    if (input_pressed(input, SDL_SCANCODE_RIGHT) || input_pressed(input, SDL_SCANCODE_D)) {
        state->cursor_x = state->cursor_x < (MINES_SIZE - 1) ? state->cursor_x + 1 : (MINES_SIZE - 1);
    }
    if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
        state->cursor_y = state->cursor_y > 0 ? state->cursor_y - 1 : 0;
    }
    if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
        state->cursor_y = state->cursor_y < (MINES_SIZE - 1) ? state->cursor_y + 1 : (MINES_SIZE - 1);
    }
    if (input_pressed(input, SDL_SCANCODE_RETURN)) {
        mines_reveal(state, state->cursor_x, state->cursor_y);
    }
    if (input_pressed(input, SDL_SCANCODE_F) || input_pressed(input, SDL_SCANCODE_SPACE)) {
        mines_toggle_flag(state, state->cursor_x, state->cursor_y);
    }
}

static void space_reset(ArcadeSpaceState *state, uint32_t now_ms) {
    memset(state, 0, sizeof(*state));
    state->player_x = 260.0f;
    state->enemy_direction = 1;
    state->lives = 3;
    state->alive_count = SPACE_ENEMY_COUNT;
    state->overlay_selection = 0;
    state->last_enemy_step_at = now_ms;
    state->last_enemy_fire_at = now_ms;

    int index = 0;
    for (int row = 0; row < SPACE_ROWS; ++row) {
        for (int col = 0; col < SPACE_COLS; ++col) {
            state->enemies[index].x = 70.0f + (float)col * 58.0f;
            state->enemies[index].y = 54.0f + (float)row * 42.0f;
            state->enemies[index].alive = true;
            index++;
        }
    }
}

static void space_fire_enemy(ArcadeSpaceState *state) {
    if (state->alive_count <= 0) {
        return;
    }

    int alive_indices[SPACE_ENEMY_COUNT];
    int alive_total = 0;
    for (int i = 0; i < SPACE_ENEMY_COUNT; ++i) {
        if (state->enemies[i].alive) {
            alive_indices[alive_total++] = i;
        }
    }
    if (alive_total == 0) {
        return;
    }

    const ArcadeEnemy *enemy = &state->enemies[alive_indices[rand() % alive_total]];
    for (int i = 0; i < (int)(sizeof(state->enemy_shots) / sizeof(state->enemy_shots[0])); ++i) {
        if (state->enemy_shots[i].alive) {
            continue;
        }
        state->enemy_shots[i].alive = true;
        state->enemy_shots[i].x = enemy->x + 14.0f;
        state->enemy_shots[i].y = enemy->y + 18.0f;
        return;
    }
}

static void space_update(ArcadeSpaceState *state, const InputState *input, uint32_t now_ms) {
    if (state->won || state->lost) {
        return;
    }

    if (state->paused) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->overlay_selection = 0;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->overlay_selection = 1;
        }
        return;
    }

    if (input_down(input, SDL_SCANCODE_LEFT) || input_down(input, SDL_SCANCODE_A)) {
        state->player_x -= 4.0f;
    }
    if (input_down(input, SDL_SCANCODE_RIGHT) || input_down(input, SDL_SCANCODE_D)) {
        state->player_x += 4.0f;
    }
    if (state->player_x < 24.0f) {
        state->player_x = 24.0f;
    }
    if (state->player_x > 496.0f) {
        state->player_x = 496.0f;
    }

    if ((input_pressed(input, SDL_SCANCODE_SPACE) || input_pressed(input, SDL_SCANCODE_RETURN))
        && !state->player_shot.alive) {
        state->player_shot.alive = true;
        state->player_shot.x = state->player_x + 13.0f;
        state->player_shot.y = 316.0f;
    }

    const uint32_t step_interval = (uint32_t)(480 - (SPACE_ENEMY_COUNT - state->alive_count) * 15);
    if (now_ms - state->last_enemy_step_at >= (step_interval > 120u ? step_interval : 120u)) {
        state->last_enemy_step_at = now_ms;
        bool hit_edge = false;
        for (int i = 0; i < SPACE_ENEMY_COUNT; ++i) {
            if (!state->enemies[i].alive) {
                continue;
            }
            state->enemies[i].x += 12.0f * (float)state->enemy_direction;
            if (state->enemies[i].x < 30.0f || state->enemies[i].x > 500.0f) {
                hit_edge = true;
            }
        }
        if (hit_edge) {
            state->enemy_direction *= -1;
            for (int i = 0; i < SPACE_ENEMY_COUNT; ++i) {
                if (!state->enemies[i].alive) {
                    continue;
                }
                state->enemies[i].x += 12.0f * (float)state->enemy_direction;
                state->enemies[i].y += 18.0f;
                if (state->enemies[i].y > 290.0f) {
                    state->lost = true;
                }
            }
        }
    }

    if (now_ms - state->last_enemy_fire_at >= 900u) {
        state->last_enemy_fire_at = now_ms;
        space_fire_enemy(state);
    }

    if (state->player_shot.alive) {
        state->player_shot.y -= 8.0f;
        if (state->player_shot.y < 0.0f) {
            state->player_shot.alive = false;
        }
    }

    for (int i = 0; i < (int)(sizeof(state->enemy_shots) / sizeof(state->enemy_shots[0])); ++i) {
        if (!state->enemy_shots[i].alive) {
            continue;
        }
        state->enemy_shots[i].y += 5.0f;
        if (state->enemy_shots[i].y > 360.0f) {
            state->enemy_shots[i].alive = false;
            continue;
        }
        if (state->enemy_shots[i].y >= 316.0f &&
            state->enemy_shots[i].x >= state->player_x &&
            state->enemy_shots[i].x <= state->player_x + 28.0f) {
            state->enemy_shots[i].alive = false;
            state->lives--;
            if (state->lives <= 0) {
                state->lost = true;
            }
        }
    }

    if (state->player_shot.alive) {
        for (int i = 0; i < SPACE_ENEMY_COUNT; ++i) {
            if (!state->enemies[i].alive) {
                continue;
            }
            const float dx = state->player_shot.x - state->enemies[i].x;
            const float dy = state->player_shot.y - state->enemies[i].y;
            if (dx >= 0.0f && dx <= 30.0f && dy >= 0.0f && dy <= 20.0f) {
                state->player_shot.alive = false;
                state->enemies[i].alive = false;
                state->alive_count--;
                state->score += 100;
                break;
            }
        }
    }

    if (state->alive_count <= 0) {
        state->won = true;
    }
}

static int puzzle_choice_count(const ArcadePuzzleState *state) {
    if (state->photo_count >= PUZZLE_MIN_PHOTO_CHOICES) {
        return state->photo_count;
    }
    return PUZZLE_MIN_PHOTO_CHOICES;
}

static int puzzle_random_photo_index(const ArcadePuzzleState *state) {
    const int count = puzzle_choice_count(state);
    return count > 0 ? (rand() % count) : 0;
}

static SDL_Rect puzzle_slot_rect(
    const ArcadePuzzleState *state,
    SDL_Rect image_rect
);

static void puzzle_start_round(ArcadePuzzleState *state, uint32_t now_ms) {
    int wrong_a = 0;
    int wrong_b = 0;
    int correct_slot = 0;

    state->puzzle_photo_index = puzzle_random_photo_index(state);
    wrong_a = puzzle_random_photo_index(state);
    while (wrong_a == state->puzzle_photo_index) {
        wrong_a = puzzle_random_photo_index(state);
    }
    wrong_b = puzzle_random_photo_index(state);
    while (wrong_b == state->puzzle_photo_index || wrong_b == wrong_a) {
        wrong_b = puzzle_random_photo_index(state);
    }

    correct_slot = rand() % ARCADE_PUZZLE_OPTION_COUNT;
    for (int i = 0; i < ARCADE_PUZZLE_OPTION_COUNT; ++i) {
        state->option_photo_indices[i] = (i == 0) ? wrong_a : wrong_b;
    }
    state->option_photo_indices[correct_slot] = state->puzzle_photo_index;
    state->correct_option = correct_slot;
    state->selected_option = 0;
    state->hover_option = -1;
    state->drag_option = -1;
    state->dragging = false;
    state->mouse_was_down = false;
    state->drag_x = 0;
    state->drag_y = 0;
    state->drag_offset_x = 0;
    state->drag_offset_y = 0;

    state->src_w_permil = 220 + rand() % 90;
    state->src_h_permil = 220 + rand() % 90;
    state->src_x_permil = 90 + rand() % (PUZZLE_PERMIL - state->src_w_permil - 180);
    state->src_y_permil = 90 + rand() % (PUZZLE_PERMIL - state->src_h_permil - 180);

    state->paused = false;
    state->feedback_active = false;
    state->feedback_success = false;
    state->feedback_timeout = false;
    state->overlay_selection = 0;
    state->round_started_at = now_ms;
    state->feedback_until = 0;
}

static void puzzle_reset(ArcadePuzzleState *state, uint32_t now_ms) {
    const int photo_count = state->photo_count;
    memset(state, 0, sizeof(*state));
    state->photo_count = photo_count;
    state->current_round = 0;
    puzzle_start_round(state, now_ms);
}

static void puzzle_finish_round(
    ArcadePuzzleState *state,
    bool success,
    bool timeout,
    uint32_t now_ms
) {
    if (success) {
        state->score++;
    }
    state->feedback_active = true;
    state->feedback_success = success;
    state->feedback_timeout = timeout;
    state->feedback_until = now_ms + PUZZLE_FEEDBACK_MS;
}

static void puzzle_advance_after_feedback(ArcadePuzzleState *state, uint32_t now_ms) {
    if (state->current_round + 1 >= PUZZLE_ROUNDS_TO_PLAY) {
        state->won = state->score >= PUZZLE_REQUIRED_SCORE;
        state->lost = !state->won;
        state->feedback_active = false;
        state->overlay_selection = 0;
        return;
    }

    state->current_round++;
    puzzle_start_round(state, now_ms);
}

static bool rect_contains_point(const SDL_Rect *rect, int x, int y) {
    return rect &&
        x >= rect->x &&
        y >= rect->y &&
        x < rect->x + rect->w &&
        y < rect->y + rect->h;
}

static SDL_Rect puzzle_scale_rect(const SDL_Rect *panel, float x, float y, float w, float h) {
    const float sx = panel ? ((float)panel->w / PUZZLE_LOGICAL_W) : 1.0f;
    const float sy = panel ? ((float)panel->h / PUZZLE_LOGICAL_H) : 1.0f;
    const float scale = sx < sy ? sx : sy;
    const float offset_x = panel ? panel->x + ((float)panel->w - PUZZLE_LOGICAL_W * scale) * 0.5f : 0.0f;
    const float offset_y = panel ? panel->y + ((float)panel->h - PUZZLE_LOGICAL_H * scale) * 0.5f : 0.0f;
    SDL_Rect out = {
        (int)lroundf(offset_x + x * scale),
        (int)lroundf(offset_y + y * scale),
        (int)lroundf(w * scale),
        (int)lroundf(h * scale)
    };
    if (out.w < 1) out.w = 1;
    if (out.h < 1) out.h = 1;
    return out;
}

static void puzzle_get_layout(
    const ArcadePuzzleState *state,
    const SDL_Rect *panel,
    SDL_Rect *slot_out,
    SDL_Rect option_out[ARCADE_PUZZLE_OPTION_COUNT],
    SDL_Rect preview_out[ARCADE_PUZZLE_OPTION_COUNT]
) {
    SDL_Rect image_box = puzzle_scale_rect(panel, 54.0f, 230.0f, 760.0f, 420.0f);

    if (slot_out) {
        *slot_out = puzzle_slot_rect(state, image_box);
    }

    if (option_out || preview_out) {
        const float option_x = 894.0f;
        const float option_y = 280.0f;
        const float option_w = 338.0f;
        const float option_h = 112.0f;
        const float option_gap = 32.0f;
        for (int i = 0; i < ARCADE_PUZZLE_OPTION_COUNT; ++i) {
            SDL_Rect item = puzzle_scale_rect(panel, option_x, option_y + (float)i * (option_h + option_gap), option_w, option_h);
            SDL_Rect preview = puzzle_scale_rect(panel, option_x + 18.0f, option_y + 16.0f + (float)i * (option_h + option_gap), option_w - 36.0f, 64.0f);
            if (option_out) {
                option_out[i] = item;
            }
            if (preview_out) {
                preview_out[i] = preview;
            }
        }
    }
}

static void puzzle_update(ArcadePuzzleState *state, const InputState *input, uint32_t now_ms, const SDL_Rect *panel) {
    if (state->won || state->lost) {
        return;
    }

    if (state->feedback_active) {
        if (now_ms >= state->feedback_until) {
            puzzle_advance_after_feedback(state, now_ms);
        }
        return;
    }

    if (state->paused) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->overlay_selection = 0;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->overlay_selection = 1;
        }
        return;
    }

    if (now_ms - state->round_started_at >= PUZZLE_ROUND_TIME_MS) {
        puzzle_finish_round(state, false, true, now_ms);
        return;
    }

    state->hover_option = -1;
    if (panel) {
        SDL_Rect slot = {0, 0, 0, 0};
        SDL_Rect option_rects[ARCADE_PUZZLE_OPTION_COUNT];
        SDL_Rect preview_rects[ARCADE_PUZZLE_OPTION_COUNT];
        int mx = 0;
        int my = 0;
        const Uint32 buttons = SDL_GetMouseState(&mx, &my);
        const bool mouse_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        const bool mouse_pressed = mouse_down && !state->mouse_was_down;
        const bool mouse_released = !mouse_down && state->mouse_was_down;

        {
            SDL_Window *mouse_window = SDL_GetMouseFocus();
            int window_w = 0;
            int window_h = 0;
            if (mouse_window) {
                SDL_GetWindowSize(mouse_window, &window_w, &window_h);
            }
            if (window_w > 0 && window_h > 0) {
                const int logical_w = panel->w > WINDOW_W ? panel->w : WINDOW_W;
                const int logical_h = panel->h > WINDOW_H ? panel->h : WINDOW_H;
                mx = (mx * logical_w) / window_w;
                my = (my * logical_h) / window_h;
            }
        }

        puzzle_get_layout(state, panel, &slot, option_rects, preview_rects);
        state->drag_x = mx;
        state->drag_y = my;

        for (int i = 0; i < ARCADE_PUZZLE_OPTION_COUNT; ++i) {
            if (rect_contains_point(&option_rects[i], mx, my) || rect_contains_point(&preview_rects[i], mx, my)) {
                state->hover_option = i;
                break;
            }
        }

        if (mouse_pressed && state->hover_option >= 0) {
            state->dragging = true;
            state->drag_option = state->hover_option;
            state->selected_option = state->hover_option;
            state->drag_offset_x = mx - preview_rects[state->hover_option].x;
            state->drag_offset_y = my - preview_rects[state->hover_option].y;
        }

        if (state->dragging && state->drag_option >= 0) {
            state->selected_option = state->drag_option;
        }

        if (mouse_released) {
            if (state->dragging && state->drag_option >= 0 && rect_contains_point(&slot, mx, my)) {
                puzzle_finish_round(state, state->drag_option == state->correct_option, false, now_ms);
            }
            state->dragging = false;
            state->drag_option = -1;
        }

        state->mouse_was_down = mouse_down;
        if (state->feedback_active) {
            return;
        }
    }

    if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W) ||
        input_pressed(input, SDL_SCANCODE_LEFT) || input_pressed(input, SDL_SCANCODE_A)) {
        state->selected_option =
            (state->selected_option + ARCADE_PUZZLE_OPTION_COUNT - 1) % ARCADE_PUZZLE_OPTION_COUNT;
    }
    if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S) ||
        input_pressed(input, SDL_SCANCODE_RIGHT) || input_pressed(input, SDL_SCANCODE_D)) {
        state->selected_option = (state->selected_option + 1) % ARCADE_PUZZLE_OPTION_COUNT;
    }

}

void arcade_init(ArcadeState *state) {
    memset(state, 0, sizeof(*state));
    state->screen = ARCADE_SCREEN_MENU;
    state->finish_input_screen = ARCADE_SCREEN_MENU;
}

void arcade_set_puzzle_photo_count(ArcadeState *state, int photo_count) {
    if (!state) {
        return;
    }
    if (photo_count < 0) {
        photo_count = 0;
    }
    state->puzzle.photo_count = photo_count;
}

void arcade_open(ArcadeState *state, uint32_t now_ms) {
    state->active = true;
    state->screen = ARCADE_SCREEN_MENU;
    state->menu_selection = 0;
    arcade_reset_finish_input_guard(state);
    tetris_reset(&state->tetris, now_ms);
    mines_reset(&state->mines);
    space_reset(&state->space, now_ms);
    maze_reset(&state->maze, now_ms);
    puzzle_reset(&state->puzzle, now_ms);
}

void arcade_close(ArcadeState *state) {
    state->active = false;
}

int arcade_take_pending_key_reward(ArcadeState *state) {
    const int reward = state->pending_key_reward;
    state->pending_key_reward = 0;
    return reward;
}

bool arcade_handle_escape(ArcadeState *state) {
    if (!state->active) {
        return false;
    }

    if (state->screen == ARCADE_SCREEN_MENU) {
        return false;
    }

    if (state->screen == ARCADE_SCREEN_SPACE) {
        if (state->space.won || state->space.lost) {
            return true;
        } else {
            state->space.paused = !state->space.paused;
            state->space.overlay_selection = 0;
        }
        return true;
    }

    if (state->screen == ARCADE_SCREEN_TETRIS) {
        if (state->tetris.game_over) {
            return true;
        } else {
            state->tetris.paused = !state->tetris.paused;
            state->tetris.overlay_selection = 0;
        }
        return true;
    }

    if (state->screen == ARCADE_SCREEN_MINES) {
        if (state->mines.won || state->mines.lost) {
            return true;
        } else {
            state->mines.paused = !state->mines.paused;
            state->mines.overlay_selection = 0;
        }
        return true;
    }

    if (state->screen == ARCADE_SCREEN_MAZE) {
        if (state->maze.won) {
            return true;
        } else {
            state->maze.paused = !state->maze.paused;
            state->maze.overlay_selection = 0;
            state->maze.last_updated_at = SDL_GetTicks();
        }
        return true;
    }

    if (state->screen == ARCADE_SCREEN_PUZZLE) {
        if (state->puzzle.won || state->puzzle.lost) {
            return true;
        } else {
            state->puzzle.paused = !state->puzzle.paused;
            state->puzzle.overlay_selection = 0;
        }
        return true;
    }

    state->screen = ARCADE_SCREEN_MENU;
    return true;
}

void arcade_update(ArcadeState *state, const InputState *input, uint32_t now_ms, const SDL_Rect *panel) {
    if (!state->active) {
        return;
    }

    const bool tetris_finished = state->screen == ARCADE_SCREEN_TETRIS && state->tetris.game_over;
    const bool mines_finished = state->screen == ARCADE_SCREEN_MINES &&
        (state->mines.won || state->mines.lost);
    const bool space_finished = state->screen == ARCADE_SCREEN_SPACE &&
        (state->space.won || state->space.lost);
    const bool maze_finished = state->screen == ARCADE_SCREEN_MAZE && state->maze.won;
    const bool puzzle_finished = state->screen == ARCADE_SCREEN_PUZZLE &&
        (state->puzzle.won || state->puzzle.lost);

    if (input_pressed(input, SDL_SCANCODE_BACKSPACE) &&
        !tetris_finished &&
        !mines_finished &&
        !space_finished &&
        !maze_finished &&
        !puzzle_finished) {
        state->screen = ARCADE_SCREEN_MENU;
        arcade_reset_finish_input_guard(state);
        state->tetris.paused = false;
        state->mines.paused = false;
        state->space.paused = false;
        state->maze.paused = false;
        state->puzzle.paused = false;
        state->maze.last_updated_at = now_ms;
        return;
    }

    if (state->screen == ARCADE_SCREEN_MENU) {
        if (input_pressed(input, SDL_SCANCODE_UP) || input_pressed(input, SDL_SCANCODE_W)) {
            state->menu_selection = (state->menu_selection + ARCADE_MENU_COUNT - 1) % ARCADE_MENU_COUNT;
        }
        if (input_pressed(input, SDL_SCANCODE_DOWN) || input_pressed(input, SDL_SCANCODE_S)) {
            state->menu_selection = (state->menu_selection + 1) % ARCADE_MENU_COUNT;
        }
        if (arcade_activate_pressed(input)) {
            state->screen = (ArcadeScreen)(state->menu_selection + 1);
            arcade_reset_finish_input_guard(state);
            if (state->screen == ARCADE_SCREEN_MAZE) {
                state->maze.last_updated_at = now_ms;
            }
        }
        return;
    }

    if (state->screen == ARCADE_SCREEN_TETRIS) {
        tetris_update(&state->tetris, input, now_ms);
        if (state->tetris.paused) {
            const bool activate = arcade_activate_pressed(input);
            if (activate) {
                if (state->tetris.overlay_selection == 0) {
                    state->tetris.paused = false;
                } else {
                    state->tetris.paused = false;
                    state->screen = ARCADE_SCREEN_MENU;
                    arcade_reset_finish_input_guard(state);
                }
            }
        } else if (state->tetris.game_over &&
                   arcade_finished_overlay_accept(state, input)) {
            const ArcadeResult result = state->tetris.won ? ARCADE_RESULT_WIN : ARCADE_RESULT_LOSE;
            arcade_set_result(
                state,
                ARCADE_SCREEN_TETRIS,
                result
            );
            arcade_award_keys_for_result(state, result);
            state->tetris.paused = false;
            state->screen = ARCADE_SCREEN_MENU;
            arcade_reset_finish_input_guard(state);
        }
    } else if (state->screen == ARCADE_SCREEN_MINES) {
        mines_update(&state->mines, input);
        if (state->mines.paused) {
            const bool activate = arcade_activate_pressed(input);
            if (activate) {
                if (state->mines.overlay_selection == 0) {
                    state->mines.paused = false;
                } else {
                    state->mines.paused = false;
                    state->screen = ARCADE_SCREEN_MENU;
                    arcade_reset_finish_input_guard(state);
                }
            }
        } else if ((state->mines.won || state->mines.lost) &&
                   arcade_finished_overlay_accept(state, input)) {
            const ArcadeResult result = state->mines.won ? ARCADE_RESULT_WIN : ARCADE_RESULT_LOSE;
            arcade_set_result(
                state,
                ARCADE_SCREEN_MINES,
                result
            );
            arcade_award_keys_for_result(state, result);
            state->mines.paused = false;
            state->screen = ARCADE_SCREEN_MENU;
            arcade_reset_finish_input_guard(state);
        }
    } else if (state->screen == ARCADE_SCREEN_SPACE) {
        space_update(&state->space, input, now_ms);
        if (state->space.paused) {
            const bool activate = arcade_activate_pressed(input);
            if (activate) {
                if (state->space.overlay_selection == 0) {
                    state->space.paused = false;
                } else {
                    state->space.paused = false;
                    state->screen = ARCADE_SCREEN_MENU;
                    arcade_reset_finish_input_guard(state);
                }
            }
        } else if ((state->space.won || state->space.lost) &&
                   arcade_finished_overlay_accept(state, input)) {
            const ArcadeResult result = state->space.won ? ARCADE_RESULT_WIN : ARCADE_RESULT_LOSE;
            arcade_set_result(
                state,
                ARCADE_SCREEN_SPACE,
                result
            );
            arcade_award_keys_for_result(state, result);
            state->space.paused = false;
            state->screen = ARCADE_SCREEN_MENU;
            arcade_reset_finish_input_guard(state);
        }
    } else if (state->screen == ARCADE_SCREEN_MAZE) {
        maze_update(&state->maze, input, now_ms);
        if (state->maze.paused) {
            const bool activate = arcade_activate_pressed(input);
            if (activate) {
                if (state->maze.overlay_selection == 0) {
                    state->maze.paused = false;
                    state->maze.last_updated_at = now_ms;
                } else {
                    state->maze.paused = false;
                    state->screen = ARCADE_SCREEN_MENU;
                    arcade_reset_finish_input_guard(state);
                }
            }
        } else if (state->maze.won &&
                   arcade_finished_overlay_accept(state, input)) {
            arcade_set_result(state, ARCADE_SCREEN_MAZE, ARCADE_RESULT_WIN);
            arcade_award_keys_for_result(state, ARCADE_RESULT_WIN);
            state->maze.paused = false;
            state->screen = ARCADE_SCREEN_MENU;
            arcade_reset_finish_input_guard(state);
        }
    } else if (state->screen == ARCADE_SCREEN_PUZZLE) {
        SDL_Rect inner = {0, 0, 0, 0};
        const SDL_Rect *puzzle_panel = NULL;
        if (panel) {
            inner.x = panel->x + 18;
            inner.y = panel->y + 18;
            inner.w = panel->w - 36;
            inner.h = panel->h - 36;
            puzzle_panel = &inner;
        }
        puzzle_update(&state->puzzle, input, now_ms, puzzle_panel);
        if (state->puzzle.paused) {
            const bool activate = arcade_activate_pressed(input);
            if (activate) {
                if (state->puzzle.overlay_selection == 0) {
                    state->puzzle.paused = false;
                    state->puzzle.round_started_at = now_ms;
                } else {
                    state->puzzle.paused = false;
                    state->screen = ARCADE_SCREEN_MENU;
                    arcade_reset_finish_input_guard(state);
                }
            }
        } else if ((state->puzzle.won || state->puzzle.lost) &&
                   arcade_finished_overlay_accept(state, input)) {
            const ArcadeResult result = state->puzzle.won ? ARCADE_RESULT_WIN : ARCADE_RESULT_LOSE;
            arcade_set_result(state, ARCADE_SCREEN_PUZZLE, result);
            arcade_award_keys_for_result(state, result);
            state->puzzle.paused = false;
            state->screen = ARCADE_SCREEN_MENU;
            arcade_reset_finish_input_guard(state);
        }
    }
}

static const TextureAsset *puzzle_photo_for_index(
    const ArcadeRenderAssets *assets,
    int photo_index
) {
    if (!assets || !assets->puzzle_photos || assets->puzzle_photo_count <= 0) {
        return NULL;
    }
    if (photo_index < 0) {
        photo_index = 0;
    }
    photo_index %= assets->puzzle_photo_count;
    if (!assets->puzzle_photos[photo_index].loaded) {
        return NULL;
    }
    return &assets->puzzle_photos[photo_index];
}

static SDL_Rect puzzle_fit_texture_contain(const TextureAsset *asset, SDL_Rect box) {
    SDL_Rect out = box;
    if (!asset || !asset->loaded || asset->w <= 0 || asset->h <= 0 || box.w <= 0 || box.h <= 0) {
        return out;
    }

    {
        const float scale_x = (float)box.w / (float)asset->w;
        const float scale_y = (float)box.h / (float)asset->h;
        const float scale = scale_x < scale_y ? scale_x : scale_y;
        out.w = (int)((float)asset->w * scale);
        out.h = (int)((float)asset->h * scale);
        if (out.w < 1) out.w = 1;
        if (out.h < 1) out.h = 1;
        out.x = box.x + (box.w - out.w) / 2;
        out.y = box.y + (box.h - out.h) / 2;
    }
    return out;
}

static SDL_Rect puzzle_source_rect(
    const ArcadePuzzleState *state,
    const TextureAsset *asset
) {
    SDL_Rect src = {0, 0, 1, 1};
    if (!state || !asset || !asset->loaded || asset->w <= 0 || asset->h <= 0) {
        return src;
    }

    src.x = (asset->w * state->src_x_permil) / PUZZLE_PERMIL;
    src.y = (asset->h * state->src_y_permil) / PUZZLE_PERMIL;
    src.w = (asset->w * state->src_w_permil) / PUZZLE_PERMIL;
    src.h = (asset->h * state->src_h_permil) / PUZZLE_PERMIL;
    if (src.w < 1) src.w = 1;
    if (src.h < 1) src.h = 1;
    if (src.x + src.w > asset->w) src.x = asset->w - src.w;
    if (src.y + src.h > asset->h) src.y = asset->h - src.h;
    if (src.x < 0) src.x = 0;
    if (src.y < 0) src.y = 0;
    return src;
}

static SDL_Rect puzzle_slot_rect(
    const ArcadePuzzleState *state,
    SDL_Rect image_rect
) {
    SDL_Rect slot = {
        image_rect.x + (image_rect.w * state->src_x_permil) / PUZZLE_PERMIL,
        image_rect.y + (image_rect.h * state->src_y_permil) / PUZZLE_PERMIL,
        (image_rect.w * state->src_w_permil) / PUZZLE_PERMIL,
        (image_rect.h * state->src_h_permil) / PUZZLE_PERMIL
    };
    if (slot.w < 18) slot.w = 18;
    if (slot.h < 18) slot.h = 18;
    return slot;
}

static void puzzle_draw_fallback_image(
    SDL_Renderer *renderer,
    const SDL_Rect *rect,
    int photo_index
) {
    const SDL_Color colors[5] = {
        {88, 130, 186, 255},
        {196, 114, 94, 255},
        {104, 170, 126, 255},
        {208, 178, 92, 255},
        {140, 116, 186, 255}
    };
    SDL_Color base = colors[(photo_index < 0 ? 0 : photo_index) % 5];
    fill_rect(renderer, rect, base);
    for (int y = rect->y; y < rect->y + rect->h; y += 18) {
        SDL_Rect stripe = {rect->x, y, rect->w, 8};
        SDL_Color c = {
            (Uint8)(base.r > 30 ? base.r - 26 : base.r),
            (Uint8)(base.g > 30 ? base.g - 26 : base.g),
            (Uint8)(base.b > 30 ? base.b - 26 : base.b),
            130
        };
        fill_rect(renderer, &stripe, c);
    }
}

static void puzzle_draw_panel_shell(SDL_Renderer *renderer, const SDL_Rect *rect, SDL_Color fill, SDL_Color border) {
    SDL_Rect shadow = {rect->x + 4, rect->y + 6, rect->w, rect->h};
    SDL_Rect glow = {rect->x + 1, rect->y + 1, rect->w - 2, 18};
    fill_rect(renderer, &shadow, (SDL_Color){4, 7, 12, 110});
    fill_rect(renderer, rect, fill);
    stroke_rect(renderer, rect, border);
    if (glow.w > 0 && glow.h > 0) {
        fill_rect(renderer, &glow, (SDL_Color){20, 31, 46, 96});
    }
}

static void puzzle_draw_stat_chip(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *rect,
    const char *label,
    const char *value,
    SDL_Color value_color
) {
    puzzle_draw_panel_shell(renderer, rect, (SDL_Color){17, 30, 47, 206}, (SDL_Color){255, 255, 255, 26});
    arcade_draw_text(renderer, font, label, rect->x + 10, rect->y + 8, (SDL_Color){172, 185, 198, 255});
    arcade_draw_text_centered(renderer, font, value, rect, rect->y + rect->h / 2, value_color);
}

static void render_puzzle(
    const ArcadePuzzleState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel,
    const ArcadeRenderAssets *assets
) {
    SDL_Rect header = puzzle_scale_rect(panel, 24.0f, 20.0f, 1232.0f, 110.0f);
    SDL_Rect stage = puzzle_scale_rect(panel, 24.0f, 150.0f, 820.0f, 540.0f);
    SDL_Rect side = puzzle_scale_rect(panel, 870.0f, 150.0f, 386.0f, 540.0f);
    SDL_Rect image_box = puzzle_scale_rect(panel, 54.0f, 230.0f, 760.0f, 420.0f);
    SDL_Rect round_chip = puzzle_scale_rect(panel, 850.0f, 38.0f, 112.0f, 62.0f);
    SDL_Rect score_chip = puzzle_scale_rect(panel, 972.0f, 38.0f, 104.0f, 62.0f);
    SDL_Rect mode_chip = puzzle_scale_rect(panel, 1086.0f, 38.0f, 148.0f, 62.0f);
    SDL_Rect timer_box = puzzle_scale_rect(panel, 672.0f, 78.0f, 140.0f, 16.0f);
    const TextureAsset *main_photo = puzzle_photo_for_index(assets, state->puzzle_photo_index);
    SDL_Rect image_rect = puzzle_fit_texture_contain(main_photo, image_box);
    SDL_Rect slot = puzzle_slot_rect(state, image_rect);
    uint32_t elapsed = SDL_GetTicks() - state->round_started_at;
    uint32_t remaining = elapsed >= PUZZLE_ROUND_TIME_MS ? 0u : PUZZLE_ROUND_TIME_MS - elapsed;
    char text[96];

    if (assets && assets->puzzle_background && assets->puzzle_background->loaded) {
        SDL_RenderCopy(renderer, assets->puzzle_background->texture, NULL, panel);
    } else {
        fill_rect(renderer, panel, (SDL_Color){6, 10, 16, 255});
    }
    fill_rect(renderer, panel, (SDL_Color){6, 10, 16, 88});
    {
        SDL_Rect top_wash = {panel->x, panel->y, panel->w, (panel->h * 32) / 100};
        SDL_Rect bottom_wash = {panel->x, panel->y + (panel->h * 68) / 100, panel->w, (panel->h * 32) / 100};
        fill_rect(renderer, &top_wash, (SDL_Color){31, 57, 84, 68});
        fill_rect(renderer, &bottom_wash, (SDL_Color){7, 11, 18, 118});
    }

    puzzle_draw_panel_shell(renderer, &header, (SDL_Color){11, 18, 28, 198}, (SDL_Color){255, 255, 255, 26});
    puzzle_draw_panel_shell(renderer, &stage, (SDL_Color){14, 23, 34, 214}, (SDL_Color){255, 255, 255, 24});
    puzzle_draw_panel_shell(renderer, &side, (SDL_Color){18, 29, 42, 226}, (SDL_Color){255, 255, 255, 24});

    arcade_draw_text(renderer, font, "PUZZLE ROOM", header.x + 24, header.y + 10, (SDL_Color){239, 191, 102, 255});
    arcade_draw_text(renderer, font, "Find the missing fragment.", header.x + 24, header.y + 42, (SDL_Color){246, 243, 236, 255});
    arcade_draw_text(renderer, font, "Pick one piece and drop it into the empty spot.", header.x + 26, header.y + 72, (SDL_Color){172, 185, 198, 255});

    SDL_snprintf(text, sizeof(text), "ROUND %d/%d   SCORE %d/%d",
                 state->current_round + 1,
                 PUZZLE_ROUNDS_TO_PLAY,
                 state->score,
                 PUZZLE_REQUIRED_SCORE);
    {
        char round_label[24];
        char score_label[24];
        SDL_snprintf(round_label, sizeof(round_label), "%d / %d", state->current_round + 1, PUZZLE_ROUNDS_TO_PLAY);
        SDL_snprintf(score_label, sizeof(score_label), "%d", state->score);
        puzzle_draw_stat_chip(renderer, font, &round_chip, "ROUND", round_label, (SDL_Color){246, 243, 236, 255});
        puzzle_draw_stat_chip(renderer, font, &score_chip, "SCORE", score_label, (SDL_Color){125, 202, 165, 255});
        puzzle_draw_stat_chip(renderer, font, &mode_chip, "MODE", "Arcade", (SDL_Color){239, 191, 102, 255});
    }

    {
        SDL_Rect filled = timer_box;
        filled.w = (int)((timer_box.w * remaining) / PUZZLE_ROUND_TIME_MS);
        puzzle_draw_panel_shell(renderer, &timer_box, (SDL_Color){24, 38, 56, 224}, (SDL_Color){255, 255, 255, 24});
        fill_rect(renderer, &filled, remaining < 3500u ? (SDL_Color){220, 112, 112, 255} : (SDL_Color){239, 191, 102, 255});
        arcade_draw_text_centered(renderer, font, "TIME", &timer_box, timer_box.y - 22, (SDL_Color){239, 191, 102, 255});
    }

    arcade_draw_text(renderer, font, "MAIN IMAGE", stage.x + 30, stage.y + 12, (SDL_Color){239, 191, 102, 255});
    arcade_draw_text(renderer, font, "Place the right fragment in the empty spot.", stage.x + 32, stage.y + 42, (SDL_Color){246, 243, 236, 255});
    {
        SDL_Rect image_shell = {image_box.x - 12, image_box.y - 12, image_box.w + 24, image_box.h + 24};
        puzzle_draw_panel_shell(renderer, &image_shell, (SDL_Color){8, 14, 22, 214}, (SDL_Color){255, 255, 255, 16});
        fill_rect(renderer, &image_box, (SDL_Color){7, 10, 15, 255});
    }
    if (main_photo) {
        SDL_RenderCopy(renderer, main_photo->texture, NULL, &image_rect);
    } else {
        puzzle_draw_fallback_image(renderer, &image_rect, state->puzzle_photo_index);
    }

    if (state->feedback_active && state->feedback_success && main_photo) {
        SDL_Rect src = puzzle_source_rect(state, main_photo);
        SDL_RenderCopy(renderer, main_photo->texture, &src, &slot);
        stroke_rect(renderer, &slot, (SDL_Color){120, 255, 150, 255});
    } else {
        fill_rect(renderer, &slot, (SDL_Color){6, 10, 16, 220});
        stroke_rect(
            renderer,
            &slot,
            state->feedback_active && !state->feedback_success
                ? (SDL_Color){255, 110, 110, 255}
                : (SDL_Color){255, 244, 180, 220}
        );
        arcade_draw_text_centered(renderer, font, "DROP HERE", &slot, slot.y + slot.h / 2 - 8, (SDL_Color){255, 244, 180, 220});
    }

    arcade_draw_text(renderer, font, "OPTIONS", side.x + 18, side.y + 18, (SDL_Color){239, 191, 102, 255});
    arcade_draw_text(renderer, font, "Pick one piece.", side.x + 18, side.y + 52, (SDL_Color){246, 243, 236, 255});
    arcade_draw_text(renderer, font, "Drag it to the slot.", side.x + 18, side.y + 78, (SDL_Color){172, 185, 198, 255});

    {
        for (int i = 0; i < ARCADE_PUZZLE_OPTION_COUNT; ++i) {
            SDL_Rect option_rects[ARCADE_PUZZLE_OPTION_COUNT];
            SDL_Rect preview_rects[ARCADE_PUZZLE_OPTION_COUNT];
            puzzle_get_layout(state, panel, NULL, option_rects, preview_rects);
            SDL_Rect item = option_rects[i];
            SDL_Rect preview = preview_rects[i];
            const bool selected = i == state->selected_option || i == state->hover_option || i == state->drag_option;
            const TextureAsset *option_photo = puzzle_photo_for_index(assets, state->option_photo_indices[i]);
            SDL_Color item_fill = selected ? (SDL_Color){71, 108, 149, 140} : (SDL_Color){41, 63, 87, 120};
            SDL_Color border = selected ? (SDL_Color){239, 191, 102, 255} : (SDL_Color){255, 255, 255, 22};

            if (state->feedback_active && selected) {
                item_fill = state->feedback_success ? (SDL_Color){42, 110, 70, 170} : (SDL_Color){128, 54, 54, 170};
                border = state->feedback_success ? (SDL_Color){125, 202, 165, 255} : (SDL_Color){220, 112, 112, 255};
            }

            puzzle_draw_panel_shell(renderer, &item, item_fill, border);
            fill_rect(renderer, &preview, (SDL_Color){8, 12, 20, 255});
            if (state->dragging && state->drag_option == i) {
                fill_rect(renderer, &preview, (SDL_Color){10, 12, 18, 255});
                arcade_draw_text_centered(renderer, font, "DRAGGING", &preview, preview.y + preview.h / 2 - 8, (SDL_Color){255, 244, 180, 180});
            } else if (option_photo) {
                SDL_Rect src = puzzle_source_rect(state, option_photo);
                SDL_RenderCopy(renderer, option_photo->texture, &src, &preview);
            } else {
                puzzle_draw_fallback_image(renderer, &preview, state->option_photo_indices[i]);
            }
            stroke_rect(renderer, &preview, (SDL_Color){220, 230, 255, 70});
            SDL_snprintf(text, sizeof(text), "Fragment %d  |  Drag piece", i + 1);
            arcade_draw_text_centered(renderer, font, text, &item, item.y + item.h - 24, (SDL_Color){239, 191, 102, 255});
        }
    }

    if (state->dragging && state->drag_option >= 0 && state->drag_option < ARCADE_PUZZLE_OPTION_COUNT) {
        const TextureAsset *drag_photo = puzzle_photo_for_index(assets, state->option_photo_indices[state->drag_option]);
        SDL_Rect drag_rect = puzzle_scale_rect(panel, 0.0f, 0.0f, 302.0f, 64.0f);
        drag_rect.x = state->drag_x - state->drag_offset_x;
        drag_rect.y = state->drag_y - state->drag_offset_y;
        fill_rect(renderer, &drag_rect, (SDL_Color){8, 12, 20, 230});
        if (drag_photo) {
            SDL_Rect src = puzzle_source_rect(state, drag_photo);
            SDL_RenderCopy(renderer, drag_photo->texture, &src, &drag_rect);
        } else {
            puzzle_draw_fallback_image(renderer, &drag_rect, state->option_photo_indices[state->drag_option]);
        }
        stroke_rect(renderer, &drag_rect, (SDL_Color){255, 244, 180, 255});
    }

    if (state->feedback_active) {
        SDL_Rect overlay = {stage.x + (stage.w - 220) / 2, stage.y + (stage.h - 96) / 2, 220, 96};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 238});
        stroke_rect(renderer, &overlay, state->feedback_success ? (SDL_Color){120, 255, 150, 255} : (SDL_Color){255, 120, 120, 255});
        arcade_draw_text_centered(
            renderer,
            font,
            state->feedback_success ? "CORRECT" : (state->feedback_timeout ? "TIME OUT" : "WRONG PIECE"),
            &overlay,
            overlay.y + 22,
            state->feedback_success ? (SDL_Color){120, 255, 150, 255} : (SDL_Color){255, 140, 140, 255}
        );
        arcade_draw_text_centered(renderer, font, "NEXT ROUND", &overlay, overlay.y + 56, (SDL_Color){236, 238, 250, 255});
    }

    if (state->paused || state->won || state->lost) {
        SDL_Rect overlay = {
            panel->x + (panel->w - 242) / 2,
            panel->y + (panel->h - 150) / 2,
            242,
            150
        };
        SDL_Rect primary_button = {overlay.x + 42, overlay.y + 64, 158, 30};
        SDL_Rect secondary_button = {overlay.x + 42, overlay.y + 104, 158, 30};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 242});
        stroke_rect(renderer, &overlay, (SDL_Color){255, 222, 92, 255});

        if (state->paused) {
            arcade_draw_text_centered(renderer, font, "PAUSED", &overlay, overlay.y + 20, (SDL_Color){255, 228, 118, 255});
            fill_rect(renderer, &primary_button, state->overlay_selection == 0 ? (SDL_Color){64, 110, 190, 255} : (SDL_Color){40, 58, 90, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){220, 230, 255, 255});
            arcade_draw_text_centered(renderer, font, "RESUME", &primary_button, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
            fill_rect(renderer, &secondary_button, state->overlay_selection == 1 ? (SDL_Color){164, 74, 74, 255} : (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &secondary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text_centered(renderer, font, "QUIT", &secondary_button, secondary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        } else {
            arcade_draw_text_centered(renderer, font, state->won ? "YOU WIN" : "YOU LOSE", &overlay, overlay.y + 18,
                                      state->won ? (SDL_Color){120, 255, 150, 255} : (SDL_Color){255, 120, 120, 255});
            SDL_snprintf(text, sizeof(text), "SCORE %d/%d", state->score, PUZZLE_ROUNDS_TO_PLAY);
            arcade_draw_text_centered(renderer, font, text, &overlay, overlay.y + 46, (SDL_Color){236, 238, 250, 255});
            fill_rect(renderer, &primary_button, (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text_centered(renderer, font, "QUIT", &primary_button, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        }
    }
}

static void render_tetris(
    const ArcadeTetrisState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel
) {
    const int cell = 16;
    const int content_w = TETRIS_COLS * cell + 24 + 170;
    const int content_x = panel->x + (panel->w - content_w) / 2;
    const SDL_Rect board = {content_x, panel->y + 56, TETRIS_COLS * cell, TETRIS_ROWS * cell};
    const SDL_Rect side = {board.x + board.w + 24, board.y, 170, 220};
    fill_rect(renderer, &board, (SDL_Color){12, 14, 20, 255});
    stroke_rect(renderer, &board, (SDL_Color){218, 190, 88, 255});
    fill_rect(renderer, &side, (SDL_Color){20, 22, 30, 255});
    stroke_rect(renderer, &side, (SDL_Color){110, 120, 160, 255});

    arcade_draw_text(renderer, font, "TETRIS", board.x, panel->y + 16, (SDL_Color){255, 228, 118, 255});
    arcade_draw_text(renderer, font, "BACKSPACE MENU", side.x, side.y + 140, (SDL_Color){200, 206, 228, 255});

    char text[64];
    SDL_snprintf(text, sizeof(text), "SCORE %u", state->score);
    arcade_draw_text(renderer, font, text, side.x + 12, side.y + 18, (SDL_Color){236, 238, 250, 255});
    SDL_snprintf(text, sizeof(text), "LINES %u", state->lines);
    arcade_draw_text(renderer, font, text, side.x + 12, side.y + 52, (SDL_Color){236, 238, 250, 255});
    SDL_snprintf(text, sizeof(text), "BLOCKS %d/%d", state->pieces_used, TETRIS_PIECE_LIMIT);
    arcade_draw_text(renderer, font, text, side.x + 12, side.y + 86, (SDL_Color){236, 238, 250, 255});
    SDL_snprintf(text, sizeof(text), "TARGET %u", TETRIS_WIN_SCORE + 1u);
    arcade_draw_text(renderer, font, text, side.x + 12, side.y + 110, (SDL_Color){236, 238, 250, 255});
    arcade_draw_text(renderer, font, "ARROWS MOVE", side.x + 12, side.y + 144, (SDL_Color){170, 200, 255, 255});
    arcade_draw_text(renderer, font, "SPACE DROP", side.x + 12, side.y + 168, (SDL_Color){170, 200, 255, 255});

    for (int y = 0; y < TETRIS_ROWS; ++y) {
        for (int x = 0; x < TETRIS_COLS; ++x) {
            SDL_Rect cell_rect = {board.x + x * cell, board.y + y * cell, cell - 1, cell - 1};
            fill_rect(renderer, &cell_rect, TETRIS_COLORS[state->cells[y][x]]);
        }
    }

    if (!state->game_over) {
        for (int i = 0; i < 4; ++i) {
            const CellPoint block = TETRIS_SHAPES[state->current.type][state->current.rotation][i];
            SDL_Rect cell_rect = {
                board.x + (state->current.x + block.x) * cell,
                board.y + (state->current.y + block.y) * cell,
                cell - 1,
                cell - 1
            };
            fill_rect(renderer, &cell_rect, TETRIS_COLORS[state->current.type + 1]);
        }
    } else {
        arcade_draw_text(
            renderer,
            font,
            state->won ? "YOU WIN" : "YOU LOSE",
            side.x + 12,
            side.y + 194,
            state->won ? (SDL_Color){120, 255, 120, 255} : (SDL_Color){255, 110, 110, 255}
        );
    }

    if (state->paused || state->game_over) {
        SDL_Rect overlay = {board.x + 28, board.y + 86, 196, 136};
        SDL_Rect primary_button = {overlay.x + 24, overlay.y + 54, 148, 30};
        SDL_Rect secondary_button = {overlay.x + 24, overlay.y + 94, 148, 30};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 240});
        stroke_rect(renderer, &overlay, (SDL_Color){255, 222, 92, 255});

        if (state->paused) {
            arcade_draw_text(renderer, font, "PAUSED", overlay.x + 54, overlay.y + 16, (SDL_Color){255, 228, 118, 255});
            fill_rect(renderer, &primary_button, state->overlay_selection == 0 ? (SDL_Color){64, 110, 190, 255} : (SDL_Color){40, 58, 90, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){220, 230, 255, 255});
            arcade_draw_text(renderer, font, "RESUME", primary_button.x + 34, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
            fill_rect(renderer, &secondary_button, state->overlay_selection == 1 ? (SDL_Color){164, 74, 74, 255} : (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &secondary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", secondary_button.x + 52, secondary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        } else {
            arcade_draw_text(
                renderer,
                font,
                state->won ? "YOU WIN" : "YOU LOSE",
                overlay.x + 34,
                overlay.y + 16,
                state->won ? (SDL_Color){120, 255, 120, 255} : (SDL_Color){255, 120, 120, 255}
            );
            fill_rect(renderer, &primary_button, (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", primary_button.x + 52, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        }
    }
}

static void render_mines(
    const ArcadeMinesState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel
) {
    const int cell = 28;
    const SDL_Rect board = {
        panel->x + (panel->w - MINES_SIZE * cell) / 2,
        panel->y + 78,
        MINES_SIZE * cell,
        MINES_SIZE * cell
    };
    fill_rect(renderer, &board, (SDL_Color){24, 28, 34, 255});
    stroke_rect(renderer, &board, (SDL_Color){218, 190, 88, 255});

    arcade_draw_text(renderer, font, "MINESWEEPER", board.x, panel->y + 18, (SDL_Color){255, 228, 118, 255});
    arcade_draw_text(renderer, font, "ENTER OPEN   F FLAG   ESC PAUSE", board.x - 10, panel->y + 42, (SDL_Color){210, 218, 236, 255});
    arcade_draw_text(renderer, font, "BACKSPACE MENU", board.x + 160, panel->y + 18, (SDL_Color){210, 218, 236, 255});

    char text[64];
    SDL_snprintf(text, sizeof(text), "FLAGS %d/%d", state->flags_used, MINES_COUNT);
    arcade_draw_text(renderer, font, text, board.x + 88, board.y + board.h + 18, (SDL_Color){170, 200, 255, 255});

    for (int y = 0; y < MINES_SIZE; ++y) {
        for (int x = 0; x < MINES_SIZE; ++x) {
            const ArcadeMineCell *cell_state = &state->cells[y][x];
            SDL_Rect cell_rect = {board.x + x * cell, board.y + y * cell, cell - 2, cell - 2};
            SDL_Color fill = cell_state->revealed
                ? (cell_state->has_mine ? (SDL_Color){180, 50, 50, 255} : (SDL_Color){196, 205, 188, 255})
                : (SDL_Color){73, 87, 102, 255};
            fill_rect(renderer, &cell_rect, fill);
            if (x == state->cursor_x && y == state->cursor_y) {
                stroke_rect(renderer, &cell_rect, (SDL_Color){255, 222, 92, 255});
            }

            if (cell_state->flagged && !cell_state->revealed) {
                arcade_draw_text(renderer, font, "F", cell_rect.x + 8, cell_rect.y + 4, (SDL_Color){255, 96, 96, 255});
            } else if (cell_state->revealed && cell_state->has_mine) {
                arcade_draw_text(renderer, font, "*", cell_rect.x + 8, cell_rect.y + 4, (SDL_Color){255, 255, 255, 255});
            } else if (cell_state->revealed && cell_state->adjacent > 0) {
                char num[2] = {(char)('0' + cell_state->adjacent), '\0'};
                arcade_draw_text(renderer, font, num, cell_rect.x + 8, cell_rect.y + 4, (SDL_Color){22, 60, 120, 255});
            }
        }
    }

    if (state->won) {
        arcade_draw_text(renderer, font, "CLEAR", board.x + 106, board.y + 116, (SDL_Color){80, 220, 120, 255});
    } else if (state->lost) {
        arcade_draw_text(renderer, font, "BOOM", board.x + 110, board.y + 116, (SDL_Color){255, 110, 110, 255});
    }

    if (state->paused || state->won || state->lost) {
        SDL_Rect overlay = {board.x + 30, board.y + 96, 220, 140};
        SDL_Rect primary_button = {overlay.x + 36, overlay.y + 58, 148, 30};
        SDL_Rect secondary_button = {overlay.x + 36, overlay.y + 98, 148, 30};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 240});
        stroke_rect(renderer, &overlay, (SDL_Color){255, 222, 92, 255});

        if (state->paused) {
            arcade_draw_text(renderer, font, "PAUSED", overlay.x + 66, overlay.y + 18, (SDL_Color){255, 228, 118, 255});
            fill_rect(renderer, &primary_button, state->overlay_selection == 0 ? (SDL_Color){64, 110, 190, 255} : (SDL_Color){40, 58, 90, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){220, 230, 255, 255});
            arcade_draw_text(renderer, font, "RESUME", primary_button.x + 34, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
            fill_rect(renderer, &secondary_button, state->overlay_selection == 1 ? (SDL_Color){164, 74, 74, 255} : (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &secondary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", secondary_button.x + 52, secondary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        } else {
            arcade_draw_text(renderer, font, state->won ? "YOU WIN" : "YOU LOSE", overlay.x + 58, overlay.y + 18, state->won ? (SDL_Color){120, 255, 120, 255} : (SDL_Color){255, 120, 120, 255});
            fill_rect(renderer, &primary_button, (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", primary_button.x + 52, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        }
    }
}

static void render_space(
    const ArcadeSpaceState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel
) {
    SDL_Rect play = {panel->x + 24, panel->y + 52, panel->w - 48, panel->h - 76};
    fill_rect(renderer, &play, (SDL_Color){8, 12, 24, 255});
    stroke_rect(renderer, &play, (SDL_Color){80, 140, 255, 255});
    arcade_draw_text(renderer, font, "SPACE INVADERS", play.x, panel->y + 18, (SDL_Color){255, 228, 118, 255});
    arcade_draw_text(renderer, font, "MOVE SHOOT ESC PAUSE", play.x + 255, panel->y + 18, (SDL_Color){210, 218, 236, 255});

    char text[64];
    SDL_snprintf(text, sizeof(text), "SCORE %d", state->score);
    arcade_draw_text(renderer, font, text, play.x + 14, play.y + 10, (SDL_Color){210, 240, 255, 255});
    SDL_snprintf(text, sizeof(text), "LIVES %d", state->lives);
    arcade_draw_text(renderer, font, text, play.x + 150, play.y + 10, (SDL_Color){210, 240, 255, 255});

    for (int i = 0; i < SPACE_ENEMY_COUNT; ++i) {
        if (!state->enemies[i].alive) {
            continue;
        }
        SDL_Rect enemy = {play.x + (int)state->enemies[i].x, play.y + (int)state->enemies[i].y, 30, 20};
        fill_rect(renderer, &enemy, (SDL_Color){120, 255, 120, 255});
    }

    SDL_Rect player = {play.x + (int)state->player_x, play.y + 316, 28, 16};
    fill_rect(renderer, &player, (SDL_Color){255, 255, 255, 255});

    if (state->player_shot.alive) {
        SDL_Rect shot = {play.x + (int)state->player_shot.x, play.y + (int)state->player_shot.y, 3, 10};
        fill_rect(renderer, &shot, (SDL_Color){255, 220, 90, 255});
    }

    for (int i = 0; i < (int)(sizeof(state->enemy_shots) / sizeof(state->enemy_shots[0])); ++i) {
        if (!state->enemy_shots[i].alive) {
            continue;
        }
        SDL_Rect shot = {play.x + (int)state->enemy_shots[i].x, play.y + (int)state->enemy_shots[i].y, 3, 10};
        fill_rect(renderer, &shot, (SDL_Color){255, 80, 80, 255});
    }

    if (state->won) {
        arcade_draw_text(renderer, font, "VICTORY", play.x + 360, play.y + 140, (SDL_Color){120, 255, 120, 255});
    } else if (state->lost) {
        arcade_draw_text(renderer, font, "GAME OVER", play.x + 334, play.y + 140, (SDL_Color){255, 110, 110, 255});
    }

    if (state->paused || state->won || state->lost) {
        SDL_Rect overlay = {play.x + 150, play.y + 92, 220, 140};
        SDL_Rect primary_button = {overlay.x + 36, overlay.y + 58, 148, 30};
        SDL_Rect secondary_button = {overlay.x + 36, overlay.y + 98, 148, 30};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 240});
        stroke_rect(renderer, &overlay, (SDL_Color){255, 222, 92, 255});

        if (state->paused) {
            arcade_draw_text(renderer, font, "PAUSED", overlay.x + 66, overlay.y + 18, (SDL_Color){255, 228, 118, 255});
            fill_rect(renderer, &primary_button, state->overlay_selection == 0 ? (SDL_Color){64, 110, 190, 255} : (SDL_Color){40, 58, 90, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){220, 230, 255, 255});
            arcade_draw_text(renderer, font, "RESUME", primary_button.x + 34, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
            fill_rect(renderer, &secondary_button, state->overlay_selection == 1 ? (SDL_Color){164, 74, 74, 255} : (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &secondary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", secondary_button.x + 52, secondary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        } else {
            arcade_draw_text(renderer, font, state->won ? "YOU WIN" : "YOU LOSE", overlay.x + 58, overlay.y + 18, state->won ? (SDL_Color){120, 255, 120, 255} : (SDL_Color){255, 120, 120, 255});
            fill_rect(renderer, &primary_button, (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", primary_button.x + 52, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        }
    }
}

static SDL_Color maze_column_color_for_tile(int wall_type) {
    switch (wall_type) {
        case MAZE_TILE_FINISH:
            return (SDL_Color){100, 170, 102, 255};
        case MAZE_TILE_SWALL:
            return (SDL_Color){124, 84, 62, 255};
        case MAZE_TILE_FAN:
            return (SDL_Color){124, 130, 160, 255};
        default:
            return (SDL_Color){118, 118, 138, 255};
    }
}

static void maze_draw_background(SDL_Renderer *renderer, const SDL_Rect *view) {
    SDL_Rect ceiling = {view->x, view->y, view->w, view->h / 2};
    SDL_Rect floor = {view->x, view->y + view->h / 2, view->w, view->h - view->h / 2};
    fill_rect(renderer, &ceiling, (SDL_Color){54, 54, 54, 255});
    fill_rect(renderer, &floor, (SDL_Color){34, 34, 34, 255});
}

static void maze_render_scene(
    const ArcadeMazeState *state,
    SDL_Renderer *renderer,
    const SDL_Rect *view,
    const ArcadeRenderAssets *assets
) {
    const TextureAsset *wall_texture = assets ? assets->maze_wall_texture : NULL;
    const TextureAsset *finish_texture = assets ? assets->maze_finish_texture : NULL;
    const TextureAsset *swall_texture = assets ? assets->maze_swall_texture : NULL;
    const TextureAsset *fan_texture = assets ? assets->maze_fan_texture : NULL;
    int fan_cols = 1;
    int fan_rows = 1;
    int fan_frame_count = 1;

    if (!state || !renderer || !view || view->w <= 0 || view->h <= 0) {
        return;
    }

    if (fan_texture && fan_texture->loaded && fan_texture->w > 0 && fan_texture->h > 0) {
        maze_detect_fan_sheet_layout(fan_texture->w, fan_texture->h, &fan_cols, &fan_rows);
        fan_frame_count = fan_cols * fan_rows;
        if (fan_frame_count < 1) {
            fan_frame_count = 1;
            fan_cols = 1;
            fan_rows = 1;
        }
    }

    maze_draw_background(renderer, view);

    {
        const float dir_x = cosf(state->angle);
        const float dir_y = sinf(state->angle);
        const float plane_x = -dir_y * 0.57735026919f;
        const float plane_y = dir_x * 0.57735026919f;

        for (int x = 0; x < view->w; ++x) {
            const float camera_x = (2.0f * x / (float)view->w) - 1.0f;
            const float ray_dir_x = dir_x + plane_x * camera_x;
            const float ray_dir_y = dir_y + plane_y * camera_x;

            int map_x = (int)state->x;
            int map_y = (int)state->y;
            const float delta_dist_x = (fabsf(ray_dir_x) < 0.00001f) ? 1e30f : fabsf(1.0f / ray_dir_x);
            const float delta_dist_y = (fabsf(ray_dir_y) < 0.00001f) ? 1e30f : fabsf(1.0f / ray_dir_y);
            int step_x = 0;
            int step_y = 0;
            float side_dist_x = 0.0f;
            float side_dist_y = 0.0f;
            int side = 0;

            if (ray_dir_x < 0.0f) {
                step_x = -1;
                side_dist_x = (state->x - (float)map_x) * delta_dist_x;
            } else {
                step_x = 1;
                side_dist_x = ((float)map_x + 1.0f - state->x) * delta_dist_x;
            }

            if (ray_dir_y < 0.0f) {
                step_y = -1;
                side_dist_y = (state->y - (float)map_y) * delta_dist_y;
            } else {
                step_y = 1;
                side_dist_y = ((float)map_y + 1.0f - state->y) * delta_dist_y;
            }

            while (true) {
                if (side_dist_x < side_dist_y) {
                    side_dist_x += delta_dist_x;
                    map_x += step_x;
                    side = 0;
                } else {
                    side_dist_y += delta_dist_y;
                    map_y += step_y;
                    side = 1;
                }

                if (map_x < 0 || map_x >= MAZE_MAP_WIDTH || map_y < 0 || map_y >= MAZE_MAP_HEIGHT) {
                    break;
                }

                if (maze_tile_is_solid(MAZE_MAP[map_y][map_x])) {
                    break;
                }
            }

            int wall_type = 1;
            if (map_x >= 0 && map_x < MAZE_MAP_WIDTH && map_y >= 0 && map_y < MAZE_MAP_HEIGHT) {
                wall_type = MAZE_MAP[map_y][map_x];
            }

            const TextureAsset *column_texture = wall_texture;
            bool draw_fan_overlay = false;
            if (wall_type == MAZE_TILE_FINISH && finish_texture && finish_texture->loaded) {
                column_texture = finish_texture;
            } else if (wall_type == MAZE_TILE_SWALL && swall_texture && swall_texture->loaded) {
                column_texture = swall_texture;
            }
            if (wall_type == MAZE_TILE_FAN &&
                fan_texture && fan_texture->loaded &&
                fan_texture->w > 0 && fan_texture->h > 0 &&
                fan_frame_count > 0) {
                draw_fan_overlay = true;
            }

            {
                float perp_wall_dist = 0.0f;
                if (side == 0) {
                    perp_wall_dist = side_dist_x - delta_dist_x;
                } else {
                    perp_wall_dist = side_dist_y - delta_dist_y;
                }
                if (perp_wall_dist < 0.03f) {
                    perp_wall_dist = 0.03f;
                }

                {
                    const int line_height = (int)((float)view->h / perp_wall_dist);
                    const int draw_start_raw = view->y + (-line_height / 2) + (view->h / 2);
                    const int draw_end_raw = view->y + (line_height / 2) + (view->h / 2);
                    int draw_start = draw_start_raw;
                    int draw_end = draw_end_raw;

                    if (draw_start < view->y) {
                        draw_start = view->y;
                    }
                    if (draw_end >= view->y + view->h) {
                        draw_end = view->y + view->h - 1;
                    }
                    if (draw_end < draw_start) {
                        continue;
                    }

                    {
                        float wall_x = 0.0f;
                        if (side == 0) {
                            wall_x = state->y + perp_wall_dist * ray_dir_y;
                        } else {
                            wall_x = state->x + perp_wall_dist * ray_dir_x;
                        }
                        wall_x -= floorf(wall_x);

                        const Uint8 mod = (Uint8)(55.0f + 200.0f * ((side == 1 ? 0.78f : 1.0f) /
                            (1.0f + 0.12f * perp_wall_dist * perp_wall_dist)));
                        const SDL_Rect dst = {view->x + x, draw_start, 1, draw_end - draw_start + 1};

                        if (column_texture && column_texture->loaded && column_texture->w > 0 && column_texture->h > 0) {
                            int tex_x = (int)(wall_x * (float)column_texture->w);
                            if (tex_x < 0) {
                                tex_x = 0;
                            } else if (tex_x >= column_texture->w) {
                                tex_x = column_texture->w - 1;
                            }

                            if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f)) {
                                tex_x = column_texture->w - tex_x - 1;
                            }

                            {
                                const float tex_step = (line_height > 0)
                                    ? ((float)column_texture->h / (float)line_height)
                                    : 0.0f;
                                int tex_y = (int)((draw_start - draw_start_raw) * tex_step);
                                int tex_h = (int)(dst.h * tex_step);
                                if (tex_y < 0) {
                                    tex_y = 0;
                                }
                                if (tex_h < 1) {
                                    tex_h = 1;
                                }
                                if (tex_y + tex_h > column_texture->h) {
                                    tex_h = column_texture->h - tex_y;
                                }
                                if (tex_h < 1) {
                                    tex_h = 1;
                                }

                                SDL_Rect src = {tex_x, tex_y, 1, tex_h};
                                SDL_SetTextureColorMod(column_texture->texture, mod, mod, mod);
                                SDL_RenderCopy(renderer, column_texture->texture, &src, &dst);
                            }
                        } else {
                            SDL_Color color = maze_column_color_for_tile(wall_type);
                            color.r = (Uint8)((color.r * mod) / 255);
                            color.g = (Uint8)((color.g * mod) / 255);
                            color.b = (Uint8)((color.b * mod) / 255);
                            fill_rect(renderer, &dst, color);
                        }

                        if (draw_fan_overlay) {
                            const int frame_w = fan_texture->w / fan_cols;
                            const int frame_h = fan_texture->h / fan_rows;
                            if (frame_w > 0 && frame_h > 0) {
                                const float fan_half_width = MAZE_FAN_OVERLAY_WIDTH_FRACTION * 0.5f;
                                const float fan_left = 0.5f - fan_half_width;
                                const float fan_right = 0.5f + fan_half_width;
                                if (wall_x >= fan_left && wall_x <= fan_right) {
                                    const int frame_index = (int)((SDL_GetTicks() / MAZE_FAN_FRAME_TIME_MS) % (Uint32)fan_frame_count);
                                    const int frame_col = frame_index % fan_cols;
                                    const int frame_row = frame_index / fan_cols;
                                    float fan_u = (wall_x - fan_left) / (fan_right - fan_left);

                                    if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f)) {
                                        fan_u = 1.0f - fan_u;
                                    }

                                    {
                                        int fan_tex_x = (int)(fan_u * (float)frame_w);
                                        if (fan_tex_x < 0) {
                                            fan_tex_x = 0;
                                        } else if (fan_tex_x >= frame_w) {
                                            fan_tex_x = frame_w - 1;
                                        }

                                        {
                                            const int fan_draw_start_raw = draw_start_raw +
                                                (int)((line_height * (1.0f - MAZE_FAN_OVERLAY_HEIGHT_FRACTION)) * 0.5f);
                                            const int fan_draw_end_raw = fan_draw_start_raw +
                                                (int)(line_height * MAZE_FAN_OVERLAY_HEIGHT_FRACTION);
                                            int fan_draw_start = fan_draw_start_raw;
                                            int fan_draw_end = fan_draw_end_raw;

                                            if (fan_draw_start < draw_start) {
                                                fan_draw_start = draw_start;
                                            }
                                            if (fan_draw_end > draw_end) {
                                                fan_draw_end = draw_end;
                                            }

                                            if (fan_draw_end >= fan_draw_start) {
                                                int fan_draw_h = fan_draw_end - fan_draw_start + 1;
                                                int fan_raw_h = fan_draw_end_raw - fan_draw_start_raw + 1;
                                                if (fan_raw_h < 1) {
                                                    fan_raw_h = 1;
                                                }

                                                int fan_tex_y = (int)(((fan_draw_start - fan_draw_start_raw) / (float)fan_raw_h) * frame_h);
                                                int fan_tex_h = (int)((fan_draw_h / (float)fan_raw_h) * frame_h);
                                                if (fan_tex_y < 0) {
                                                    fan_tex_y = 0;
                                                }
                                                if (fan_tex_h < 1) {
                                                    fan_tex_h = 1;
                                                }
                                                if (fan_tex_y + fan_tex_h > frame_h) {
                                                    fan_tex_h = frame_h - fan_tex_y;
                                                }
                                                if (fan_tex_h < 1) {
                                                    fan_tex_h = 1;
                                                }

                                                {
                                                    SDL_Rect fan_src = {
                                                        frame_col * frame_w + fan_tex_x,
                                                        frame_row * frame_h + fan_tex_y,
                                                        1,
                                                        fan_tex_h
                                                    };
                                                    SDL_Rect fan_dst = {view->x + x, fan_draw_start, 1, fan_draw_h};
                                                    SDL_SetTextureColorMod(fan_texture->texture, mod, mod, mod);
                                                    SDL_RenderCopy(renderer, fan_texture->texture, &fan_src, &fan_dst);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (wall_texture && wall_texture->loaded) {
        SDL_SetTextureColorMod(wall_texture->texture, 255, 255, 255);
    }
    if (finish_texture && finish_texture->loaded) {
        SDL_SetTextureColorMod(finish_texture->texture, 255, 255, 255);
    }
    if (swall_texture && swall_texture->loaded) {
        SDL_SetTextureColorMod(swall_texture->texture, 255, 255, 255);
    }
    if (fan_texture && fan_texture->loaded) {
        SDL_SetTextureColorMod(fan_texture->texture, 255, 255, 255);
    }
}

static void maze_render_car_overlay(
    SDL_Renderer *renderer,
    const SDL_Rect *view,
    const TextureAsset *car_texture
) {
    if (!renderer || !view || !car_texture || !car_texture->loaded || car_texture->w <= 0 || car_texture->h <= 0) {
        return;
    }

    {
        int dst_w = (view->w * 58) / 100;
        int dst_h = (int)((float)dst_w * ((float)car_texture->h / (float)car_texture->w));
        const int max_h = (view->h * 68) / 100;

        if (dst_h > max_h) {
            dst_h = max_h;
            dst_w = (int)((float)dst_h * ((float)car_texture->w / (float)car_texture->h));
        }
        if (dst_w < 1) {
            dst_w = 1;
        }
        if (dst_h < 1) {
            dst_h = 1;
        }

        {
            SDL_Rect dst = {
                view->x + (view->w - dst_w) / 2,
                view->y + view->h - dst_h,
                dst_w,
                dst_h
            };
            SDL_RenderCopy(renderer, car_texture->texture, NULL, &dst);
        }
    }
}

static void maze_render_minimap(
    const ArcadeMazeState *state,
    SDL_Renderer *renderer,
    const SDL_Rect *rect
) {
    if (!state || !renderer || !rect || rect->w <= 0 || rect->h <= 0) {
        return;
    }

    fill_rect(renderer, rect, (SDL_Color){18, 20, 28, 255});
    stroke_rect(renderer, rect, (SDL_Color){100, 116, 152, 255});

    {
        const int cell = rect->w / MAZE_MAP_WIDTH;
        if (cell < 2) {
            return;
        }

        for (int y = 0; y < MAZE_MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAZE_MAP_WIDTH; ++x) {
                SDL_Color color = {52, 58, 70, 255};
                SDL_Rect cell_rect = {
                    rect->x + x * cell + 1,
                    rect->y + y * cell + 1,
                    cell - 2,
                    cell - 2
                };

                switch (MAZE_MAP[y][x]) {
                    case 0:
                        color = (SDL_Color){64, 74, 90, 255};
                        break;
                    case MAZE_TILE_FINISH:
                        color = (SDL_Color){90, 190, 120, 255};
                        break;
                    case MAZE_TILE_SPAWN:
                        color = (SDL_Color){80, 156, 206, 255};
                        break;
                    case MAZE_TILE_FAN:
                        color = (SDL_Color){208, 170, 84, 255};
                        break;
                    case MAZE_TILE_SWALL:
                        color = (SDL_Color){140, 96, 74, 255};
                        break;
                    default:
                        color = (SDL_Color){36, 40, 48, 255};
                        break;
                }

                fill_rect(renderer, &cell_rect, color);
            }
        }

        {
            const int player_size = cell > 6 ? 6 : cell - 1;
            SDL_Rect player = {
                rect->x + (int)(state->x * cell) - player_size / 2,
                rect->y + (int)(state->y * cell) - player_size / 2,
                player_size,
                player_size
            };
            fill_rect(renderer, &player, (SDL_Color){255, 244, 164, 255});
        }
    }
}

static void render_maze(
    const ArcadeMazeState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel,
    const ArcadeRenderAssets *assets
) {
    const int side_w = panel->w >= 470 ? 150 : 0;
    SDL_Rect view = {
        panel->x + 20,
        panel->y + 64,
        panel->w - 40 - (side_w > 0 ? side_w + 14 : 0),
        panel->h - 94
    };

    if (view.w < 140) {
        view.w = 140;
    }
    if (view.h < 120) {
        view.h = 120;
    }

    arcade_draw_text(renderer, font, "CAR MAZE", view.x, panel->y + 18, (SDL_Color){255, 228, 118, 255});
    arcade_draw_text(renderer, font, "UP/DOWN DRIVE   LEFT/RIGHT TURN", view.x + 110, panel->y + 18, (SDL_Color){210, 218, 236, 255});

    if (side_w > 0) {
        SDL_Rect side = {view.x + view.w + 14, view.y, side_w, view.h};
        SDL_Rect minimap = {
            side.x + 15,
            side.y + 132,
            side.w - 30,
            side.w - 30
        };

        fill_rect(renderer, &side, (SDL_Color){16, 20, 32, 255});
        stroke_rect(renderer, &side, (SDL_Color){90, 106, 140, 255});
        arcade_draw_text(renderer, font, "GOAL", side.x + 18, side.y + 16, (SDL_Color){255, 228, 118, 255});
        arcade_draw_text(renderer, font, "Reach the", side.x + 18, side.y + 46, (SDL_Color){214, 220, 240, 255});
        arcade_draw_text(renderer, font, "green exit", side.x + 18, side.y + 70, (SDL_Color){120, 255, 120, 255});
        arcade_draw_text(renderer, font, "tile.", side.x + 18, side.y + 94, (SDL_Color){214, 220, 240, 255});
        maze_render_minimap(state, renderer, &minimap);
        arcade_draw_text(renderer, font, "BACKSPACE MENU", side.x + 8, side.y + side.h - 30, (SDL_Color){170, 200, 255, 255});

        if (state->won) {
            arcade_draw_text(renderer, font, "EXIT FOUND", side.x + 14, side.y + side.h - 58, (SDL_Color){120, 255, 120, 255});
        }
    } else {
        arcade_draw_text(renderer, font, "BACKSPACE MENU", view.x + 212, panel->y + 18, (SDL_Color){210, 218, 236, 255});
    }

    fill_rect(renderer, &view, (SDL_Color){10, 10, 12, 255});
    SDL_RenderSetClipRect(renderer, &view);
    maze_render_scene(state, renderer, &view, assets);
    maze_render_car_overlay(renderer, &view, assets ? assets->maze_car_texture : NULL);
    SDL_RenderSetClipRect(renderer, NULL);
    stroke_rect(renderer, &view, (SDL_Color){218, 190, 88, 255});

    if (state->paused || state->won) {
        SDL_Rect overlay = {
            view.x + (view.w - 220) / 2,
            view.y + (view.h - 140) / 2,
            220,
            140
        };
        SDL_Rect primary_button = {overlay.x + 36, overlay.y + 58, 148, 30};
        SDL_Rect secondary_button = {overlay.x + 36, overlay.y + 98, 148, 30};
        fill_rect(renderer, &overlay, (SDL_Color){12, 16, 30, 240});
        stroke_rect(renderer, &overlay, (SDL_Color){255, 222, 92, 255});

        if (state->paused) {
            arcade_draw_text(renderer, font, "PAUSED", overlay.x + 66, overlay.y + 18, (SDL_Color){255, 228, 118, 255});
            fill_rect(renderer, &primary_button, state->overlay_selection == 0 ? (SDL_Color){64, 110, 190, 255} : (SDL_Color){40, 58, 90, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){220, 230, 255, 255});
            arcade_draw_text(renderer, font, "RESUME", primary_button.x + 34, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
            fill_rect(renderer, &secondary_button, state->overlay_selection == 1 ? (SDL_Color){164, 74, 74, 255} : (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &secondary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", secondary_button.x + 52, secondary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        } else {
            arcade_draw_text(renderer, font, "YOU WIN", overlay.x + 58, overlay.y + 18, (SDL_Color){120, 255, 120, 255});
            fill_rect(renderer, &primary_button, (SDL_Color){90, 44, 44, 255});
            stroke_rect(renderer, &primary_button, (SDL_Color){255, 220, 220, 255});
            arcade_draw_text(renderer, font, "QUIT", primary_button.x + 52, primary_button.y + 5, (SDL_Color){255, 255, 255, 255});
        }
    }
}

void arcade_render(
    const ArcadeState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel,
    const ArcadeRenderAssets *assets
) {
    if (!state->active || !renderer || !panel) {
        return;
    }

    const TextureAsset *tic_texture = assets ? assets->tic_texture : NULL;
    const TextureAsset *x_texture = assets ? assets->x_texture : NULL;
    SDL_Rect inner = {panel->x + 18, panel->y + 18, panel->w - 36, panel->h - 36};
    fill_rect(renderer, &inner, (SDL_Color){14, 14, 20, 220});

    if (state->screen == ARCADE_SCREEN_MENU) {
        arcade_draw_text(renderer, font, "ARCADE SELECT", inner.x + 26, inner.y + 20, (SDL_Color){255, 228, 118, 255});
        arcade_draw_text(renderer, font, "ENTER PLAY   ESC CLOSE", inner.x + 280, inner.y + 22, (SDL_Color){208, 214, 236, 255});

        const int menu_top = inner.y + 76;
        const int menu_bottom_margin = 20;
        const int item_h = 48;
        int menu_gap = 10;
        if (ARCADE_MENU_COUNT > 1) {
            const int available_gap = inner.h - (menu_top - inner.y) - menu_bottom_margin - item_h * ARCADE_MENU_COUNT;
            menu_gap = available_gap / (ARCADE_MENU_COUNT - 1);
            if (menu_gap < 8) {
                menu_gap = 8;
            }
        }

        for (int i = 0; i < ARCADE_MENU_COUNT; ++i) {
            SDL_Rect item = {inner.x + 44, menu_top + i * (item_h + menu_gap), inner.w - 88, item_h};
            const bool selected = i == state->menu_selection;
            fill_rect(renderer, &item, selected ? (SDL_Color){60, 78, 132, 255} : (SDL_Color){28, 34, 48, 255});
            stroke_rect(renderer, &item, selected ? (SDL_Color){255, 222, 92, 255} : (SDL_Color){88, 102, 132, 255});
            arcade_draw_text(
                renderer,
                font,
                ARCADE_MENU_LABELS[i],
                item.x + 22,
                item.y + 11,
                selected ? (SDL_Color){255, 244, 208, 255} : (SDL_Color){214, 220, 240, 255}
            );

            if (state->results[i] != ARCADE_RESULT_NONE) {
                const TextureAsset *badge = state->results[i] == ARCADE_RESULT_WIN ? tic_texture : x_texture;
                SDL_Rect badge_rect = {item.x + item.w - 38, item.y + 7, 34, 34};
                if (badge && badge->loaded) {
                    SDL_RenderCopy(renderer, badge->texture, NULL, &badge_rect);
                } else {
                    arcade_draw_text(
                        renderer,
                        font,
                        state->results[i] == ARCADE_RESULT_WIN ? "OK" : "X",
                        badge_rect.x,
                        badge_rect.y,
                        state->results[i] == ARCADE_RESULT_WIN
                            ? (SDL_Color){120, 255, 120, 255}
                            : (SDL_Color){255, 120, 120, 255}
                    );
                }
            }
        }
        return;
    }

    if (state->screen == ARCADE_SCREEN_TETRIS) {
        render_tetris(&state->tetris, renderer, font, &inner);
    } else if (state->screen == ARCADE_SCREEN_MINES) {
        render_mines(&state->mines, renderer, font, &inner);
    } else if (state->screen == ARCADE_SCREEN_SPACE) {
        render_space(&state->space, renderer, font, &inner);
    } else if (state->screen == ARCADE_SCREEN_MAZE) {
        render_maze(&state->maze, renderer, font, &inner, assets);
    } else if (state->screen == ARCADE_SCREEN_PUZZLE) {
        render_puzzle(&state->puzzle, renderer, font, &inner, assets);
    }
}
