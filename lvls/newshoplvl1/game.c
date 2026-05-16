#include "game.h"

#include "admin.h"
#include "arcade.h"
#include "audio.h"
#include "barista.h"
#include "billiards.h"
#include "input.h"
#include "map.h"
#include "newshop.h"
#include "online_client.h"
#include "player.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRICE_JETPACK_BASE 5
#define PRICE_MAGNET_BASE 4
#define PRICE_SHOES_BASE 6
#define SHOP_START_KEYS 10
#define SHOP_DEFAULT_LIVES 9
#define SHOP_MSG_DURATION_MS 2600u
#define SHOP_ITEM_COUNT 3
#define SHOP_SCENE_FADE_MS 260u
#define ARCADE_PUZZLE_FADE_MS 900u
#define ARCADE_PUZZLE_W 1280
#define ARCADE_PUZZLE_H 720
#define TV_CONFIG_PATH "shop_tv_config.dat"
#define AD_CONFIG_PATH "shop_ad_config.dat"
#define ARCADE_CONFIG_PATH "shop_arcade_config.dat"
#define ARCADE_POPUP_CONFIG_PATH "shop_arcade_popup_config.dat"
#define NPC_CONFIG_PATH "shop_npc_config.dat"
#define TV_MIN_SIZE 24.0f
#define NPC_MIN_SIZE 20.0f
#define AD_MAX_FRAMES 306
#define AD_FRAME_DURATION_MS 33u
#define NPC_SKIN_COUNT 14
#define NPC_SKIN_FRAME_COLS 6
#define NPC_SKIN_FRAME_ROWS 6
#define NPC_SKIN_ANIM_FRAME_MS 140u
#define SHOP_PUZZLE_PHOTO_MAX 10

typedef struct {
    const char *path;
    const char *label;
} NpcSkinDef;

static const NpcSkinDef NPC_SKINS[NPC_SKIN_COUNT] = {
    {"assets/npc/make-his-mouth-move-.png", "His Mouth 1"},
    {"assets/npc/make-his-mouth-move--1.png", "His Mouth 2"},
    {"assets/npc/make-his-mouth-move--2.png", "His Mouth 3"},
    {"assets/npc/make-him-move-his-mo.png", "Him Move"},
    {"assets/npc/make-her-mouth-move-.png", "Her Mouth 1"},
    {"assets/npc/make-her-mouth-move--1.png", "Her Mouth 2"},
    {"assets/npc/make-her-mouth-move--2.png", "Her Mouth 3"},
    {"assets/npc/make-her-look-like-t.png", "Her Look"},
    {"assets/npc/idel.png", "Idle"},
    {"assets/npc/idel-.png", "Idle Alt 1"},
    {"assets/npc/idel-1.png", "Idle Alt 2"},
    {"assets/npc/idel-2.png", "Idle Alt 3"},
    {"assets/npc/idel-and-waving-.png", "Idle Waving"},
    {"assets/npc/idel-moving-his-head.png", "Idle Head Move"}
};

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;

    bool running;

    InputState input;
    AudioState audio;
    MapState map;
    Player player;
    Player player2;
    PlayerAssets player_assets;
    PlayerAssets player2_assets;
    BaristaState barista;
    BilliardsState billiards;
    ArcadeState arcade;

    TextureAsset tex_billiards_table;
    TextureAsset tex_billiards_popup;
    TextureAsset tex_billiards_correct;
    TextureAsset tex_food_buffet;
    TextureAsset tex_food_buffet2;
    TextureAsset tex_wall;
    TextureAsset tex_door;
    TextureAsset tex_window;
    TextureAsset tex_tv;
    TextureAsset tex_arcade;
    TextureAsset tex_arcade_playing;
    TextureAsset tex_ad_frames[AD_MAX_FRAMES];
    int ad_frame_count;
    TextureAsset tex_x_button;
    TextureAsset tex_table;
    TextureAsset tex_barista;

    TextureAsset tex_baton;
    TextureAsset tex_white_ball;
    TextureAsset tex_eight_ball;
    TextureAsset tex_ball1;
    TextureAsset tex_npc_skins[NPC_SKIN_COUNT];
    TextureAsset tex_shop_frame;
    TextureAsset tex_shop_item_1;
    TextureAsset tex_shop_item_2;
    TextureAsset tex_shop_item_3;
    TextureAsset tex_tic;
    TextureAsset tex_x_mark;
    TextureAsset tex_maze_wall;
    TextureAsset tex_maze_finish;
    TextureAsset tex_maze_swall;
    TextureAsset tex_maze_fan;
    TextureAsset tex_maze_car;
    TextureAsset tex_puzzle_background;
    TextureAsset tex_puzzle_photos[SHOP_PUZZLE_PHOTO_MAX];
    int puzzle_photo_count;
    int npc_skin_count;

    bool shop_open;
    bool arcade_popup_open;
    bool arcade_puzzle_fullscreen;
    uint32_t arcade_puzzle_fade_started_at;
    bool duo_enabled;
    bool online_hosted;
    int control_scheme;
    InteractBind player_interact_bind[2];
    int player_skin_number[2];
    int shop_selection;
    int lives_held;
    int lives_held_p2;
    int extra_hearts_held;
    int extra_hearts_held_p2;
    int keys_held;
    int active_shop_player;
    int buy_count_jetpack;
    int buy_count_magnet;
    int buy_count_shoes;
    int player_buy_count_jetpack[2];
    int player_buy_count_magnet[2];
    int player_buy_count_shoes[2];
    int owned_jetpack;
    int owned_magnet;
    int owned_shoes;
    int purchased_jetpack;
    int purchased_magnet;
    int purchased_shoes;
    int player_purchased_jetpack[2];
    int player_purchased_magnet[2];
    int player_purchased_shoes[2];
    char shop_msg[128];
    uint32_t shop_msg_started_at;
    bool billiards_used_this_visit;
    bool billiards_result_checked;
    bool billiards_was_visible;
    Rect tv_rect;
    Rect ad_rect;
    Rect arcade_popup_rect;
    AdminNpcConfig npc_config;
    bool tv_admin_open;
    int tv_admin_field;
    int tv_admin_target;
    int tv_admin_npc_index;
    uint32_t scene_fade_started_at;
    uint8_t scene_fade_alpha;
    int scene_fade_direction;
    bool close_after_fade;
} GameState;

typedef enum {
    TV_ADMIN_FIELD_X = 0,
    TV_ADMIN_FIELD_Y,
    TV_ADMIN_FIELD_W,
    TV_ADMIN_FIELD_H,
    TV_ADMIN_FIELD_Z,
    TV_ADMIN_FIELD_COUNT
} TvAdminField;

typedef enum {
    TV_ADMIN_TARGET_TV = 0,
    TV_ADMIN_TARGET_AD,
    TV_ADMIN_TARGET_ARCADE,
    TV_ADMIN_TARGET_ARCADE_POPUP,
    TV_ADMIN_TARGET_NPC,
    TV_ADMIN_TARGET_COUNT
} TvAdminTarget;

static int clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

float ease_out_cubic(float value) {
    const float clamped = clampf(value, 0.0f, 1.0f);
    return 1.0f - powf(1.0f - clamped, 3.0f);
}

float ease_in_out_cubic(float value) {
    const float clamped = clampf(value, 0.0f, 1.0f);
    if (clamped < 0.5f) {
        return 4.0f * powf(clamped, 3.0f);
    }
    return 1.0f - powf(-2.0f * clamped + 2.0f, 3.0f) / 2.0f;
}

float ease_out_back(float value, float overshoot) {
    const float clamped = clampf(value, 0.0f, 1.0f);
    const float adjusted = clamped - 1.0f;
    const float coefficient = overshoot + 1.0f;

    return 1.0f + coefficient * adjusted * adjusted * adjusted + overshoot * adjusted * adjusted;
}

bool rect_overlaps(Rect a, Rect b) {
    return (
        a.x < b.x + b.w &&
        a.x + a.w > b.x &&
        a.y < b.y + b.h &&
        a.y + a.h > b.y
    );
}

bool rect_touch_or_overlap(Rect a, Rect b) {
    return (
        a.x <= b.x + b.w &&
        a.x + a.w >= b.x &&
        a.y <= b.y + b.h &&
        a.y + a.h >= b.y
    );
}

static bool rect_contains_rect(Rect outer, Rect inner) {
    return (
        inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.w <= outer.x + outer.w &&
        inner.y + inner.h <= outer.y + outer.h
    );
}

static bool spawn_candidate_valid(const MapState *map, float x, float y) {
    const Rect player_rect = {
        .x = x,
        .y = y,
        .w = (float)PLAYER_W,
        .h = (float)PLAYER_H
    };
    bool inside_walkable = false;
    Rect solids[MAX_SOLID_OBSTACLES];
    const int solid_count = map_get_solid_obstacles(map, solids, MAX_SOLID_OBSTACLES);

    for (int i = 0; i < map->walkable_count; ++i) {
        if (rect_contains_rect(map->walkable[i], player_rect)) {
            inside_walkable = true;
            break;
        }
    }
    if (!inside_walkable) {
        return false;
    }

    for (int i = 0; i < solid_count; ++i) {
        if (rect_overlaps(player_rect, solids[i])) {
            return false;
        }
    }

    return true;
}

static void choose_duo_spawn(
    const MapState *map,
    float base_x,
    float base_y,
    float *out_x,
    float *out_y
) {
    const float horizontal_step = (float)PLAYER_W + 10.0f;
    const float offsets[] = {
        horizontal_step,
        -horizontal_step,
        horizontal_step * 2.0f,
        -horizontal_step * 2.0f,
        0.0f
    };
    const float max_x = fmaxf(0.0f, (float)map->world_w - (float)PLAYER_W);
    const float max_y = fmaxf(0.0f, (float)map->world_h - (float)PLAYER_H);
    float best_x = base_x;
    float best_y = base_y;
    float best_dist_sq = INFINITY;
    bool found = false;

    for (int i = 0; i < (int)(sizeof(offsets) / sizeof(offsets[0])); ++i) {
        const float candidate_x = clampf(base_x + offsets[i], 0.0f, max_x);
        const float candidate_y = clampf(base_y, 0.0f, max_y);
        if (spawn_candidate_valid(map, candidate_x, candidate_y)) {
            *out_x = candidate_x;
            *out_y = candidate_y;
            return;
        }
    }

    for (int y = 0; y <= (int)max_y; ++y) {
        for (int x = 0; x <= (int)max_x; ++x) {
            const float candidate_x = (float)x;
            const float candidate_y = (float)y;
            if (!spawn_candidate_valid(map, candidate_x, candidate_y)) {
                continue;
            }
            const float dx = candidate_x - base_x;
            const float dy = candidate_y - base_y;
            const float dist_sq = dx * dx + dy * dy;
            if (!found || dist_sq < best_dist_sq) {
                found = true;
                best_dist_sq = dist_sq;
                best_x = candidate_x;
                best_y = candidate_y;
            }
        }
    }

    *out_x = found ? best_x : clampf(base_x, 0.0f, max_x);
    *out_y = found ? best_y : clampf(base_y, 0.0f, max_y);
}

static Rect default_tv_rect(const MapState *map) {
    Rect rect;
    rect.w = 128.0f;
    rect.h = 80.0f;
    rect.x = (float)map->world_w - rect.w - 34.0f;
    rect.y = 66.0f;
    return rect;
}

static Rect default_ad_rect(const MapState *map) {
    Rect rect;
    rect.w = 220.0f;
    rect.h = 124.0f;
    rect.x = fmaxf(16.0f, (float)map->world_w - rect.w - 186.0f);
    rect.y = 58.0f;
    return rect;
}

static Rect default_arcade_popup_rect(void) {
    Rect rect;
    rect.w = WINDOW_W * 0.72f;
    rect.h = WINDOW_H * 0.72f;
    rect.x = ((float)WINDOW_W - rect.w) * 0.5f;
    rect.y = ((float)WINDOW_H - rect.h) * 0.5f;
    return rect;
}

static void clamp_rect_to_screen(Rect *rect, float min_size) {
    if (rect->w < min_size) {
        rect->w = min_size;
    }
    if (rect->h < min_size) {
        rect->h = min_size;
    }

    if (rect->w > (float)WINDOW_W) {
        rect->w = (float)WINDOW_W;
    }
    if (rect->h > (float)WINDOW_H) {
        rect->h = (float)WINDOW_H;
    }

    rect->x = clampf(rect->x, 0.0f, fmaxf(0.0f, (float)WINDOW_W - rect->w));
    rect->y = clampf(rect->y, 0.0f, fmaxf(0.0f, (float)WINDOW_H - rect->h));
}

static void clamp_rect_to_world(const MapState *map, Rect *rect, float min_size) {
    if (rect->w < min_size) {
        rect->w = min_size;
    }
    if (rect->h < min_size) {
        rect->h = min_size;
    }

    if (rect->w > (float)map->world_w) {
        rect->w = (float)map->world_w;
    }
    if (rect->h > (float)map->world_h) {
        rect->h = (float)map->world_h;
    }

    {
        const float max_x = fmaxf(0.0f, (float)map->world_w - rect->w);
        const float max_y = fmaxf(0.0f, (float)map->world_h - rect->h);
        rect->x = clampf(rect->x, 0.0f, max_x);
        rect->y = clampf(rect->y, 0.0f, max_y);
    }
}

static void clamp_tv_rect_to_world(GameState *game) {
    clamp_rect_to_world(&game->map, &game->tv_rect, TV_MIN_SIZE);
}

static void clamp_ad_rect_to_world(GameState *game) {
    clamp_rect_to_world(&game->map, &game->ad_rect, TV_MIN_SIZE);
}

static void clamp_arcade_rect_to_world(GameState *game) {
    Rect rect = game->map.arcade_rect;
    clamp_rect_to_world(&game->map, &rect, TV_MIN_SIZE);
    map_set_arcade_rect(&game->map, rect);
}

static void clamp_arcade_popup_rect_to_screen(GameState *game) {
    clamp_rect_to_screen(&game->arcade_popup_rect, TV_MIN_SIZE);
}

static int normalize_npc_skin_index(const GameState *game, int skin_index) {
    if (game->npc_skin_count <= 0) {
        return 0;
    }

    int normalized = skin_index % game->npc_skin_count;
    if (normalized < 0) {
        normalized += game->npc_skin_count;
    }
    return normalized;
}

static Rect default_npc_rect(const MapState *map, int index) {
    Rect rect;
    rect.w = 64.0f;
    rect.h = 104.0f;
    rect.x = clampf(map->spawn_x + 42.0f + (float)(index * 18), 0.0f, (float)map->world_w - rect.w);
    rect.y = clampf(map->spawn_y - 20.0f, 0.0f, (float)map->world_h - rect.h);
    return rect;
}

