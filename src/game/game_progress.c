#define _POSIX_C_SOURCE 200809L

#include "game_progress.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static GameProgress g_game_progress;
static int g_game_progress_initialized = 0;
static char g_game_progress_root[PATH_MAX];
static int g_game_progress_root_ready = 0;

static int game_progress_path_is_absolute(const char* path)
{
    if (!path || !path[0]) return 0;
    if (path[0] == '/') return 1;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return 1;
    return 0;
}

static int game_progress_file_exists(const char* path)
{
    FILE* f = NULL;

    if (!path) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int game_progress_dir_has_project_markers(const char* dir_path)
{
    static const char* markers[] = {
        "src/main_menu/main.c",
        "lvls/launcher/main.c"
    };
    char marker_path[PATH_MAX];
    size_t i = 0;

    if (!dir_path || !dir_path[0]) return 0;

    for (i = 0; i < sizeof(markers) / sizeof(markers[0]); ++i) {
        if (snprintf(marker_path, sizeof(marker_path), "%s/%s", dir_path, markers[i]) >= (int)sizeof(marker_path)) {
            continue;
        }
        if (game_progress_file_exists(marker_path)) {
            return 1;
        }
    }

    return 0;
}

static int game_progress_find_root_from_base(const char* base_dir, char* out_path, size_t out_path_size)
{
    char current[PATH_MAX];
    char* slash = NULL;

    if (!base_dir || !base_dir[0] || !out_path || out_path_size == 0) return 0;
    if (snprintf(current, sizeof(current), "%s", base_dir) >= (int)sizeof(current)) return 0;

    while (current[0] != '\0') {
        if (game_progress_dir_has_project_markers(current)) {
            snprintf(out_path, out_path_size, "%s", current);
            return 1;
        }

        slash = strrchr(current, '/');
        if (!slash) break;
        if (slash == current) {
            current[1] = '\0';
            if (game_progress_dir_has_project_markers(current)) {
                snprintf(out_path, out_path_size, "%s", current);
                return 1;
            }
            break;
        }
        *slash = '\0';
    }

    return 0;
}

static int game_progress_detect_project_root(char* out_path, size_t out_path_size)
{
    char exe_path[PATH_MAX];
    char cwd_path[PATH_MAX];
    ssize_t exe_len = 0;
    char* slash = NULL;

    if (!out_path || out_path_size == 0) return 0;

    exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (exe_len > 0) {
        exe_path[exe_len] = '\0';
        slash = strrchr(exe_path, '/');
        if (slash) {
            *slash = '\0';
            if (game_progress_find_root_from_base(exe_path, out_path, out_path_size)) {
                return 1;
            }
        }
    }

    if (getcwd(cwd_path, sizeof(cwd_path)) &&
        game_progress_find_root_from_base(cwd_path, out_path, out_path_size)) {
        return 1;
    }

    return 0;
}

static int game_progress_resolve_path(const char* relative_path, char* out_path, size_t out_path_size)
{
    if (!relative_path || !out_path || out_path_size == 0) return 0;

    if (game_progress_path_is_absolute(relative_path)) {
        snprintf(out_path, out_path_size, "%s", relative_path);
        return 1;
    }

    if (!g_game_progress_root_ready &&
        game_progress_detect_project_root(g_game_progress_root, sizeof(g_game_progress_root))) {
        g_game_progress_root_ready = 1;
    }

    if (g_game_progress_root_ready &&
        snprintf(out_path, out_path_size, "%s/%s", g_game_progress_root, relative_path) < (int)out_path_size) {
        return 1;
    }

    snprintf(out_path, out_path_size, "%s", relative_path);
    return 1;
}

static int game_progress_build_temp_path(const char* resolved_path, char* out_path, size_t out_path_size)
{
    if (!resolved_path || !out_path || out_path_size == 0) return 0;
    return snprintf(out_path, out_path_size, "%s.tmp", resolved_path) < (int)out_path_size;
}

static void game_progress_apply_legacy_level_aliases(GameProgress* in_out_progress)
{
    if (!in_out_progress) return;

    if (in_out_progress->current_level == 5) {
        in_out_progress->current_level = 4;
    }
    if (in_out_progress->highest_level_unlocked == 5) {
        in_out_progress->highest_level_unlocked = 4;
    }

    /*
     * The CSV still uses the old level5 columns for the pool level. That
     * level now runs as level 4, after the new level 3 placeholder.
     */
    if (in_out_progress->level3_points > 0) {
        if (in_out_progress->level5_points <= 0) {
            in_out_progress->level5_points = in_out_progress->level3_points;
        }
        if (in_out_progress->level3_completed) {
            in_out_progress->level5_completed = 1;
        }
        in_out_progress->level3_points = 0;
    }
    if (in_out_progress->level5_completed) {
        in_out_progress->level3_completed = 1;
        if (in_out_progress->current_level < 4) {
            in_out_progress->current_level = 4;
        }
        if (in_out_progress->highest_level_unlocked < 4) {
            in_out_progress->highest_level_unlocked = 4;
        }
    } else if (in_out_progress->current_level >= 4 && !in_out_progress->level3_completed) {
        in_out_progress->level3_completed = 1;
    }
}

static int game_progress_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void game_progress_copy_name(char* dest, size_t dest_size, const char* src)
{
    size_t out_len = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    while (src[out_len] != '\0' && out_len + 1 < dest_size) {
        dest[out_len] = src[out_len];
        ++out_len;
    }
    dest[out_len] = '\0';
}

static int game_progress_name_is_blank(const char* text)
{
    if (!text) return 1;

    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 0;
        }
        ++text;
    }

    return 1;
}

