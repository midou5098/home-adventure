#ifndef GAME_PROGRESS_H
#define GAME_PROGRESS_H

#include <stddef.h>

#include "game_session.h"

#define GAME_PROGRESS_PLAYER_NAME_MAX 18
#define GAME_PROGRESS_LAST_SAVE_PATH "saves/last_game.csv"
#define GAME_PROGRESS_LAST_SAVE_SOLO_PATH "saves/last_game_solo.csv"
#define GAME_PROGRESS_LAST_SAVE_DUO_PATH "saves/last_game_duo.csv"
#define GAME_PROGRESS_BEST_SCORE_PATH "saves/best_score.csv"
#define GAME_PROGRESS_LEADERBOARD_PATH "saves/leaderboard.txt"

typedef struct {
    int master;
    int music;
    int vfx;
    int brightness;
    int fullscreen;
} GameOptionsState;

typedef struct {
    char player_name[GAME_PROGRESS_PLAYER_NAME_MAX + 1];
    GameSelection selection;
    GameOptionsState options;
    int current_lives[2];
    int save_enabled;
    int current_level;
    int highest_level_unlocked;
    int score;
    int has_started_game;
    int level1_completed;
    int level1_lives_remaining;
    int level1_points;
    int bonus_lives_for_level2;
    int level2_completed;
    int level2_starting_lives;
    int level2_player_lives_lost;
    int level2_marv_lives_lost;
    int level2_multiplayer;
    int level2_mini_key_found;
    int level2_points;
    int level3_completed;
    int level3_points;
    int level5_completed;
    int level5_points;
} GameProgress;

typedef struct {
    char player_name[GAME_PROGRESS_PLAYER_NAME_MAX + 1];
    int score;
    int time_sec;
    int keys_left;
    int keys_spent;
    int health_left;
} GameLeaderboardEntry;

void game_progress_set_defaults(GameProgress* out_progress);
void game_progress_normalize(GameProgress* in_out_progress);

void game_progress_get(GameProgress* out_progress);
void game_progress_set(const GameProgress* progress);

void game_progress_set_player_name(const char* player_name);
void game_progress_set_selection(const GameSelection* selection);
void game_progress_set_options(int master, int music, int vfx, int brightness, int fullscreen);
void game_progress_set_level_progress(int current_level, int highest_level_unlocked, int score);
void game_progress_mark_started_game(int has_started_game);

int game_progress_save_to_path(const char* path, const GameProgress* progress);
int game_progress_load_from_path(const char* path, GameProgress* out_progress);
int game_progress_delete_path(const char* path);
int game_progress_has_save_file(const char* path);
int game_progress_save_global_to_path(const char* path);
int game_progress_load_global_from_path(const char* path);
int game_progress_delete_global_path(const char* path);
int game_progress_has_global_save(const char* path);
int game_progress_is_resumable(const GameProgress* progress);
int game_progress_has_resumable_save(const char* path);
const char* game_progress_last_save_path_for_mode(int mode);
int game_progress_copy_last_save_path_for_mode(int mode, char* out_path, size_t out_path_size);
int game_progress_load_best_score(const char* path, char* out_name, size_t out_name_size, int* out_score);
int game_progress_save_best_score(const char* path, const char* player_name, int score);
int game_progress_update_best_score(const char* path, const char* player_name, int score);
int game_progress_load_leaderboard(const char* path, GameLeaderboardEntry* out_entries, size_t max_entries, size_t* out_count);
int game_progress_record_leaderboard_entry(const char* path, const GameLeaderboardEntry* entry);

#endif /* GAME_PROGRESS_H */