static AdminNpcEntry *get_selected_admin_npc(GameState *game) {
    if (game->npc_config.count <= 0) {
        game->tv_admin_npc_index = 0;
        return NULL;
    }

    if (game->tv_admin_npc_index < 0) {
        game->tv_admin_npc_index = 0;
    }
    if (game->tv_admin_npc_index >= game->npc_config.count) {
        game->tv_admin_npc_index = game->npc_config.count - 1;
    }
    return &game->npc_config.entries[game->tv_admin_npc_index];
}

static const AdminNpcEntry *get_selected_admin_npc_const(const GameState *game) {
    if (game->npc_config.count <= 0) {
        return NULL;
    }

    int index = game->tv_admin_npc_index;
    if (index < 0) {
        index = 0;
    }
    if (index >= game->npc_config.count) {
        index = game->npc_config.count - 1;
    }
    return &game->npc_config.entries[index];
}

static void sanitize_npc_entry(GameState *game, AdminNpcEntry *entry) {
    if (!entry) {
        return;
    }

    entry->skin_index = normalize_npc_skin_index(game, entry->skin_index);
    entry->z_index = clampi(entry->z_index, -100, 100);
    clamp_rect_to_world(&game->map, &entry->rect, NPC_MIN_SIZE);
}

static void sanitize_npc_config(GameState *game) {
    if (game->npc_config.count < 0) {
        game->npc_config.count = 0;
    }
    if (game->npc_config.count > ADMIN_MAX_NPCS) {
        game->npc_config.count = ADMIN_MAX_NPCS;
    }

    for (int i = 0; i < game->npc_config.count; ++i) {
        sanitize_npc_entry(game, &game->npc_config.entries[i]);
    }

    if (game->npc_config.count <= 0) {
        game->tv_admin_npc_index = 0;
    } else {
        game->tv_admin_npc_index = clampi(game->tv_admin_npc_index, 0, game->npc_config.count - 1);
    }
}

static void add_admin_npc(GameState *game) {
    if (game->npc_config.count >= ADMIN_MAX_NPCS) {
        return;
    }

    const int index = game->npc_config.count;
    AdminNpcEntry *entry = &game->npc_config.entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->skin_index = index % (game->npc_skin_count > 0 ? game->npc_skin_count : 1);
    entry->z_index = 0;
    entry->flip_x = false;
    entry->rect = default_npc_rect(&game->map, index);
    sanitize_npc_entry(game, entry);

    game->npc_config.count++;
    game->tv_admin_npc_index = game->npc_config.count - 1;
}

static void remove_selected_admin_npc(GameState *game) {
    const int count = game->npc_config.count;
    if (count <= 0) {
        return;
    }

    const int index = clampi(game->tv_admin_npc_index, 0, count - 1);
    for (int i = index; i < count - 1; ++i) {
        game->npc_config.entries[i] = game->npc_config.entries[i + 1];
    }
    memset(&game->npc_config.entries[count - 1], 0, sizeof(game->npc_config.entries[count - 1]));
    game->npc_config.count = count - 1;
    sanitize_npc_config(game);
}

static TextureAsset load_texture(SDL_Renderer *renderer, const char *path) {
    TextureAsset asset;
    memset(&asset, 0, sizeof(asset));

    asset.texture = IMG_LoadTexture(renderer, path);
    if (!asset.texture) {
        SDL_Log("Failed to load texture '%s': %s", path, IMG_GetError());
        return asset;
    }

    SDL_QueryTexture(asset.texture, NULL, NULL, &asset.w, &asset.h);
    asset.loaded = true;
    return asset;
}

static TextureAsset load_texture_first_available(
    SDL_Renderer *renderer,
    const char *const *paths,
    int path_count
) {
    TextureAsset asset;
    memset(&asset, 0, sizeof(asset));

    for (int i = 0; i < path_count; ++i) {
        asset = load_texture(renderer, paths[i]);
        if (asset.loaded) {
            return asset;
        }
    }

    return asset;
}

static void load_animated_ad_frames(GameState *game) {
    game->ad_frame_count = 0;

    for (int i = 1; i <= AD_MAX_FRAMES; ++i) {
        TextureAsset frame;
        char frame_path[256];
        memset(&frame, 0, sizeof(frame));

        SDL_snprintf(frame_path, sizeof(frame_path), "../animated_ad/frame (%d).png", i);
        frame = load_texture(game->renderer, frame_path);
        if (!frame.loaded) {
            SDL_snprintf(frame_path, sizeof(frame_path), "../../animated_ad/frame (%d).png", i);
            frame = load_texture(game->renderer, frame_path);
        }
        if (!frame.loaded) {
            SDL_snprintf(frame_path, sizeof(frame_path), "animated_ad/frame (%d).png", i);
            frame = load_texture(game->renderer, frame_path);
        }
        if (!frame.loaded) {
            break;
        }

        game->tex_ad_frames[game->ad_frame_count++] = frame;
    }
}

static void load_puzzle_photo_textures(GameState *game) {
    static const char *const puzzle_photo_paths[SHOP_PUZZLE_PHOTO_MAX] = {
        "arcade mini games/puzzle/gamepzl/photos/bg_pool.webp",
        "arcade mini games/puzzle/gamepzl/photos/wmremove-transformed_1.webp",
        "arcade mini games/puzzle/gamepzl/photos/foor_buffet.webp",
        "arcade mini games/puzzle/gamepzl/photos/Screenshot From 2026-04-19 17-19-24.png",
        "arcade mini games/puzzle/gamepzl/photos/Screenshot From 2026-04-19 17-19-57.png",
        "arcade mini games/puzzle/gamepzl/photos/Screenshot From 2026-04-19 17-20-16.png",
        "arcade mini games/puzzle/gamepzl/photos/billiards.webp",
        "arcade mini games/puzzle/gamepzl/photos/bg_street_far.webp",
        "arcade mini games/puzzle/gamepzl/photos/bg_street_mid.webp",
        "arcade mini games/puzzle/gamepzl/photos/bg_street_near.webp"
    };

    game->puzzle_photo_count = 0;
    for (int i = 0; i < SHOP_PUZZLE_PHOTO_MAX; ++i) {
        TextureAsset photo = load_texture(game->renderer, puzzle_photo_paths[i]);
        if (!photo.loaded) {
            continue;
        }
        game->tex_puzzle_photos[game->puzzle_photo_count++] = photo;
    }
    arcade_set_puzzle_photo_count(&game->arcade, game->puzzle_photo_count);
}

static SpriteSheet load_sprite_sheet(SDL_Renderer *renderer, const char *path, int cols, int rows) {
    SpriteSheet sheet;
    memset(&sheet, 0, sizeof(sheet));

    sheet.base = load_texture(renderer, path);
    sheet.cols = cols;
    sheet.rows = rows;

    if (sheet.base.loaded && cols > 0 && rows > 0) {
        sheet.frame_count = cols * rows;
        sheet.frame_w = sheet.base.w / cols;
        sheet.frame_h = sheet.base.h / rows;
    }

    return sheet;
}

static int normalize_skin_number(int skin_number) {
    return (skin_number == 2) ? 2 : 1;
}

static SpriteSheet load_player_sheet_for_skin_number(
    GameState *game,
    int skin_number,
    const char *relative_name,
    int cols,
    int rows
) {
    SpriteSheet sheet;
    char path[256];
    const int resolved_skin = normalize_skin_number(skin_number);
    SDL_snprintf(path, sizeof(path), "assets/skin%d/%s", resolved_skin, relative_name);
    sheet = load_sprite_sheet(game->renderer, path, cols, rows);
    if (!sheet.base.loaded && resolved_skin != 1) {
        SDL_snprintf(path, sizeof(path), "assets/skin1/%s", relative_name);
        sheet = load_sprite_sheet(game->renderer, path, cols, rows);
    }
    return sheet;
}

static SpriteSheet load_player_sheet_for_skin_number_first_available(
    GameState *game,
    int skin_number,
    const char *const *relative_names,
    int relative_name_count,
    int cols,
    int rows
) {
    SpriteSheet sheet;
    char path[256];
    const int resolved_skin = normalize_skin_number(skin_number);

    memset(&sheet, 0, sizeof(sheet));

    for (int i = 0; i < relative_name_count; ++i) {
        SDL_snprintf(path, sizeof(path), "assets/skin%d/%s", resolved_skin, relative_names[i]);
        sheet = load_sprite_sheet(game->renderer, path, cols, rows);
        if (sheet.base.loaded) {
            return sheet;
        }
    }

    if (resolved_skin != 1) {
        for (int i = 0; i < relative_name_count; ++i) {
            SDL_snprintf(path, sizeof(path), "assets/skin1/%s", relative_names[i]);
            sheet = load_sprite_sheet(game->renderer, path, cols, rows);
            if (sheet.base.loaded) {
                return sheet;
            }
        }
    }

    return sheet;
}

static void load_player_assets_for_skin(
    GameState *game,
    int skin_number,
    PlayerAssets *out_assets
) {
    static const char *const idle_down_skin1_names[] = {
        "idel-.png",
        "idel side and looking down.png"
    };
    static const char *const idle_down_skin2_names[] = {
        "idel side and looking down.png",
        "idel-.png"
    };
    static const char *const idle_side_skin1_names[] = {
        "idellokingside.png",
        "idel side and looking down.png"
    };
    static const char *const idle_side_skin2_names[] = {
        "idel side and looking down.png",
        "idellokingside.png"
    };
    static const char *const walk_left_skin1_names[] = {
        "wallking-in-place-li.png",
        "wallking-side.png"
    };
    static const char *const walk_left_skin2_names[] = {
        "wallking-side.png",
        "wallking-in-place-li.png"
    };

    const int resolved_skin = normalize_skin_number(skin_number);
    const bool skin2 = resolved_skin == 2;
    const char *const *idle_down_names = skin2 ? idle_down_skin2_names : idle_down_skin1_names;
    const char *const *idle_side_names = skin2 ? idle_side_skin2_names : idle_side_skin1_names;
    const char *const *walk_left_names = skin2 ? walk_left_skin2_names : walk_left_skin1_names;

    out_assets->idle_down = load_player_sheet_for_skin_number_first_available(
        game,
        resolved_skin,
        idle_down_names,
        2,
        5,
        5
    );
    out_assets->idle_up = load_player_sheet_for_skin_number(
        game,
        resolved_skin,
        "idel looking top.png",
        5,
        5
    );
    out_assets->idle_side = load_player_sheet_for_skin_number_first_available(
        game,
        resolved_skin,
        idle_side_names,
        2,
        5,
        5
    );
    out_assets->walk_down = load_player_sheet_for_skin_number(
        game,
        resolved_skin,
        "walking down.png",
        skin2 ? 6 : 5,
        skin2 ? 6 : 5
    );
    out_assets->walk_up = load_player_sheet_for_skin_number(
        game,
        resolved_skin,
        "walking-away-from-th.png",
        6,
        6
    );
    out_assets->walk_left = load_player_sheet_for_skin_number_first_available(
        game,
        resolved_skin,
        walk_left_names,
        2,
        skin2 ? 5 : 6,
        skin2 ? 5 : 6
    );
}

static void unload_texture(TextureAsset *asset) {
    if (asset->texture) {
        SDL_DestroyTexture(asset->texture);
        asset->texture = NULL;
    }
    asset->loaded = false;
    asset->w = 0;
    asset->h = 0;
}

static void draw_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int x,
    int y,
    SDL_Color color
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
    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void draw_text_right(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const char *text,
    int right_x,
    int y,
    SDL_Color color
) {
    int text_w = 0;
    int text_h = 0;
    if (!font || !text || !text[0]) {
        return;
    }
    if (TTF_SizeUTF8(font, text, &text_w, &text_h) != 0) {
        text_w = 0;
    }
    draw_text(renderer, font, text, right_x - text_w, y, color);
}

static int shop_price(int base_price, int purchase_count) {
    return base_price + purchase_count * 3;
}

static int clamp_player_index(int player_index) {
    return player_index == 1 ? 1 : 0;
}

static void sync_shop_counts_from_active_player(GameState *game) {
    const int p = clamp_player_index(game->active_shop_player);

    game->buy_count_jetpack = game->player_buy_count_jetpack[p];
    game->buy_count_magnet = game->player_buy_count_magnet[p];
    game->buy_count_shoes = game->player_buy_count_shoes[p];
    game->owned_jetpack = game->buy_count_jetpack;
    game->owned_magnet = game->buy_count_magnet;
    game->owned_shoes = game->buy_count_shoes;
}

static void sync_active_player_shop_counts(GameState *game) {
    const int p = clamp_player_index(game->active_shop_player);

    game->player_buy_count_jetpack[p] = game->buy_count_jetpack;
    game->player_buy_count_magnet[p] = game->buy_count_magnet;
    game->player_buy_count_shoes[p] = game->buy_count_shoes;
}

static void open_barista_shop(GameState *game, int player_index) {
    game->active_shop_player = clamp_player_index(player_index);
    sync_shop_counts_from_active_player(game);
    game->shop_open = true;
    game->shop_selection = 0;
    input_clear_movement_keys(&game->input);
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.is_walking = false;
    game->player2.vx = 0.0f;
    game->player2.vy = 0.0f;
    game->player2.is_walking = false;
    audio_set_skin2_sideways(&game->audio, false);
    audio_set_walking(&game->audio, false);
}

static void close_barista_shop(GameState *game) {
    game->shop_open = false;
}

static void begin_scene_fade(GameState *game, uint32_t now_ms, int direction, bool close_after_fade) {
    if (!game) {
        return;
    }

    game->scene_fade_started_at = now_ms;
    game->scene_fade_direction = direction;
    game->close_after_fade = close_after_fade;
    game->scene_fade_alpha = (direction > 0) ? 0u : 255u;
}

static void stop_player_motion(GameState *game) {
    game->player.vx = 0.0f;
    game->player.vy = 0.0f;
    game->player.is_walking = false;
    game->player2.vx = 0.0f;
    game->player2.vy = 0.0f;
    game->player2.is_walking = false;
    audio_set_skin2_sideways(&game->audio, false);
    audio_set_walking(&game->audio, false);
}

static Rect *get_admin_target_rect(GameState *game) {
    if (game->tv_admin_target == TV_ADMIN_TARGET_AD) {
        return &game->ad_rect;
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE) {
        return &game->map.arcade_rect;
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE_POPUP) {
        return &game->arcade_popup_rect;
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        AdminNpcEntry *npc = get_selected_admin_npc(game);
        if (!npc) {
            return NULL;
        }
        return &npc->rect;
    }
    return &game->tv_rect;
}

static const char *get_admin_target_name(const GameState *game) {
    if (game->tv_admin_target == TV_ADMIN_TARGET_AD) {
        return "AD";
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE) {
        return "ARCADE";
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE_POPUP) {
        return "ARCADE POPUP";
    }
    if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        return "NPC";
    }
    return "TV";
}

static void clamp_admin_target_rect(GameState *game) {
    if (game->tv_admin_target == TV_ADMIN_TARGET_AD) {
        clamp_ad_rect_to_world(game);
    } else if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE) {
        clamp_arcade_rect_to_world(game);
    } else if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE_POPUP) {
        clamp_arcade_popup_rect_to_screen(game);
    } else if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        AdminNpcEntry *npc = get_selected_admin_npc(game);
        if (npc) {
            sanitize_npc_entry(game, npc);
        }
    } else {
        clamp_tv_rect_to_world(game);
    }
}