static void game_progress_copy_best_score_name(char* dest, size_t dest_size, const char* src)
{
    if (game_progress_name_is_blank(src)) {
        game_progress_copy_name(dest, dest_size, "PLAYER");
        return;
    }

    game_progress_copy_name(dest, dest_size, src);
}

static void game_progress_trim_line(char* text)
{
    size_t len = 0;

    if (!text) return;

    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || isspace((unsigned char)text[len - 1]))) {
        text[--len] = '\0';
    }
}

static int game_progress_csv_write_escaped(FILE* f, const char* text)
{
    const char* p = NULL;

    if (!f) return 0;
    if (!text) text = "";

    fputc('"', f);
    for (p = text; *p != '\0'; ++p) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
    return 1;
}

static int game_progress_csv_read_field(const char** cursor, char* out, size_t out_size)
{
    const char* p = NULL;
    size_t len = 0;

    if (!cursor || !*cursor || !out || out_size == 0) return 0;

    p = *cursor;
    out[0] = '\0';

    if (*p == '"') {
        ++p;
        while (*p != '\0') {
            if (*p == '"' && p[1] == '"') {
                if (len + 1 < out_size) out[len++] = '"';
                p += 2;
                continue;
            }
            if (*p == '"') {
                ++p;
                break;
            }
            if (len + 1 < out_size) out[len++] = *p;
            ++p;
        }
        if (*p == ',') ++p;
    } else {
        while (*p != '\0' && *p != ',') {
            if (len + 1 < out_size) out[len++] = *p;
            ++p;
        }
        if (*p == ',') ++p;
    }

    out[len] = '\0';
    *cursor = p;
    return 1;
}

static int game_progress_ensure_parent_dir(const char* path)
{
    char dir_path[512];
    const char* slash = NULL;
    size_t dir_len = 0;

    if (!path || !path[0]) return 0;

    slash = strrchr(path, '/');
    if (!slash) return 1;

    dir_len = (size_t)(slash - path);
    if (dir_len == 0 || dir_len >= sizeof(dir_path)) return 0;

    memcpy(dir_path, path, dir_len);
    dir_path[dir_len] = '\0';

    if (mkdir(dir_path, 0777) == 0) return 1;
    return (errno == EEXIST) ? 1 : 0;
}

static void game_progress_ensure_initialized(void)
{
    if (g_game_progress_initialized) return;
    game_progress_set_defaults(&g_game_progress);
    g_game_progress_initialized = 1;
}

