#include "skin_registry.h"
#include "asset_paths.h"
#include "debug_log.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define SKIN_REGISTRY_ROOT "assets/skin"

static int skin_registry_is_directory(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int skin_registry_file_exists(const char* path)
{
    struct stat st;
    if (!path || stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

static int skin_registry_parse_id(const char* name)
{
    int id = 0;
    int in_digits = 0;

    if (!name || !name[0]) return -1;

    for (const char* p = name; *p; ++p) {
        unsigned char c = (unsigned char)(*p);
        if (isdigit(c)) {
            in_digits = 1;
            if (id > 99999999) return -1;
            id = (id * 10) + (int)(c - '0');
        } else if (in_digits) {
            break;
        }
    }

    return (in_digits && id > 0) ? id : -1;
}

static int skin_registry_is_image_name(const char* file_name)
{
    size_t n = 0;
    if (!file_name) return 0;

    n = strlen(file_name);
    if (n < 4) return 0;

    if (n >= 4 && strcasecmp(file_name + n - 4, ".png") == 0) return 1;
    if (n >= 4 && strcasecmp(file_name + n - 4, ".bmp") == 0) return 1;
    if (n >= 4 && strcasecmp(file_name + n - 4, ".jpg") == 0) return 1;
    if (n >= 5 && strcasecmp(file_name + n - 5, ".jpeg") == 0) return 1;
    if (n >= 5 && strcasecmp(file_name + n - 5, ".webp") == 0) return 1;
    return 0;
}

static int skin_registry_contains_ci(const char* haystack, const char* needle)
{
    char lower_h[SKIN_REGISTRY_PATH_MAX];
    char lower_n[64];
    size_t h_len = 0;
    size_t n_len = 0;

    if (!haystack || !needle) return 0;

    h_len = strlen(haystack);
    n_len = strlen(needle);
    if (n_len == 0 || n_len >= sizeof(lower_n)) return 0;

    if (h_len >= sizeof(lower_h)) h_len = sizeof(lower_h) - 1;
    for (size_t i = 0; i < h_len; ++i) {
        lower_h[i] = (char)tolower((unsigned char)haystack[i]);
    }
    lower_h[h_len] = '\0';

    for (size_t i = 0; i < n_len; ++i) {
        lower_n[i] = (char)tolower((unsigned char)needle[i]);
    }
    lower_n[n_len] = '\0';

    return strstr(lower_h, lower_n) != NULL;
}

static void skin_registry_pick_best(char* dst, size_t dst_size, int* best_score, const char* full_path, int score)
{
    if (!dst || !dst_size || !best_score || !full_path) return;
    if (score <= *best_score) return;
    snprintf(dst, dst_size, "%s", full_path);
    *best_score = score;
}

static int skin_registry_join_path(char* out, size_t out_size, const char* left, const char* right)
{
    int written = 0;
    if (!out || out_size == 0 || !left || !right) return 0;
    written = snprintf(out, out_size, "%s/%s", left, right);
    if (written < 0 || (size_t)written >= out_size) return 0;
    return 1;
}

static void skin_registry_trim(char* s)
{
    size_t start = 0;
    size_t end = 0;
    size_t n = 0;

    if (!s) return;

    n = strlen(s);
    while (start < n && isspace((unsigned char)s[start])) start++;
    end = n;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    if (start > 0) {
        memmove(s, s + start, end - start);
    }
    s[end - start] = '\0';
}

static void skin_registry_set_default_name(SkinDefinition* skin)
{
    if (!skin) return;
    snprintf(skin->display_name, sizeof(skin->display_name), "%03d", skin->id);
}

static void skin_registry_load_display_name(SkinDefinition* skin)
{
    const char* candidates[] = {"name.txt", "skin_name.txt", "display_name.txt"};
    char name_path[SKIN_REGISTRY_PATH_MAX];
    char line[SKIN_REGISTRY_NAME_MAX];
    FILE* f = NULL;

    if (!skin) return;
    skin_registry_set_default_name(skin);
    if (!skin->folder_path[0]) return;

    for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); ++i) {
        if (!skin_registry_join_path(name_path, sizeof(name_path), skin->folder_path, candidates[i])) continue;
        if (!skin_registry_file_exists(name_path)) continue;

        f = fopen(name_path, "r");
        if (!f) continue;

        if (fgets(line, sizeof(line), f)) {
            line[sizeof(line) - 1] = '\0';
            skin_registry_trim(line);
            if (line[0]) {
                snprintf(skin->display_name, sizeof(skin->display_name), "%s", line);
            }
        }
        fclose(f);
        break;
    }
}

static void skin_registry_apply_choose_asset_overrides(SkinDefinition* skin)
{
    if (!skin) return;

    if (!skin->choose_idle_path[0]) {
        snprintf(skin->choose_idle_path, sizeof(skin->choose_idle_path), "%s", skin->idle_path);
    }
    if (!skin->choose_select_path[0]) {
        snprintf(skin->choose_select_path, sizeof(skin->choose_select_path), "%s", skin->run_path);
    }
}

static int skin_registry_discover_assets(const char* folder_path, SkinDefinition* out_skin)
{
    DIR* dir = NULL;
    struct dirent* ent = NULL;
    int idle_score = -1;
    int run_score = -1;
    int choose_idle_score = -1;
    int choose_select_score = -1;

    if (!folder_path || !out_skin) return 0;

    dir = opendir(folder_path);
    if (!dir) {
        debug_logf("skin_registry: cannot open folder '%s'", folder_path);
        return 0;
    }

    while ((ent = readdir(dir)) != NULL) {
        char full_path[SKIN_REGISTRY_PATH_MAX];
        const char* file_name = ent->d_name;
        int is_regular = 0;
        int is_choose_file = 0;
        int is_preview_file = 0;
        int has_idle_token = 0;
        int has_run_token = 0;
        int has_select_token = 0;
        int has_game_idle_token = 0;

        if (!file_name || file_name[0] == '.') continue;
        if (!skin_registry_is_image_name(file_name)) continue;

#ifdef DT_REG
        if (ent->d_type == DT_REG) {
            is_regular = 1;
        } else if (ent->d_type == DT_UNKNOWN) {
            if (skin_registry_join_path(full_path, sizeof(full_path), folder_path, file_name)) {
                is_regular = skin_registry_file_exists(full_path);
            }
        }
#else
        is_regular = 1;
#endif
        if (!is_regular) continue;
        if (!skin_registry_join_path(full_path, sizeof(full_path), folder_path, file_name)) continue;
        if (!skin_registry_file_exists(full_path)) continue;

        is_choose_file = skin_registry_contains_ci(file_name, "choose");
        is_preview_file = skin_registry_contains_ci(file_name, "preview");
        has_idle_token = skin_registry_contains_ci(file_name, "idle") || skin_registry_contains_ci(file_name, "dle");
        has_run_token = skin_registry_contains_ci(file_name, "run");
        has_select_token = skin_registry_contains_ci(file_name, "select");
        has_game_idle_token = skin_registry_contains_ci(file_name, "game_idle") ||
                              skin_registry_contains_ci(file_name, "move_idle");

        if (skin_registry_contains_ci(file_name, "choose_idle")) {
            skin_registry_pick_best(out_skin->choose_idle_path,
                                    sizeof(out_skin->choose_idle_path),
                                    &choose_idle_score,
                                    full_path,
                                    520);
        } else if (is_choose_file && has_idle_token) {
            skin_registry_pick_best(out_skin->choose_idle_path,
                                    sizeof(out_skin->choose_idle_path),
                                    &choose_idle_score,
                                    full_path,
                                    470);
        } else if (skin_registry_contains_ci(file_name, "preview_idle")) {
            skin_registry_pick_best(out_skin->choose_idle_path,
                                    sizeof(out_skin->choose_idle_path),
                                    &choose_idle_score,
                                    full_path,
                                    420);
        } else if (is_preview_file && has_idle_token) {
            skin_registry_pick_best(out_skin->choose_idle_path,
                                    sizeof(out_skin->choose_idle_path),
                                    &choose_idle_score,
                                    full_path,
                                    390);
        } else if (has_idle_token) {
            skin_registry_pick_best(out_skin->choose_idle_path,
                                    sizeof(out_skin->choose_idle_path),
                                    &choose_idle_score,
                                    full_path,
                                    160);
        }

        if (skin_registry_contains_ci(file_name, "choose_select")) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    520);
        } else if (is_choose_file && has_select_token) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    470);
        } else if (skin_registry_contains_ci(file_name, "preview_select")) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    420);
        } else if (is_preview_file && has_select_token) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    390);
        } else if (has_select_token) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    280);
        } else if (is_choose_file && has_run_token) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    260);
        } else if (has_run_token) {
            skin_registry_pick_best(out_skin->choose_select_path,
                                    sizeof(out_skin->choose_select_path),
                                    &choose_select_score,
                                    full_path,
                                    140);
        }

        if (has_game_idle_token && !is_choose_file && !is_preview_file) {
            skin_registry_pick_best(out_skin->idle_path,
                                    sizeof(out_skin->idle_path),
                                    &idle_score,
                                    full_path,
                                    300);
        } else if (has_idle_token && !is_choose_file && !is_preview_file) {
            skin_registry_pick_best(out_skin->idle_path,
                                    sizeof(out_skin->idle_path),
                                    &idle_score,
                                    full_path,
                                    220);
        }

        if (has_run_token && !is_choose_file && !is_preview_file) {
            skin_registry_pick_best(out_skin->run_path,
                                    sizeof(out_skin->run_path),
                                    &run_score,
                                    full_path,
                                    220);
        }
    }

    closedir(dir);

    if (!out_skin->idle_path[0] && out_skin->choose_idle_path[0]) {
        snprintf(out_skin->idle_path, sizeof(out_skin->idle_path), "%s", out_skin->choose_idle_path);
    }
    if (!out_skin->idle_path[0]) return 0;
    if (!out_skin->run_path[0]) {
        snprintf(out_skin->run_path, sizeof(out_skin->run_path), "%s", out_skin->idle_path);
    }
    if (!out_skin->choose_idle_path[0]) {
        snprintf(out_skin->choose_idle_path, sizeof(out_skin->choose_idle_path), "%s", out_skin->idle_path);
    }
    if (!out_skin->choose_select_path[0]) {
        snprintf(out_skin->choose_select_path, sizeof(out_skin->choose_select_path), "%s", out_skin->run_path);
    }

    return 1;
}