static bool save_admin_layouts(GameState *game) {
    sanitize_npc_config(game);
    const bool tv_ok = admin_save_tv_rect(&game->tv_rect, TV_CONFIG_PATH);
    const bool ad_ok = admin_save_ad_rect(&game->ad_rect, AD_CONFIG_PATH);
    const bool arcade_ok = admin_save_arcade_rect(&game->map.arcade_rect, ARCADE_CONFIG_PATH);
    const bool arcade_popup_ok = admin_save_arcade_popup_rect(
        &game->arcade_popup_rect,
        ARCADE_POPUP_CONFIG_PATH
    );
    const bool npc_ok = admin_save_npc_config(&game->npc_config, NPC_CONFIG_PATH);
    if (!tv_ok || !ad_ok || !arcade_ok || !arcade_popup_ok || !npc_ok) {
        SDL_Log(
            "Failed to save admin layout: tv=%d ad=%d arcade=%d arcade_popup=%d npc=%d",
            tv_ok ? 1 : 0,
            ad_ok ? 1 : 0,
            arcade_ok ? 1 : 0,
            arcade_popup_ok ? 1 : 0,
            npc_ok ? 1 : 0
        );
    }
    return tv_ok && ad_ok && arcade_ok && arcade_popup_ok && npc_ok;
}

static void apply_tv_admin_delta(GameState *game, float delta) {
    Rect *rect = get_admin_target_rect(game);
    if (!rect) {
        return;
    }

    switch (game->tv_admin_field) {
        case TV_ADMIN_FIELD_X:
            rect->x += delta;
            break;
        case TV_ADMIN_FIELD_Y:
            rect->y += delta;
            break;
        case TV_ADMIN_FIELD_W:
            rect->w += delta;
            break;
        case TV_ADMIN_FIELD_H:
            rect->h += delta;
            break;
        case TV_ADMIN_FIELD_Z:
            break;
        default:
            break;
    }
    clamp_admin_target_rect(game);
}

static void handle_tv_admin_input(GameState *game, uint32_t now_ms) {
    if (input_pressed(&game->input, SDL_SCANCODE_1) || input_pressed(&game->input, SDL_SCANCODE_T)) {
        game->tv_admin_target = TV_ADMIN_TARGET_TV;
    } else if (input_pressed(&game->input, SDL_SCANCODE_2) || input_pressed(&game->input, SDL_SCANCODE_G)) {
        game->tv_admin_target = TV_ADMIN_TARGET_AD;
    } else if (input_pressed(&game->input, SDL_SCANCODE_3) || input_pressed(&game->input, SDL_SCANCODE_V)) {
        game->tv_admin_target = TV_ADMIN_TARGET_ARCADE;
    } else if (input_pressed(&game->input, SDL_SCANCODE_4) || input_pressed(&game->input, SDL_SCANCODE_C)) {
        game->tv_admin_target = TV_ADMIN_TARGET_ARCADE_POPUP;
    } else if (input_pressed(&game->input, SDL_SCANCODE_5) || input_pressed(&game->input, SDL_SCANCODE_B)) {
        game->tv_admin_target = TV_ADMIN_TARGET_NPC;
    }

    if (game->tv_admin_target != TV_ADMIN_TARGET_NPC && game->tv_admin_field == TV_ADMIN_FIELD_Z) {
        game->tv_admin_field = TV_ADMIN_FIELD_X;
    }

    if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        if (input_pressed(&game->input, SDL_SCANCODE_LEFTBRACKET) && game->npc_config.count > 0) {
            game->tv_admin_npc_index = (game->tv_admin_npc_index + game->npc_config.count - 1) % game->npc_config.count;
        } else if (input_pressed(&game->input, SDL_SCANCODE_RIGHTBRACKET) && game->npc_config.count > 0) {
            game->tv_admin_npc_index = (game->tv_admin_npc_index + 1) % game->npc_config.count;
        }

        if (input_pressed(&game->input, SDL_SCANCODE_N)) {
            if (game->npc_config.count >= ADMIN_MAX_NPCS) {
                SDL_snprintf(game->shop_msg, sizeof(game->shop_msg), "NPC limit reached (%d).", ADMIN_MAX_NPCS);
            } else {
                add_admin_npc(game);
                SDL_snprintf(
                    game->shop_msg,
                    sizeof(game->shop_msg),
                    "NPC %d added.",
                    game->tv_admin_npc_index + 1
                );
            }
            game->shop_msg_started_at = now_ms;
        }

        if (
            input_pressed(&game->input, SDL_SCANCODE_DELETE) ||
            input_pressed(&game->input, SDL_SCANCODE_BACKSPACE)
        ) {
            if (game->npc_config.count <= 0) {
                SDL_snprintf(game->shop_msg, sizeof(game->shop_msg), "No NPC to remove.");
            } else {
                remove_selected_admin_npc(game);
                SDL_snprintf(game->shop_msg, sizeof(game->shop_msg), "NPC removed.");
            }
            game->shop_msg_started_at = now_ms;
        }

        AdminNpcEntry *npc = get_selected_admin_npc(game);
        if (npc) {
            if (input_pressed(&game->input, SDL_SCANCODE_Q)) {
                npc->skin_index = normalize_npc_skin_index(game, npc->skin_index - 1);
            } else if (input_pressed(&game->input, SDL_SCANCODE_E)) {
                npc->skin_index = normalize_npc_skin_index(game, npc->skin_index + 1);
            }
            if (input_pressed(&game->input, SDL_SCANCODE_F)) {
                npc->flip_x = !npc->flip_x;
            }
        }
    }

    if (input_pressed(&game->input, SDL_SCANCODE_X)) {
        game->tv_admin_field = TV_ADMIN_FIELD_X;
    } else if (input_pressed(&game->input, SDL_SCANCODE_Y)) {
        game->tv_admin_field = TV_ADMIN_FIELD_Y;
    } else if (input_pressed(&game->input, SDL_SCANCODE_W)) {
        game->tv_admin_field = TV_ADMIN_FIELD_W;
    } else if (input_pressed(&game->input, SDL_SCANCODE_H)) {
        game->tv_admin_field = TV_ADMIN_FIELD_H;
    } else if (input_pressed(&game->input, SDL_SCANCODE_Z) && game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        game->tv_admin_field = TV_ADMIN_FIELD_Z;
    } else if (input_pressed(&game->input, SDL_SCANCODE_TAB)) {
        const int field_count = (game->tv_admin_target == TV_ADMIN_TARGET_NPC)
            ? TV_ADMIN_FIELD_COUNT
            : TV_ADMIN_FIELD_Z;
        if (game->tv_admin_field >= field_count) {
            game->tv_admin_field = TV_ADMIN_FIELD_X;
        } else {
            game->tv_admin_field = (game->tv_admin_field + 1) % field_count;
        }
    }

    {
        int direction = 0;
        const bool fast_step = (
            input_down(&game->input, SDL_SCANCODE_LSHIFT) ||
            input_down(&game->input, SDL_SCANCODE_RSHIFT)
        );

        if (
            input_pressed(&game->input, SDL_SCANCODE_RIGHT) ||
            input_pressed(&game->input, SDL_SCANCODE_UP) ||
            input_pressed(&game->input, SDL_SCANCODE_D)
        ) {
            direction += 1;
        }
        if (
            input_pressed(&game->input, SDL_SCANCODE_LEFT) ||
            input_pressed(&game->input, SDL_SCANCODE_DOWN) ||
            input_pressed(&game->input, SDL_SCANCODE_A)
        ) {
            direction -= 1;
        }

        if (direction != 0) {
            if (game->tv_admin_target == TV_ADMIN_TARGET_NPC && game->tv_admin_field == TV_ADMIN_FIELD_Z) {
                AdminNpcEntry *npc = get_selected_admin_npc(game);
                if (npc) {
                    npc->z_index += direction * (fast_step ? 5 : 1);
                    sanitize_npc_entry(game, npc);
                }
            } else {
                const float step = fast_step ? 10.0f : 2.0f;
                apply_tv_admin_delta(game, step * direction);
            }
        }
    }

    if (input_pressed(&game->input, SDL_SCANCODE_R)) {
        if (game->tv_admin_target == TV_ADMIN_TARGET_AD) {
            game->ad_rect = default_ad_rect(&game->map);
            clamp_ad_rect_to_world(game);
        } else if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE) {
            map_set_arcade_rect(&game->map, map_default_arcade_rect());
            clamp_arcade_rect_to_world(game);
        } else if (game->tv_admin_target == TV_ADMIN_TARGET_ARCADE_POPUP) {
            game->arcade_popup_rect = default_arcade_popup_rect();
            clamp_arcade_popup_rect_to_screen(game);
        } else if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
            AdminNpcEntry *npc = get_selected_admin_npc(game);
            if (npc) {
                const int skin_index = npc->skin_index;
                const int index = game->tv_admin_npc_index;
                const int z_index = npc->z_index;
                npc->rect = default_npc_rect(&game->map, index);
                npc->flip_x = false;
                npc->skin_index = normalize_npc_skin_index(game, skin_index);
                npc->z_index = z_index;
                sanitize_npc_entry(game, npc);
            }
        } else {
            game->tv_rect = default_tv_rect(&game->map);
            clamp_tv_rect_to_world(game);
        }
    }

    if (
        input_pressed(&game->input, SDL_SCANCODE_RETURN) ||
        input_pressed(&game->input, SDL_SCANCODE_KP_ENTER)
    ) {
        if (save_admin_layouts(game)) {
            snprintf(
                game->shop_msg,
                sizeof(game->shop_msg),
                "%s admin: saved.",
                get_admin_target_name(game)
            );
        } else {
            snprintf(
                game->shop_msg,
                sizeof(game->shop_msg),
                "%s admin: save failed.",
                get_admin_target_name(game)
            );
        }
        game->shop_msg_started_at = now_ms;
    }
}

static void perform_barista_purchase(GameState *game, uint32_t now_ms) {
    const int base_prices[SHOP_ITEM_COUNT] = {
        PRICE_JETPACK_BASE,
        PRICE_MAGNET_BASE,
        PRICE_SHOES_BASE
    };
    const char *item_names[SHOP_ITEM_COUNT] = {"Jetpack", "Magnet", "Shoes"};
    int *purchase_counts[SHOP_ITEM_COUNT] = {
        &game->buy_count_jetpack,
        &game->buy_count_magnet,
        &game->buy_count_shoes
    };
    int *owned_counts[SHOP_ITEM_COUNT] = {
        &game->owned_jetpack,
        &game->owned_magnet,
        &game->owned_shoes
    };
    int *purchased_counts[SHOP_ITEM_COUNT] = {
        &game->purchased_jetpack,
        &game->purchased_magnet,
        &game->purchased_shoes
    };
    int *player_purchased_counts[SHOP_ITEM_COUNT] = {
        &game->player_purchased_jetpack[clamp_player_index(game->active_shop_player)],
        &game->player_purchased_magnet[clamp_player_index(game->active_shop_player)],
        &game->player_purchased_shoes[clamp_player_index(game->active_shop_player)]
    };

    const int selected = game->shop_selection;
    if (selected < 0 || selected >= SHOP_ITEM_COUNT) {
        close_barista_shop(game);
        return;
    }

    const int price = shop_price(base_prices[selected], *purchase_counts[selected]);
    if (game->keys_held >= price) {
        game->keys_held -= price;
        (*purchase_counts[selected])++;
        (*owned_counts[selected])++;
        (*purchased_counts[selected])++;
        (*player_purchased_counts[selected])++;
        sync_active_player_shop_counts(game);
        snprintf(
            game->shop_msg,
            sizeof(game->shop_msg),
            "%s bought. Next cost: %d keys.",
            item_names[selected],
            shop_price(base_prices[selected], *purchase_counts[selected])
        );
    } else {
        snprintf(game->shop_msg, sizeof(game->shop_msg), "Not enough keys!");
    }
    game->shop_msg_started_at = now_ms;
    close_barista_shop(game);
}

static void draw_shop_hud(GameState *game, uint32_t now_ms) {
    const int hud_right = WINDOW_W - 14;
    char keys_text[48];
    snprintf(keys_text, sizeof(keys_text), "Keys: %d", game->keys_held);
    draw_text_right(
        game->renderer,
        game->font,
        keys_text,
        hud_right,
        14,
        (SDL_Color){245, 214, 92, 255}
    );

    if (game->duo_enabled) {
        char p1_text[64];
        char p2_text[64];
        snprintf(
            p1_text,
            sizeof(p1_text),
            "P1 Lives: %d  Hearts: %d",
            game->lives_held,
            game->extra_hearts_held
        );
        snprintf(
            p2_text,
            sizeof(p2_text),
            "P2 Lives: %d  Hearts: %d",
            game->lives_held_p2,
            game->extra_hearts_held_p2
        );
        draw_text_right(
            game->renderer,
            game->font,
            p1_text,
            hud_right,
            36,
            (SDL_Color){236, 96, 96, 255}
        );
        draw_text_right(
            game->renderer,
            game->font,
            p2_text,
            hud_right,
            58,
            (SDL_Color){96, 170, 255, 255}
        );
    } else {
        char lives_text[48];
        char hearts_text[64];
        snprintf(lives_text, sizeof(lives_text), "Lives: %d", game->lives_held);
        snprintf(hearts_text, sizeof(hearts_text), "Extra Hearts: %d", game->extra_hearts_held);
        draw_text_right(
            game->renderer,
            game->font,
            lives_text,
            hud_right,
            36,
            (SDL_Color){236, 96, 96, 255}
        );
        draw_text_right(
            game->renderer,
            game->font,
            hearts_text,
            hud_right,
            58,
            (SDL_Color){96, 170, 255, 255}
        );
    }

    if (!game->shop_msg[0] || (now_ms - game->shop_msg_started_at) > SHOP_MSG_DURATION_MS) {
        return;
    }

    int msg_w = 0;
    int msg_h = 0;
    if (game->font) {
        TTF_SizeUTF8(game->font, game->shop_msg, &msg_w, &msg_h);
    }
    if (msg_w <= 0) {
        msg_w = (int)strlen(game->shop_msg) * 8;
        msg_h = 18;
    }

    const int bubble_w = msg_w + 24;
    const int bubble_h = msg_h + 16;
    const int bubble_x = (WINDOW_W - bubble_w) / 2;
    const int bubble_y = game->duo_enabled ? 86 : 48;
    SDL_Rect bubble = {bubble_x, bubble_y, bubble_w, bubble_h};

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 28, 22, 18, 220);
    SDL_RenderFillRect(game->renderer, &bubble);
    SDL_SetRenderDrawColor(game->renderer, 194, 170, 96, 240);
    SDL_RenderDrawRect(game->renderer, &bubble);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);

    draw_text(
        game->renderer,
        game->font,
        game->shop_msg,
        bubble_x + 12,
        bubble_y + 8,
        (SDL_Color){255, 247, 198, 255}
    );
}