void game_progress_set_defaults(GameProgress* out_progress)
{
    if (!out_progress) return;

    memset(out_progress, 0, sizeof(*out_progress));
    out_progress->selection.player_count = 1;
    out_progress->selection.selected_skin[0] = 1;
    out_progress->selection.selected_skin[1] = 0;
    out_progress->selection.control_scheme[0] = 1;
    out_progress->selection.control_scheme[1] = 0;
    out_progress->options.master = 10;
    out_progress->options.music = 10;
    out_progress->options.vfx = 10;
    out_progress->options.brightness = 10;
    out_progress->options.fullscreen = 0;
    out_progress->current_lives[0] = 9;
    out_progress->current_lives[1] = 0;
    out_progress->save_enabled = 1;
    out_progress->current_level = 1;
    out_progress->highest_level_unlocked = 1;
    out_progress->score = 0;
    out_progress->has_started_game = 0;
}

void game_progress_normalize(GameProgress* in_out_progress)
{
    if (!in_out_progress) return;

    if (in_out_progress->selection.duo_mode) {
        in_out_progress->selection.duo_mode = 1;
        in_out_progress->selection.player_count = 2;
    } else {
        in_out_progress->selection.duo_mode = 0;
        in_out_progress->selection.player_count = 1;
        in_out_progress->selection.selected_skin[1] = 0;
        in_out_progress->selection.control_scheme[1] = 0;
    }

    in_out_progress->selection.selected_skin[0] =
        game_progress_clamp_int(in_out_progress->selection.selected_skin[0], 1, 9999);
    in_out_progress->selection.selected_skin[1] =
        game_progress_clamp_int(in_out_progress->selection.selected_skin[1], 0, 9999);
    in_out_progress->selection.control_scheme[0] =
        game_progress_clamp_int(in_out_progress->selection.control_scheme[0], 1, 3);
    in_out_progress->selection.control_scheme[1] =
        game_progress_clamp_int(in_out_progress->selection.control_scheme[1], 0, 3);
    in_out_progress->selection.resume_from_save = in_out_progress->selection.resume_from_save ? 1 : 0;

    in_out_progress->options.master = game_progress_clamp_int(in_out_progress->options.master, 0, 10);
    in_out_progress->options.music = game_progress_clamp_int(in_out_progress->options.music, 0, 10);
    in_out_progress->options.vfx = game_progress_clamp_int(in_out_progress->options.vfx, 0, 10);
    in_out_progress->options.brightness = game_progress_clamp_int(in_out_progress->options.brightness, 0, 10);
    in_out_progress->options.fullscreen = in_out_progress->options.fullscreen ? 1 : 0;

    in_out_progress->current_lives[0] = game_progress_clamp_int(in_out_progress->current_lives[0], 0, 9999);
    in_out_progress->current_lives[1] = game_progress_clamp_int(in_out_progress->current_lives[1], 0, 9999);
    in_out_progress->save_enabled = in_out_progress->save_enabled ? 1 : 0;
    in_out_progress->current_level = game_progress_clamp_int(in_out_progress->current_level, 1, 9999);
    in_out_progress->highest_level_unlocked =
        game_progress_clamp_int(in_out_progress->highest_level_unlocked, 1, 9999);
    if (in_out_progress->score < 0) in_out_progress->score = 0;
    in_out_progress->has_started_game = in_out_progress->has_started_game ? 1 : 0;
    in_out_progress->level1_completed = in_out_progress->level1_completed ? 1 : 0;
    in_out_progress->level1_lives_remaining = game_progress_clamp_int(in_out_progress->level1_lives_remaining, 0, 9999);
    in_out_progress->level1_points = game_progress_clamp_int(in_out_progress->level1_points, 0, 99999999);
    in_out_progress->bonus_lives_for_level2 = game_progress_clamp_int(in_out_progress->bonus_lives_for_level2, 0, 9999);
    in_out_progress->level2_completed = in_out_progress->level2_completed ? 1 : 0;
    in_out_progress->level2_starting_lives = game_progress_clamp_int(in_out_progress->level2_starting_lives, 0, 9999);
    in_out_progress->level2_player_lives_lost = game_progress_clamp_int(in_out_progress->level2_player_lives_lost, 0, 9999);
    in_out_progress->level2_marv_lives_lost = game_progress_clamp_int(in_out_progress->level2_marv_lives_lost, -1, 9999);
    in_out_progress->level2_multiplayer = in_out_progress->level2_multiplayer ? 1 : 0;
    in_out_progress->level2_mini_key_found = in_out_progress->level2_mini_key_found ? 1 : 0;
    in_out_progress->level2_points = game_progress_clamp_int(in_out_progress->level2_points, 0, 99999999);
    in_out_progress->level3_completed = in_out_progress->level3_completed ? 1 : 0;
    in_out_progress->level3_points = game_progress_clamp_int(in_out_progress->level3_points, 0, 99999999);
    in_out_progress->level5_completed = in_out_progress->level5_completed ? 1 : 0;
    in_out_progress->level5_points = game_progress_clamp_int(in_out_progress->level5_points, 0, 99999999);
    game_progress_apply_legacy_level_aliases(in_out_progress);
    if (in_out_progress->highest_level_unlocked < in_out_progress->current_level) {
        in_out_progress->highest_level_unlocked = in_out_progress->current_level;
    }
    in_out_progress->player_name[GAME_PROGRESS_PLAYER_NAME_MAX] = '\0';
}

