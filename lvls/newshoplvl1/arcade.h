#ifndef SDLVERSIONSHOP_ARCADE_H
#define SDLVERSIONSHOP_ARCADE_H

#include "game.h"
#include "input.h"

#define ARCADE_RESULT_COUNT 5
#define ARCADE_PUZZLE_OPTION_COUNT 3

typedef enum {
    ARCADE_SCREEN_MENU = 0,
    ARCADE_SCREEN_TETRIS,
    ARCADE_SCREEN_MINES,
    ARCADE_SCREEN_SPACE,
    ARCADE_SCREEN_MAZE,
    ARCADE_SCREEN_PUZZLE
} ArcadeScreen;

typedef enum {
    ARCADE_RESULT_NONE = 0,
    ARCADE_RESULT_WIN,
    ARCADE_RESULT_LOSE
} ArcadeResult;

typedef struct {
    int type;
    int rotation;
    int x;
    int y;
} ArcadeTetrisPiece;

typedef struct {
    int cells[20][10];
    ArcadeTetrisPiece current;
    ArcadeTetrisPiece next;
    uint32_t score;
    uint32_t lines;
    bool game_over;
    bool won;
    bool paused;
    int overlay_selection;
    int pieces_used;
    uint32_t fall_started_at;
} ArcadeTetrisState;

typedef struct {
    bool has_mine;
    bool revealed;
    bool flagged;
    uint8_t adjacent;
} ArcadeMineCell;

typedef struct {
    ArcadeMineCell cells[10][10];
    int cursor_x;
    int cursor_y;
    int revealed_safe_cells;
    int flags_used;
    bool first_click;
    bool won;
    bool lost;
    bool paused;
    int overlay_selection;
} ArcadeMinesState;

typedef struct {
    float x;
    float y;
    bool alive;
} ArcadeEnemy;

typedef struct {
    float x;
    float y;
    bool alive;
} ArcadeShot;

typedef struct {
    float player_x;
    ArcadeEnemy enemies[18];
    ArcadeShot player_shot;
    ArcadeShot enemy_shots[6];
    int enemy_direction;
    int score;
    int lives;
    int alive_count;
    bool won;
    bool lost;
    bool paused;
    int overlay_selection;
    uint32_t last_enemy_step_at;
    uint32_t last_enemy_fire_at;
} ArcadeSpaceState;

typedef struct {
    float x;
    float y;
    float angle;
    bool won;
    bool paused;
    int overlay_selection;
    uint32_t last_updated_at;
} ArcadeMazeState;

typedef struct {
    int photo_count;
    int current_round;
    int score;
    int puzzle_photo_index;
    int option_photo_indices[ARCADE_PUZZLE_OPTION_COUNT];
    int correct_option;
    int selected_option;
    int hover_option;
    int drag_option;
    bool dragging;
    bool mouse_was_down;
    int drag_x;
    int drag_y;
    int drag_offset_x;
    int drag_offset_y;
    int src_x_permil;
    int src_y_permil;
    int src_w_permil;
    int src_h_permil;
    bool won;
    bool lost;
    bool paused;
    bool feedback_active;
    bool feedback_success;
    bool feedback_timeout;
    int overlay_selection;
    uint32_t round_started_at;
    uint32_t feedback_until;
} ArcadePuzzleState;

typedef struct {
    const TextureAsset *tic_texture;
    const TextureAsset *x_texture;
    const TextureAsset *maze_wall_texture;
    const TextureAsset *maze_finish_texture;
    const TextureAsset *maze_swall_texture;
    const TextureAsset *maze_fan_texture;
    const TextureAsset *maze_car_texture;
    const TextureAsset *puzzle_background;
    const TextureAsset *puzzle_photos;
    int puzzle_photo_count;
} ArcadeRenderAssets;

typedef struct {
    bool active;
    ArcadeScreen screen;
    int menu_selection;
    bool finish_input_ready;
    ArcadeScreen finish_input_screen;
    int pending_key_reward;
    ArcadeResult results[ARCADE_RESULT_COUNT];
    ArcadeTetrisState tetris;
    ArcadeMinesState mines;
    ArcadeSpaceState space;
    ArcadeMazeState maze;
    ArcadePuzzleState puzzle;
} ArcadeState;

void arcade_init(ArcadeState *state);
void arcade_set_puzzle_photo_count(ArcadeState *state, int photo_count);
void arcade_open(ArcadeState *state, uint32_t now_ms);
void arcade_close(ArcadeState *state);
bool arcade_handle_escape(ArcadeState *state);
void arcade_update(ArcadeState *state, const InputState *input, uint32_t now_ms, const SDL_Rect *panel);
int arcade_take_pending_key_reward(ArcadeState *state);
void arcade_render(
    const ArcadeState *state,
    SDL_Renderer *renderer,
    TTF_Font *font,
    const SDL_Rect *panel,
    const ArcadeRenderAssets *assets
);

#endif