static void draw_tv_admin_overlay(GameState *game) {
    if (!game->tv_admin_open) {
        return;
    }

    SDL_Rect panel = {20, 86, 438, 338};
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(game->renderer, &panel);
    SDL_SetRenderDrawColor(game->renderer, 255, 203, 82, 255);
    SDL_RenderDrawRect(game->renderer, &panel);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);

    {
        char title[48];
        snprintf(title, sizeof(title), "LAYOUT ADMIN (%s)  [F1 close]", get_admin_target_name(game));
        draw_text(
            game->renderer,
            game->font,
            title,
            panel.x + 12,
            panel.y + 10,
            (SDL_Color){255, 222, 121, 255}
        );
    }

    draw_text(
        game->renderer,
        game->font,
        "Target: [1]/[T]=TV [2]/[G]=AD [3]/[V]=Arcade [4]/[C]=Arcade Popup [5]/[B]=NPC",
        panel.x + 12,
        panel.y + 34,
        (SDL_Color){221, 221, 221, 255}
    );
    draw_text(
        game->renderer,
        game->font,
        "Select: X Y W H (NPC: Z) or TAB",
        panel.x + 12,
        panel.y + 56,
        (SDL_Color){221, 221, 221, 255}
    );
    draw_text(
        game->renderer,
        game->font,
        "Adjust: Arrows / A,D",
        panel.x + 12,
        panel.y + 78,
        (SDL_Color){221, 221, 221, 255}
    );
    draw_text(
        game->renderer,
        game->font,
        "SHIFT: faster   ENTER: save   R: reset",
        panel.x + 12,
        panel.y + 100,
        (SDL_Color){201, 201, 201, 255}
    );

    int fields_start_y = panel.y + 132;
    if (game->tv_admin_target == TV_ADMIN_TARGET_NPC) {
        const AdminNpcEntry *npc = get_selected_admin_npc_const(game);
        if (npc) {
            const int skin_index = normalize_npc_skin_index(game, npc->skin_index);
            char line[144];
            SDL_snprintf(
                line,
                sizeof(line),
                "NPC: %d/%d   Skin: %d/%d (%s)",
                game->tv_admin_npc_index + 1,
                game->npc_config.count,
                skin_index + 1,
                game->npc_skin_count,
                NPC_SKINS[skin_index].label
            );
            draw_text(
                game->renderer,
                game->font,
                line,
                panel.x + 12,
                panel.y + 124,
                (SDL_Color){221, 221, 221, 255}
            );
            SDL_snprintf(
                line,
                sizeof(line),
                "Flip X: %s   Z index: %d",
                npc->flip_x ? "ON" : "OFF",
                npc->z_index
            );
            draw_text(
                game->renderer,
                game->font,
                line,
                panel.x + 12,
                panel.y + 146,
                (SDL_Color){221, 221, 221, 255}
            );
            draw_text(
                game->renderer,
                game->font,
                "[N] add  [Delete/Backspace] remove  [LeftBracket]/[RightBracket] select NPC",
                panel.x + 12,
                panel.y + 168,
                (SDL_Color){201, 201, 201, 255}
            );
            draw_text(
                game->renderer,
                game->font,
                "[Q/E] skin   [F] flip   [Z] field for Z index",
                panel.x + 12,
                panel.y + 188,
                (SDL_Color){201, 201, 201, 255}
            );
        } else {
            draw_text(
                game->renderer,
                game->font,
                "No NPC entries yet. Press [N] to add one.",
                panel.x + 12,
                panel.y + 124,
                (SDL_Color){255, 208, 120, 255}
            );
        }

        fields_start_y = panel.y + 212;
    }

    {
        const Rect *rect = get_admin_target_rect(game);
        if (rect) {
            const int field_count = (game->tv_admin_target == TV_ADMIN_TARGET_NPC)
                ? TV_ADMIN_FIELD_COUNT
                : TV_ADMIN_FIELD_Z;
            const AdminNpcEntry *npc = get_selected_admin_npc_const(game);
            const char *labels[TV_ADMIN_FIELD_COUNT] = {"X", "Y", "W", "H", "Z"};
            const int values[TV_ADMIN_FIELD_COUNT] = {
                (int)lroundf(rect->x),
                (int)lroundf(rect->y),
                (int)lroundf(rect->w),
                (int)lroundf(rect->h),
                npc ? npc->z_index : 0
            };

            for (int i = 0; i < field_count; ++i) {
                char line[48];
                snprintf(line, sizeof(line), "%s: %d", labels[i], values[i]);
                draw_text(
                    game->renderer,
                    game->font,
                    line,
                    panel.x + 32,
                    fields_start_y + i * 22,
                    (i == game->tv_admin_field)
                        ? (SDL_Color){255, 226, 104, 255}
                        : (SDL_Color){214, 214, 214, 255}
                );
            }
        }
    }

    if (game->tv_admin_target == TV_ADMIN_TARGET_AD && game->ad_frame_count <= 0) {
        draw_text(
            game->renderer,
            game->font,
            "AD frames not found in animated_ad/.",
            panel.x + 12,
            panel.y + panel.h - 20,
            (SDL_Color){255, 120, 120, 255}
        );
    }
}

static void draw_barista_shop_overlay(GameState *game) {
    if (!game->shop_open) {
        return;
    }

    const char *labels[SHOP_ITEM_COUNT] = {
        "Jetpack  -  skip 5 floors",
        "Magnet   -  attract keys (10s)",
        "Shoes    -  higher jump  (15s)"
    };
    const int base_prices[SHOP_ITEM_COUNT] = {
        PRICE_JETPACK_BASE,
        PRICE_MAGNET_BASE,
        PRICE_SHOES_BASE
    };
    const int buy_counts[SHOP_ITEM_COUNT] = {
        game->buy_count_jetpack,
        game->buy_count_magnet,
        game->buy_count_shoes
    };
    TextureAsset *icons[SHOP_ITEM_COUNT] = {
        &game->tex_shop_item_1,
        &game->tex_shop_item_2,
        &game->tex_shop_item_3
    };

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, 130);
    SDL_Rect backdrop = {0, 0, WINDOW_W, WINDOW_H};
    SDL_RenderFillRect(game->renderer, &backdrop);

    int fW = (int)(WINDOW_W * 0.90f);
    int fH = (int)((float)fW / 1.5f);
    if (fH > WINDOW_H - 24) {
        fH = WINDOW_H - 24;
        fW = (int)((float)fH * 1.5f);
    }
    if (fW > WINDOW_W - 24) {
        fW = WINDOW_W - 24;
    }
    const int fX = (WINDOW_W - fW) / 2;
    const int fY = (WINDOW_H - fH) / 2;
    SDL_Rect panel = {fX, fY, fW, fH};

    if (game->tex_shop_frame.loaded) {
        SDL_RenderCopy(game->renderer, game->tex_shop_frame.texture, NULL, &panel);
    } else {
        SDL_SetRenderDrawColor(game->renderer, 214, 182, 118, 250);
        SDL_RenderFillRect(game->renderer, &panel);
        SDL_SetRenderDrawColor(game->renderer, 128, 88, 42, 255);
        SDL_RenderDrawRect(game->renderer, &panel);
    }

    const int iX = fX + (int)(fW * 0.12f);
    const int iY = fY + (int)(fH * 0.12f);

    draw_text(game->renderer, game->font, "SHOP", iX, iY, (SDL_Color){255, 215, 60, 255});
    draw_text(
        game->renderer,
        game->font,
        "UP/DOWN select    ENTER buy    ESC close",
        iX,
        iY + 30,
        (SDL_Color){22, 22, 22, 255}
    );

    char keys_text[48];
    snprintf(keys_text, sizeof(keys_text), "Keys: %d", game->keys_held);
    draw_text(
        game->renderer,
        game->font,
        keys_text,
        fX + fW - 145,
        iY + 4,
        (SDL_Color){245, 214, 92, 255}
    );

    SDL_SetRenderDrawColor(game->renderer, 180, 140, 55, 255);
    SDL_RenderDrawLine(game->renderer, iX, iY + 62, fX + fW - 60, iY + 62);

    const int row_height = (fH - 190) / SHOP_ITEM_COUNT;
    const int row_start_y = iY + 72;
    for (int i = 0; i < SHOP_ITEM_COUNT; ++i) {
        const int price = shop_price(base_prices[i], buy_counts[i]);
        const bool selected = i == game->shop_selection;
        const bool affordable = game->keys_held >= price;
        const int iy = row_start_y + i * row_height;
        SDL_Color label_color = affordable
            ? (selected ? (SDL_Color){255, 244, 190, 255} : (SDL_Color){90, 75, 55, 255})
            : (selected ? (SDL_Color){230, 190, 160, 255} : (SDL_Color){80, 30, 30, 255});
        SDL_Color price_color = affordable
            ? (selected ? (SDL_Color){255, 228, 90, 255} : (SDL_Color){255, 200, 40, 255})
            : (selected ? (SDL_Color){220, 130, 80, 255} : (SDL_Color){170, 80, 40, 255});

        if (selected) {
            SDL_SetRenderDrawColor(game->renderer, 255, 214, 92, 55);
            SDL_Rect row_highlight = {iX - 12, iy, fW - (iX - fX) - 62, row_height - 8};
            SDL_RenderFillRect(game->renderer, &row_highlight);
            SDL_SetRenderDrawColor(game->renderer, 255, 220, 50, 220);
            SDL_RenderDrawRect(game->renderer, &row_highlight);
            draw_text(
                game->renderer,
                game->font,
                "<-",
                fX + fW - 62,
                iy + 18,
                (SDL_Color){255, 220, 50, 255}
            );
        }

        if (icons[i] && icons[i]->loaded) {
            SDL_Rect icon_dst = {iX, iy + 4, 58, 58};
            SDL_RenderCopy(game->renderer, icons[i]->texture, NULL, &icon_dst);
        } else {
            SDL_SetRenderDrawColor(game->renderer, 65, 65, 65, 200);
            SDL_Rect icon_fallback = {iX + 8, iy + 12, 42, 34};
            SDL_RenderFillRect(game->renderer, &icon_fallback);
            draw_text(game->renderer, game->font, "?", iX + 22, iy + 18, (SDL_Color){130, 130, 130, 255});
        }

        draw_text(game->renderer, game->font, labels[i], iX + 76, iy + 8, label_color);

        char price_line[48];
        snprintf(price_line, sizeof(price_line), "%d keys", price);
        draw_text(
            game->renderer,
            game->font,
            price_line,
            iX + 76,
            iy + 36,
            price_color
        );
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
}

static void draw_arcade_popup_overlay(GameState *game) {
    if (!game->arcade_popup_open) {
        return;
    }

    ArcadeRenderAssets render_assets = {
        .tic_texture = &game->tex_tic,
        .x_texture = &game->tex_x_mark,
        .maze_wall_texture = &game->tex_maze_wall,
        .maze_finish_texture = &game->tex_maze_finish,
        .maze_swall_texture = &game->tex_maze_swall,
        .maze_fan_texture = &game->tex_maze_fan,
        .maze_car_texture = &game->tex_maze_car,
        .puzzle_background = &game->tex_puzzle_background,
        .puzzle_photos = game->tex_puzzle_photos,
        .puzzle_photo_count = game->puzzle_photo_count
    };

    const bool puzzle_fullscreen = game->arcade.screen == ARCADE_SCREEN_PUZZLE;
    SDL_Rect panel = puzzle_fullscreen ? (SDL_Rect){0, 0, ARCADE_PUZZLE_W, ARCADE_PUZZLE_H} : (SDL_Rect){
        .x = (int)lroundf(game->arcade_popup_rect.x),
        .y = (int)lroundf(game->arcade_popup_rect.y),
        .w = (int)lroundf(game->arcade_popup_rect.w),
        .h = (int)lroundf(game->arcade_popup_rect.h)
    };

    if (puzzle_fullscreen) {
        SDL_RenderSetLogicalSize(game->renderer, ARCADE_PUZZLE_W, ARCADE_PUZZLE_H);
        SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, 220);
        SDL_RenderFillRect(game->renderer, &(SDL_Rect){0, 0, ARCADE_PUZZLE_W, ARCADE_PUZZLE_H});
        SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
    } else if (game->tex_arcade_playing.loaded) {
        SDL_RenderCopy(game->renderer, game->tex_arcade_playing.texture, NULL, &panel);
    } else {
        SDL_SetRenderDrawColor(game->renderer, 34, 22, 48, 255);
        SDL_RenderFillRect(game->renderer, &panel);
        SDL_SetRenderDrawColor(game->renderer, 230, 198, 88, 255);
        SDL_RenderDrawRect(game->renderer, &panel);
    }

    arcade_render(
        &game->arcade,
        game->renderer,
        game->font,
        &panel,
        &render_assets
    );

    if (puzzle_fullscreen) {
        uint32_t fade_elapsed = SDL_GetTicks() - game->arcade_puzzle_fade_started_at;
        Uint8 alpha = 0;
        if (fade_elapsed < ARCADE_PUZZLE_FADE_MS) {
            alpha = (Uint8)(255u - (255u * fade_elapsed) / ARCADE_PUZZLE_FADE_MS);
        }
        if (alpha > 0) {
            SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, alpha);
            SDL_RenderFillRect(game->renderer, &(SDL_Rect){0, 0, ARCADE_PUZZLE_W, ARCADE_PUZZLE_H});
            SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
        }
        SDL_RenderSetLogicalSize(game->renderer, WINDOW_W, WINDOW_H);
    }

    if (!puzzle_fullscreen && game->tv_admin_open && game->tv_admin_target == TV_ADMIN_TARGET_ARCADE_POPUP) {
        SDL_SetRenderDrawColor(game->renderer, 255, 210, 90, 255);
        SDL_RenderDrawRect(game->renderer, &panel);
    }
}

static void draw_world_texture(
    GameState *game,
    const TextureAsset *texture,
    Rect world_rect,
    SDL_Color fallback_fill,
    SDL_Color fallback_stroke,
    bool draw_stroke
) {
    SDL_Rect dst = map_world_to_screen_rect(&game->map, world_rect);

    if (texture && texture->loaded) {
        SDL_RenderCopy(game->renderer, texture->texture, NULL, &dst);
        return;
    }

    SDL_SetRenderDrawColor(
        game->renderer,
        fallback_fill.r,
        fallback_fill.g,
        fallback_fill.b,
        fallback_fill.a
    );
    SDL_RenderFillRect(game->renderer, &dst);

    if (draw_stroke) {
        SDL_SetRenderDrawColor(
            game->renderer,
            fallback_stroke.r,
            fallback_stroke.g,
            fallback_stroke.b,
            fallback_stroke.a
        );
        SDL_RenderDrawRect(game->renderer, &dst);
    }
}