static int skin_registry_compare_id(const void* a, const void* b)
{
    const SkinDefinition* sa = (const SkinDefinition*)a;
    const SkinDefinition* sb = (const SkinDefinition*)b;

    if (sa->id < sb->id) return -1;
    if (sa->id > sb->id) return 1;
    return 0;
}

static int skin_registry_add_fallback(SkinDefinition* out_skins, int max_skins)
{
    SkinDefinition fallback = {0};

    if (!out_skins || max_skins <= 0) return 0;
    if (!skin_registry_file_exists(ASSET_GAME_SKIN1_IDLE)) return 0;

    fallback.id = 1;
    snprintf(fallback.folder_path, sizeof(fallback.folder_path), "assets/skin/skin1");
    snprintf(fallback.idle_path, sizeof(fallback.idle_path), "%s", ASSET_GAME_SKIN1_IDLE);
    if (skin_registry_file_exists(ASSET_GAME_SKIN1_RUN)) {
        snprintf(fallback.run_path, sizeof(fallback.run_path), "%s", ASSET_GAME_SKIN1_RUN);
    } else {
        snprintf(fallback.run_path, sizeof(fallback.run_path), "%s", ASSET_GAME_SKIN1_IDLE);
    }
    skin_registry_load_display_name(&fallback);
    skin_registry_apply_choose_asset_overrides(&fallback);

    out_skins[0] = fallback;
    debug_logf("skin_registry: using fallback skin id=1");
    return 1;
}