void game_progress_get(GameProgress* out_progress)
{
    game_progress_ensure_initialized();
    if (!out_progress) return;
    *out_progress = g_game_progress;
}

void game_progress_set(const GameProgress* progress)
{
    game_progress_ensure_initialized();
    if (!progress) return;
    g_game_progress = *progress;
    game_progress_normalize(&g_game_progress);
}

void game_progress_set_player_name(const char* player_name)
{
    game_progress_ensure_initialized();
    game_progress_copy_name(g_game_progress.player_name, sizeof(g_game_progress.player_name), player_name);
}

void game_progress_set_selection(const GameSelection* selection)
{
    game_progress_ensure_initialized();
    if (!selection) return;
    g_game_progress.selection = *selection;
    game_progress_normalize(&g_game_progress);
}

void game_progress_set_options(int master, int music, int vfx, int brightness, int fullscreen)
{
    game_progress_ensure_initialized();
    g_game_progress.options.master = master;
    g_game_progress.options.music = music;
    g_game_progress.options.vfx = vfx;
    g_game_progress.options.brightness = brightness;
    g_game_progress.options.fullscreen = fullscreen;
    game_progress_normalize(&g_game_progress);
}

void game_progress_set_level_progress(int current_level, int highest_level_unlocked, int score)
{
    game_progress_ensure_initialized();
    g_game_progress.current_level = current_level;
    g_game_progress.highest_level_unlocked = highest_level_unlocked;
    g_game_progress.score = score;
    game_progress_normalize(&g_game_progress);
}

void game_progress_mark_started_game(int has_started_game)
{
    game_progress_ensure_initialized();
    g_game_progress.has_started_game = has_started_game ? 1 : 0;
}