static void draw_animated_ad(GameState *game, uint32_t now_ms) {
    const TextureAsset *frame = NULL;
    if (game->ad_frame_count > 0) {
        const int frame_index = (int)((now_ms / AD_FRAME_DURATION_MS) % (uint32_t)game->ad_frame_count);
        frame = &game->tex_ad_frames[frame_index];
    }

    draw_world_texture(
        game,
        frame,
        game->ad_rect,
        (SDL_Color){22, 22, 22, 255},
        (SDL_Color){244, 185, 72, 255},
        true
    );
}

static void draw_player_with_assets(
    GameState *game,
    const Player *player,
    const PlayerAssets *assets,
    SDL_Color fallback_color
);

static void draw_single_npc(GameState *game, int npc_index, uint32_t now_ms) {
    if (npc_index < 0 || npc_index >= game->npc_config.count) {
        return;
    }

    const AdminNpcEntry *npc = &game->npc_config.entries[npc_index];
    const int skin_index = normalize_npc_skin_index(game, npc->skin_index);
    const TextureAsset *skin = (skin_index >= 0 && skin_index < game->npc_skin_count)
        ? &game->tex_npc_skins[skin_index]
        : NULL;
    SDL_Rect dst = map_world_to_screen_rect(&game->map, npc->rect);
    bool rendered = false;

    if (skin && skin->loaded) {
        const int frame_w = skin->w / NPC_SKIN_FRAME_COLS;
        const int frame_h = skin->h / NPC_SKIN_FRAME_ROWS;
        if (frame_w > 0 && frame_h > 0) {
            const int frame_index = (int)((now_ms / NPC_SKIN_ANIM_FRAME_MS) % NPC_SKIN_FRAME_COLS);
            SDL_Rect src = {
                .x = frame_index * frame_w,
                .y = 0,
                .w = frame_w,
                .h = frame_h
            };
            SDL_RenderCopyEx(
                game->renderer,
                skin->texture,
                &src,
                &dst,
                0.0,
                NULL,
                npc->flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE
            );
            rendered = true;
        }
    }

    if (!rendered) {
        SDL_SetRenderDrawColor(game->renderer, 190, 170, 140, 255);
        SDL_RenderFillRect(game->renderer, &dst);
        SDL_SetRenderDrawColor(game->renderer, 86, 68, 52, 255);
        SDL_RenderDrawRect(game->renderer, &dst);
    }

    if (
        game->tv_admin_open &&
        game->tv_admin_target == TV_ADMIN_TARGET_NPC &&
        npc_index == clampi(game->tv_admin_npc_index, 0, game->npc_config.count - 1)
    ) {
        SDL_SetRenderDrawColor(game->renderer, 255, 210, 90, 255);
        SDL_RenderDrawRect(game->renderer, &dst);
    }
}

typedef enum {
    ACTOR_LAYER_PLAYER_1 = 0,
    ACTOR_LAYER_PLAYER_2,
    ACTOR_LAYER_NPC
} ActorLayerType;

typedef struct {
    int z_index;
    float sort_y;
    ActorLayerType type;
    int npc_index;
} ActorLayerEntry;

static int compare_actor_layer_entry(const ActorLayerEntry *a, const ActorLayerEntry *b) {
    if (a->z_index < b->z_index) {
        return -1;
    }
    if (a->z_index > b->z_index) {
        return 1;
    }
    if (a->sort_y < b->sort_y) {
        return -1;
    }
    if (a->sort_y > b->sort_y) {
        return 1;
    }
    if (a->type < b->type) {
        return -1;
    }
    if (a->type > b->type) {
        return 1;
    }
    if (a->npc_index < b->npc_index) {
        return -1;
    }
    if (a->npc_index > b->npc_index) {
        return 1;
    }
    return 0;
}

static void sort_actor_layers(ActorLayerEntry *entries, int count) {
    for (int i = 1; i < count; ++i) {
        ActorLayerEntry key = entries[i];
        int j = i - 1;
        while (j >= 0 && compare_actor_layer_entry(&key, &entries[j]) < 0) {
            entries[j + 1] = entries[j];
            --j;
        }
        entries[j + 1] = key;
    }
}

static void draw_layered_actors(GameState *game, uint32_t now_ms) {
    ActorLayerEntry entries[ADMIN_MAX_NPCS + 2];
    int count = 0;

    entries[count++] = (ActorLayerEntry){
        .z_index = 0,
        .sort_y = game->player.y + game->player.h,
        .type = ACTOR_LAYER_PLAYER_1,
        .npc_index = -1
    };

    if (game->duo_enabled) {
        entries[count++] = (ActorLayerEntry){
            .z_index = 0,
            .sort_y = game->player2.y + game->player2.h,
            .type = ACTOR_LAYER_PLAYER_2,
            .npc_index = -1
        };
    }

    for (int i = 0; i < game->npc_config.count; ++i) {
        const AdminNpcEntry *npc = &game->npc_config.entries[i];
        entries[count++] = (ActorLayerEntry){
            .z_index = npc->z_index,
            .sort_y = npc->rect.y + npc->rect.h,
            .type = ACTOR_LAYER_NPC,
            .npc_index = i
        };
    }

    sort_actor_layers(entries, count);

    for (int i = 0; i < count; ++i) {
        if (entries[i].type == ACTOR_LAYER_NPC) {
            draw_single_npc(game, entries[i].npc_index, now_ms);
        } else if (entries[i].type == ACTOR_LAYER_PLAYER_2) {
            draw_player_with_assets(
                game,
                &game->player2,
                &game->player2_assets,
                (SDL_Color){0x6f, 0xc0, 0xff, 255}
            );
        } else {
            draw_player_with_assets(
                game,
                &game->player,
                &game->player_assets,
                (SDL_Color){0xe8, 0xc5, 0x47, 255}
            );
        }
    }
}

static void draw_top_wall_covers(GameState *game) {
    const float front_cover_y = (game->map.top_wall_cover_count > 0)
        ? game->map.top_wall_covers[0].y
        : 0.0f;
    for (int i = 0; i < game->map.top_wall_cover_count; ++i) {
        const Rect cover = game->map.top_wall_covers[i];
        const float y_offset = (fabsf(cover.y - front_cover_y) < 0.0001f)
            ? 0.0f
            : -WALL_COVER_HEIGHT;
        const Rect world_rect = {
            .x = cover.x,
            .y = cover.y + y_offset,
            .w = cover.w,
            .h = WALL_COVER_HEIGHT
        };

        draw_world_texture(
            game,
            &game->tex_wall,
            world_rect,
            (SDL_Color){214, 216, 220, 255},
            (SDL_Color){106, 107, 114, 255},
            false
        );
    }
}

static void draw_barista(GameState *game) {
    SDL_Rect dst = map_world_to_screen_rect(&game->map, game->map.barista_rect);

    if (game->tex_barista.loaded) {
        const int cols = 6;
        const int rows = 6;
        const int frame_w = game->tex_barista.w / cols;
        const int frame_h = game->tex_barista.h / rows;

        SDL_Rect src = {
            .x = (game->barista.frame % cols) * frame_w,
            .y = (game->barista.frame / cols) * frame_h,
            .w = frame_w,
            .h = frame_h
        };

        SDL_RenderCopy(game->renderer, game->tex_barista.texture, &src, &dst);
        return;
    }

    SDL_SetRenderDrawColor(game->renderer, 246, 209, 174, 255);
    SDL_Rect body = {dst.x + 8, dst.y, dst.w - 16, dst.h / 2};
    SDL_RenderFillRect(game->renderer, &body);

    SDL_SetRenderDrawColor(game->renderer, 26, 26, 26, 255);
    SDL_Rect outfit = {dst.x + 6, dst.y + dst.h / 3, dst.w - 12, dst.h - dst.h / 3};
    SDL_RenderFillRect(game->renderer, &outfit);
}

static void draw_player_with_assets(
    GameState *game,
    const Player *player,
    const PlayerAssets *assets,
    SDL_Color fallback_color
) {
    if (player->active_sheet && player->active_sheet->base.loaded) {
        const SpriteSheet *sheet = player->active_sheet;

        SDL_Rect src = {
            .x = (player->frame % sheet->cols) * sheet->frame_w,
            .y = (player->frame / sheet->cols) * sheet->frame_h,
            .w = sheet->frame_w,
            .h = sheet->frame_h
        };

        const float draw_h = PLAYER_SPRITE_DRAW_HEIGHT;
        const float draw_w = draw_h * ((float)sheet->frame_w / (float)sheet->frame_h);
        const float draw_x = game->map.offset_x + player->x + ((float)player->w - draw_w) * 0.5f;
        const float draw_y = game->map.offset_y + player->y + player->h - draw_h;

        SDL_Rect dst = {
            .x = (int)lroundf(draw_x),
            .y = (int)lroundf(draw_y),
            .w = (int)lroundf(draw_w),
            .h = (int)lroundf(draw_h)
        };

        const bool flip = (player->facing == FACING_RIGHT) &&
            (sheet == &assets->walk_left || sheet == &assets->idle_side);

        SDL_RenderCopyEx(
            game->renderer,
            sheet->base.texture,
            &src,
            &dst,
            0.0,
            NULL,
            flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE
        );
        return;
    }

    SDL_Rect dst = {
        .x = (int)lroundf(game->map.offset_x + player->x),
        .y = (int)lroundf(game->map.offset_y + player->y),
        .w = player->w,
        .h = player->h
    };

    SDL_SetRenderDrawColor(
        game->renderer,
        fallback_color.r,
        fallback_color.g,
        fallback_color.b,
        fallback_color.a
    );
    SDL_RenderFillRect(game->renderer, &dst);
    SDL_SetRenderDrawColor(game->renderer, 0x33, 0x33, 0x33, 255);
    SDL_RenderDrawRect(game->renderer, &dst);
}

static InteractBind normalize_interact_bind(InteractBind bind, InteractBind fallback) {
    if (bind == INTERACT_BIND_0 || bind == INTERACT_BIND_F || bind == INTERACT_BIND_E) {
        return bind;
    }
    return fallback;
}

static const char *interaction_key_label_for_bind(InteractBind bind) {
    const char *label = session_interact_bind_label(bind);
    return label ? label : "?";
}

static const char *interaction_key_label(const GameState *game) {
    static char label[16];
    const char *p1 = interaction_key_label_for_bind(game->player_interact_bind[0]);
    const char *p2 = interaction_key_label_for_bind(game->player_interact_bind[1]);

    if (!game->duo_enabled) {
        SDL_snprintf(label, sizeof(label), "[%s]", p1);
        return label;
    }
    if (SDL_strcmp(p1, p2) == 0) {
        SDL_snprintf(label, sizeof(label), "[%s]", p1);
        return label;
    }
    SDL_snprintf(label, sizeof(label), "[%s/%s]", p1, p2);
    return label;
}

static void draw_interaction_prompt(GameState *game) {
    if (billiards_visible(&game->billiards) || game->shop_open || game->arcade_popup_open) {
        return;
    }

    const InteractionTarget target_p1 = map_get_interaction_target(
        &game->map,
        player_rect(&game->player),
        false
    );
    const InteractionTarget target_p2 = game->duo_enabled
        ? map_get_interaction_target(&game->map, player_rect(&game->player2), false)
        : TARGET_NONE;
    const InteractionTarget target = (target_p1 != TARGET_NONE) ? target_p1 : target_p2;
    const Player *prompt_player = (target_p1 != TARGET_NONE) ? &game->player : &game->player2;

    if (target == TARGET_NONE) {
        return;
    }

    char prompt_text[64];
    prompt_text[0] = '\0';
    const char *key_label = interaction_key_label(game);
    if (target == TARGET_BARISTA) {
        SDL_snprintf(prompt_text, sizeof(prompt_text), "%s Open Shop", key_label);
    } else if (target == TARGET_BILLIARDS) {
        SDL_snprintf(
            prompt_text,
            sizeof(prompt_text),
            "%s Billiards%s",
            key_label,
            game->billiards_used_this_visit ? " (used)" : ""
        );
    } else if (target == TARGET_ARCADE) {
        SDL_snprintf(prompt_text, sizeof(prompt_text), "%s Play Arcade", key_label);
    } else if (target == TARGET_EXIT_DOOR) {
        SDL_snprintf(prompt_text, sizeof(prompt_text), "%s Exit", key_label);
    }

    if (!prompt_text[0]) {
        return;
    }

    const int prompt_x = (int)fmaxf(8.0f, game->map.offset_x + prompt_player->x - 2.0f);
    const int prompt_y = (int)fmaxf(18.0f, game->map.offset_y + prompt_player->y - 10.0f);

    draw_text(
        game->renderer,
        game->font,
        prompt_text,
        prompt_x,
        prompt_y,
        (SDL_Color){243, 236, 210, 255}
    );
}

static void draw_world(GameState *game, uint32_t now_ms) {
    SDL_SetRenderDrawColor(game->renderer, 0x1a, 0x1a, 0x2e, 255);
    SDL_RenderClear(game->renderer);

    map_draw_floor(game->renderer, &game->map);

    draw_top_wall_covers(game);

    draw_world_texture(
        game,
        &game->tex_door,
        game->map.door,
        (SDL_Color){239, 239, 239, 255},
        (SDL_Color){154, 154, 154, 255},
        true
    );

    draw_world_texture(
        game,
        &game->tex_window,
        game->map.wall_window,
        (SDL_Color){231, 239, 247, 255},
        (SDL_Color){90, 104, 117, 255},
        true
    );

    draw_world_texture(
        game,
        &game->tex_tv,
        game->tv_rect,
        (SDL_Color){40, 40, 48, 255},
        (SDL_Color){200, 200, 200, 255},
        true
    );
    draw_world_texture(
        game,
        &game->tex_arcade,
        game->map.arcade_rect,
        (SDL_Color){73, 46, 96, 255},
        (SDL_Color){223, 190, 82, 255},
        true
    );
    draw_animated_ad(game, now_ms);

    draw_barista(game);

    draw_world_texture(
        game,
        &game->tex_billiards_table,
        game->map.billiards_table,
        (SDL_Color){95, 64, 38, 255},
        (SDL_Color){46, 125, 50, 255},
        false
    );

    draw_world_texture(
        game,
        &game->tex_food_buffet,
        game->map.food_buffet,
        (SDL_Color){194, 198, 204, 255},
        (SDL_Color){143, 149, 157, 255},
        false
    );

    draw_world_texture(
        game,
        &game->tex_food_buffet2,
        game->map.food_buffet2,
        (SDL_Color){216, 221, 228, 255},
        (SDL_Color){163, 173, 184, 255},
        false
    );

    for (int i = 0; i < game->map.decor_table_count; ++i) {
        draw_world_texture(
            game,
            &game->tex_table,
            game->map.decor_tables[i],
            (SDL_Color){215, 199, 178, 255},
            (SDL_Color){125, 94, 60, 255},
            true
        );
    }

    map_draw_wall_segments(game->renderer, &game->map);
    draw_layered_actors(game, now_ms);
    draw_interaction_prompt(game);
    draw_tv_admin_overlay(game);
    draw_barista_shop_overlay(game);
    draw_arcade_popup_overlay(game);

    billiards_render(
        &game->billiards,
        &game->map,
        game->renderer,
        game->font,
        &game->tex_billiards_popup,
        &game->tex_x_button,
        &game->tex_baton,
        &game->tex_white_ball,
        &game->tex_eight_ball,
        &game->tex_ball1,
        &game->tex_billiards_correct,
        now_ms
    );

    if (!(game->arcade_popup_open && game->arcade.screen == ARCADE_SCREEN_PUZZLE)) {
        draw_shop_hud(game, now_ms);
    }

    if (game->scene_fade_alpha > 0) {
        SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(game->renderer, 0, 0, 0, game->scene_fade_alpha);
        SDL_Rect full = {0, 0, WINDOW_W, WINDOW_H};
        SDL_RenderFillRect(game->renderer, &full);
        SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_NONE);
    }

    if (game->online_hosted) {
        online_client_submit_frame(game->renderer, 1);
    }

    SDL_RenderPresent(game->renderer);
}