int skin_registry_load(SkinDefinition* out_skins, int max_skins)
{
    DIR* root = NULL;
    struct dirent* ent = NULL;
    int count = 0;

    if (!out_skins || max_skins <= 0) return 0;
    memset(out_skins, 0, sizeof(*out_skins) * (size_t)max_skins);

    if (!skin_registry_is_directory(SKIN_REGISTRY_ROOT)) {
        debug_logf("skin_registry: root '%s' is missing", SKIN_REGISTRY_ROOT);
        return skin_registry_add_fallback(out_skins, max_skins);
    }

    root = opendir(SKIN_REGISTRY_ROOT);
    if (!root) {
        debug_logf("skin_registry: cannot open '%s'", SKIN_REGISTRY_ROOT);
        return skin_registry_add_fallback(out_skins, max_skins);
    }

    while ((ent = readdir(root)) != NULL) {
        char folder_path[SKIN_REGISTRY_PATH_MAX];
        SkinDefinition skin = {0};
        int skin_id = -1;

        if (count >= max_skins) break;
        if (ent->d_name[0] == '.') continue;
        if (!skin_registry_join_path(folder_path, sizeof(folder_path), SKIN_REGISTRY_ROOT, ent->d_name)) continue;
        if (!skin_registry_is_directory(folder_path)) continue;

        skin_id = skin_registry_parse_id(ent->d_name);
        if (skin_id < 1) {
            debug_logf("skin_registry: skip folder without numeric id '%s'", folder_path);
            continue;
        }

        skin.id = skin_id;
        snprintf(skin.folder_path, sizeof(skin.folder_path), "%s", folder_path);

        if (!skin_registry_discover_assets(folder_path, &skin)) {
            debug_logf("skin_registry: skip skin id=%d folder='%s' (missing idle asset)", skin_id, folder_path);
            continue;
        }
        skin_registry_load_display_name(&skin);
        skin_registry_apply_choose_asset_overrides(&skin);

        out_skins[count++] = skin;
    }
    closedir(root);

    if (count <= 0) {
        return skin_registry_add_fallback(out_skins, max_skins);
    }

    qsort(out_skins, (size_t)count, sizeof(out_skins[0]), skin_registry_compare_id);

    {
        int write_index = 0;
        for (int i = 0; i < count; ++i) {
            if (write_index > 0 && out_skins[i].id == out_skins[write_index - 1].id) {
                debug_logf("skin_registry: duplicate id=%d skipped folder='%s'",
                           out_skins[i].id,
                           out_skins[i].folder_path);
                continue;
            }
            if (write_index != i) {
                out_skins[write_index] = out_skins[i];
            }
            write_index++;
        }
        count = write_index;
    }

    debug_logf("skin_registry: loaded %d skin(s)", count);
    return count;
}

const SkinDefinition* skin_registry_find_by_id(const SkinDefinition* skins, int skin_count, int id)
{
    if (!skins || skin_count <= 0 || id < 1) return NULL;
    for (int i = 0; i < skin_count; ++i) {
        if (skins[i].id == id) return &skins[i];
    }
    return NULL;
}

const SkinDefinition* skin_registry_first(const SkinDefinition* skins, int skin_count)
{
    if (!skins || skin_count <= 0) return NULL;
    return &skins[0];
}