int game_progress_save_to_path(const char* path, const GameProgress* progress)
{
    FILE* f = NULL;
    GameProgress normalized = {0};
    char resolved_path[512];
    char temp_path[544];

    if (!path || !progress) return 0;

    normalized = *progress;
    game_progress_normalize(&normalized);

    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;
    if (!game_progress_build_temp_path(resolved_path, temp_path, sizeof(temp_path))) return 0;
    if (!game_progress_ensure_parent_dir(resolved_path)) return 0;

    f = fopen(temp_path, "w");
    if (!f && errno == EACCES) {
        if (remove(temp_path) == 0) {
            f = fopen(temp_path, "w");
        }
    }
    if (!f) return 0;

    fprintf(f, "player_name,duo_mode,player_count,selected_skin1,selected_skin2,control_scheme1,control_scheme2,resume_from_save,save_enabled,master,music,vfx,brightness,fullscreen,current_lives1,current_lives2,current_level,highest_level_unlocked,score,has_started_game,level1_completed,level1_lives_remaining,level1_points,bonus_lives_for_level2,level2_completed,level2_starting_lives,level2_player_lives_lost,level2_marv_lives_lost,level2_multiplayer,level2_mini_key_found,level2_points,level3_completed,level3_points,level5_completed,level5_points\n");
    game_progress_csv_write_escaped(f, normalized.player_name);
    fprintf(f, ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            normalized.selection.duo_mode,
            normalized.selection.player_count,
            normalized.selection.selected_skin[0],
            normalized.selection.selected_skin[1],
            normalized.selection.control_scheme[0],
            normalized.selection.control_scheme[1],
            normalized.selection.resume_from_save,
            normalized.save_enabled,
            normalized.options.master,
            normalized.options.music,
            normalized.options.vfx,
            normalized.options.brightness,
            normalized.options.fullscreen,
            normalized.current_lives[0],
            normalized.current_lives[1],
            normalized.current_level,
            normalized.highest_level_unlocked,
            normalized.score,
            normalized.has_started_game,
            normalized.level1_completed,
            normalized.level1_lives_remaining,
            normalized.level1_points,
            normalized.bonus_lives_for_level2,
            normalized.level2_completed,
            normalized.level2_starting_lives,
            normalized.level2_player_lives_lost,
            normalized.level2_marv_lives_lost,
            normalized.level2_multiplayer,
            normalized.level2_mini_key_found,
            normalized.level2_points,
            normalized.level3_completed,
            normalized.level3_points,
            normalized.level5_completed,
            normalized.level5_points);

    fclose(f);
    if (rename(temp_path, resolved_path) != 0) {
        remove(resolved_path);
        if (rename(temp_path, resolved_path) != 0) {
            remove(temp_path);
            return 0;
        }
    }
    return 1;
}

int game_progress_load_from_path(const char* path, GameProgress* out_progress)
{
    FILE* f = NULL;
    GameProgress loaded = {0};
    char line[2048];
    char player_name[128];
    char header[2048];
    char resolved_path[512];
    const char* cursor = NULL;
    int field_count = 0;

    if (!path || !out_progress) return 0;

    game_progress_set_defaults(&loaded);

    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;

    f = fopen(resolved_path, "r");
    if (!f) return 0;

    if (!fgets(header, sizeof(header), f)) {
        fclose(f);
        return 0;
    }
    game_progress_trim_line(header);
    if (strcmp(header, "player_name,duo_mode,player_count,selected_skin1,selected_skin2,control_scheme1,control_scheme2,resume_from_save,save_enabled,master,music,vfx,brightness,fullscreen,current_lives1,current_lives2,current_level,highest_level_unlocked,score,has_started_game,level1_completed,level1_lives_remaining,level1_points,bonus_lives_for_level2,level2_completed,level2_starting_lives,level2_player_lives_lost,level2_marv_lives_lost,level2_multiplayer,level2_mini_key_found,level2_points,level3_completed,level3_points,level5_completed,level5_points") != 0) {
        fclose(f);
        return 0;
    }
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    game_progress_trim_line(line);
    cursor = line;
    if (!game_progress_csv_read_field(&cursor, player_name, sizeof(player_name))) {
        fclose(f);
        return 0;
    }
    game_progress_copy_name(loaded.player_name, sizeof(loaded.player_name), player_name);
    loaded.selection.duo_mode = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.player_count = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.selected_skin[0] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.selected_skin[1] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.control_scheme[0] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.control_scheme[1] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.selection.resume_from_save = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.save_enabled = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.options.master = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.options.music = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.options.vfx = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.options.brightness = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.options.fullscreen = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.current_lives[0] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.current_lives[1] = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.current_level = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.highest_level_unlocked = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.score = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.has_started_game = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level1_completed = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level1_lives_remaining = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level1_points = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.bonus_lives_for_level2 = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_completed = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_starting_lives = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_player_lives_lost = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_marv_lives_lost = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_multiplayer = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_mini_key_found = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level2_points = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level3_completed = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level3_points = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level5_completed = atoi(cursor); ++field_count; cursor = strchr(cursor, ','); if (!cursor) { fclose(f); return 0; } ++cursor;
    loaded.level5_points = atoi(cursor);
    ++field_count;

    fclose(f);

    if (field_count != 34) {
        return 0;
    }

    game_progress_normalize(&loaded);
    *out_progress = loaded;
    return 1;
}