static bool load_assets(GameState *game) {
    static const char *const shop_frame_paths[] = {
        "assets/static/ui_frame.png",
        "../level1-climb/assets/static/ui_frame.png",
        "../../level1-climb/assets/static/ui_frame.png"
    };
    static const char *const shop_item_1_paths[] = {
        "assets/static/item_jetpack.png",
        "../level1-climb/assets/static/item_jetpack.png",
        "../../level1-climb/assets/static/item_jetpack.png"
    };
    static const char *const shop_item_2_paths[] = {
        "assets/static/item_magnet.png",
        "../level1-climb/assets/static/item_magnet.png",
        "../../level1-climb/assets/static/item_magnet.png"
    };
    static const char *const shop_item_3_paths[] = {
        "assets/static/item_shoes.png",
        "../level1-climb/assets/static/item_shoes.png",
        "../../level1-climb/assets/static/item_shoes.png"
    };

    game->tex_billiards_table = load_texture(game->renderer, "assets/billiards.png");
    game->tex_billiards_popup = load_texture(game->renderer, "assets/billiards tabel mini game.png");
    game->tex_billiards_correct = load_texture(game->renderer, "assets/billiards-quiz-correct-visual.png");
    game->tex_food_buffet = load_texture(game->renderer, "assets/foor buffet.png");
    game->tex_food_buffet2 = load_texture(game->renderer, "assets/food buffet2.png");
    game->tex_wall = load_texture(game->renderer, "assets/wall.png");
    game->tex_door = load_texture(game->renderer, "assets/door.png");
    game->tex_window = load_texture(game->renderer, "assets/window.png");
    game->tex_tv = load_texture(game->renderer, "assets/tv.png");
    game->tex_arcade = load_texture(game->renderer, "assets/arcade.png");
    game->tex_arcade_playing = load_texture(game->renderer, "assets/arcadeplaying.png");
    if (game->tex_arcade_playing.texture) {
        SDL_SetTextureBlendMode(game->tex_arcade_playing.texture, SDL_BLENDMODE_BLEND);
    }
    load_animated_ad_frames(game);
    game->tex_x_button = load_texture(game->renderer, "assets/x button.png");
    game->tex_table = load_texture(game->renderer, "assets/tabel.png");
    game->tex_barista = load_texture(game->renderer, "assets/idel-shop-barista-animation.png");
    game->tex_shop_frame = load_texture_first_available(
        game->renderer,
        shop_frame_paths,
        (int)(sizeof(shop_frame_paths) / sizeof(shop_frame_paths[0]))
    );
    game->tex_shop_item_1 = load_texture_first_available(
        game->renderer,
        shop_item_1_paths,
        (int)(sizeof(shop_item_1_paths) / sizeof(shop_item_1_paths[0]))
    );
    game->tex_shop_item_2 = load_texture_first_available(
        game->renderer,
        shop_item_2_paths,
        (int)(sizeof(shop_item_2_paths) / sizeof(shop_item_2_paths[0]))
    );
    game->tex_shop_item_3 = load_texture_first_available(
        game->renderer,
        shop_item_3_paths,
        (int)(sizeof(shop_item_3_paths) / sizeof(shop_item_3_paths[0]))
    );
    game->tex_tic = load_texture(game->renderer, "arcade mini games/tic.png");
    game->tex_x_mark = load_texture(game->renderer, "arcade mini games/x.png");
    game->tex_maze_wall = load_texture(game->renderer, "arcade mini games/maze/walls.png");
    game->tex_maze_finish = load_texture(game->renderer, "arcade mini games/maze/finall.png");
    game->tex_maze_swall = load_texture(game->renderer, "arcade mini games/maze/swall.png");
    game->tex_maze_fan = load_texture(game->renderer, "arcade mini games/maze/fan.png");
    game->tex_maze_car = load_texture(game->renderer, "arcade mini games/maze/car.png");
    game->tex_puzzle_background = load_texture(game->renderer, "arcade mini games/puzzle/gamepzl/background-image/homealonebackground.png");
    if (game->tex_maze_fan.texture) {
        SDL_SetTextureBlendMode(game->tex_maze_fan.texture, SDL_BLENDMODE_BLEND);
    }
    if (game->tex_maze_car.texture) {
        SDL_SetTextureBlendMode(game->tex_maze_car.texture, SDL_BLENDMODE_BLEND);
    }
    load_puzzle_photo_textures(game);
    game->npc_skin_count = NPC_SKIN_COUNT;
    for (int i = 0; i < game->npc_skin_count; ++i) {
        game->tex_npc_skins[i] = load_texture(game->renderer, NPC_SKINS[i].path);
    }

    load_player_assets_for_skin(game, game->player_skin_number[0], &game->player_assets);
    load_player_assets_for_skin(game, game->player_skin_number[1], &game->player2_assets);

    game->tex_baton = load_texture(game->renderer, "assets/minigame/baton.png");
    game->tex_white_ball = load_texture(game->renderer, "assets/minigame/whiteball.png");
    game->tex_eight_ball = load_texture(game->renderer, "assets/minigame/ball8-removebg-preview (1).png");
    game->tex_ball1 = load_texture(game->renderer, "assets/minigame/ball1-removebg-preview.png");

    return true;
}

static void unload_assets(GameState *game) {
    unload_texture(&game->tex_billiards_table);
    unload_texture(&game->tex_billiards_popup);
    unload_texture(&game->tex_billiards_correct);
    unload_texture(&game->tex_food_buffet);
    unload_texture(&game->tex_food_buffet2);
    unload_texture(&game->tex_wall);
    unload_texture(&game->tex_door);
    unload_texture(&game->tex_window);
    unload_texture(&game->tex_tv);
    unload_texture(&game->tex_arcade);
    unload_texture(&game->tex_arcade_playing);
    for (int i = 0; i < game->ad_frame_count; ++i) {
        unload_texture(&game->tex_ad_frames[i]);
    }
    game->ad_frame_count = 0;
    unload_texture(&game->tex_x_button);
    unload_texture(&game->tex_table);
    unload_texture(&game->tex_barista);
    unload_texture(&game->tex_shop_frame);
    unload_texture(&game->tex_shop_item_1);
    unload_texture(&game->tex_shop_item_2);
    unload_texture(&game->tex_shop_item_3);
    unload_texture(&game->tex_tic);
    unload_texture(&game->tex_x_mark);
    unload_texture(&game->tex_maze_wall);
    unload_texture(&game->tex_maze_finish);
    unload_texture(&game->tex_maze_swall);
    unload_texture(&game->tex_maze_fan);
    unload_texture(&game->tex_maze_car);
    unload_texture(&game->tex_puzzle_background);
    for (int i = 0; i < game->puzzle_photo_count; ++i) {
        unload_texture(&game->tex_puzzle_photos[i]);
    }
    game->puzzle_photo_count = 0;
    arcade_set_puzzle_photo_count(&game->arcade, 0);
    for (int i = 0; i < game->npc_skin_count; ++i) {
        unload_texture(&game->tex_npc_skins[i]);
    }
    game->npc_skin_count = 0;

    unload_texture(&game->player_assets.idle_down.base);
    unload_texture(&game->player_assets.idle_up.base);
    unload_texture(&game->player_assets.idle_side.base);
    unload_texture(&game->player_assets.walk_down.base);
    unload_texture(&game->player_assets.walk_up.base);
    unload_texture(&game->player_assets.walk_left.base);
    unload_texture(&game->player2_assets.idle_down.base);
    unload_texture(&game->player2_assets.idle_up.base);
    unload_texture(&game->player2_assets.idle_side.base);
    unload_texture(&game->player2_assets.walk_down.base);
    unload_texture(&game->player2_assets.walk_up.base);
    unload_texture(&game->player2_assets.walk_left.base);

    unload_texture(&game->tex_baton);
    unload_texture(&game->tex_white_ball);
    unload_texture(&game->tex_eight_ball);
    unload_texture(&game->tex_ball1);
}

static TTF_Font *load_font(void) {
    const char *paths[] = {
        "arcade mini games/puzzle/gamepzl/font/ka1.ttf",
        "../newshoplvl1/arcade mini games/puzzle/gamepzl/font/ka1.ttf",
        "../../lvls/newshoplvl1/arcade mini games/puzzle/gamepzl/font/ka1.ttf",
        "lvls/newshoplvl1/arcade mini games/puzzle/gamepzl/font/ka1.ttf",
        "assets/main_menu/fonts/pixel_operator.ttf",
        "../../assets/main_menu/fonts/pixel_operator.ttf",
        "assets/choose_menu/fonts/default.ttf",
        "../../assets/choose_menu/fonts/default.ttf",
        "assets/load_scene/fonts/text_load.ttf",
        "../../assets/load_scene/fonts/text_load.ttf",
        "assets/enigme/assets/font.ttf",
        "../../assets/enigme/assets/font.ttf",
        "assets/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"
    };

    const int count = (int)(sizeof(paths) / sizeof(paths[0]));
    for (int i = 0; i < count; ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], 18);
        if (font) {
            return font;
        }
    }

    SDL_Log("No TTF font found; text rendering will be disabled.");
    return NULL;
}

static bool input_control_down(
    const GameState *game,
    SDL_Scancode wasd_key,
    SDL_Scancode arrow_key
) {
    if (game->duo_enabled) {
        return input_down(&game->input, wasd_key) || input_down(&game->input, arrow_key);
    }
    if (game->control_scheme == NEWSHOP_CONTROL_SCHEME_WASD) {
        return input_down(&game->input, wasd_key);
    }
    if (game->control_scheme == NEWSHOP_CONTROL_SCHEME_ARROWS) {
        return input_down(&game->input, arrow_key);
    }
    return input_down(&game->input, wasd_key) || input_down(&game->input, arrow_key);
}

static bool input_control_pressed(
    const GameState *game,
    SDL_Scancode wasd_key,
    SDL_Scancode arrow_key
) {
    if (game->duo_enabled) {
        return input_pressed(&game->input, wasd_key) || input_pressed(&game->input, arrow_key);
    }
    if (game->control_scheme == NEWSHOP_CONTROL_SCHEME_WASD) {
        return input_pressed(&game->input, wasd_key);
    }
    if (game->control_scheme == NEWSHOP_CONTROL_SCHEME_ARROWS) {
        return input_pressed(&game->input, arrow_key);
    }
    return input_pressed(&game->input, wasd_key) || input_pressed(&game->input, arrow_key);
}

static bool input_interact_pressed(const GameState *game) {
    switch (game->player_interact_bind[0]) {
        case INTERACT_BIND_E:
            return input_pressed(&game->input, SDL_SCANCODE_E) ||
                game->input.controller_interact_pressed;
        case INTERACT_BIND_F:
            return input_pressed(&game->input, SDL_SCANCODE_F) ||
                game->input.controller_interact_pressed;
        case INTERACT_BIND_0:
            return input_pressed(&game->input, SDL_SCANCODE_0) ||
                input_pressed(&game->input, SDL_SCANCODE_KP_0) ||
                game->input.controller_interact_pressed;
        default:
            return input_pressed(&game->input, SDL_SCANCODE_F) ||
                game->input.controller_interact_pressed;
    }
}

static int input_move_horizontal(const GameState *game, int player_index) {
    if (game->duo_enabled) {
        if (player_index == 0) {
            return (input_down(&game->input, SDL_SCANCODE_RIGHT) ? 1 : 0) -
                (input_down(&game->input, SDL_SCANCODE_LEFT) ? 1 : 0);
        }
        return (input_down(&game->input, SDL_SCANCODE_D) ? 1 : 0) -
            (input_down(&game->input, SDL_SCANCODE_A) ? 1 : 0);
    }
    return (input_control_down(game, SDL_SCANCODE_D, SDL_SCANCODE_RIGHT) ? 1 : 0) -
        (input_control_down(game, SDL_SCANCODE_A, SDL_SCANCODE_LEFT) ? 1 : 0);
}

static int input_move_vertical(const GameState *game, int player_index) {
    if (game->duo_enabled) {
        if (player_index == 0) {
            return (input_down(&game->input, SDL_SCANCODE_DOWN) ? 1 : 0) -
                (input_down(&game->input, SDL_SCANCODE_UP) ? 1 : 0);
        }
        return (input_down(&game->input, SDL_SCANCODE_S) ? 1 : 0) -
            (input_down(&game->input, SDL_SCANCODE_W) ? 1 : 0);
    }
    return (input_control_down(game, SDL_SCANCODE_S, SDL_SCANCODE_DOWN) ? 1 : 0) -
        (input_control_down(game, SDL_SCANCODE_W, SDL_SCANCODE_UP) ? 1 : 0);
}

