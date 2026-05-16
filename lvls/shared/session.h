#ifndef SHARED_SESSION_H
#define SHARED_SESSION_H

typedef enum {
    GAME_MODE_SOLO = 1,
    GAME_MODE_DUO = 2
} GameMode;

typedef enum {
    CONTROL_SCHEME_ARROWS = 1,
    CONTROL_SCHEME_WASD = 2,
    CONTROL_SCHEME_CONTROLLER = 3
} ControlScheme;

typedef enum {
    INTERACT_BIND_E = 1,
    INTERACT_BIND_F = 2,
    INTERACT_BIND_0 = 3
} InteractBind;

typedef struct {
    int keys_collected;
    int keys_left;
    int keys_spent;
    int height_reached;
    int time_taken_sec;
    int lives_remaining;
    int completed;
    int points;
} Level1Result;

typedef struct {
    int starting_lives;
    int player_lives_lost;
    int marv_lives_lost;
    int mini_key_found;
    int multiplayer;
    int completed;
    int points;
} Level2Result;

typedef struct {
    int completed;
    int points;
} Level3Result;

typedef struct {
    int completed;
    int points;
} Level5Result;

typedef struct {
    /*
     * Legacy single-player fields kept for compatibility with existing code.
     * They mirror player 1 startup values.
     */
    int skin_number;
    ControlScheme control_scheme;

    GameMode mode;
    int player_skin_number[2];
    ControlScheme player_control_scheme[2];
    InteractBind player_interact_bind[2];

    Level1Result level1;
    Level2Result level2;
    Level3Result level3;
    /* Legacy field name used by the moved pool level, now played as level 4. */
    Level5Result level5;

    int  total_points;
    char grade;

    int bonus_lives_for_level2;
    int quit_requested;
    int save_enabled;
    int dev_jump_to_level2;
    int dev_jump_to_final;
    int dev_jump_to_final_cutscene;
} GameSession;

/*
 * Level-life carry file helpers.
 * These persist Level 1 life state so Level 2 can start with
 * (remaining lives + bonus lives), even across level boundaries.
 */
int session_save_level_life_carry(int level1_completed, int lives_remaining, int bonus_lives);
int session_load_level_life_carry(int *out_level2_start_lives,
                                  int *out_lives_remaining,
                                  int *out_bonus_lives);
int session_clear_level_life_carry(void);
int session_autosave_progress(GameSession *session, int current_level, int player1_lives, int player2_lives);

void session_reset(GameSession *session);
void session_set_start_config(GameSession *session, GameMode mode,
                              int player1_skin_number, int player2_skin_number);
int session_calculate_level1_points(GameSession *session);
int session_calculate_level2_points(GameSession *session);
int session_calculate_total_points(GameSession *session);
char session_assign_grade(int total_points);
const char *session_control_scheme_label(ControlScheme scheme);
const char *session_game_mode_label(GameMode mode);
const char *session_interact_bind_label(InteractBind bind);
const char *session_grade_flavor(char grade);

#endif