int game_progress_delete_path(const char* path)
{
    char resolved_path[512];

    if (!path) return 0;
    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;
    if (remove(resolved_path) == 0) return 1;
    return (errno == ENOENT) ? 1 : 0;
}

int game_progress_has_save_file(const char* path)
{
    GameProgress loaded = {0};

    if (!path) return 0;
    return game_progress_load_from_path(path, &loaded);
}

int game_progress_save_global_to_path(const char* path)
{
    game_progress_ensure_initialized();
    return game_progress_save_to_path(path, &g_game_progress);
}

int game_progress_load_global_from_path(const char* path)
{
    GameProgress loaded = {0};

    if (!game_progress_load_from_path(path, &loaded)) return 0;
    g_game_progress = loaded;
    g_game_progress_initialized = 1;
    return 1;
}

int game_progress_delete_global_path(const char* path)
{
    if (!game_progress_delete_path(path)) return 0;
    game_progress_set_defaults(&g_game_progress);
    g_game_progress_initialized = 1;
    return 1;
}

int game_progress_has_global_save(const char* path)
{
    return game_progress_has_save_file(path);
}

int game_progress_is_resumable(const GameProgress* progress)
{
    if (!progress) return 0;

    if (!progress->has_started_game) return 0;
    if (!progress->save_enabled) return 0;
    if (progress->selection.resume_from_save) return 1;
    if (progress->current_level > 1) return 1;
    if (progress->score > 0) return 1;
    if (progress->level1_completed || progress->level2_completed ||
        progress->level3_completed || progress->level5_completed) {
        return 1;
    }
    if (progress->level1_points > 0 || progress->level2_points > 0 ||
        progress->level3_points > 0 || progress->level5_points > 0) {
        return 1;
    }
    return 0;
}

int game_progress_has_resumable_save(const char* path)
{
    GameProgress loaded = {0};

    if (!game_progress_load_from_path(path, &loaded)) return 0;
    return game_progress_is_resumable(&loaded);
}

const char* game_progress_last_save_path_for_mode(int mode)
{
    return (mode == 2) ? GAME_PROGRESS_LAST_SAVE_DUO_PATH : GAME_PROGRESS_LAST_SAVE_SOLO_PATH;
}

int game_progress_copy_last_save_path_for_mode(int mode, char* out_path, size_t out_path_size)
{
    const char* src = game_progress_last_save_path_for_mode(mode);

    if (!out_path || out_path_size == 0) return 0;
    game_progress_copy_name(out_path, out_path_size, src);
    return 1;
}

int game_progress_load_best_score(const char* path, char* out_name, size_t out_name_size, int* out_score)
{
    FILE* f = NULL;
    char line[512];
    char name[128];
    char resolved_path[512];
    const char* cursor = NULL;

    if (out_name && out_name_size > 0) out_name[0] = '\0';
    if (out_score) *out_score = 0;
    if (!path) return 0;

    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;

    f = fopen(resolved_path, "r");
    if (!f) return 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    game_progress_trim_line(line);
    if (strcmp(line, "player_name,score") != 0) {
        fclose(f);
        return 0;
    }
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    game_progress_trim_line(line);
    cursor = line;
    if (!game_progress_csv_read_field(&cursor, name, sizeof(name))) return 0;
    if (out_name && out_name_size > 0) game_progress_copy_best_score_name(out_name, out_name_size, name);
    if (out_score) *out_score = atoi(cursor);
    return 1;
}