static bool input_interact_pressed_for_player(const GameState *game, int player_index) {
    if (!game->duo_enabled) {
        (void)player_index;
        return input_interact_pressed(game);
    }
    if (game->online_hosted) {
        const InteractBind bind = game->player_interact_bind[clamp_player_index(player_index)];
        bool pressed = false;
        switch (bind) {
            case INTERACT_BIND_E:
                pressed = (player_index == 1)
                    ? input_pressed_from_remote(&game->input, SDL_SCANCODE_E)
                    : input_pressed_without_remote(&game->input, SDL_SCANCODE_E);
                break;
            case INTERACT_BIND_F:
                pressed = (player_index == 1)
                    ? input_pressed_from_remote(&game->input, SDL_SCANCODE_F)
                    : input_pressed_without_remote(&game->input, SDL_SCANCODE_F);
                break;
            case INTERACT_BIND_0:
                pressed = (player_index == 1)
                    ? (input_pressed_from_remote(&game->input, SDL_SCANCODE_0) ||
                       input_pressed_from_remote(&game->input, SDL_SCANCODE_KP_0))
                    : (input_pressed_without_remote(&game->input, SDL_SCANCODE_0) ||
                       input_pressed_without_remote(&game->input, SDL_SCANCODE_KP_0));
                break;
            default:
                pressed = (player_index == 1)
                    ? input_pressed_from_remote(&game->input, SDL_SCANCODE_F)
                    : input_pressed_without_remote(&game->input, SDL_SCANCODE_F);
                break;
        }
        if (player_index == 0) {
            pressed = pressed || game->input.controller_interact_pressed;
        }
        return pressed;
    }
    if (player_index == 0) {
        switch (game->player_interact_bind[0]) {
            case INTERACT_BIND_E:
                return input_pressed(&game->input, SDL_SCANCODE_E) ||
                    game->input.controller_interact_pressed;
            case INTERACT_BIND_F:
                return input_pressed(&game->input, SDL_SCANCODE_F) ||
                    game->input.controller_interact_pressed;
            case INTERACT_BIND_0:
                return input_pressed(&game->input, SDL_SCANCODE_0) ||
                    input_pressed(&game->input, SDL_SCANCODE_KP_0) ||
                    game->input.controller_interact_pressed;
            default:
                return input_pressed(&game->input, SDL_SCANCODE_0) ||
                    input_pressed(&game->input, SDL_SCANCODE_KP_0) ||
                    game->input.controller_interact_pressed;
        }
    }
    switch (game->player_interact_bind[1]) {
        case INTERACT_BIND_E:
            return input_pressed(&game->input, SDL_SCANCODE_E);
        case INTERACT_BIND_F:
            return input_pressed(&game->input, SDL_SCANCODE_F);
        case INTERACT_BIND_0:
            return input_pressed(&game->input, SDL_SCANCODE_0) ||
                input_pressed(&game->input, SDL_SCANCODE_KP_0);
        default:
            return input_pressed(&game->input, SDL_SCANCODE_F);
    }
}

static bool input_active_shop_player_confirm_pressed(const GameState *game) {
    if (
        input_pressed(&game->input, SDL_SCANCODE_RETURN) ||
        input_pressed(&game->input, SDL_SCANCODE_KP_ENTER)
    ) {
        return true;
    }

    if (!game->duo_enabled) {
        return input_interact_pressed(game);
    }

    return input_interact_pressed_for_player(game, game->active_shop_player);
}

static void resolve_player_npc_collisions(GameState *game, Player *player, float previous_x, float previous_y) {
    if (game->npc_config.count <= 0) {
        return;
    }

    Rect player_hitbox = player_rect(player);
    const bool moved_x = fabsf(player_hitbox.x - previous_x) > 0.001f;
    const bool moved_y = fabsf(player_hitbox.y - previous_y) > 0.001f;

    for (int pass = 0; pass < 2; ++pass) {
        bool changed = false;

        for (int i = 0; i < game->npc_config.count; ++i) {
            const Rect npc = game->npc_config.entries[i].rect;
            if (!rect_overlaps(player_hitbox, npc)) {
                continue;
            }

            if (moved_x && !moved_y) {
                if (player_hitbox.x >= previous_x) {
                    player_hitbox.x = npc.x - player_hitbox.w;
                } else {
                    player_hitbox.x = npc.x + npc.w;
                }
            } else if (moved_y && !moved_x) {
                if (player_hitbox.y >= previous_y) {
                    player_hitbox.y = npc.y - player_hitbox.h;
                } else {
                    player_hitbox.y = npc.y + npc.h;
                }
            } else {
                const float overlap_left = (player_hitbox.x + player_hitbox.w) - npc.x;
                const float overlap_right = (npc.x + npc.w) - player_hitbox.x;
                const float overlap_top = (player_hitbox.y + player_hitbox.h) - npc.y;
                const float overlap_bottom = (npc.y + npc.h) - player_hitbox.y;
                const float resolve_x = fminf(overlap_left, overlap_right);
                const float resolve_y = fminf(overlap_top, overlap_bottom);

                if (resolve_x < resolve_y) {
                    if ((player_hitbox.x + player_hitbox.w * 0.5f) < (npc.x + npc.w * 0.5f)) {
                        player_hitbox.x = npc.x - player_hitbox.w;
                    } else {
                        player_hitbox.x = npc.x + npc.w;
                    }
                } else {
                    if ((player_hitbox.y + player_hitbox.h * 0.5f) < (npc.y + npc.h * 0.5f)) {
                        player_hitbox.y = npc.y - player_hitbox.h;
                    } else {
                        player_hitbox.y = npc.y + npc.h;
                    }
                }
            }

            changed = true;
        }

        if (!changed) {
            break;
        }
    }

    player_hitbox.x = clampf(player_hitbox.x, 0.0f, fmaxf(0.0f, (float)game->map.world_w - player_hitbox.w));
    player_hitbox.y = clampf(player_hitbox.y, 0.0f, fmaxf(0.0f, (float)game->map.world_h - player_hitbox.h));
    player->x = player_hitbox.x;
    player->y = player_hitbox.y;
}

static void update_player_movement(
    GameState *game,
    Player *player,
    int player_index,
    float dt_seconds
) {
    const int horizontal = input_move_horizontal(game, player_index);
    const int vertical = input_move_vertical(game, player_index);
    const float previous_x = player->x;
    const float previous_y = player->y;

    player->vx = horizontal * PLAYER_SPEED;
    player->vy = vertical * PLAYER_SPEED;
    player_update_facing(player, horizontal, vertical);

    map_move_player(
        &game->map,
        &player->x,
        &player->y,
        (float)player->w,
        (float)player->h,
        player->vx * dt_seconds,
        player->vy * dt_seconds
    );
    resolve_player_npc_collisions(game, player, previous_x, previous_y);

    player->is_walking = hypotf(player->x - previous_x, player->y - previous_y) > 0.2f;
}

static void update_walking_audio_state(GameState *game) {
    const bool walking = game->player.is_walking || (game->duo_enabled && game->player2.is_walking);
    const bool p1_skin2_sideways = (
        game->player_skin_number[0] == 2 &&
        game->player.is_walking &&
        (game->player.facing == FACING_LEFT || game->player.facing == FACING_RIGHT)
    );
    const bool p2_skin2_sideways = (
        game->duo_enabled &&
        game->player_skin_number[1] == 2 &&
        game->player2.is_walking &&
        (game->player2.facing == FACING_LEFT || game->player2.facing == FACING_RIGHT)
    );
    const bool skin2_sideways = p1_skin2_sideways || p2_skin2_sideways;

    audio_set_skin2_sideways(&game->audio, skin2_sideways);
    audio_set_walking(&game->audio, walking);
}

static void resolve_billiards_visit_result(GameState *game, uint32_t now_ms) {
    const bool visible = billiards_visible(&game->billiards);

    if (
        game->billiards_was_visible &&
        !visible &&
        game->billiards_used_this_visit &&
        !game->billiards_result_checked
    ) {
        if (game->billiards.session.is_complete && game->billiards.session.did_pass) {
            game->extra_hearts_held++;
            if (game->duo_enabled) {
                game->extra_hearts_held_p2++;
                snprintf(game->shop_msg, sizeof(game->shop_msg), "Billiards clear! +1 extra heart each.");
            } else {
                snprintf(game->shop_msg, sizeof(game->shop_msg), "Billiards clear! +1 extra heart.");
            }
            game->shop_msg_started_at = now_ms;
        } else if (game->billiards.session.is_complete) {
            snprintf(game->shop_msg, sizeof(game->shop_msg), "Billiards failed. No extra heart.");
            game->shop_msg_started_at = now_ms;
        }
        game->billiards_result_checked = true;
    }

    game->billiards_was_visible = visible;
}

static void update_game(GameState *game, float dt_seconds, uint32_t now_ms) {
    if (game->scene_fade_direction != 0) {
        const uint32_t elapsed = now_ms - game->scene_fade_started_at;
        const float progress = clampf((float)elapsed / (float)SHOP_SCENE_FADE_MS, 0.0f, 1.0f);

        if (game->scene_fade_direction > 0) {
            game->scene_fade_alpha = (uint8_t)lroundf(progress * 255.0f);
        } else {
            game->scene_fade_alpha = (uint8_t)lroundf((1.0f - progress) * 255.0f);
        }

        if (progress >= 1.0f) {
            game->scene_fade_alpha = (game->scene_fade_direction > 0) ? 255u : 0u;
            game->scene_fade_direction = 0;
            if (game->close_after_fade) {
                game->running = false;
                return;
            }
        }
    }

    if (game->input.quit_requested) {
        game->running = false;
        return;
    }

    if (input_pressed(&game->input, SDL_SCANCODE_F1)) {
        game->tv_admin_open = !game->tv_admin_open;
        if (game->tv_admin_open) {
            if (game->shop_open) {
                close_barista_shop(game);
            }
            if (billiards_visible(&game->billiards)) {
                billiards_close(&game->billiards, now_ms);
            }
            input_clear_movement_keys(&game->input);
            stop_player_motion(game);
        } else {
            save_admin_layouts(game);
        }
    }

    resolve_billiards_visit_result(game, now_ms);

    if (game->tv_admin_open) {
        if (input_pressed(&game->input, SDL_SCANCODE_ESCAPE)) {
            game->tv_admin_open = false;
            save_admin_layouts(game);
            return;
        }

        stop_player_motion(game);
        handle_tv_admin_input(game, now_ms);
        player_update_animation(&game->player, &game->player_assets, dt_seconds * 1000.0f);
        if (game->duo_enabled) {
            player_update_animation(&game->player2, &game->player2_assets, dt_seconds * 1000.0f);
        }
        return;
    }

    if (input_pressed(&game->input, SDL_SCANCODE_ESCAPE)) {
        if (game->shop_open) {
            close_barista_shop(game);
        } else if (game->arcade_popup_open) {
            if (!arcade_handle_escape(&game->arcade)) {
                arcade_close(&game->arcade);
                game->arcade_popup_open = false;
                game->arcade_puzzle_fullscreen = false;
            }
        } else if (billiards_visible(&game->billiards)) {
            billiards_close(&game->billiards, now_ms);
        } else {
            game->running = false;
        }
    }

    barista_update(&game->barista, now_ms);

    if (billiards_visible(&game->billiards)) {
        stop_player_motion(game);

        billiards_update(&game->billiards, &game->map, &game->input, now_ms);
        audio_update_billiards(&game->audio, &game->billiards);

        player_update_animation(&game->player, &game->player_assets, dt_seconds * 1000.0f);
        if (game->duo_enabled) {
            player_update_animation(&game->player2, &game->player2_assets, dt_seconds * 1000.0f);
        }
        return;
    }

    if (game->arcade_popup_open) {
        InputState arcade_input;
        const bool puzzle_was_fullscreen = game->arcade_puzzle_fullscreen;
        SDL_Rect arcade_panel = game->arcade.screen == ARCADE_SCREEN_PUZZLE ? (SDL_Rect){0, 0, ARCADE_PUZZLE_W, ARCADE_PUZZLE_H} : (SDL_Rect){
            .x = (int)lroundf(game->arcade_popup_rect.x),
            .y = (int)lroundf(game->arcade_popup_rect.y),
            .w = (int)lroundf(game->arcade_popup_rect.w),
            .h = (int)lroundf(game->arcade_popup_rect.h)
        };

        stop_player_motion(game);
        if (game->online_hosted) {
            input_copy_without_remote(&arcade_input, &game->input);
            arcade_update(&game->arcade, &arcade_input, now_ms, &arcade_panel);
        } else {
            arcade_update(&game->arcade, &game->input, now_ms, &arcade_panel);
        }
        game->arcade_puzzle_fullscreen = game->arcade.screen == ARCADE_SCREEN_PUZZLE;
        if (game->arcade_puzzle_fullscreen && !puzzle_was_fullscreen) {
            game->arcade_puzzle_fade_started_at = now_ms;
        }
        {
            const int reward = arcade_take_pending_key_reward(&game->arcade);
            if (reward > 0) {
                game->keys_held += reward;
                audio_play_key_pickup(&game->audio);
                SDL_snprintf(game->shop_msg, sizeof(game->shop_msg), "Arcade clear! +%d keys.", reward);
                game->shop_msg_started_at = now_ms;
            }
        }
        audio_set_tetris_music(
            &game->audio,
            game->arcade.active &&
            game->arcade.screen == ARCADE_SCREEN_TETRIS &&
            !game->arcade.tetris.paused &&
            !game->arcade.tetris.game_over
        );

        player_update_animation(&game->player, &game->player_assets, dt_seconds * 1000.0f);
        if (game->duo_enabled) {
            player_update_animation(&game->player2, &game->player2_assets, dt_seconds * 1000.0f);
        }
        return;
    }

    audio_set_tetris_music(&game->audio, false);

    if (game->shop_open) {
        stop_player_motion(game);

        if (input_control_pressed(game, SDL_SCANCODE_W, SDL_SCANCODE_UP)) {
            game->shop_selection = (game->shop_selection + SHOP_ITEM_COUNT - 1) % SHOP_ITEM_COUNT;
        }
        if (input_control_pressed(game, SDL_SCANCODE_S, SDL_SCANCODE_DOWN)) {
            game->shop_selection = (game->shop_selection + 1) % SHOP_ITEM_COUNT;
        }
        if (input_active_shop_player_confirm_pressed(game)) {
            perform_barista_purchase(game, now_ms);
        }

        player_update_animation(&game->player, &game->player_assets, dt_seconds * 1000.0f);
        if (game->duo_enabled) {
            player_update_animation(&game->player2, &game->player2_assets, dt_seconds * 1000.0f);
        }
        return;
    }

    update_player_movement(game, &game->player, 0, dt_seconds);
    if (game->duo_enabled) {
        update_player_movement(game, &game->player2, 1, dt_seconds);
    }
    update_walking_audio_state(game);

    {
        const bool p1_interact = input_interact_pressed_for_player(game, 0);
        const bool p2_interact = input_interact_pressed_for_player(game, 1);
        InteractionTarget target = TARGET_NONE;
        int interacting_player = -1;

        if (p1_interact) {
            target = map_get_interaction_target(&game->map, player_rect(&game->player), false);
            if (target != TARGET_NONE) {
                interacting_player = 0;
            }
        }
        if (target == TARGET_NONE && game->duo_enabled && p2_interact) {
            target = map_get_interaction_target(&game->map, player_rect(&game->player2), false);
            if (target != TARGET_NONE) {
                interacting_player = 1;
            }
        }

        if (target == TARGET_BILLIARDS) {
            if (game->billiards_used_this_visit) {
                snprintf(
                    game->shop_msg,
                    sizeof(game->shop_msg),
                    "Billiards can be used only once per shop visit."
                );
                game->shop_msg_started_at = now_ms;
            } else {
                billiards_open(&game->billiards, &game->map, now_ms);
                input_clear_movement_keys(&game->input);
                stop_player_motion(game);
                audio_reset_billiards(&game->audio);
                game->billiards_used_this_visit = true;
                game->billiards_result_checked = false;
                game->billiards_was_visible = true;
            }
        } else if (target == TARGET_BARISTA) {
            SDL_Log("barista interact");
            open_barista_shop(game, interacting_player);
        } else if (target == TARGET_ARCADE) {
            if (game->online_hosted && interacting_player == 1) {
                SDL_snprintf(
                    game->shop_msg,
                    sizeof(game->shop_msg),
                    "Only the host can use the arcade online."
                );
                game->shop_msg_started_at = now_ms;
            } else {
                SDL_Log("arcade interact: player used the arcade machine");
                game->arcade_popup_open = true;
                game->arcade_puzzle_fullscreen = false;
                game->arcade_puzzle_fade_started_at = now_ms;
                arcade_set_puzzle_photo_count(&game->arcade, game->puzzle_photo_count);
                arcade_open(&game->arcade, now_ms);
                input_clear_movement_keys(&game->input);
                stop_player_motion(game);
            }
        } else if (target == TARGET_EXIT_DOOR) {
            begin_scene_fade(game, now_ms, 1, true);
            input_clear_movement_keys(&game->input);
            stop_player_motion(game);
            return;
        }
    }

    billiards_update(&game->billiards, &game->map, &game->input, now_ms);
    audio_update_billiards(&game->audio, &game->billiards);

    player_update_animation(&game->player, &game->player_assets, dt_seconds * 1000.0f);
    if (game->duo_enabled) {
        player_update_animation(&game->player2, &game->player2_assets, dt_seconds * 1000.0f);
    }
}

