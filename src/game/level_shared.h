#ifndef LEVEL_SHARED_H
#define LEVEL_SHARED_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "game_session.h"

typedef struct {
    int level_number;
    int world_w;
    int world_h;
    int start_x;
    int finish_x;
    int floor_y;
    int player_w;
    int player_h;
    float player_speed;
    SDL_Color bg_top;
    SDL_Color bg_bottom;
    SDL_Color ground_color;
    SDL_Color finish_color;
    char objective_text[128];
} LevelConfig;

void level_config_set_defaults(LevelConfig* cfg, int level_number);

typedef struct {
    float x;
    float y;
    float vy;
    int moving;
    int on_ground;
    int jump_was_down;
    int facing;
    int frame;
    float frame_timer;
    int reached_finish;
    int skin_id;
    int control_scheme;
    SDL_Color tint;
} LevelPlayer;

typedef struct {
    SDL_Texture* idle_sheet;
    SDL_Texture* run_sheet;
    int skin_id;
    int frame_cols;
    int idle_rows;
    int run_rows;
    int idle_frame_w;
    int idle_frame_h;
    int run_frame_w;
    int run_frame_h;
} LevelPlayerSkin;

typedef struct {
    SDL_Renderer* renderer;
    LevelPlayerSkin player_skins[2];
    TTF_Font* hud_font;
    SDL_Texture* hud_level_tex;
    SDL_Texture* hud_objective_tex;
    SDL_Texture* hud_controls_p1_tex;
    SDL_Texture* hud_controls_p2_tex;
    SDL_Texture* hud_complete_tex;
    SDL_Rect hud_level_rect;
    SDL_Rect hud_objective_rect;
    SDL_Rect hud_controls_p1_rect;
    SDL_Rect hud_controls_p2_rect;
    SDL_Rect hud_complete_rect;
    LevelConfig config;
    GameSelection selection;
    LevelPlayer players[2];
    int player_count;
    int active;
    int return_requested;
    int next_level_requested;
    int level_complete;
    float level_complete_timer;
} LevelRuntime;

int level_runtime_init(LevelRuntime* runtime, SDL_Renderer* renderer, const LevelConfig* cfg);
void level_runtime_enter(LevelRuntime* runtime, const GameSelection* selection);
void level_runtime_leave(LevelRuntime* runtime);
void level_runtime_handle_event(LevelRuntime* runtime, const SDL_Event* e);
void level_runtime_update(LevelRuntime* runtime, float dt);
void level_runtime_render(LevelRuntime* runtime);
int level_runtime_consume_return_request(LevelRuntime* runtime);
int level_runtime_consume_next_level_request(LevelRuntime* runtime);
void level_runtime_cleanup(LevelRuntime* runtime);

#endif /* LEVEL_SHARED_H */