int game_progress_save_best_score(const char* path, const char* player_name, int score)
{
    FILE* f = NULL;
    char name[GAME_PROGRESS_PLAYER_NAME_MAX + 1];
    char resolved_path[512];
    char temp_path[544];

    if (!path) return 0;
    if (score < 0) score = 0;
    game_progress_copy_best_score_name(name, sizeof(name), player_name);
    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;
    if (!game_progress_build_temp_path(resolved_path, temp_path, sizeof(temp_path))) return 0;
    if (!game_progress_ensure_parent_dir(resolved_path)) return 0;
    f = fopen(temp_path, "w");
    if (!f && errno == EACCES) {
        if (remove(temp_path) == 0) f = fopen(temp_path, "w");
    }
    if (!f) return 0;
    fprintf(f, "player_name,score\n");
    game_progress_csv_write_escaped(f, name);
    fprintf(f, ",%d\n", score);
    fclose(f);
    if (rename(temp_path, resolved_path) != 0) {
        remove(resolved_path);
        if (rename(temp_path, resolved_path) != 0) {
            remove(temp_path);
            return 0;
        }
    }
    return 1;
}

int game_progress_update_best_score(const char* path, const char* player_name, int score)
{
    char best_name[GAME_PROGRESS_PLAYER_NAME_MAX + 1];
    int best_score = 0;

    if (score < 0) score = 0;
    if (game_progress_load_best_score(path, best_name, sizeof(best_name), &best_score) && best_score >= score) {
        return 1;
    }
    return game_progress_save_best_score(path, player_name, score);
}

static void game_progress_set_placeholder_leaderboard(GameLeaderboardEntry* entries, size_t count)
{
    static const GameLeaderboardEntry placeholders[3] = {
        {"PLAYER_A", 3000, 420, 8, 2, 9},
        {"PLAYER_B", 2200, 560, 4, 5, 6},
        {"PLAYER_C", 1400, 720, 1, 7, 3}
    };
    size_t i;

    if (!entries) return;
    for (i = 0; i < count && i < 3; ++i) {
        entries[i] = placeholders[i];
    }
}

static int game_progress_leaderboard_entry_compare(const GameLeaderboardEntry* a,
                                                   const GameLeaderboardEntry* b)
{
    if (!a || !b) return 0;
    if (a->score != b->score) return (b->score - a->score);
    if (a->time_sec != b->time_sec) return (a->time_sec - b->time_sec);
    if (a->health_left != b->health_left) return (b->health_left - a->health_left);
    if (a->keys_left != b->keys_left) return (b->keys_left - a->keys_left);
    return 0;
}

static void game_progress_sort_leaderboard(GameLeaderboardEntry* entries, size_t count)
{
    size_t i;
    size_t j;

    if (!entries) return;
    for (i = 0; i < count; ++i) {
        for (j = i + 1; j < count; ++j) {
            if (game_progress_leaderboard_entry_compare(&entries[i], &entries[j]) > 0) {
                GameLeaderboardEntry tmp = entries[i];
                entries[i] = entries[j];
                entries[j] = tmp;
            }
        }
    }
}

static int game_progress_save_leaderboard_entries(const char* path,
                                                  const GameLeaderboardEntry* entries,
                                                  size_t count)
{
    FILE* f = NULL;
    char resolved_path[512];
    char temp_path[544];
    size_t i;

    if (!path || !entries) return 0;
    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;
    if (!game_progress_build_temp_path(resolved_path, temp_path, sizeof(temp_path))) return 0;
    if (!game_progress_ensure_parent_dir(resolved_path)) return 0;

    f = fopen(temp_path, "w");
    if (!f && errno == EACCES) {
        if (remove(temp_path) == 0) f = fopen(temp_path, "w");
    }
    if (!f) return 0;

    fprintf(f, "player_name,score,time_sec,keys_left,keys_spent,health_left\n");
    for (i = 0; i < count; ++i) {
        char name[GAME_PROGRESS_PLAYER_NAME_MAX + 1];
        game_progress_copy_best_score_name(name, sizeof(name), entries[i].player_name);
        game_progress_csv_write_escaped(f, name);
        fprintf(f, ",%d,%d,%d,%d,%d\n",
                entries[i].score < 0 ? 0 : entries[i].score,
                entries[i].time_sec < 0 ? 0 : entries[i].time_sec,
                entries[i].keys_left < 0 ? 0 : entries[i].keys_left,
                entries[i].keys_spent < 0 ? 0 : entries[i].keys_spent,
                entries[i].health_left < 0 ? 0 : entries[i].health_left);
    }
    fclose(f);

    if (rename(temp_path, resolved_path) != 0) {
        remove(resolved_path);
        if (rename(temp_path, resolved_path) != 0) {
            remove(temp_path);
            return 0;
        }
    }
    return 1;
}

