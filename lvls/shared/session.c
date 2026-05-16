#include "session.h"
#include "game_progress.h"
#include "online_client.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LEVEL_LIFE_CARRY_MAGIC   0x4C434152u /* "LCAR" */
#define LEVEL_LIFE_CARRY_VERSION 1u
#define LEVEL1_MAX_LIVES         9
#define LEVEL1_MAX_BONUS_LIVES   99
#define LEVEL2_MAX_START_LIVES   (LEVEL1_MAX_LIVES + LEVEL1_MAX_BONUS_LIVES)
#define KEYS_MIN_TO_PASS_LEVEL1  10
#define KEYS_PER_BONUS_HEART     5

typedef struct {
    uint32_t magic;
    uint32_t version;
    int level1_completed;
    int lives_remaining;
    int bonus_lives;
    int level2_start_lives;
} LevelLifeCarryRecord;

static const char *kLevelLifeCarryPaths[] = {
    ".level_life_carry.bin",
    "../.level_life_carry.bin",
    "../../.level_life_carry.bin"
};

static void session_debug_log(const char *fmt, ...)
{
    char line[512];
    va_list args;
    FILE *log_file;
    const char *paths[] = {
        ".level_transition.log",
        "../.level_transition.log"
    };
    size_t i;

    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        log_file = fopen(paths[i], "a");
        if (!log_file) continue;
        fprintf(log_file, "%s\n", line);
        fclose(log_file);
    }
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double clamp_double(double value, double min_value, double max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int write_level_life_carry_record(const LevelLifeCarryRecord *record)
{
    size_t i;
    int wrote_any = 0;

    if (!record) return 0;

    for (i = 0; i < sizeof(kLevelLifeCarryPaths) / sizeof(kLevelLifeCarryPaths[0]); i++) {
        FILE *file = fopen(kLevelLifeCarryPaths[i], "wb");
        if (!file) continue;
        if (fwrite(record, sizeof(*record), 1, file) == 1) {
            wrote_any = 1;
            session_debug_log("[carry] write ok path=%s lvl1_completed=%d lives=%d bonus=%d start=%d",
                              kLevelLifeCarryPaths[i],
                              record->level1_completed,
                              record->lives_remaining,
                              record->bonus_lives,
                              record->level2_start_lives);
        } else {
            session_debug_log("[carry] write failed path=%s", kLevelLifeCarryPaths[i]);
        }
        fclose(file);
    }

    return wrote_any;
}

static int read_level_life_carry_record(LevelLifeCarryRecord *out_record)
{
    size_t i;

    if (!out_record) return 0;

    for (i = 0; i < sizeof(kLevelLifeCarryPaths) / sizeof(kLevelLifeCarryPaths[0]); i++) {
        LevelLifeCarryRecord record;
        FILE *file = fopen(kLevelLifeCarryPaths[i], "rb");
        if (!file) continue;
        if (fread(&record, sizeof(record), 1, file) == 1
            && record.magic == LEVEL_LIFE_CARRY_MAGIC
            && record.version == LEVEL_LIFE_CARRY_VERSION) {
            fclose(file);
            *out_record = record;
            session_debug_log("[carry] read ok path=%s lvl1_completed=%d lives=%d bonus=%d start=%d",
                              kLevelLifeCarryPaths[i],
                              record.level1_completed,
                              record.lives_remaining,
                              record.bonus_lives,
                              record.level2_start_lives);
            return 1;
        }
        session_debug_log("[carry] read invalid path=%s", kLevelLifeCarryPaths[i]);
        fclose(file);
    }

    session_debug_log("[carry] read miss");
    return 0;
}

int session_clear_level_life_carry(void)
{
    size_t i;
    int removed_any = 0;
    int had_error = 0;

    for (i = 0; i < sizeof(kLevelLifeCarryPaths) / sizeof(kLevelLifeCarryPaths[0]); i++) {
        if (remove(kLevelLifeCarryPaths[i]) == 0) {
            removed_any = 1;
            session_debug_log("[carry] clear removed path=%s", kLevelLifeCarryPaths[i]);
            continue;
        }
        if (errno != ENOENT)
            had_error = 1;
    }

    if (had_error && !removed_any)
        return 0;
    session_debug_log("[carry] clear done removed_any=%d had_error=%d", removed_any, had_error);
    return 1;
}

int session_save_level_life_carry(int level1_completed, int lives_remaining, int bonus_lives)
{
    LevelLifeCarryRecord record;

    if (!level1_completed)
        return session_clear_level_life_carry();

    lives_remaining = clamp_int(lives_remaining, 0, LEVEL1_MAX_LIVES);
    bonus_lives = clamp_int(bonus_lives, 0, LEVEL1_MAX_BONUS_LIVES);

    record.magic = LEVEL_LIFE_CARRY_MAGIC;
    record.version = LEVEL_LIFE_CARRY_VERSION;
    record.level1_completed = 1;
    record.lives_remaining = lives_remaining;
    record.bonus_lives = bonus_lives;
    record.level2_start_lives = clamp_int(lives_remaining + bonus_lives, 1, LEVEL2_MAX_START_LIVES);

    session_debug_log("[carry] save request lvl1_completed=%d lives=%d bonus=%d start=%d",
                      record.level1_completed,
                      record.lives_remaining,
                      record.bonus_lives,
                      record.level2_start_lives);
    return write_level_life_carry_record(&record);
}

int session_load_level_life_carry(int *out_level2_start_lives,
                                  int *out_lives_remaining,
                                  int *out_bonus_lives)
{
    LevelLifeCarryRecord record;
    int lives_remaining;
    int bonus_lives;
    int start_lives;

    if (!out_level2_start_lives) return 0;
    if (!read_level_life_carry_record(&record)) return 0;
    if (!record.level1_completed) return 0;

    lives_remaining = clamp_int(record.lives_remaining, 0, LEVEL1_MAX_LIVES);
    bonus_lives = clamp_int(record.bonus_lives, 0, LEVEL1_MAX_BONUS_LIVES);
    start_lives = clamp_int(record.level2_start_lives, 1, LEVEL2_MAX_START_LIVES);
    if (start_lives != clamp_int(lives_remaining + bonus_lives, 1, LEVEL2_MAX_START_LIVES))
        start_lives = clamp_int(lives_remaining + bonus_lives, 1, LEVEL2_MAX_START_LIVES);

    *out_level2_start_lives = start_lives;
    if (out_lives_remaining) *out_lives_remaining = lives_remaining;
    if (out_bonus_lives) *out_bonus_lives = bonus_lives;
    session_debug_log("[carry] load result start=%d lives=%d bonus=%d",
                      start_lives, lives_remaining, bonus_lives);
    return 1;
}

void session_reset(GameSession *session)
{
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->mode = GAME_MODE_SOLO;
    session->player_skin_number[0] = 1;
    session->player_skin_number[1] = 2;
    session->player_control_scheme[0] = CONTROL_SCHEME_ARROWS;
    session->player_control_scheme[1] = CONTROL_SCHEME_WASD;
    session->player_interact_bind[0] = INTERACT_BIND_0;
    session->player_interact_bind[1] = INTERACT_BIND_F;
    session->skin_number = session->player_skin_number[0];
    session->control_scheme = session->player_control_scheme[0];
    session->level2.marv_lives_lost = -1;
    session->grade = 'F';
}

void session_set_start_config(GameSession *session, GameMode mode,
                              int player1_skin_number, int player2_skin_number)
{
    GameMode resolved_mode;
    int p1_skin;
    int p2_skin;

    if (!session) return;

    resolved_mode = (mode == GAME_MODE_DUO) ? GAME_MODE_DUO : GAME_MODE_SOLO;
    p1_skin = (player1_skin_number == 2) ? 2 : 1;
    p2_skin = (player2_skin_number == 2) ? 2 : 1;
    if (resolved_mode == GAME_MODE_DUO && p1_skin == p2_skin) {
        p2_skin = (p1_skin == 1) ? 2 : 1;
    }

    session->mode = resolved_mode;
    session->player_skin_number[0] = p1_skin;
    session->player_skin_number[1] = p2_skin;

    /* Duo controls are fixed by design: P1 arrows + [0], P2 WASD + [F]. */
    session->player_control_scheme[0] = CONTROL_SCHEME_ARROWS;
    session->player_control_scheme[1] = CONTROL_SCHEME_WASD;
    session->player_interact_bind[0] = INTERACT_BIND_0;
    session->player_interact_bind[1] = INTERACT_BIND_F;

    /* Keep legacy fields synchronized with player 1 for existing level code. */
    session->skin_number = session->player_skin_number[0];
    session->control_scheme = session->player_control_scheme[0];
}

int session_autosave_progress(GameSession *session, int current_level, int player1_lives, int player2_lives)
{
    GameProgress progress;
    char save_path[256];
    int duo_mode;

    if (!session || !session->save_enabled) return 0;
    if (!(current_level == 1 || current_level == 2 || current_level == 3 || current_level == 4)) return 0;

    duo_mode = (session->mode == GAME_MODE_DUO) ? 1 : 0;
    player1_lives = clamp_int(player1_lives, 0, LEVEL2_MAX_START_LIVES);
    player2_lives = duo_mode ? clamp_int(player2_lives, 0, LEVEL2_MAX_START_LIVES) : 0;

    game_progress_get(&progress);
    progress.selection.duo_mode = duo_mode;
    progress.selection.player_count = duo_mode ? 2 : 1;
    progress.selection.selected_skin[0] = session->player_skin_number[0];
    progress.selection.selected_skin[1] = duo_mode ? session->player_skin_number[1] : 0;
    progress.selection.control_scheme[0] = session->player_control_scheme[0];
    progress.selection.control_scheme[1] = duo_mode ? session->player_control_scheme[1] : 0;
    progress.selection.resume_from_save = 1;
    progress.selection.save_enabled = 1;
    progress.save_enabled = 1;
    progress.has_started_game = 1;
    progress.current_level = current_level;
    if (progress.highest_level_unlocked < current_level) {
        progress.highest_level_unlocked = current_level;
    }
    progress.current_lives[0] = player1_lives;
    progress.current_lives[1] = player2_lives;
    progress.score = (session->total_points < 0) ? 0 : session->total_points;
    progress.level1_completed = session->level1.completed ? 1 : 0;
    progress.level1_lives_remaining = session->level1.lives_remaining;
    progress.level1_points = session->level1.points;
    progress.bonus_lives_for_level2 = session->bonus_lives_for_level2;
    progress.level2_completed = session->level2.completed ? 1 : 0;
    progress.level2_starting_lives = session->level2.starting_lives;
    progress.level2_player_lives_lost = session->level2.player_lives_lost;
    progress.level2_marv_lives_lost = session->level2.marv_lives_lost;
    progress.level2_multiplayer = session->level2.multiplayer ? 1 : 0;
    progress.level2_mini_key_found = session->level2.mini_key_found ? 1 : 0;
    progress.level2_points = session->level2.points;
    progress.level3_completed = session->level3.completed ? 1 : 0;
    progress.level3_points = session->level3.points;
    progress.level5_completed = session->level5.completed ? 1 : 0;
    progress.level5_points = session->level5.points;
    game_progress_set(&progress);

    game_progress_copy_last_save_path_for_mode(duo_mode ? 2 : 1, save_path, sizeof(save_path));
    session_debug_log("[autosave] level=%d mode=%d p1=%d p2=%d path=%s",
                      current_level, duo_mode ? 2 : 1, player1_lives, player2_lives, save_path);

    {
        int saved = game_progress_save_global_to_path(save_path);
        if (saved) {
            online_client_notify_save(current_level,
                                      progress.score,
                                      player1_lives,
                                      player2_lives,
                                      "AUTOSAVE");
        }
        return saved;
    }
}

int session_calculate_level1_points(GameSession *session)
{
    int keys_collected;
    int keys_collected_total;
    int height_reached;
    int time_taken_sec;
    int lives_remaining;
    int extra_keys;
    double key_points;
    double height_points;
    double time_bonus;
    double lives_bonus;

    if (!session) return 0;

    keys_collected_total = clamp_int(session->level1.keys_collected, 0, 9999);
    keys_collected = clamp_int(keys_collected_total, 0, 10);
    height_reached = clamp_int(session->level1.height_reached, 0, 6000);
    time_taken_sec = clamp_int(session->level1.time_taken_sec, 0, 36000);
    lives_remaining = clamp_int(session->level1.lives_remaining, 0, 9);

    if (!session->level1.completed) {
        session->level1.points = 0;
        session->bonus_lives_for_level2 = 0;
        return 0;
    }

    key_points = ((double)keys_collected / 10.0) * 500.0;
    height_points = (clamp_double((double)height_reached / 6000.0, 0.0, 1.0)) * 300.0;

    if (time_taken_sec <= 300) time_bonus = 100.0;
    else if (time_taken_sec >= 600) time_bonus = 0.0;
    else time_bonus = ((600.0 - (double)time_taken_sec) / 300.0) * 100.0;

    lives_bonus = ((double)lives_remaining / 9.0) * 100.0;

    session->level1.points = (int)(key_points + height_points + time_bonus + lives_bonus + 0.5);
    extra_keys = keys_collected_total - KEYS_MIN_TO_PASS_LEVEL1;
    if (extra_keys < 0) extra_keys = 0;
    session->bonus_lives_for_level2 = clamp_int(extra_keys / KEYS_PER_BONUS_HEART, 0, LEVEL1_MAX_BONUS_LIVES);
    return session->level1.points;
}

int session_calculate_level2_points(GameSession *session)
{
    int starting_lives;
    int player_lives_lost;
    int marv_lives_lost;
    int total_lost;
    int total_capacity;
    double survival_points;

    if (!session) return 0;

    if (!session->level2.completed) {
        session->level2.points = 0;
        return 0;
    }

    starting_lives = session->level2.starting_lives;
    if (starting_lives <= 0)
        starting_lives = LEVEL1_MAX_LIVES + clamp_int(session->bonus_lives_for_level2, 0, LEVEL1_MAX_BONUS_LIVES);

    player_lives_lost = clamp_int(session->level2.player_lives_lost, 0, starting_lives);
    marv_lives_lost = session->level2.multiplayer
        ? clamp_int(session->level2.marv_lives_lost, 0, starting_lives)
        : 0;

    total_lost = player_lives_lost + marv_lives_lost;
    total_capacity = session->level2.multiplayer ? (starting_lives * 2) : starting_lives;
    if (total_capacity <= 0) total_capacity = 1;

    survival_points = (1.0 - ((double)total_lost / (double)total_capacity)) * 900.0;
    if (survival_points < 0.0) survival_points = 0.0;

    session->level2.points = (int)(survival_points + (session->level2.mini_key_found ? 100 : 0) + 0.5);
    return session->level2.points;
}

char session_assign_grade(int total_points)
{
    if (total_points >= 1700) return 'S';
    if (total_points >= 1300) return 'A';
    if (total_points >= 900) return 'B';
    if (total_points >= 500) return 'C';
    if (total_points >= 1) return 'D';
    return 'F';
}

int session_calculate_total_points(GameSession *session)
{
    if (!session) return 0;

    session->total_points = session->level1.points
        + session->level2.points
        + session->level3.points
        + session->level5.points;
    session->grade = session_assign_grade(session->total_points);
    return session->total_points;
}

const char *session_control_scheme_label(ControlScheme scheme)
{
    if (scheme == CONTROL_SCHEME_CONTROLLER)
        return "Controller";
    if (scheme == CONTROL_SCHEME_WASD)
        return "WASD";
    return "Arrow Keys";
}

const char *session_game_mode_label(GameMode mode)
{
    if (mode == GAME_MODE_DUO)
        return "Duo";
    return "Solo";
}

const char *session_interact_bind_label(InteractBind bind)
{
    switch (bind) {
        case INTERACT_BIND_E: return "E";
        case INTERACT_BIND_F: return "F";
        case INTERACT_BIND_0: return "0";
        default: return "?";
    }
}

const char *session_grade_flavor(char grade)
{
    switch (grade) {
        case 'S': return "Kevin never stood a chance.";
        case 'A': return "Sharp and relentless.";
        case 'B': return "Harry would be proud.";
        case 'C': return "You made it. Barely.";
        case 'D': return "The rats have seen better.";
        default: return "Kevin wins this round.";
    }
}