static bool init_game(
    GameState *game,
    SDL_Window *shared_window,
    SDL_Renderer *shared_renderer,
    bool embedded_mode,
    const NewShopState *shop_state
) {
    memset(game, 0, sizeof(*game));

    if (embedded_mode) {
        game->window = shared_window;
        game->renderer = shared_renderer;
        if (!game->window || !game->renderer) {
            SDL_Log("Embedded newshoplvl1 requires a valid SDL window and renderer.");
            return false;
        }
    } else {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
            SDL_Log("IMG_Init failed: %s", IMG_GetError());
            return false;
        }

        if (TTF_Init() == -1) {
            SDL_Log("TTF_Init failed: %s", TTF_GetError());
        }

        game->window = SDL_CreateWindow(
            "Game Shop SDL2",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_W,
            WINDOW_H,
            0
        );
        if (!game->window) {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }

        game->renderer = SDL_CreateRenderer(
            game->window,
            -1,
            SDL_RENDERER_ACCELERATED
        );
        if (!game->renderer) {
            game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!game->renderer) {
            SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
            return false;
        }
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    game->font = load_font();

    if (!audio_init(&game->audio)) {
        SDL_Log("Audio init failed; continuing without sound.");
    }

    input_init(&game->input);
    billiards_init(&game->billiards);
    arcade_init(&game->arcade);
    barista_init(&game->barista);

    MapConfig config = map_default_config();
    admin_load_config(&config, "shop_config.dat");
    map_build(&game->map, &config);
    game->tv_rect = default_tv_rect(&game->map);
    admin_load_tv_rect(&game->tv_rect, TV_CONFIG_PATH);
    game->ad_rect = default_ad_rect(&game->map);
    admin_load_ad_rect(&game->ad_rect, AD_CONFIG_PATH);
    admin_load_arcade_rect(&game->map.arcade_rect, ARCADE_CONFIG_PATH);
    game->arcade_popup_rect = default_arcade_popup_rect();
    admin_load_arcade_popup_rect(&game->arcade_popup_rect, ARCADE_POPUP_CONFIG_PATH);
    clamp_tv_rect_to_world(game);
    clamp_ad_rect_to_world(game);
    clamp_arcade_rect_to_world(game);
    clamp_arcade_popup_rect_to_screen(game);
    game->npc_skin_count = NPC_SKIN_COUNT;
    memset(&game->npc_config, 0, sizeof(game->npc_config));
    admin_load_npc_config(&game->npc_config, NPC_CONFIG_PATH);
    game->tv_admin_npc_index = 0;
    sanitize_npc_config(game);
    game->tv_admin_open = false;
    game->tv_admin_field = TV_ADMIN_FIELD_X;
    game->tv_admin_target = TV_ADMIN_TARGET_TV;

    game->duo_enabled = shop_state ? (shop_state->duo_enabled != 0) : false;
    game->online_hosted = shop_state ? (shop_state->online_hosted != 0) : false;
    game->player_skin_number[0] = normalize_skin_number(
        shop_state ? shop_state->player_skin_number[0] : 1
    );
    game->player_skin_number[1] = normalize_skin_number(
        shop_state ? shop_state->player_skin_number[1] : 2
    );
    player_init(&game->player, game->map.spawn_x, game->map.spawn_y);
    {
        float spawn2_x = game->map.spawn_x;
        float spawn2_y = game->map.spawn_y;
        choose_duo_spawn(&game->map, game->map.spawn_x, game->map.spawn_y, &spawn2_x, &spawn2_y);
        player_init(&game->player2, spawn2_x, spawn2_y);
    }
    game->control_scheme = shop_state ? shop_state->control_scheme : 0;
    if (
        game->control_scheme != NEWSHOP_CONTROL_SCHEME_ARROWS &&
        game->control_scheme != NEWSHOP_CONTROL_SCHEME_WASD
    ) {
        game->control_scheme = 0;
    }
    game->player_interact_bind[0] = normalize_interact_bind(
        shop_state ? shop_state->player_interact_bind[0] : INTERACT_BIND_F,
        (game->control_scheme == NEWSHOP_CONTROL_SCHEME_WASD) ? INTERACT_BIND_F : INTERACT_BIND_0
    );
    game->player_interact_bind[1] = normalize_interact_bind(
        shop_state ? shop_state->player_interact_bind[1] : INTERACT_BIND_F,
        INTERACT_BIND_F
    );
    game->lives_held = shop_state ? shop_state->player_lives[0] : SHOP_DEFAULT_LIVES;
    if (shop_state && game->lives_held == 0 && shop_state->lives > 0) {
        game->lives_held = shop_state->lives;
    }
    if (game->lives_held < 0) {
        game->lives_held = 0;
    }
    game->lives_held_p2 = shop_state ? shop_state->player_lives[1] : game->lives_held;
    if (game->lives_held_p2 < 0) {
        game->lives_held_p2 = 0;
    }
    if (!game->duo_enabled) {
        game->lives_held_p2 = game->lives_held;
        game->player_interact_bind[1] = game->player_interact_bind[0];
    }

    game->extra_hearts_held = shop_state ? shop_state->player_extra_hearts[0] : 0;
    if (shop_state && game->extra_hearts_held == 0 && shop_state->extra_hearts > 0) {
        game->extra_hearts_held = shop_state->extra_hearts;
    }
    if (game->extra_hearts_held < 0) {
        game->extra_hearts_held = 0;
    }
    game->extra_hearts_held_p2 = shop_state ? shop_state->player_extra_hearts[1] : 0;
    if (game->extra_hearts_held_p2 < 0) {
        game->extra_hearts_held_p2 = 0;
    }
    if (!game->duo_enabled) {
        game->extra_hearts_held_p2 = game->extra_hearts_held;
    }
    game->keys_held = shop_state ? shop_state->keys_held : SHOP_START_KEYS;
    if (game->keys_held < 0) {
        game->keys_held = 0;
    }
    game->shop_selection = 0;
    game->shop_open = false;
    game->arcade_popup_open = false;
    game->arcade_puzzle_fullscreen = false;
    game->arcade_puzzle_fade_started_at = 0;
    game->active_shop_player = shop_state ? clamp_player_index(shop_state->active_player) : 0;
    game->player_buy_count_jetpack[0] = shop_state ? shop_state->player_buy_count_jetpack[0] : 0;
    game->player_buy_count_magnet[0] = shop_state ? shop_state->player_buy_count_magnet[0] : 0;
    game->player_buy_count_shoes[0] = shop_state ? shop_state->player_buy_count_shoes[0] : 0;
    game->player_buy_count_jetpack[1] = shop_state ? shop_state->player_buy_count_jetpack[1] : 0;
    game->player_buy_count_magnet[1] = shop_state ? shop_state->player_buy_count_magnet[1] : 0;
    game->player_buy_count_shoes[1] = shop_state ? shop_state->player_buy_count_shoes[1] : 0;
    if (shop_state &&
        game->player_buy_count_jetpack[0] == 0 &&
        game->player_buy_count_magnet[0] == 0 &&
        game->player_buy_count_shoes[0] == 0) {
        game->player_buy_count_jetpack[0] = shop_state->buy_count_jetpack;
        game->player_buy_count_magnet[0] = shop_state->buy_count_magnet;
        game->player_buy_count_shoes[0] = shop_state->buy_count_shoes;
    }
    for (int i = 0; i < 2; ++i) {
        if (game->player_buy_count_jetpack[i] < 0) game->player_buy_count_jetpack[i] = 0;
        if (game->player_buy_count_magnet[i] < 0) game->player_buy_count_magnet[i] = 0;
        if (game->player_buy_count_shoes[i] < 0) game->player_buy_count_shoes[i] = 0;
        game->player_purchased_jetpack[i] = 0;
        game->player_purchased_magnet[i] = 0;
        game->player_purchased_shoes[i] = 0;
    }
    sync_shop_counts_from_active_player(game);
    game->purchased_jetpack = 0;
    game->purchased_magnet = 0;
    game->purchased_shoes = 0;
    game->shop_msg[0] = '\0';
    game->shop_msg_started_at = 0;
    game->billiards_used_this_visit = false;
    game->billiards_result_checked = false;
    game->billiards_was_visible = false;
    game->scene_fade_started_at = SDL_GetTicks();
    game->scene_fade_alpha = 255u;
    game->scene_fade_direction = -1;
    game->close_after_fade = false;

    load_assets(game);

    srand((unsigned int)time(NULL));

    game->running = true;
    return true;
}

static void shutdown_game(GameState *game, bool embedded_mode) {
    if (game->map.config.width > 0 && game->map.config.height > 0) {
        admin_save_config(&game->map.config, "shop_config.dat");
        save_admin_layouts(game);
    }

    input_shutdown(&game->input);
    unload_assets(game);
    audio_shutdown(&game->audio);

    if (game->font) {
        TTF_CloseFont(game->font);
        game->font = NULL;
    }

    if (!embedded_mode) {
        if (game->renderer) {
            SDL_DestroyRenderer(game->renderer);
            game->renderer = NULL;
        }
        if (game->window) {
            SDL_DestroyWindow(game->window);
            game->window = NULL;
        }

        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
    }
}

static void export_shop_state(const GameState *game, NewShopState *shop_state) {
    if (!shop_state) {
        return;
    }

    shop_state->control_scheme = game->control_scheme;
    shop_state->duo_enabled = game->duo_enabled ? 1 : 0;
    shop_state->online_hosted = game->online_hosted ? 1 : 0;
    shop_state->player_interact_bind[0] = game->player_interact_bind[0];
    shop_state->player_interact_bind[1] = game->player_interact_bind[1];
    shop_state->player_skin_number[0] = game->player_skin_number[0];
    shop_state->player_skin_number[1] = game->player_skin_number[1];
    shop_state->lives = game->lives_held;
    shop_state->extra_hearts = game->extra_hearts_held;
    shop_state->player_lives[0] = game->lives_held;
    shop_state->player_lives[1] = game->lives_held_p2;
    shop_state->player_extra_hearts[0] = game->extra_hearts_held;
    shop_state->player_extra_hearts[1] = game->extra_hearts_held_p2;
    shop_state->keys_held = game->keys_held;
    shop_state->active_player = game->active_shop_player;
    shop_state->buy_count_jetpack = game->buy_count_jetpack;
    shop_state->buy_count_magnet = game->buy_count_magnet;
    shop_state->buy_count_shoes = game->buy_count_shoes;
    shop_state->player_buy_count_jetpack[0] = game->player_buy_count_jetpack[0];
    shop_state->player_buy_count_magnet[0] = game->player_buy_count_magnet[0];
    shop_state->player_buy_count_shoes[0] = game->player_buy_count_shoes[0];
    shop_state->player_buy_count_jetpack[1] = game->player_buy_count_jetpack[1];
    shop_state->player_buy_count_magnet[1] = game->player_buy_count_magnet[1];
    shop_state->player_buy_count_shoes[1] = game->player_buy_count_shoes[1];
    shop_state->purchased_jetpack = game->purchased_jetpack;
    shop_state->purchased_magnet = game->purchased_magnet;
    shop_state->purchased_shoes = game->purchased_shoes;
    shop_state->player_purchased_jetpack[0] = game->player_purchased_jetpack[0];
    shop_state->player_purchased_magnet[0] = game->player_purchased_magnet[0];
    shop_state->player_purchased_shoes[0] = game->player_purchased_shoes[0];
    shop_state->player_purchased_jetpack[1] = game->player_purchased_jetpack[1];
    shop_state->player_purchased_magnet[1] = game->player_purchased_magnet[1];
    shop_state->player_purchased_shoes[1] = game->player_purchased_shoes[1];
}

static int run_game_loop(GameState *game) {
    uint32_t last_ticks = SDL_GetTicks();

    while (game->running) {
        const uint32_t frame_start = SDL_GetTicks();
        float dt_seconds = (float)(frame_start - last_ticks) / 1000.0f;
        if (dt_seconds > MAX_DELTA_SECONDS) {
            dt_seconds = MAX_DELTA_SECONDS;
        }
        last_ticks = frame_start;

        input_begin_frame(&game->input);
        if (game->online_hosted) {
            online_client_pump();
        }
        input_process(&game->input);

        update_game(game, dt_seconds, frame_start);
        draw_world(game, frame_start);

        const uint32_t frame_time = SDL_GetTicks() - frame_start;
        const uint32_t target_frame = 1000 / TARGET_FPS;
        if (frame_time < target_frame) {
            SDL_Delay(target_frame - frame_time);
        }
    }

    return game->input.quit_requested ? 2 : 0;
}

int game_run(void) {
    GameState game;
    if (!init_game(&game, NULL, NULL, false, NULL)) {
        shutdown_game(&game, false);
        return 1;
    }

    {
        int rc = run_game_loop(&game);
        shutdown_game(&game, false);
        return rc;
    }
}

int runNewShopLevel1(SDL_Window *shared_window, SDL_Renderer *shared_renderer, NewShopState *shop_state) {
    GameState game;
    int previous_logical_w = 0;
    int previous_logical_h = 0;

    if (!shared_window || !shared_renderer) {
        SDL_Log("runNewShopLevel1 requires a shared window and renderer.");
        return 1;
    }

    SDL_RenderGetLogicalSize(shared_renderer, &previous_logical_w, &previous_logical_h);
    SDL_RenderSetLogicalSize(shared_renderer, WINDOW_W, WINDOW_H);

    if (!init_game(&game, shared_window, shared_renderer, true, shop_state)) {
        shutdown_game(&game, true);
        SDL_RenderSetLogicalSize(shared_renderer, previous_logical_w, previous_logical_h);
        return 1;
    }

    {
        int rc = run_game_loop(&game);
        export_shop_state(&game, shop_state);
        shutdown_game(&game, true);
        SDL_RenderSetLogicalSize(shared_renderer, previous_logical_w, previous_logical_h);
        return rc;
    }
}