int game_progress_load_leaderboard(const char* path,
                                   GameLeaderboardEntry* out_entries,
                                   size_t max_entries,
                                   size_t* out_count)
{
    FILE* f = NULL;
    char line[512];
    char resolved_path[512];
    size_t count = 0;

    if (out_count) *out_count = 0;
    if (!path || !out_entries || max_entries == 0) return 0;
    if (!game_progress_resolve_path(path, resolved_path, sizeof(resolved_path))) return 0;

    f = fopen(resolved_path, "r");
    if (!f) {
        size_t placeholder_count = max_entries < 3 ? max_entries : 3;
        game_progress_set_placeholder_leaderboard(out_entries, placeholder_count);
        game_progress_save_leaderboard_entries(path, out_entries, placeholder_count);
        if (out_count) *out_count = placeholder_count;
        return 1;
    }

    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    game_progress_trim_line(line);
    if (strcmp(line, "player_name,score,time_sec,keys_left,keys_spent,health_left") != 0) {
        fclose(f);
        return 0;
    }

    while (count < max_entries && fgets(line, sizeof(line), f)) {
        const char* cursor;
        char name[128];
        GameLeaderboardEntry entry;

        game_progress_trim_line(line);
        if (!line[0]) continue;
        cursor = line;
        memset(&entry, 0, sizeof(entry));
        if (!game_progress_csv_read_field(&cursor, name, sizeof(name))) continue;
        game_progress_copy_best_score_name(entry.player_name, sizeof(entry.player_name), name);
        entry.score = atoi(cursor);
        cursor = strchr(cursor, ',');
        if (!cursor) continue;
        ++cursor;
        entry.time_sec = atoi(cursor);
        cursor = strchr(cursor, ',');
        if (!cursor) continue;
        ++cursor;
        entry.keys_left = atoi(cursor);
        cursor = strchr(cursor, ',');
        if (!cursor) continue;
        ++cursor;
        entry.keys_spent = atoi(cursor);
        cursor = strchr(cursor, ',');
        if (!cursor) continue;
        ++cursor;
        entry.health_left = atoi(cursor);

        if (entry.score < 0) entry.score = 0;
        if (entry.time_sec < 0) entry.time_sec = 0;
        if (entry.keys_left < 0) entry.keys_left = 0;
        if (entry.keys_spent < 0) entry.keys_spent = 0;
        if (entry.health_left < 0) entry.health_left = 0;
        out_entries[count++] = entry;
    }
    fclose(f);

    if (count == 0) {
        count = max_entries < 3 ? max_entries : 3;
        game_progress_set_placeholder_leaderboard(out_entries, count);
        game_progress_save_leaderboard_entries(path, out_entries, count);
    }
    game_progress_sort_leaderboard(out_entries, count);
    if (out_count) *out_count = count;
    return 1;
}

int game_progress_record_leaderboard_entry(const char* path, const GameLeaderboardEntry* entry)
{
    GameLeaderboardEntry entries[16];
    size_t count = 0;
    size_t write_count;

    if (!path || !entry) return 0;
    memset(entries, 0, sizeof(entries));
    game_progress_load_leaderboard(path, entries, 15, &count);
    if (count > 15) count = 15;
    entries[count] = *entry;
    game_progress_copy_best_score_name(entries[count].player_name, sizeof(entries[count].player_name), entry->player_name);
    if (entries[count].score < 0) entries[count].score = 0;
    if (entries[count].time_sec < 0) entries[count].time_sec = 0;
    if (entries[count].keys_left < 0) entries[count].keys_left = 0;
    if (entries[count].keys_spent < 0) entries[count].keys_spent = 0;
    if (entries[count].health_left < 0) entries[count].health_left = 0;
    count++;
    game_progress_sort_leaderboard(entries, count);
    write_count = count < 10 ? count : 10;
    return game_progress_save_leaderboard_entries(path, entries, write_count);
}
