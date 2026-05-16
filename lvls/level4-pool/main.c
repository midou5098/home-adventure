#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../shared/arcade_input.h"
#include "../shared/session.h"
#include "../../src/options/options_scene.h"
#include "online_client.h"
#include "font5x7.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define VIEW_WIDTH 1400
#define VIEW_HEIGHT 700

#define WORLD_WIDTH 2800.0f
#define WORLD_HEIGHT 700.0f
#define FLOOR_Y 470.0f
#define GRAVITY 0.28f

#define MAX_BOUNCES 5
#define MIN_STOP_SPEED 1.0f
#define BOUNCE_POWER_LOSS 0.78f
#define DIRECT_HIT_DAMAGE_SCALE 2.1f
#define DIRECT_HIT_FORCE_SCALE 0.62f
#define PLAYER_MIN_POWER 5.0f
#define PLAYER_MAX_POWER 21.0f
#define PLAYER_AIM_MIN_ANGLE (-1.45f)
#define PLAYER_AIM_MAX_ANGLE 0.15f
#define PLAYER_MAX_DRAG_DISTANCE 260.0f
#define LEVEL4_ONLINE_REMOTE_WINDOW_ID 0xFFFFFFFFu

#define MAX_SPLASHES 256
#define MAX_STATUS_TEXT 128
#define TARGET_COUNT 11
#define TV_Z_MIN 0
#define TV_Z_MAX 5
#define AD_MAX_FRAMES 306
#define AD_FPS 15
#define AIRPLANE_SHEET_COLS 6
#define AIRPLANE_SHEET_ROWS 6
#define AIRPLANE_SHEET_FRAMES 36
#define AIRPLANE_FPS 12
#define AIRPLANE_HIGH_SPEED_FPS 18
#define AIRPLANE_BASE_RENDER_HEIGHT 120.0f
#define AIRPLANE_SPEED 2.6f
#define AIRPLANE_RESPAWN_SPEED 14.0f
#define AIRPLANE_RESPAWN_ALTITUDE_OFFSET 62.0f
#define AIRPLANE_RESPAWN_DROP_Y_OFFSET 22.0f
#define PLAYER_RESPAWN_SHEET_COLS 6
#define PLAYER_RESPAWN_SHEET_ROWS 6
#define PLAYER_RESPAWN_SHEET_FRAMES 36
#define PLAYER_RESPAWN_SHEET_FPS 12
#define PLAYER_RESPAWN_RENDER_HEIGHT 150
#define PLAYER_RESPAWN_FALL_GRAVITY 0.12f
#define PLAYER_RESPAWN_FALL_MAX_VY 2.6f
#define LEVEL5_AUTO_ADVANCE_FRAMES 180
#define INTRO_FADE_IN_FRAMES 18
#define INTRO_TITLE_IN_FRAMES 36
#define INTRO_TITLE_HOLD_FRAMES 60
#define INTRO_FADE_OUT_FRAMES 48
#define LEVEL5_DEFAULT_PLAYER_HEALTH_POINTS 9
#define LEVEL5_MAX_PLAYER_HEALTH_POINTS 108
#define PLAYER_COMBAT_HEALTH 100
#define PLAYER_RESPAWN_FADE_FRAMES 30
#define PLAYER_RESPAWN_DROP_HEIGHT 120.0f
#define PLAYER_RESPAWN_TUBE_LOCK_FRAMES 28
#define SFX_CHANNEL_RESPAWN_START 0
#define SFX_CHANNEL_RESPAWN_DROP 1
#define RESERVED_SFX_CHANNELS 2

/* Keep level data layout in a header without changing runtime logic. */
#include "level5_types.h"

static const float FRAME_TIME = 1.0f / 60.0f;
static void update_airplane_motion(Game *game);
static float get_airplane_offscreen_margin(const Game *game);
static void start_respawn_sequence(Game *game, Character *player);
static ControlScheme level3ControlSchemeForPlayer(const Game *game, int player_index);
static InteractBind level3InteractBindForPlayer(const Game *game, int player_index);

static float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int clampi_local(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float randf(float min_value, float max_value) {
    return min_value + ((float) rand() / (float) RAND_MAX) * (max_value - min_value);
}

static float lengthf(float x, float y) {
    return sqrtf(x * x + y * y);
}

static int round_to_int(double value) {
    return (int) (value >= 0.0 ? value + 0.5 : value - 0.5);
}

static SDL_Color rgb(Uint8 r, Uint8 g, Uint8 b) {
    SDL_Color color = {r, g, b, 255};
    return color;
}

static SDL_Color color_with_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
}

static SDL_Color player_ui_color(int player_index) {
    return player_index == 0 ? rgb(59, 130, 246) : rgb(34, 197, 94);
}

static int clamp_skin_number(int skin) {
    return skin == 2 ? 2 : 1;
}

static int resolve_player_skin_number(const Game *game, int player_index) {
    int skin = 1;
    if (!game) return 1;
    if (player_index >= 0 && player_index < 2) {
        skin = clamp_skin_number(game->player_skin_number[player_index]);
    }

    /*
     * Keep compatibility with older single-player flows that only filled
     * session->skin_number.
     */
    if (player_index == 0 &&
        game->session &&
        game->session->mode != GAME_MODE_DUO &&
        game->session->skin_number == 2) {
        skin = 2;
    }
    return skin;
}

static const char *player_name_for_skin(int skin) {
    return clamp_skin_number(skin) == 2 ? "Harry" : "Marv";
}

static int configured_player_count(const Game *game) {
    return game->duo_mode ? 2 : 1;
}

static Character *get_player_by_index(Game *game, int index) {
    if (index == 0) return &game->player;
    if (index == 1 && game->duo_mode) return &game->player2;
    return NULL;
}

static const Character *get_player_by_index_const(const Game *game, int index) {
    if (index == 0) return &game->player;
    if (index == 1 && game->duo_mode) return &game->player2;
    return NULL;
}

static int get_player_index(const Game *game, const Character *character) {
    if (character == &game->player) return 0;
    if (game->duo_mode && character == &game->player2) return 1;
    return -1;
}

static bool is_player_character(const Game *game, const Character *character) {
    return get_player_index(game, character) >= 0;
}

static int alive_player_count(const Game *game) {
    int count = 0;
    int players = configured_player_count(game);
    for (int i = 0; i < players; ++i) {
        const Character *player = get_player_by_index_const(game, i);
        if (player && player->alive) ++count;
    }
    return count;
}

static void ensure_active_player(Game *game) {
    if (!game->duo_mode) {
        game->active_player_index = 0;
        return;
    }

    Character *active = get_player_by_index(game, game->active_player_index);
    if (active && active->alive && !active->defeated) return;

    Character *other = get_player_by_index(game, 1 - game->active_player_index);
    if (other && other->alive && !other->defeated) {
        game->active_player_index = 1 - game->active_player_index;
        return;
    }

    game->active_player_index = 0;
}

static void advance_active_player(Game *game) {
    if (!game->duo_mode) {
        game->active_player_index = 0;
        return;
    }

    int next = 1 - game->active_player_index;
    Character *candidate = get_player_by_index(game, next);
    if (candidate && candidate->alive && !candidate->defeated) {
        game->active_player_index = next;
        return;
    }

    ensure_active_player(game);
}

static Character *get_active_player(Game *game) {
    ensure_active_player(game);
    return get_player_by_index(game, game->active_player_index);
}

static Character *get_enemy_target_player(Game *game) {
    Character *active = get_active_player(game);
    if (active && active->alive && !active->defeated) return active;

    int players = configured_player_count(game);
    for (int i = 0; i < players; ++i) {
        Character *player = get_player_by_index(game, i);
        if (player && player->alive && !player->defeated) return player;
    }
    return NULL;
}

static int clamp_player_health_points(int value) {
    return clampi_local(value, 1, LEVEL5_MAX_PLAYER_HEALTH_POINTS);
}

static int resolve_player_start_health_from_session(const GameSession *session, int player_index) {
    int fallback = LEVEL5_DEFAULT_PLAYER_HEALTH_POINTS;
    int carry_start = 0;

    if (!session) return fallback;

    if (session->level2.starting_lives > 0) {
        int level2_start = clamp_player_health_points(session->level2.starting_lives);
        int lives_lost = session->level2.player_lives_lost;

        if (session->mode == GAME_MODE_DUO && session->level2.multiplayer) {
            int p1_remaining = clampi_local(level2_start - clampi_local(session->level2.player_lives_lost, 0, level2_start), 0, level2_start);
            int p2_remaining = clampi_local(level2_start - clampi_local(session->level2.marv_lives_lost, 0, level2_start), 0, level2_start);

            if ((p1_remaining == 0 && p2_remaining > 0) ||
                (p2_remaining == 0 && p1_remaining > 0)) {
                int total_remaining = p1_remaining + p2_remaining;
                int base_share = total_remaining / 2;
                int extra = total_remaining % 2;
                int survivor_index = (p2_remaining > p1_remaining) ? 1 : 0;
                int share = base_share;

                if (player_index == survivor_index)
                    share += extra;

                return clamp_player_health_points(share);
            }
        }

        if (player_index == 1 && session->mode == GAME_MODE_DUO) {
            lives_lost = session->level2.multiplayer
                ? session->level2.marv_lives_lost
                : session->level2.player_lives_lost;
        }
        lives_lost = clampi_local(lives_lost, 0, level2_start);
        return clamp_player_health_points(level2_start - lives_lost);
    }

    if (session_load_level_life_carry(&carry_start, NULL, NULL)) {
        return clamp_player_health_points(carry_start);
    }

    if (session->level1.completed) {
        int lives = clampi_local(session->level1.lives_remaining, 0, 9);
        int bonus = clampi_local(session->bonus_lives_for_level2, 0, 99);
        int total = lives + bonus;
        if (total < 1) total = 1;
        return clamp_player_health_points(total);
    }

    return fallback;
}

static void resolve_player_start_health(Game *game) {
    game->player_start_health[0] = resolve_player_start_health_from_session(game ? game->session : NULL, 0);
    game->player_start_health[1] = resolve_player_start_health_from_session(game ? game->session : NULL, 1);
    if (!game->duo_mode) game->player_start_health[1] = 0;
}

static void get_character_display_health(const Game *game, const Character *character, int *out_health, int *out_max_health) {
    int health = character ? character->health : 0;
    int max_health = character ? character->max_health : 1;

    if (game && character && is_player_character(game, character)) {
        health = character->stock_health;
        max_health = character->stock_max_health;
    }

    if (max_health < 1) max_health = 1;
    health = clampi_local(health, 0, max_health);
    if (out_health) *out_health = health;
    if (out_max_health) *out_max_health = max_health;
}

static int calculate_level5_points(const Game *game) {
    int health_bonus;
    int time_bonus;
    int elapsed_seconds;

    if (!game || game->game_state != GAME_WON) return 0;

    int players = configured_player_count(game);
    int total_health = 0;
    int total_max_health = 0;
    for (int i = 0; i < players; ++i) {
        const Character *player = get_player_by_index_const(game, i);
        int shown_health = 0;
        int shown_max = 1;
        if (!player) continue;
        get_character_display_health(game, player, &shown_health, &shown_max);
        total_health += shown_health;
        total_max_health += shown_max;
    }
    health_bonus = total_max_health > 0 ? total_health * 150 / total_max_health : 0;
    elapsed_seconds = game->time_frames / 60;
    if (elapsed_seconds <= 90) time_bonus = 150;
    else if (elapsed_seconds >= 240) time_bonus = 0;
    else time_bonus = 240 - elapsed_seconds;

    return 300 + health_bonus + time_bonus;
}

static void set_status(Game *game, const char *message, int duration_frames) {
    SDL_snprintf(game->status_text, sizeof(game->status_text), "%s", message ? message : "");
    game->status_until = game->time_frames + duration_frames;
}

static void join_path(char *out, size_t out_size, const char *base, const char *relative) {
    SDL_snprintf(out, out_size, "%s%s", base, relative);
}

static void resolve_base_paths(Game *game) {
    SDL_snprintf(game->base_path, sizeof(game->base_path), "%s", "");
    join_path(game->assets_path, sizeof(game->assets_path), game->base_path, "assets/");
    join_path(game->config_path, sizeof(game->config_path), game->base_path, "config/level_layout.cfg");
}

static void set_render_color(SDL_Renderer *renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void draw_filled_rect(SDL_Renderer *renderer, float x, float y, float w, float h, SDL_Color color) {
    SDL_FRect rect = {x, y, w, h};
    set_render_color(renderer, color);
    SDL_RenderFillRectF(renderer, &rect);
}

static void draw_rect(SDL_Renderer *renderer, float x, float y, float w, float h, SDL_Color color) {
    SDL_FRect rect = {x, y, w, h};
    set_render_color(renderer, color);
    SDL_RenderDrawRectF(renderer, &rect);
}

static void draw_line(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, SDL_Color color) {
    set_render_color(renderer, color);
    SDL_RenderDrawLineF(renderer, x1, y1, x2, y2);
}

static void draw_thick_line(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, float thickness, SDL_Color color) {
    int lines = clampi_local((int) floorf(thickness), 1, 8);
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = lengthf(dx, dy);
    float nx = 0.0f;
    float ny = 0.0f;

    if (len > 0.001f) {
        nx = -dy / len;
        ny = dx / len;
    }

    for (int i = 0; i < lines; ++i) {
        float offset = i - (lines - 1) * 0.5f;
        draw_line(renderer, x1 + nx * offset, y1 + ny * offset, x2 + nx * offset, y2 + ny * offset, color);
    }
}

static void draw_filled_circle(SDL_Renderer *renderer, float cx, float cy, float radius, SDL_Color color) {
    set_render_color(renderer, color);
    int ir = (int) ceilf(radius);
    for (int dy = -ir; dy <= ir; ++dy) {
        float fy = (float) dy;
        float span = sqrtf(fmaxf(0.0f, radius * radius - fy * fy));
        SDL_RenderDrawLineF(renderer, cx - span, cy + fy, cx + span, cy + fy);
    }
}

static void draw_circle_outline(SDL_Renderer *renderer, float cx, float cy, float radius, SDL_Color color) {
    set_render_color(renderer, color);
    for (int i = 0; i < 48; ++i) {
        float a0 = ((float) i / 48.0f) * 2.0f * (float) M_PI;
        float a1 = ((float) (i + 1) / 48.0f) * 2.0f * (float) M_PI;
        SDL_RenderDrawLineF(renderer, cx + cosf(a0) * radius, cy + sinf(a0) * radius, cx + cosf(a1) * radius, cy + sinf(a1) * radius);
    }
}

static void draw_filled_ellipse(SDL_Renderer *renderer, float cx, float cy, float rx, float ry, SDL_Color color) {
    set_render_color(renderer, color);
    int iry = (int) ceilf(ry);
    for (int dy = -iry; dy <= iry; ++dy) {
        float fy = (float) dy / ry;
        float span = rx * sqrtf(fmaxf(0.0f, 1.0f - fy * fy));
        SDL_RenderDrawLineF(renderer, cx - span, cy + dy, cx + span, cy + dy);
    }
}

static void draw_arc(SDL_Renderer *renderer, float cx, float cy, float radius, float start_angle, float end_angle, SDL_Color color, float thickness) {
    const int segments = 40;
    float step = (end_angle - start_angle) / (float) segments;
    for (int i = 0; i < segments; ++i) {
        float a0 = start_angle + step * i;
        float a1 = start_angle + step * (i + 1);
        draw_thick_line(
            renderer,
            cx + cosf(a0) * radius,
            cy + sinf(a0) * radius,
            cx + cosf(a1) * radius,
            cy + sinf(a1) * radius,
            thickness,
            color
        );
    }
}

static int text_width_px(const char *text, int scale) {
    int width = 0;
    int line_width = 0;

    for (const char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '\n') {
            if (line_width > width) width = line_width;
            line_width = 0;
            continue;
        }
        line_width += 6 * scale;
    }

    if (line_width > width) width = line_width;
    return width;
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, SDL_Color color, const char *text) {
    int origin_x = x;

    for (const char *cursor = text; *cursor; ++cursor) {
        unsigned char ch = (unsigned char) *cursor;

        if (ch == '\n') {
            x = origin_x;
            y += 8 * scale;
            continue;
        }

        ch = (unsigned char) toupper(ch);
        const uint8_t *glyph = FONT5X7[ch];

        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (glyph[row] & (1u << (4 - col))) {
                    draw_filled_rect(renderer, (float) (x + col * scale), (float) (y + row * scale), (float) scale, (float) scale, color);
                }
            }
        }

        x += 6 * scale;
    }
}

static void draw_text_centered(SDL_Renderer *renderer, int center_x, int y, int scale, SDL_Color color, const char *text) {
    draw_text(renderer, center_x - text_width_px(text, scale) / 2, y, scale, color, text);
}

static bool load_texture_asset(SDL_Renderer *renderer, TextureAsset *asset, const char *path) {
    asset->texture = IMG_LoadTexture(renderer, path);
    asset->width = 0;
    asset->height = 0;
    if (!asset->texture) return false;
    SDL_QueryTexture(asset->texture, NULL, NULL, &asset->width, &asset->height);
    SDL_SetTextureBlendMode(asset->texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(asset->texture, SDL_ScaleModeLinear);
    return true;
}

static bool load_sheet(SDL_Renderer *renderer, SpriteSheet *sheet, const char *path, int columns, int rows, int frame_count, int fps) {
    bool ok = load_texture_asset(renderer, &sheet->texture, path);
    sheet->columns = columns;
    sheet->rows = rows;
    sheet->frame_count = frame_count;
    sheet->fps = fps;
    return ok;
}

static Mix_Chunk *load_chunk_or_null(const char *path) {
    return Mix_LoadWAV(path);
}

static Mix_Music *load_music_or_null(const char *path) {
    return Mix_LoadMUS(path);
}

static void destroy_texture_asset(TextureAsset *asset) {
    if (asset->texture) {
        SDL_DestroyTexture(asset->texture);
        asset->texture = NULL;
    }
    asset->width = 0;
    asset->height = 0;
}

static void cleanup_assets(Assets *assets) {
    destroy_texture_asset(&assets->background);
    destroy_texture_asset(&assets->wall);
    destroy_texture_asset(&assets->tv);
    destroy_texture_asset(&assets->airplane);
    destroy_texture_asset(&assets->airplane_high_speed);
    destroy_texture_asset(&assets->player_respawn_skin1);
    destroy_texture_asset(&assets->player_respawn_skin2);
    for (int i = 0; i < AD_MAX_FRAMES; ++i) {
        destroy_texture_asset(&assets->ad_frames[i]);
    }
    for (int i = 0; i < SHEET_COUNT; ++i) {
        destroy_texture_asset(&assets->sheets[i].texture);
    }
    if (assets->chapter_title_tex) SDL_DestroyTexture(assets->chapter_title_tex);
    if (assets->chapter_font) TTF_CloseFont(assets->chapter_font);

    if (assets->music_background) Mix_FreeMusic(assets->music_background);
    if (assets->sfx_win) Mix_FreeChunk(assets->sfx_win);
    if (assets->sfx_splash) Mix_FreeChunk(assets->sfx_splash);
    if (assets->sfx_fire) Mix_FreeChunk(assets->sfx_fire);
    if (assets->sfx_among) Mix_FreeChunk(assets->sfx_among);
    if (assets->sfx_oh_no) Mix_FreeChunk(assets->sfx_oh_no);
    if (assets->sfx_airplane_start_move) Mix_FreeChunk(assets->sfx_airplane_start_move);
    if (assets->sfx_respawn_drop) Mix_FreeChunk(assets->sfx_respawn_drop);

    assets->music_background = NULL;
    assets->chapter_font = NULL;
    assets->chapter_title_tex = NULL;
    assets->chapter_title_w = 0;
    assets->chapter_title_h = 0;
    assets->ad_frame_count = 0;
    assets->sfx_win = NULL;
    assets->sfx_splash = NULL;
    assets->sfx_fire = NULL;
    assets->sfx_among = NULL;
    assets->sfx_oh_no = NULL;
    assets->sfx_airplane_start_move = NULL;
    assets->sfx_respawn_drop = NULL;
}

static void play_chunk_safe(Game *game, Mix_Chunk *chunk, int loops) {
    if (!game->assets.audio_ok || !chunk) return;
    Mix_PlayChannel(-1, chunk, loops);
}

static void play_chunk_on_channel_safe(Game *game, Mix_Chunk *chunk, int channel, int loops, int volume) {
    if (!game->assets.audio_ok || !chunk) return;
    if (channel >= 0) {
        Mix_Volume(channel, clampi_local(volume, 0, MIX_MAX_VOLUME));
    }
    Mix_PlayChannel(channel, chunk, loops);
}

static void ensure_background_music(Game *game) {
    if (!game->assets.audio_ok || !game->assets.music_background) return;
    Mix_VolumeMusic(options_scene_get_music_volume_sdl());
    if (!Mix_PlayingMusic()) Mix_PlayMusic(game->assets.music_background, -1);
}

static bool load_assets(Game *game) {
    Assets *assets = &game->assets;
    char path[1024];
    const char *font_paths[] = {
        "font.ttf",
        "assets/font.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        NULL
    };
    int img_flags;
    int mixer_flags;
    int audio_freq;
    Uint16 audio_format;
    int audio_channels;

    assets->image_ok = false;
    assets->image_owned = false;
    assets->audio_ok = false;
    assets->mixer_owned = false;
    assets->audio_device_owned = false;
    assets->chapter_font = NULL;
    assets->chapter_title_tex = NULL;
    assets->chapter_title_w = 0;
    assets->chapter_title_h = 0;
    assets->ad_frame_count = 0;
    assets->airplane.texture = NULL;
    assets->airplane.width = 0;
    assets->airplane.height = 0;
    assets->airplane_high_speed.texture = NULL;
    assets->airplane_high_speed.width = 0;
    assets->airplane_high_speed.height = 0;
    assets->player_respawn_skin1.texture = NULL;
    assets->player_respawn_skin1.width = 0;
    assets->player_respawn_skin1.height = 0;
    assets->player_respawn_skin2.texture = NULL;
    assets->player_respawn_skin2.width = 0;
    assets->player_respawn_skin2.height = 0;
    for (int i = 0; i < AD_MAX_FRAMES; ++i) {
        assets->ad_frames[i].texture = NULL;
        assets->ad_frames[i].width = 0;
        assets->ad_frames[i].height = 0;
    }

    img_flags = IMG_Init(0);
    if ((img_flags & IMG_INIT_PNG) == 0) {
        img_flags = IMG_Init(IMG_INIT_PNG);
        assets->image_owned = (img_flags & IMG_INIT_PNG) != 0;
    }
    assets->image_ok = (img_flags & IMG_INIT_PNG) != 0;

    mixer_flags = Mix_Init(0);
    if ((mixer_flags & MIX_INIT_MP3) == 0) {
        mixer_flags = Mix_Init(MIX_INIT_MP3);
        assets->mixer_owned = (mixer_flags & MIX_INIT_MP3) != 0;
    }

    if ((mixer_flags & MIX_INIT_MP3) != 0) {
        if (Mix_QuerySpec(&audio_freq, &audio_format, &audio_channels) == 0) {
            if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
                assets->audio_device_owned = true;
                Mix_AllocateChannels(24);
                Mix_ReserveChannels(RESERVED_SFX_CHANNELS);
                assets->audio_ok = true;
            }
        } else {
            Mix_ReserveChannels(RESERVED_SFX_CHANNELS);
            assets->audio_ok = true;
        }
    }

    join_path(path, sizeof(path), game->assets_path, "bg_pool.png");
    load_texture_asset(game->renderer, &assets->background, path);
    join_path(path, sizeof(path), game->assets_path, "volleyball_net.png");
    load_texture_asset(game->renderer, &assets->wall, path);
    join_path(path, sizeof(path), game->assets_path, "tv.png");
    load_texture_asset(game->renderer, &assets->tv, path);
    SDL_snprintf(path, sizeof(path), "../air plane.png");
    if (!load_texture_asset(game->renderer, &assets->airplane, path)) {
        SDL_snprintf(path, sizeof(path), "air plane.png");
        if (!load_texture_asset(game->renderer, &assets->airplane, path)) {
            join_path(path, sizeof(path), game->assets_path, "air plane.png");
            load_texture_asset(game->renderer, &assets->airplane, path);
        }
    }

    SDL_snprintf(path, sizeof(path), "../airplane high speed.png");
    if (!load_texture_asset(game->renderer, &assets->airplane_high_speed, path)) {
        SDL_snprintf(path, sizeof(path), "airplane high speed.png");
        if (!load_texture_asset(game->renderer, &assets->airplane_high_speed, path)) {
            join_path(path, sizeof(path), game->assets_path, "airplane high speed.png");
            if (!load_texture_asset(game->renderer, &assets->airplane_high_speed, path)) {
                SDL_snprintf(path, sizeof(path), "../animation-pack-spritesheets (2)/animation-pack/airplane high speed.png");
                load_texture_asset(game->renderer, &assets->airplane_high_speed, path);
            }
        }
    }

    SDL_snprintf(path, sizeof(path), "../skin1 respawn.png");
    if (!load_texture_asset(game->renderer, &assets->player_respawn_skin1, path)) {
        SDL_snprintf(path, sizeof(path), "skin1 respawn.png");
        if (!load_texture_asset(game->renderer, &assets->player_respawn_skin1, path)) {
            join_path(path, sizeof(path), game->assets_path, "skin1 respawn.png");
            if (!load_texture_asset(game->renderer, &assets->player_respawn_skin1, path)) {
                SDL_snprintf(path, sizeof(path), "../animation-pack-spritesheets (2)/animation-pack/skin1 respawn.png");
                load_texture_asset(game->renderer, &assets->player_respawn_skin1, path);
            }
        }
    }

    SDL_snprintf(path, sizeof(path), "../skin2 respawn.png");
    if (!load_texture_asset(game->renderer, &assets->player_respawn_skin2, path)) {
        SDL_snprintf(path, sizeof(path), "skin2 respawn.png");
        if (!load_texture_asset(game->renderer, &assets->player_respawn_skin2, path)) {
            join_path(path, sizeof(path), game->assets_path, "skin2 respawn.png");
            if (!load_texture_asset(game->renderer, &assets->player_respawn_skin2, path)) {
                SDL_snprintf(path, sizeof(path), "../animation-pack-spritesheets (2)/animation-pack/skin2 respawn.png");
                load_texture_asset(game->renderer, &assets->player_respawn_skin2, path);
            }
        }
    }

    for (int i = 1; i <= AD_MAX_FRAMES; ++i) {
        char frame_path[1024];
        int frame_index = assets->ad_frame_count;
        SDL_snprintf(frame_path, sizeof(frame_path), "../animated_ad/frame (%d).png", i);
        if (!load_texture_asset(game->renderer, &assets->ad_frames[frame_index], frame_path)) {
            SDL_snprintf(frame_path, sizeof(frame_path), "animated_ad/frame (%d).png", i);
            if (!load_texture_asset(game->renderer, &assets->ad_frames[frame_index], frame_path)) break;
        }
        assets->ad_frame_count++;
    }

    join_path(path, sizeof(path), game->assets_path, "skin1_idle.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "skin1_happy_jump.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR_HAPPY], path, 5, 5, 25, 10);
    join_path(path, sizeof(path), game->assets_path, "skin1_light_cigarette.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR_SMOKE], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "skin2_idle.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR2], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "skin2_happy_jump.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR2_HAPPY], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "skin2_light_cigarette.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_BURGLAR2_SMOKE], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "kevin_idle.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_KEVIN], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "friend_back.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_FRIEND_BACK], path, 6, 6, 36, 10);
    join_path(path, sizeof(path), game->assets_path, "friends_front.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_FRIEND_FRONT], path, 6, 10, 60, 10);
    join_path(path, sizeof(path), game->assets_path, "inner_tube.png");
    load_sheet(game->renderer, &assets->sheets[SHEET_INNER_TUBE], path, 6, 6, 36, 12);

    for (int i = 0; font_paths[i] && !assets->chapter_font; ++i)
        assets->chapter_font = TTF_OpenFont(font_paths[i], 42);
    if (assets->chapter_font) {
        SDL_Surface *title_surface = TTF_RenderUTF8_Blended(assets->chapter_font,
                                                            "Chapter 4 : The Pool",
                                                            (SDL_Color){30, 30, 60, 255});
        if (title_surface) {
            assets->chapter_title_tex = SDL_CreateTextureFromSurface(game->renderer, title_surface);
            assets->chapter_title_w = title_surface->w;
            assets->chapter_title_h = title_surface->h;
            SDL_FreeSurface(title_surface);
            if (assets->chapter_title_tex) {
                SDL_SetTextureBlendMode(assets->chapter_title_tex, SDL_BLENDMODE_BLEND);
            }
        }
    }

    if (assets->audio_ok) {
        /* Prefer new asset names from the fulllvl pack; fall back to legacy names. */
        join_path(path, sizeof(path), game->assets_path, "music_bg.mp3");
        assets->music_background = load_music_or_null(path);
        if (!assets->music_background) {
            join_path(path, sizeof(path), game->assets_path, "bg_music.mp3");
            assets->music_background = load_music_or_null(path);
        }

        join_path(path, sizeof(path), game->assets_path, "sfx_win.wav");
        assets->sfx_win = load_chunk_or_null(path);
        if (!assets->sfx_win) {
            join_path(path, sizeof(path), game->assets_path, "happy.wav");
            assets->sfx_win = load_chunk_or_null(path);
        }

        join_path(path, sizeof(path), game->assets_path, "sfx_splash.wav");
        assets->sfx_splash = load_chunk_or_null(path);
        if (!assets->sfx_splash) {
            join_path(path, sizeof(path), game->assets_path, "water_splash.wav");
            assets->sfx_splash = load_chunk_or_null(path);
        }

        join_path(path, sizeof(path), game->assets_path, "sfx_hit.wav");
        assets->sfx_fire = load_chunk_or_null(path);
        if (!assets->sfx_fire) {
            join_path(path, sizeof(path), game->assets_path, "snowball_hit.wav");
            assets->sfx_fire = load_chunk_or_null(path);
        }

        join_path(path, sizeof(path), game->assets_path, "sfx_reveal.wav");
        assets->sfx_among = load_chunk_or_null(path);
        if (!assets->sfx_among) {
            join_path(path, sizeof(path), game->assets_path, "role_reveal.wav");
            assets->sfx_among = load_chunk_or_null(path);
        }

        join_path(path, sizeof(path), game->assets_path, "sfx_defeat.mp3");
        assets->sfx_oh_no = load_chunk_or_null(path);
        if (!assets->sfx_oh_no) {
            join_path(path, sizeof(path), game->assets_path, "oh_no.mp3");
            assets->sfx_oh_no = load_chunk_or_null(path);
        }

        SDL_snprintf(path, sizeof(path), "../airplane starto to move.wav");
        assets->sfx_airplane_start_move = load_chunk_or_null(path);
        if (!assets->sfx_airplane_start_move) {
            SDL_snprintf(path, sizeof(path), "../airplane start to move.wav");
            assets->sfx_airplane_start_move = load_chunk_or_null(path);
        }
        if (!assets->sfx_airplane_start_move) {
            SDL_snprintf(path, sizeof(path), "airplane starto to move.wav");
            assets->sfx_airplane_start_move = load_chunk_or_null(path);
        }
        if (!assets->sfx_airplane_start_move) {
            SDL_snprintf(path, sizeof(path), "airplane start to move.wav");
            assets->sfx_airplane_start_move = load_chunk_or_null(path);
        }
        if (!assets->sfx_airplane_start_move) {
            join_path(path, sizeof(path), game->assets_path, "airplane starto to move.wav");
            assets->sfx_airplane_start_move = load_chunk_or_null(path);
        }
        if (!assets->sfx_airplane_start_move) {
            join_path(path, sizeof(path), game->assets_path, "airplane start to move.wav");
            assets->sfx_airplane_start_move = load_chunk_or_null(path);
        }

        SDL_snprintf(path, sizeof(path), "../respawn.wav");
        assets->sfx_respawn_drop = load_chunk_or_null(path);
        if (!assets->sfx_respawn_drop) {
            SDL_snprintf(path, sizeof(path), "../respawn sound effect.wav");
            assets->sfx_respawn_drop = load_chunk_or_null(path);
        }
        if (!assets->sfx_respawn_drop) {
            SDL_snprintf(path, sizeof(path), "respawn.wav");
            assets->sfx_respawn_drop = load_chunk_or_null(path);
        }
        if (!assets->sfx_respawn_drop) {
            SDL_snprintf(path, sizeof(path), "respawn sound effect.wav");
            assets->sfx_respawn_drop = load_chunk_or_null(path);
        }
        if (!assets->sfx_respawn_drop) {
            join_path(path, sizeof(path), game->assets_path, "respawn.wav");
            assets->sfx_respawn_drop = load_chunk_or_null(path);
        }
        if (!assets->sfx_respawn_drop) {
            join_path(path, sizeof(path), game->assets_path, "respawn sound effect.wav");
            assets->sfx_respawn_drop = load_chunk_or_null(path);
        }

        if (assets->sfx_win) Mix_VolumeChunk(assets->sfx_win, (int) (MIX_MAX_VOLUME * 0.75f));
        if (assets->sfx_splash) Mix_VolumeChunk(assets->sfx_splash, (int) (MIX_MAX_VOLUME * 0.70f));
        if (assets->sfx_fire) Mix_VolumeChunk(assets->sfx_fire, (int) (MIX_MAX_VOLUME * 0.55f));
        if (assets->sfx_among) Mix_VolumeChunk(assets->sfx_among, (int) (MIX_MAX_VOLUME * 0.75f));
        if (assets->sfx_oh_no) Mix_VolumeChunk(assets->sfx_oh_no, (int) (MIX_MAX_VOLUME * 0.85f));
        if (assets->sfx_airplane_start_move) Mix_VolumeChunk(assets->sfx_airplane_start_move, (int) (MIX_MAX_VOLUME * 0.88f));
        if (assets->sfx_respawn_drop) Mix_VolumeChunk(assets->sfx_respawn_drop, (int) (MIX_MAX_VOLUME * 0.85f));
    }

    ensure_background_music(game);
    return true;
}

static void sanitize_level_config(LevelConfig *config) {
    config->player.x = clampf_local(config->player.x, 0.0f, WORLD_WIDTH);
    config->player.float_y = clampf_local(config->player.float_y, 220.0f, WORLD_HEIGHT);
    config->player.render_height = clampi_local(config->player.render_height, 40, 180);
    config->player2.x = clampf_local(config->player2.x, 0.0f, WORLD_WIDTH);
    config->player2.float_y = clampf_local(config->player2.float_y, 220.0f, WORLD_HEIGHT);
    config->player2.render_height = clampi_local(config->player2.render_height, 40, 180);

    for (int i = 0; i < 3; ++i) {
        config->enemies[i].x = clampf_local(config->enemies[i].x, 0.0f, WORLD_WIDTH);
        config->enemies[i].float_y = clampf_local(config->enemies[i].float_y, 220.0f, WORLD_HEIGHT);
        config->enemies[i].render_height = clampi_local(config->enemies[i].render_height, 32, 220);
        config->enemies[i].sprite_cols = clampi_local(config->enemies[i].sprite_cols, 1, 20);
        config->enemies[i].sprite_rows = clampi_local(config->enemies[i].sprite_rows, 1, 20);
        config->enemies[i].sprite_frames = clampi_local(config->enemies[i].sprite_frames, 1, 200);
    }

    for (int i = 0; i < 3; ++i) {
        config->blocks[i].x = clampf_local(config->blocks[i].x, 0.0f, WORLD_WIDTH);
        config->blocks[i].y = clampf_local(config->blocks[i].y, 120.0f, WORLD_HEIGHT);
        config->blocks[i].width = clampf_local(config->blocks[i].width, 20.0f, 220.0f);
        config->blocks[i].height = clampf_local(config->blocks[i].height, 20.0f, 320.0f);
    }

    config->tv.width = clampf_local(config->tv.width, 24.0f, 900.0f);
    config->tv.height = clampf_local(config->tv.height, 24.0f, 520.0f);
    config->tv.x = clampf_local(config->tv.x, 0.0f, WORLD_WIDTH - config->tv.width);
    config->tv.y = clampf_local(config->tv.y, 0.0f, WORLD_HEIGHT - config->tv.height);
    config->tv.z_index = clampi_local(config->tv.z_index, TV_Z_MIN, TV_Z_MAX);

    config->ad.width = clampf_local(config->ad.width, 24.0f, 900.0f);
    config->ad.height = clampf_local(config->ad.height, 24.0f, 520.0f);
    config->ad.x = clampf_local(config->ad.x, 0.0f, WORLD_WIDTH - config->ad.width);
    config->ad.y = clampf_local(config->ad.y, 0.0f, WORLD_HEIGHT - config->ad.height);
    config->ad.z_index = clampi_local(config->ad.z_index, TV_Z_MIN, TV_Z_MAX);

    config->airplane.altitude = clampf_local(config->airplane.altitude, 24.0f, 260.0f);
    config->airplane.size = clampf_local(config->airplane.size, 0.2f, 3.0f);
}

static bool save_level_config(const Game *game) {
    FILE *file = fopen(game->config_path, "wb");
    if (!file) return false;

    const LevelConfig *config = &game->config;
    fprintf(file, "player_x=%.2f\n", config->player.x);
    fprintf(file, "player_y=%.2f\n", config->player.float_y);
    fprintf(file, "player_sprite_h=%d\n", config->player.render_height);
    fprintf(file, "player2_x=%.2f\n", config->player2.x);
    fprintf(file, "player2_y=%.2f\n", config->player2.float_y);
    fprintf(file, "player2_sprite_h=%d\n", config->player2.render_height);

    for (int i = 0; i < 3; ++i) {
        fprintf(file, "enemy%d_x=%.2f\n", i, config->enemies[i].x);
        fprintf(file, "enemy%d_y=%.2f\n", i, config->enemies[i].float_y);
        fprintf(file, "enemy%d_sprite_h=%d\n", i, config->enemies[i].render_height);
        fprintf(file, "enemy%d_cols=%d\n", i, config->enemies[i].sprite_cols);
        fprintf(file, "enemy%d_rows=%d\n", i, config->enemies[i].sprite_rows);
        fprintf(file, "enemy%d_frames=%d\n", i, config->enemies[i].sprite_frames);
    }

    for (int i = 0; i < 3; ++i) {
        fprintf(file, "block%d_x=%.2f\n", i, config->blocks[i].x);
        fprintf(file, "block%d_y=%.2f\n", i, config->blocks[i].y);
        fprintf(file, "block%d_w=%.2f\n", i, config->blocks[i].width);
        fprintf(file, "block%d_h=%.2f\n", i, config->blocks[i].height);
    }
    fprintf(file, "tv_x=%.2f\n", config->tv.x);
    fprintf(file, "tv_y=%.2f\n", config->tv.y);
    fprintf(file, "tv_w=%.2f\n", config->tv.width);
    fprintf(file, "tv_h=%.2f\n", config->tv.height);
    fprintf(file, "tv_z=%d\n", config->tv.z_index);
    fprintf(file, "ad_x=%.2f\n", config->ad.x);
    fprintf(file, "ad_y=%.2f\n", config->ad.y);
    fprintf(file, "ad_w=%.2f\n", config->ad.width);
    fprintf(file, "ad_h=%.2f\n", config->ad.height);
    fprintf(file, "ad_z=%d\n", config->ad.z_index);
    fprintf(file, "plane_altitude=%.2f\n", config->airplane.altitude);
    fprintf(file, "plane_size=%.2f\n", config->airplane.size);

    fclose(file);
    return true;
}

static bool load_level_config(const char *path, LevelConfig *out_config) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;

    *out_config = DEFAULT_LEVEL_CONFIG;
    bool ad_config_seen = false;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[64];
        double value = 0.0;
        if (sscanf(line, " %63[^=]=%lf", key, &value) != 2) continue;

        if (strcmp(key, "player_x") == 0) out_config->player.x = (float) value;
        else if (strcmp(key, "player_y") == 0) out_config->player.float_y = (float) value;
        else if (strcmp(key, "player_sprite_h") == 0) out_config->player.render_height = round_to_int(value);
        else if (strcmp(key, "player2_x") == 0) out_config->player2.x = (float) value;
        else if (strcmp(key, "player2_y") == 0) out_config->player2.float_y = (float) value;
        else if (strcmp(key, "player2_sprite_h") == 0) out_config->player2.render_height = round_to_int(value);
        else if (strcmp(key, "tv_x") == 0) out_config->tv.x = (float) value;
        else if (strcmp(key, "tv_y") == 0) out_config->tv.y = (float) value;
        else if (strcmp(key, "tv_w") == 0) out_config->tv.width = (float) value;
        else if (strcmp(key, "tv_h") == 0) out_config->tv.height = (float) value;
        else if (strcmp(key, "tv_z") == 0) out_config->tv.z_index = round_to_int(value);
        else if (strcmp(key, "ad_x") == 0) { out_config->ad.x = (float) value; ad_config_seen = true; }
        else if (strcmp(key, "ad_y") == 0) { out_config->ad.y = (float) value; ad_config_seen = true; }
        else if (strcmp(key, "ad_w") == 0) { out_config->ad.width = (float) value; ad_config_seen = true; }
        else if (strcmp(key, "ad_h") == 0) { out_config->ad.height = (float) value; ad_config_seen = true; }
        else if (strcmp(key, "ad_z") == 0) { out_config->ad.z_index = round_to_int(value); ad_config_seen = true; }
        else if (strcmp(key, "plane_altitude") == 0) out_config->airplane.altitude = (float) value;
        else if (strcmp(key, "plane_size") == 0) out_config->airplane.size = (float) value;
        else {
            for (int i = 0; i < 3; ++i) {
                char expected[32];

                SDL_snprintf(expected, sizeof(expected), "enemy%d_x", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].x = (float) value; break; }
                SDL_snprintf(expected, sizeof(expected), "enemy%d_y", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].float_y = (float) value; break; }
                SDL_snprintf(expected, sizeof(expected), "enemy%d_sprite_h", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].render_height = round_to_int(value); break; }
                SDL_snprintf(expected, sizeof(expected), "enemy%d_cols", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].sprite_cols = round_to_int(value); break; }
                SDL_snprintf(expected, sizeof(expected), "enemy%d_rows", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].sprite_rows = round_to_int(value); break; }
                SDL_snprintf(expected, sizeof(expected), "enemy%d_frames", i);
                if (strcmp(key, expected) == 0) { out_config->enemies[i].sprite_frames = round_to_int(value); break; }

                SDL_snprintf(expected, sizeof(expected), "block%d_x", i);
                if (strcmp(key, expected) == 0) { out_config->blocks[i].x = (float) value; break; }
                SDL_snprintf(expected, sizeof(expected), "block%d_y", i);
                if (strcmp(key, expected) == 0) { out_config->blocks[i].y = (float) value; break; }
                SDL_snprintf(expected, sizeof(expected), "block%d_w", i);
                if (strcmp(key, expected) == 0) { out_config->blocks[i].width = (float) value; break; }
                SDL_snprintf(expected, sizeof(expected), "block%d_h", i);
                if (strcmp(key, expected) == 0) { out_config->blocks[i].height = (float) value; break; }
            }
        }
    }

    if (!ad_config_seen) {
        out_config->ad.width = out_config->tv.width * 0.82f;
        out_config->ad.height = out_config->tv.height * 0.56f;
        out_config->ad.x = out_config->tv.x + (out_config->tv.width - out_config->ad.width) * 0.5f;
        out_config->ad.y = out_config->tv.y + out_config->tv.height * 0.23f;
        out_config->ad.z_index = clampi_local(out_config->tv.z_index - 1, TV_Z_MIN, TV_Z_MAX);
    }

    fclose(file);
    sanitize_level_config(out_config);
    return true;
}

static Platform make_platform(float x, float floor_top, float width, SDL_Color color1, SDL_Color color2) {
    Platform platform;
    platform.x = x;
    platform.floor_top = floor_top;
    platform.y = floor_top - 6.0f;
    platform.base_y = floor_top - 6.0f;
    platform.width = width;
    platform.height = 26.0f;
    platform.color1 = color1;
    platform.color2 = color2;
    platform.bob_offset = randf(0.0f, (float) M_PI * 2.0f);
    platform.tilt = 0.0f;
    platform.drift = 0.0f;
    platform.hidden = false;
    platform.flip_x = false;
    return platform;
}

static void sync_character_to_float(Character *character) {
    character->x = character->platform.x + character->rider_offset_x;
    character->y = character->platform.y - 16.0f;
}

static void attach_player_to_float_after_respawn(Character *character) {
    if (!character) return;
    character->x = character->platform.x;
    character->rider_offset_x = 0.0f;
    character->y = character->platform.y - 16.0f;
    character->vx = 0.0f;
    character->vy = 0.0f;
    character->on_float = true;
    character->respawn_airborne = false;
    character->respawn_tube_lock_frames = PLAYER_RESPAWN_TUBE_LOCK_FRAMES;
}

static void init_characters(Game *game) {
    Character *player = &game->player;
    memset(player, 0, sizeof(*player));
    player->radius = 20.0f;
    player->name = player_name_for_skin(game->player_skin_number[0]);
    player->angle = -0.75f;
    player->display_angle = player->angle;
    player->power = 14.0f;
    player->skin_color = rgb(209, 213, 219);
    player->hat_color = rgb(17, 24, 39);
    player->body_color = rgb(55, 65, 81);
    player->float_color1 = rgb(14, 165, 233);
    player->float_color2 = rgb(3, 105, 161);
    player->default_sheet = SHEET_BURGLAR;

    Character *player2 = &game->player2;
    memset(player2, 0, sizeof(*player2));
    player2->radius = 20.0f;
    player2->name = player_name_for_skin(game->player_skin_number[1]);
    player2->angle = -0.75f;
    player2->display_angle = player2->angle;
    player2->power = 14.0f;
    player2->skin_color = rgb(191, 219, 254);
    player2->hat_color = rgb(30, 64, 175);
    player2->body_color = rgb(37, 99, 235);
    player2->float_color1 = rgb(59, 130, 246);
    player2->float_color2 = rgb(30, 64, 175);
    player2->default_sheet = SHEET_BURGLAR2;

    Character *kevin = &game->enemies[0];
    memset(kevin, 0, sizeof(*kevin));
    kevin->radius = 20.0f;
    kevin->name = "Kevin";
    kevin->angle = -0.75f;
    kevin->display_angle = -(float) M_PI + 0.25f;
    kevin->power = 14.0f;
    kevin->skin_color = rgb(255, 107, 107);
    kevin->hat_color = rgb(127, 29, 29);
    kevin->body_color = rgb(185, 28, 28);
    kevin->float_color1 = rgb(244, 63, 94);
    kevin->float_color2 = rgb(190, 18, 60);
    kevin->default_sheet = SHEET_KEVIN;

    Character *friend_back = &game->enemies[1];
    memset(friend_back, 0, sizeof(*friend_back));
    friend_back->radius = 20.0f;
    friend_back->name = "Friend";
    friend_back->angle = -0.75f;
    friend_back->display_angle = -(float) M_PI + 0.25f;
    friend_back->power = 14.0f;
    friend_back->skin_color = rgb(167, 139, 250);
    friend_back->hat_color = rgb(76, 29, 149);
    friend_back->body_color = rgb(124, 58, 237);
    friend_back->float_color1 = rgb(34, 197, 94);
    friend_back->float_color2 = rgb(21, 128, 61);
    friend_back->default_sheet = SHEET_FRIEND_BACK;

    Character *scout = &game->enemies[2];
    memset(scout, 0, sizeof(*scout));
    scout->radius = 20.0f;
    scout->name = "Scout";
    scout->angle = -0.75f;
    scout->display_angle = -(float) M_PI + 0.25f;
    scout->power = 14.0f;
    scout->skin_color = rgb(252, 165, 165);
    scout->hat_color = rgb(31, 41, 55);
    scout->body_color = rgb(51, 65, 85);
    scout->float_color1 = rgb(245, 158, 11);
    scout->float_color2 = rgb(217, 119, 6);
    scout->default_sheet = SHEET_FRIEND_FRONT;

    game->blocks[0].color = rgb(245, 158, 11);
    game->blocks[0].bob_offset = randf(0.0f, (float) M_PI * 2.0f);
    game->blocks[0].tilt_offset = randf(0.0f, (float) M_PI * 2.0f);

    game->blocks[1].color = rgb(251, 113, 133);
    game->blocks[1].bob_offset = randf(0.0f, (float) M_PI * 2.0f);
    game->blocks[1].tilt_offset = randf(0.0f, (float) M_PI * 2.0f);

    game->blocks[2].color = rgb(139, 92, 246);
    game->blocks[2].bob_offset = randf(0.0f, (float) M_PI * 2.0f);
    game->blocks[2].tilt_offset = randf(0.0f, (float) M_PI * 2.0f);
}

static void apply_character_placement(Character *character, const PlacementConfig *placement) {
    character->platform.x = placement->x;
    character->platform.floor_top = placement->float_y;
    character->platform.base_y = placement->float_y - 6.0f;
    character->platform.y = character->platform.base_y;

    if (character->on_float) {
        sync_character_to_float(character);
    } else {
        character->x = placement->x + character->rider_offset_x;
        character->y = placement->float_y - 22.0f;
    }
}

static void apply_level_config_to_scene(Game *game) {
    apply_character_placement(&game->player, &game->config.player);
    apply_character_placement(&game->player2, &game->config.player2);
    for (int i = 0; i < 3; ++i) {
        apply_character_placement(&game->enemies[i], &game->config.enemies[i]);
        game->blocks[i].x = game->config.blocks[i].x;
        game->blocks[i].y = game->config.blocks[i].y;
        game->blocks[i].width = game->config.blocks[i].width;
        game->blocks[i].height = game->config.blocks[i].height;
    }
}

static void reset_character(Character *character, const PlacementConfig *placement, int health, bool flip_float) {
    character->platform = make_platform(placement->x, placement->float_y, 110.0f, character->float_color1, character->float_color2);
    character->platform.flip_x = flip_float;
    character->health = health;
    character->max_health = health;
    character->stock_health = health;
    character->stock_max_health = health;
    character->respawn_fade_frames = 0;
    character->respawn_tube_lock_frames = 0;
    character->respawn_airborne = false;
    character->alive = true;
    character->defeated = false;
    character->vx = 0.0f;
    character->vy = 0.0f;
    character->on_float = true;
    character->scripted_doomed = false;
    character->death_cue_played = false;
    character->rider_offset_x = 0.0f;
    character->knock_timer = 0;
    character->display_angle = character->angle;
    sync_character_to_float(character);
}

static const PlacementConfig *get_player_spawn(const Game *game, const Character *player) {
    int index = get_player_index(game, player);
    if (index == 0) return &game->config.player;
    if (index == 1) return &game->config.player2;
    return NULL;
}

static void respawn_player(Game *game, Character *player) {
    const PlacementConfig *spawn = get_player_spawn(game, player);
    if (!spawn) return;

    player->platform = make_platform(spawn->x, spawn->float_y, 110.0f, player->float_color1, player->float_color2);
    player->platform.flip_x = false;
    player->health = player->max_health;
    player->alive = true;
    player->defeated = false;
    player->vx = randf(-1.0f, 1.0f);
    player->vy = 2.8f;
    player->on_float = false;
    player->scripted_doomed = false;
    player->death_cue_played = false;
    player->rider_offset_x = randf(-player->platform.width * 0.12f, player->platform.width * 0.12f);
    player->knock_timer = 0;
    player->respawn_fade_frames = PLAYER_RESPAWN_FADE_FRAMES;
    player->respawn_tube_lock_frames = 0;
    player->respawn_airborne = true;
    player->angle = -0.75f;
    player->power = 14.0f;
    player->display_angle = player->angle;
    player->x = player->platform.x + player->rider_offset_x;
    player->y = player->platform.y - 16.0f - PLAYER_RESPAWN_DROP_HEIGHT;
}

static void begin_player_respawn_drop(Game *game, int player_index, float airplane_altitude) {
    Character *player = get_player_by_index(game, player_index);
    const PlacementConfig *spawn = get_player_spawn(game, player);
    if (!player || !spawn) return;

    /* Play once when the player starts the in-air respawn drop. */
    play_chunk_on_channel_safe(game, game->assets.sfx_respawn_drop, SFX_CHANNEL_RESPAWN_DROP, 0, MIX_MAX_VOLUME);
    respawn_player(game, player);
    player->x = spawn->x;
    player->rider_offset_x = 0.0f;
    player->vx = randf(-0.28f, 0.28f);
    player->vy = 0.42f;
    player->y = airplane_altitude + AIRPLANE_RESPAWN_DROP_Y_OFFSET;
    game->active_player_index = player_index;
}

static void update_respawn_sequence(Game *game) {
    if (!game->respawn_sequence.active) return;

    Character *player = get_player_by_index(game, game->respawn_sequence.player_index);
    const PlacementConfig *spawn = get_player_spawn(game, player);
    float margin = get_airplane_offscreen_margin(game);
    float left_edge = -margin;
    float right_edge = WORLD_WIDTH + margin;

    if (!player || !spawn) {
        game->respawn_sequence.active = false;
        return;
    }

    if (game->respawn_sequence.stage == RESPAWN_FLIGHT_TO_LEFT) {
        game->airplane_facing_right = false;
        game->airplane_x -= AIRPLANE_RESPAWN_SPEED;
        if (game->airplane_x <= left_edge) {
            game->airplane_x = left_edge;
            game->respawn_sequence.stage = RESPAWN_FLIGHT_TO_RIGHT;
            game->airplane_facing_right = true;
        }
        return;
    }

    game->airplane_facing_right = true;
    game->airplane_x += AIRPLANE_RESPAWN_SPEED;

    if (!game->respawn_sequence.drop_started && game->airplane_x >= spawn->x) {
        begin_player_respawn_drop(game, game->respawn_sequence.player_index, game->respawn_sequence.flight_altitude);
        game->respawn_sequence.drop_started = true;
    }

    if (game->airplane_x >= right_edge) {
        game->airplane_x = right_edge;
        if (!game->respawn_sequence.drop_started) {
            begin_player_respawn_drop(game, game->respawn_sequence.player_index, game->respawn_sequence.flight_altitude);
            game->respawn_sequence.drop_started = true;
        }
        game->respawn_sequence.active = false;
    }
}

static void start_respawn_sequence(Game *game, Character *player) {
    int player_index = get_player_index(game, player);
    const PlacementConfig *spawn = get_player_spawn(game, player);
    if (player_index < 0) return;

    game->respawn_sequence.active = true;
    game->respawn_sequence.player_index = player_index;
    game->respawn_sequence.stage = RESPAWN_FLIGHT_TO_LEFT;
    game->respawn_sequence.drop_started = false;
    game->respawn_sequence.flight_altitude = clampf_local(
        game->config.airplane.altitude + AIRPLANE_RESPAWN_ALTITUDE_OFFSET,
        72.0f,
        FLOOR_Y - 120.0f
    );
    game->airplane_facing_right = false;

    /* Play once when the high-speed airplane respawn sequence starts. */
    play_chunk_on_channel_safe(game, game->assets.sfx_airplane_start_move, SFX_CHANNEL_RESPAWN_START, 0, MIX_MAX_VOLUME);

    if (spawn) {
        player->platform = make_platform(spawn->x, spawn->float_y, 110.0f, player->float_color1, player->float_color2);
        player->platform.flip_x = false;
    }
    player->alive = false;
    player->defeated = false;
    player->health = 0;
    player->on_float = false;
    player->respawn_airborne = false;
    player->respawn_tube_lock_frames = 0;
    player->respawn_fade_frames = 0;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->rider_offset_x = 0.0f;
}

static void reset_game(Game *game) {
    int player1_start_health = game->player_start_health[0] > 0
        ? game->player_start_health[0]
        : LEVEL5_DEFAULT_PLAYER_HEALTH_POINTS;
    int player2_start_health = game->player_start_health[1] > 0
        ? game->player_start_health[1]
        : player1_start_health;

    player1_start_health = clamp_player_health_points(player1_start_health);
    player2_start_health = clamp_player_health_points(player2_start_health);

    reset_character(&game->player, &game->config.player, PLAYER_COMBAT_HEALTH, false);
    game->player.stock_health = player1_start_health;
    game->player.stock_max_health = player1_start_health;
    game->player.angle = -0.75f;
    game->player.power = 14.0f;
    game->player.display_angle = game->player.angle;

    if (game->duo_mode) {
        reset_character(&game->player2, &game->config.player2, PLAYER_COMBAT_HEALTH, false);
        game->player2.stock_health = player2_start_health;
        game->player2.stock_max_health = player2_start_health;
        game->player2.angle = -0.75f;
        game->player2.power = 14.0f;
        game->player2.display_angle = game->player2.angle;
    } else {
        game->player2.health = 0;
        game->player2.max_health = PLAYER_COMBAT_HEALTH;
        game->player2.stock_health = 0;
        game->player2.stock_max_health = 0;
        game->player2.respawn_fade_frames = 0;
        game->player2.respawn_tube_lock_frames = 0;
        game->player2.respawn_airborne = false;
        game->player2.alive = false;
        game->player2.defeated = true;
        game->player2.on_float = false;
        game->player2.vx = 0.0f;
        game->player2.vy = 0.0f;
        game->player2.rider_offset_x = 0.0f;
        game->player2.knock_timer = 0;
    }

    reset_character(&game->enemies[0], &game->config.enemies[0], 65, true);
    reset_character(&game->enemies[1], &game->config.enemies[1], 65, true);
    reset_character(&game->enemies[2], &game->config.enemies[2], 50, true);
    apply_level_config_to_scene(game);

    memset(&game->projectile, 0, sizeof(game->projectile));
    game->projectile.radius = 8.0f;
    game->current_turn = TURN_ENEMY;
    game->game_state = GAME_ENEMY_THINKING;
    game->enemy_shoot_timer = 45;
    game->enemy_turn_index = 0;
    game->enemy_miss_streak = 0;
    game->enemy_shot_hit_current = false;
    game->active_player_index = 0;
    game->camera_x = 0.0f;
    game->win_animation_start = -1;
    game->win_audio_played = false;
    game->opening_shot_done = false;
    game->kevin_throw_until = 0;
    game->among_focus_until = 0;
    game->pending_among_reveal = false;
    game->mouse_aim.active = false;
    game->splash_count = 0;
    game->outcome_frame = -1;
    game->skip_key_count = 0;
    game->throw_key_count = 0;
    game->respawn_sequence.active = false;
    game->respawn_sequence.player_index = 0;
    game->respawn_sequence.stage = RESPAWN_FLIGHT_TO_LEFT;
    game->respawn_sequence.drop_started = false;
    game->respawn_sequence.flight_altitude = clampf_local(
        game->config.airplane.altitude + AIRPLANE_RESPAWN_ALTITUDE_OFFSET,
        72.0f,
        FLOOR_Y - 120.0f
    );

    for (int i = 0; i < 3; ++i) {
        game->enemies[i].display_angle = -(float) M_PI + 0.25f;
    }

    ensure_active_player(game);
}

static int living_enemy_count(const Game *game) {
    int count = 0;
    for (int i = 0; i < 3; ++i) {
        if (game->enemies[i].alive && !game->enemies[i].defeated) ++count;
    }
    return count;
}

static Character *get_current_enemy_shooter(Game *game) {
    int living = living_enemy_count(game);
    if (living == 0) return NULL;

    game->enemy_turn_index %= living;
    int seen = 0;
    for (int i = 0; i < 3; ++i) {
        if (game->enemies[i].alive && !game->enemies[i].defeated) {
            if (seen == game->enemy_turn_index) return &game->enemies[i];
            ++seen;
        }
    }
    return NULL;
}

static void advance_enemy_turn(Game *game) {
    int living = living_enemy_count(game);
    if (living == 0) {
        game->enemy_turn_index = 0;
        return;
    }
    game->enemy_turn_index = (game->enemy_turn_index + 1) % living;
}

static float get_cinematic_zoom_scale(const Game *game) {
    return game->time_frames < game->among_focus_until ? 1.35f : 1.0f;
}

static float get_view_scale_x(const Game *game) {
    return game->admin_mode ? ((float) VIEW_WIDTH / WORLD_WIDTH) : get_cinematic_zoom_scale(game);
}

static float get_view_scale_y(const Game *game) {
    return game->admin_mode ? ((float) VIEW_HEIGHT / WORLD_HEIGHT) : get_cinematic_zoom_scale(game);
}

static float get_view_offset_x(const Game *game) {
    if (game->admin_mode) return 0.0f;
    float scale = get_view_scale_x(game);
    return ((float) VIEW_WIDTH - (float) VIEW_WIDTH * scale) * 0.5f;
}

static float get_view_offset_y(const Game *game) {
    if (game->admin_mode) return 0.0f;
    float scale = get_view_scale_y(game);
    return ((float) VIEW_HEIGHT - (float) VIEW_HEIGHT * scale) * 0.5f;
}

static float world_to_screen_x(const Game *game, float x) {
    return x - game->camera_x;
}

static float world_to_view_x(const Game *game, float x) {
    if (game->admin_mode) return x * get_view_scale_x(game);
    return world_to_screen_x(game, x) * get_view_scale_x(game) + get_view_offset_x(game);
}

static float world_to_view_y(const Game *game, float y) {
    if (game->admin_mode) return y * get_view_scale_y(game);
    return y * get_view_scale_y(game) + get_view_offset_y(game);
}

static Vec2 view_to_world(const Game *game, float x, float y) {
    Vec2 out;
    if (game->admin_mode) {
        out.x = x / get_view_scale_x(game);
        out.y = y / get_view_scale_y(game);
        return out;
    }
    float scale = get_view_scale_x(game);
    out.x = (x - get_view_offset_x(game)) / scale + game->camera_x;
    out.y = (y - get_view_offset_y(game)) / scale;
    return out;
}

static float scale_length(const Game *game, float value) {
    return value * get_view_scale_x(game);
}

static float scale_height(const Game *game, float value) {
    return value * get_view_scale_y(game);
}

static SpriteSheet *get_sheet(Game *game, SheetId id) {
    return &game->assets.sheets[id];
}

static int get_looped_sheet_frame(const Game *game, const SpriteSheet *sheet, float pause_seconds, float play_seconds) {
    int pause_frames = clampi_local((int) lroundf(pause_seconds * 60.0f), 1, 100000);
    int play_frames = clampi_local((int) lroundf(play_seconds * 60.0f), 1, 100000);
    int cycle_frames = pause_frames + play_frames;
    int cycle_frame = game->time_frames % cycle_frames;

    if (cycle_frame < pause_frames) return 0;

    int animation_frame = cycle_frame - pause_frames;
    int step = clampi_local((int) lroundf(60.0f / (float) sheet->fps), 1, 60);
    return (animation_frame / step) % sheet->frame_count;
}

static int get_sequential_sheet_frame(const Game *game, const SpriteSheet *sheet, int start_frame) {
    int elapsed = game->time_frames - start_frame;
    if (elapsed < 0) elapsed = 0;
    int step = clampi_local((int) lroundf(60.0f / (float) sheet->fps), 1, 60);
    return clampi_local(elapsed / step, 0, sheet->frame_count - 1);
}

static SheetId player_idle_sheet_id(const Game *game, int player_index) {
    int skin = resolve_player_skin_number(game, player_index);
    return skin == 2 ? SHEET_BURGLAR2 : SHEET_BURGLAR;
}

static SheetId player_happy_sheet_id(const Game *game, int player_index) {
    int skin = resolve_player_skin_number(game, player_index);
    return skin == 2 ? SHEET_BURGLAR2_HAPPY : SHEET_BURGLAR_HAPPY;
}

static SheetId player_smoke_sheet_id(const Game *game, int player_index) {
    int skin = resolve_player_skin_number(game, player_index);
    return skin == 2 ? SHEET_BURGLAR2_SMOKE : SHEET_BURGLAR_SMOKE;
}

static const TextureAsset *player_respawn_sheet(const Game *game, const Character *character) {
    int player_index = 0;
    int skin = 1;
    if (!game || !character) return NULL;
    if (game->duo_mode && character == &game->player2) player_index = 1;
    skin = resolve_player_skin_number(game, player_index);
    if (skin == 2 && game->assets.player_respawn_skin2.texture) return &game->assets.player_respawn_skin2;
    if (game->assets.player_respawn_skin1.texture) return &game->assets.player_respawn_skin1;
    if (game->assets.player_respawn_skin2.texture) return &game->assets.player_respawn_skin2;
    return NULL;
}

static SheetState get_player_sheet_state(Game *game, int player_index) {
    if (game->game_state == GAME_WON && game->win_animation_start >= 0) {
        SpriteSheet *sheet = get_sheet(game, player_happy_sheet_id(game, player_index));
        SheetState state = {sheet, get_sequential_sheet_frame(game, sheet, game->win_animation_start)};
        return state;
    }

    const int trigger_frames = 20 * 60;
    SpriteSheet *smoke = get_sheet(game, player_smoke_sheet_id(game, player_index));
    int smoke_duration = smoke->frame_count * clampi_local((int) lroundf(60.0f / (float) smoke->fps), 1, 60);
    int trigger_offset = game->time_frames - trigger_frames;

    if (trigger_offset >= 0 && (trigger_offset % trigger_frames) < smoke_duration) {
        int start_frame = game->time_frames - (trigger_offset % trigger_frames);
        SheetState state = {smoke, get_sequential_sheet_frame(game, smoke, start_frame)};
        return state;
    }

    SpriteSheet *idle = get_sheet(game, player_idle_sheet_id(game, player_index));
    int step = clampi_local((int) lroundf(60.0f / (float) idle->fps), 1, 60);
    SheetState state = {idle, (game->time_frames / step) % idle->frame_count};
    return state;
}

static void play_death_cue(Game *game, Character *character) {
    if (is_player_character(game, character)) return;
    if (character->death_cue_played) return;
    character->death_cue_played = true;
    play_chunk_safe(game, game->assets.sfx_oh_no, 0);
}

static void update_camera(Game *game) {
    if (game->admin_mode) {
        game->camera_x = 0.0f;
        return;
    }

    Character *focus_player = get_active_player(game);
    if (!focus_player) focus_player = &game->player;
    float target_x = focus_player->x - VIEW_WIDTH * 0.3f;
    float zoom = get_cinematic_zoom_scale(game);

    if (game->respawn_sequence.active) {
        target_x = game->airplane_x - ((float) VIEW_WIDTH / zoom) * 0.5f;
    } else if (game->pending_among_reveal && game->enemies[2].alive && game->enemies[2].defeated) {
        target_x = game->enemies[2].x - ((float) VIEW_WIDTH / zoom) * 0.5f;
    } else if (game->time_frames < game->among_focus_until) {
        target_x = game->enemies[0].x - ((float) VIEW_WIDTH / zoom) * 0.5f;
    } else if (game->projectile.active) {
        target_x = game->projectile.x - VIEW_WIDTH * 0.5f;
    } else if (game->current_turn == TURN_ENEMY && game->game_state == GAME_ENEMY_THINKING) {
        Character *shooter = get_current_enemy_shooter(game);
        if (shooter) target_x = shooter->x - VIEW_WIDTH * 0.5f;
    }

    game->camera_x += (target_x - game->camera_x) * 0.1f;
    game->camera_x = clampf_local(game->camera_x, 0.0f, WORLD_WIDTH - VIEW_WIDTH);
}

static bool circle_hit(const Projectile *projectile, const Character *character) {
    float dx = projectile->x - character->x;
    float dy = projectile->y - character->y;
    float limit = projectile->radius + character->radius;
    return dx * dx + dy * dy < limit * limit;
}

static bool projectile_hits_block(const Projectile *projectile, const Block *block) {
    return projectile->x + projectile->radius > block->x &&
           projectile->x - projectile->radius < block->x + block->width &&
           projectile->y + projectile->radius > block->y &&
           projectile->y - projectile->radius < block->y + block->height;
}

static void update_projectile_power(Projectile *projectile) {
    projectile->current_power = lengthf(projectile->vx, projectile->vy);
}

static void weaken_projectile_after_bounce(Projectile *projectile) {
    projectile->vx *= BOUNCE_POWER_LOSS;
    projectile->vy *= BOUNCE_POWER_LOSS;
    update_projectile_power(projectile);
}

static void bounce_from_block(Projectile *projectile, const Block *block) {
    float overlap_left = (projectile->x + projectile->radius) - block->x;
    float overlap_right = (block->x + block->width) - (projectile->x - projectile->radius);
    float overlap_top = (projectile->y + projectile->radius) - block->y;
    float overlap_bottom = (block->y + block->height) - (projectile->y - projectile->radius);
    float min_overlap = overlap_left;
    if (overlap_right < min_overlap) min_overlap = overlap_right;
    if (overlap_top < min_overlap) min_overlap = overlap_top;
    if (overlap_bottom < min_overlap) min_overlap = overlap_bottom;

    if (min_overlap == overlap_left) {
        projectile->x = block->x - projectile->radius;
        projectile->vx = -fabsf(projectile->vx) * 0.75f;
    } else if (min_overlap == overlap_right) {
        projectile->x = block->x + block->width + projectile->radius;
        projectile->vx = fabsf(projectile->vx) * 0.75f;
    } else if (min_overlap == overlap_top) {
        projectile->y = block->y - projectile->radius;
        projectile->vy = -fabsf(projectile->vy) * 0.72f;
        projectile->vx *= 0.93f;
    } else {
        projectile->y = block->y + block->height + projectile->radius;
        projectile->vy = fabsf(projectile->vy) * 0.72f;
    }

    projectile->bounces += 1;
    weaken_projectile_after_bounce(projectile);
}

static void begin_character_fall(Game *game, Character *character, float vx, float vy, bool custom_launch) {
    if (!character->alive || character->defeated) return;

    bool was_scripted_victim = (character == &game->enemies[2]) && character->scripted_doomed;
    character->defeated = true;
    character->health = 0;
    character->on_float = false;
    character->scripted_doomed = false;
    character->rider_offset_x = 0.0f;
    character->respawn_tube_lock_frames = 0;
    if (custom_launch) {
        character->vx = vx;
        character->vy = vy;
    } else {
        character->vy = fminf(character->vy, -4.2f);
    }
    if (character->knock_timer < 18) character->knock_timer = 18;
    play_death_cue(game, character);

    if (was_scripted_victim) game->pending_among_reveal = true;

    if (is_player_character(game, character)) {
        ensure_active_player(game);
    } else if (living_enemy_count(game) == 0) {
        game->game_state = GAME_WON;
        if (game->win_animation_start < 0) game->win_animation_start = game->time_frames;
        if (!game->win_audio_played) {
            play_chunk_safe(game, game->assets.sfx_win, 0);
            game->win_audio_played = true;
        }
    }
}

static void kill_character(Game *game, Character *character) {
    if (!character->alive) return;
    play_chunk_safe(game, game->assets.sfx_splash, 0);

    if (is_player_character(game, character)) {
        if (character->stock_health > 0) character->stock_health--;
        if (character->stock_health < 0) character->stock_health = 0;
        if (character->stock_health > 0) {
            start_respawn_sequence(game, character);
            ensure_active_player(game);
            return;
        }
    }

    character->alive = false;
    character->defeated = true;
    character->health = 0;
    character->on_float = false;
    character->scripted_doomed = false;
    character->vx = 0.0f;
    character->vy = 0.0f;
    character->respawn_tube_lock_frames = 0;
    character->knock_timer = 0;

    if (is_player_character(game, character)) {
        ensure_active_player(game);
        if (alive_player_count(game) == 0 && !game->respawn_sequence.active) game->game_state = GAME_LOST;
    }

    if (game->pending_among_reveal && character == &game->enemies[2]) {
        game->pending_among_reveal = false;
        game->camera_x = clampf_local(game->enemies[0].x - ((float) VIEW_WIDTH / get_cinematic_zoom_scale(game)) * 0.5f, 0.0f, WORLD_WIDTH - VIEW_WIDTH);
        play_chunk_safe(game, game->assets.sfx_among, 0);
        game->among_focus_until = game->time_frames + (int) lroundf(2.2f * 60.0f);
    }
}

static HitForce compute_hit_force(const Character *target, float hit_x, float hit_y, float incoming_vx, float shot_power) {
    float dir_x = incoming_vx == 0.0f ? 0.0f : (incoming_vx > 0.0f ? 1.0f : -1.0f);
    float rel_y = (target->y - hit_y) / target->radius;
    float rel_x = (hit_x - target->x) / target->radius;

    HitForce hit;
    hit.height_factor = clampf_local(1.0f + (-rel_y) * 0.55f, 0.75f, 1.55f);
    float side_factor = clampf_local(1.0f + fabsf(rel_x) * 0.3f, 1.0f, 1.3f);
    hit.horizontal_force = dir_x * (shot_power * DIRECT_HIT_FORCE_SCALE * hit.height_factor * side_factor);
    hit.upward_force = -(1.2f + shot_power * 0.16f * hit.height_factor);
    return hit;
}

static void apply_direct_hit(Game *game, Character *target, float hit_x, float hit_y) {
    if (!target->alive || target->defeated) return;

    float shot_power = clampf_local(game->projectile.current_power, 2.0f, 30.0f);
    HitForce hit = compute_hit_force(target, hit_x, hit_y, game->projectile.vx, shot_power);
    int damage = clampi_local((int) lroundf(shot_power * DIRECT_HIT_DAMAGE_SCALE * hit.height_factor * 0.55f), 8, 60);
    bool force_player_fall = is_player_character(game, target)
        && (game->projectile.owner == OWNER_ENEMY || game->projectile.owner == OWNER_SCRIPTED_ENEMY);

    if (force_player_fall) {
        float throw_x = hit.horizontal_force;
        float throw_y = fminf(hit.upward_force - 2.2f, -8.4f);

        if (fabsf(throw_x) < 8.5f) {
            throw_x = throw_x >= 0.0f ? 8.5f : -8.5f;
        }
        begin_character_fall(game, target, target->vx + throw_x, target->vy + throw_y, true);
        return;
    }

    target->health -= damage;
    target->vx += hit.horizontal_force;
    target->vy += hit.upward_force;
    target->knock_timer = 22;

    if (target->on_float) {
        target->platform.tilt += (hit.horizontal_force >= 0.0f ? 1.0f : -1.0f) * clampf_local(fabsf(hit.horizontal_force) * 0.02f, 0.05f, 0.22f);
        target->rider_offset_x += hit.horizontal_force * 0.48f;
    }

    if (target->health <= 0) {
        begin_character_fall(game, target, target->vx + (hit.horizontal_force >= 0.0f ? 0.9f : -0.9f), fminf(target->vy, hit.upward_force - 1.4f), true);
        return;
    }

    if (fabsf(target->rider_offset_x) > target->platform.width * 0.30f || fabsf(hit.horizontal_force) >= 11.5f) {
        target->on_float = false;
        play_death_cue(game, target);
        target->vx += (hit.horizontal_force >= 0.0f ? 0.8f : -0.8f);
        target->vy = fminf(target->vy, hit.upward_force - 0.8f);
    }
}

static void apply_scripted_knock_to_water(Game *game, Character *target) {
    if (!target->alive) return;
    target->scripted_doomed = true;
    target->platform.tilt = -0.22f;
    begin_character_fall(game, target, -12.0f, -8.6f, true);
}

static void end_projectile_turn(Game *game) {
    ProjectileOwner owner = game->projectile.owner;

    if (owner == OWNER_ENEMY && !game->enemy_shot_hit_current) {
        game->enemy_miss_streak = clampi_local(game->enemy_miss_streak + 1, 0, 99);
    }
    game->enemy_shot_hit_current = false;

    game->projectile.active = false;
    game->projectile.vx = 0.0f;
    game->projectile.vy = 0.0f;
    game->projectile.base_power = 0.0f;
    game->projectile.current_power = 0.0f;
    game->projectile.scripted_target = NULL;

    if (game->game_state == GAME_WON || game->game_state == GAME_LOST) {
        game->projectile.owner = OWNER_NONE;
        return;
    }

    if (owner == OWNER_PLAYER) {
        advance_active_player(game);
        game->current_turn = TURN_ENEMY;
        game->game_state = GAME_ENEMY_THINKING;
        game->enemy_shoot_timer = 55;
    } else {
        advance_enemy_turn(game);
        ensure_active_player(game);
        game->current_turn = TURN_PLAYER;
        game->game_state = GAME_AIMING;
    }

    game->projectile.owner = OWNER_NONE;
}

static void fire_projectile(Game *game, float x, float y, float angle, float power, ProjectileOwner owner) {
    play_chunk_safe(game, game->assets.sfx_fire, 0);
    game->projectile.active = true;
    game->projectile.x = x + cosf(angle) * 42.0f;
    game->projectile.y = y + sinf(angle) * 42.0f;
    game->projectile.vx = cosf(angle) * power;
    game->projectile.vy = sinf(angle) * power;
    game->projectile.bounces = 0;
    game->projectile.owner = owner;
    game->projectile.scripted_target = NULL;
    game->projectile.base_power = power;
    game->projectile.current_power = power;
    game->game_state = GAME_PROJECTILE;
}

static void create_water_splash(Game *game, float x, float y) {
    for (int i = 0; i < 16 && game->splash_count < MAX_SPLASHES; ++i) {
        SplashParticle *splash = &game->splashes[game->splash_count++];
        splash->x = x;
        splash->y = y - 2.0f;
        splash->vx = randf(-3.8f, 3.8f);
        splash->vy = randf(-6.5f, -1.6f);
        splash->life = randf(18.0f, 34.0f);
        splash->size = randf(3.0f, 8.0f);
        splash->alpha = randf(0.5f, 0.95f);
    }
}

static void update_player_aim(Game *game, float view_x, float view_y) {
    Character *active = get_active_player(game);
    if (!active || !active->alive || active->defeated) return;

    Vec2 pointer_world = view_to_world(game, view_x, view_y);
    float dx = pointer_world.x - active->x;
    float dy = pointer_world.y - active->y;
    float distance = lengthf(dx, dy);
    if (distance < 8.0f) return;

    active->angle = clampf_local(atan2f(dy, dx), PLAYER_AIM_MIN_ANGLE, PLAYER_AIM_MAX_ANGLE);
    float t = (clampf_local(distance, 36.0f, PLAYER_MAX_DRAG_DISTANCE) - 36.0f) / (PLAYER_MAX_DRAG_DISTANCE - 36.0f);
    active->power = clampf_local(PLAYER_MIN_POWER + t * (PLAYER_MAX_POWER - PLAYER_MIN_POWER), PLAYER_MIN_POWER, PLAYER_MAX_POWER);
}

static bool can_use_mouse_aim(const Game *game) {
    if (game->duo_mode) return false;
    const Character *active = get_player_by_index_const(game, game->active_player_index);
    return !game->admin_mode &&
           !game->respawn_sequence.active &&
           game->intro_state == INTRO_DONE &&
           game->current_turn == TURN_PLAYER &&
           game->game_state == GAME_AIMING &&
           active &&
           active->alive &&
           !active->defeated;
}

static void shoot_player(Game *game) {
    Character *active = get_active_player(game);
    if (!active || !active->alive || active->defeated) return;
    if (game->current_turn != TURN_PLAYER) return;
    if (game->game_state != GAME_AIMING) return;
    if (game->projectile.active) return;
    fire_projectile(game, active->x, active->y, active->angle, active->power, OWNER_PLAYER);
}

static void estimate_enemy_shot(const Character *enemy, const Character *target, float *out_angle, float *out_power) {
    float dx = target->x - enemy->x;
    float dy = target->y - enemy->y;
    float direction = dx >= 0.0f ? 1.0f : -1.0f;

    float power = clampf_local(fabsf(dx) / 75.0f + 7.5f, 8.0f, 18.0f);
    float angle = direction > 0.0f ? -0.72f : -2.42f;

    if (fabsf(dx) > 700.0f) {
        power += 1.2f;
        angle += direction > 0.0f ? -0.08f : 0.08f;
    }
    if (dy < -25.0f) {
        angle += direction > 0.0f ? -0.06f : 0.06f;
    }

    *out_angle = angle;
    *out_power = power;
}

static void shoot_enemy(Game *game) {
    Character *enemy = get_current_enemy_shooter(game);
    Character *target = get_enemy_target_player(game);
    int miss_streak = clampi_local(game->enemy_miss_streak, 0, 99);
    if (!enemy || !target) {
        if (alive_player_count(game) == 0) {
            game->game_state = GAME_LOST;
            return;
        }
        game->current_turn = TURN_PLAYER;
        game->game_state = GAME_AIMING;
        ensure_active_player(game);
        return;
    }

    if (!game->opening_shot_done && enemy == &game->enemies[0] && game->enemies[2].alive) {
        game->opening_shot_done = true;
        game->kevin_throw_until = game->time_frames + 28;
        enemy->display_angle = -2.92f;
        fire_projectile(game, enemy->x, enemy->y, -2.92f, 11.2f, OWNER_SCRIPTED_ENEMY);
        game->projectile.scripted_target = &game->enemies[2];
        return;
    }

    float aim_angle = 0.0f;
    float aim_power = 0.0f;
    estimate_enemy_shot(enemy, target, &aim_angle, &aim_power);

    bool force_good_shot = miss_streak >= 3;
    float adjustment = clampf_local((float) miss_streak / 3.0f, 0.0f, 1.0f);
    float miss_chance = force_good_shot ? 0.0f : fmaxf(0.12f, 0.30f - 0.06f * (float) miss_streak);
    bool should_miss = randf(0.0f, 1.0f) < miss_chance;
    float miss_angle_span = 0.15f - 0.05f * adjustment;
    float miss_power_span = 2.6f - 1.0f * adjustment;
    float hit_angle_span = force_good_shot ? 0.014f : (0.04f - 0.02f * adjustment);
    float hit_power_span = force_good_shot ? 0.30f : (0.75f - 0.35f * adjustment);
    float angle_error = should_miss ? randf(-miss_angle_span, miss_angle_span) : randf(-hit_angle_span, hit_angle_span);
    float power_error = should_miss ? randf(-miss_power_span, miss_power_span) : randf(-hit_power_span, hit_power_span);
    float angle = aim_angle + angle_error;
    float power = clampf_local(aim_power + power_error, 7.0f, 20.0f);
    enemy->display_angle = angle;

    game->enemy_shot_hit_current = false;
    fire_projectile(game, enemy->x, enemy->y, angle, power, OWNER_ENEMY);
}

static void update_enemy_turn(Game *game) {
    if (game->current_turn != TURN_ENEMY || game->game_state != GAME_ENEMY_THINKING) return;

    Character *shooter = get_current_enemy_shooter(game);
    Character *target = get_enemy_target_player(game);
    if (shooter && target) {
        float aim_angle = 0.0f;
        float aim_power = 0.0f;
        estimate_enemy_shot(shooter, target, &aim_angle, &aim_power);
        (void) aim_power;
        shooter->display_angle += (aim_angle - shooter->display_angle) * 0.14f;
    }

    game->enemy_shoot_timer--;
    if (game->enemy_shoot_timer <= 0) shoot_enemy(game);
}

static void update_projectile(Game *game) {
    if (!game->projectile.active) return;

    game->projectile.x += game->projectile.vx;
    game->projectile.y += game->projectile.vy;
    game->projectile.vy += GRAVITY;
    game->projectile.vx *= 0.998f;
    update_projectile_power(&game->projectile);

    if (game->projectile.owner == OWNER_SCRIPTED_ENEMY && game->projectile.scripted_target && game->projectile.scripted_target->alive) {
        Character *target = game->projectile.scripted_target;
        if (game->projectile.x - game->projectile.radius <= target->x + target->radius) {
            apply_scripted_knock_to_water(game, target);
            end_projectile_turn(game);
            return;
        }
    }

    if (game->projectile.y + game->projectile.radius >= FLOOR_Y) {
        create_water_splash(game, game->projectile.x, FLOOR_Y);
        play_chunk_safe(game, game->assets.sfx_splash, 0);
        end_projectile_turn(game);
        return;
    }

    if (game->projectile.x - game->projectile.radius <= 0.0f || game->projectile.x + game->projectile.radius >= WORLD_WIDTH) {
        end_projectile_turn(game);
        return;
    }

    for (int i = 0; i < 3; ++i) {
        if (projectile_hits_block(&game->projectile, &game->blocks[i])) {
            play_chunk_safe(game, game->assets.sfx_fire, 0);
            bounce_from_block(&game->projectile, &game->blocks[i]);
        }
    }

    if (game->projectile.owner == OWNER_ENEMY) {
        int players = configured_player_count(game);
        for (int i = 0; i < players; ++i) {
            Character *player = get_player_by_index(game, i);
            if (!player || !player->alive || player->defeated) continue;
            if (circle_hit(&game->projectile, player)) {
                game->enemy_shot_hit_current = true;
                game->enemy_miss_streak = 0;
                apply_direct_hit(game, player, game->projectile.x, game->projectile.y);
                end_projectile_turn(game);
                return;
            }
        }
    }

    for (int i = 0; i < 3; ++i) {
        Character *enemy = &game->enemies[i];
        if (!enemy->alive || enemy->defeated) continue;
        if ((game->projectile.owner == OWNER_PLAYER || game->projectile.owner == OWNER_SCRIPTED_ENEMY) && circle_hit(&game->projectile, enemy)) {
            if (game->projectile.owner == OWNER_SCRIPTED_ENEMY && enemy == &game->enemies[2]) {
                apply_scripted_knock_to_water(game, enemy);
                end_projectile_turn(game);
                return;
            }
            apply_direct_hit(game, enemy, game->projectile.x, game->projectile.y);
            end_projectile_turn(game);
            return;
        }
    }

    if (game->projectile.bounces >= MAX_BOUNCES ||
        game->projectile.current_power <= 2.2f ||
        (fabsf(game->projectile.vx) < MIN_STOP_SPEED && fabsf(game->projectile.vy) < MIN_STOP_SPEED)) {
        end_projectile_turn(game);
    }
}

static void update_float(Platform *platform, int time_frames) {
    platform->base_y = platform->floor_top - 6.0f;
    platform->y = platform->base_y + sinf(time_frames * 0.03f + platform->bob_offset) * 4.0f;
    platform->tilt *= 0.92f;
    platform->tilt = clampf_local(platform->tilt, -0.25f, 0.25f);
    platform->x += platform->drift;
    platform->x = clampf_local(platform->x, 55.0f, WORLD_WIDTH - 55.0f);
}

static void update_all_floats(Game *game) {
    update_float(&game->player.platform, game->time_frames);
    if (game->duo_mode) update_float(&game->player2.platform, game->time_frames);
    for (int i = 0; i < 3; ++i) update_float(&game->enemies[i].platform, game->time_frames);
}

static void check_float_support(Game *game, Character *character) {
    if (!character->alive || !character->on_float) return;
    if (is_player_character(game, character) && character->respawn_tube_lock_frames > 0) return;
    float half_support = character->platform.width * 0.30f;
    if (fabsf(character->rider_offset_x) > half_support) {
        character->on_float = false;
        play_death_cue(game, character);
        character->vx += (character->rider_offset_x >= 0.0f ? 1.2f : -1.2f);
        character->vy = fmaxf(character->vy, 1.8f);
    }
}

static void update_character_physics(Game *game, Character *character, bool skip_float_update) {
    if (!character->alive) return;

    if (!skip_float_update && character->on_float) update_float(&character->platform, game->time_frames);

    if (character->on_float) {
        if (is_player_character(game, character) && character->respawn_tube_lock_frames > 0) {
            character->vx = 0.0f;
            character->vy = 0.0f;
            character->rider_offset_x = 0.0f;
            sync_character_to_float(character);
        } else {
            character->rider_offset_x += character->vx;
            character->vx *= 0.84f;
            character->vy *= 0.55f;
            character->rider_offset_x = clampf_local(character->rider_offset_x, -character->platform.width * 0.50f, character->platform.width * 0.50f);
            sync_character_to_float(character);
            check_float_support(game, character);
        }
    } else {
        character->x += character->vx;
        character->y += character->vy;
        if (is_player_character(game, character) && character->respawn_airborne) {
            character->vy += PLAYER_RESPAWN_FALL_GRAVITY;
            character->vy = fminf(character->vy, PLAYER_RESPAWN_FALL_MAX_VY);
            character->vx *= 0.965f;
        } else {
            character->vy += 0.32f;
            character->vx *= 0.985f;
        }

        if (is_player_character(game, character) && character->respawn_airborne) {
            /*
             * Snap the player back onto the float as soon as their feet reach
             * the tube. Waiting for the center point to drop lower can let the
             * water kill check fire first when the float is bobbing near the
             * waterline.
             */
            float attach_y = character->platform.y - character->radius;
            if (character->y >= attach_y) {
                attach_player_to_float_after_respawn(character);
            }
        }

        if (!character->on_float && character->y + character->radius >= FLOOR_Y) {
            kill_character(game, character);
            return;
        }
    }

    character->x = clampf_local(character->x, 0.0f, WORLD_WIDTH);
    if (character->respawn_fade_frames > 0) character->respawn_fade_frames--;
    if (character->respawn_tube_lock_frames > 0) character->respawn_tube_lock_frames--;
    if (character->knock_timer > 0) character->knock_timer--;
}

static void update_characters(Game *game) {
    int players = configured_player_count(game);
    for (int i = 0; i < players; ++i) {
        Character *player = get_player_by_index(game, i);
        if (!player) continue;
        player->display_angle = player->angle;
        if (player->alive) update_character_physics(game, player, true);
    }

    for (int i = 0; i < 3; ++i) {
        if (game->enemies[i].alive) update_character_physics(game, &game->enemies[i], true);
    }

    Character *current_enemy = get_current_enemy_shooter(game);
    for (int i = 0; i < 3; ++i) {
        Character *enemy = &game->enemies[i];
        if (!enemy->alive) continue;
        if (!(game->current_turn == TURN_ENEMY && game->game_state == GAME_ENEMY_THINKING && enemy == current_enemy)) {
            enemy->display_angle += ((-(float) M_PI + 0.25f) - enemy->display_angle) * 0.08f;
        }
    }

    if (alive_player_count(game) == 0 && game->game_state != GAME_LOST && !game->respawn_sequence.active) game->game_state = GAME_LOST;
    if (living_enemy_count(game) == 0 && game->game_state != GAME_WON) {
        game->game_state = GAME_WON;
        if (game->win_animation_start < 0) game->win_animation_start = game->time_frames;
        if (!game->win_audio_played) {
            play_chunk_safe(game, game->assets.sfx_win, 0);
            game->win_audio_played = true;
        }
    }
}

static void update_splash_effects(Game *game) {
    for (int i = 0; i < game->splash_count;) {
        SplashParticle *splash = &game->splashes[i];
        splash->x += splash->vx;
        splash->y += splash->vy;
        splash->vy += 0.24f;
        splash->vx *= 0.97f;
        splash->life -= 1.0f;

        if (splash->life <= 0.0f) {
            game->splashes[i] = game->splashes[game->splash_count - 1];
            game->splash_count--;
            continue;
        }
        ++i;
    }
}

static void update_game_step(Game *game) {
    game->time_frames++;
    update_airplane_motion(game);

    if (game->intro_state != INTRO_DONE) {
        int elapsed = game->time_frames - game->intro_state_start_frame;
        if (game->intro_state == INTRO_FADE_IN) {
            game->intro_white_alpha = 255.0f;
            game->intro_title_alpha = 0.0f;
            if (elapsed >= INTRO_FADE_IN_FRAMES) {
                game->intro_state = INTRO_TITLE;
                game->intro_state_start_frame = game->time_frames;
            }
        } else if (game->intro_state == INTRO_TITLE) {
            if (elapsed < INTRO_TITLE_IN_FRAMES) {
                game->intro_title_alpha = (float) elapsed / (float) INTRO_TITLE_IN_FRAMES * 255.0f;
                game->intro_white_alpha = 255.0f;
            } else if (elapsed < INTRO_TITLE_IN_FRAMES + INTRO_TITLE_HOLD_FRAMES) {
                game->intro_title_alpha = 255.0f;
                game->intro_white_alpha = 255.0f;
            } else if (elapsed < INTRO_TITLE_IN_FRAMES + INTRO_TITLE_HOLD_FRAMES + INTRO_FADE_OUT_FRAMES) {
                float t = (float) (elapsed - INTRO_TITLE_IN_FRAMES - INTRO_TITLE_HOLD_FRAMES)
                    / (float) INTRO_FADE_OUT_FRAMES;
                game->intro_title_alpha = 255.0f;
                game->intro_white_alpha = (1.0f - t) * 255.0f;
            } else {
                game->intro_title_alpha = 0.0f;
                game->intro_white_alpha = 0.0f;
                game->intro_state = INTRO_DONE;
            }
        }
    }

    if (!game->admin_mode && game->intro_state == INTRO_DONE) {
        update_all_floats(game);
        if (game->respawn_sequence.active) {
            Character *respawning_player = get_player_by_index(game, game->respawn_sequence.player_index);
            if (respawning_player && respawning_player->alive) update_character_physics(game, respawning_player, true);
        } else {
            update_enemy_turn(game);
            update_projectile(game);
            update_characters(game);
        }
        update_splash_effects(game);
    }
    if ((game->game_state == GAME_WON || game->game_state == GAME_LOST) && game->outcome_frame < 0) {
        game->outcome_frame = game->time_frames;
    }
    if (game->outcome_frame >= 0 && game->time_frames - game->outcome_frame >= LEVEL5_AUTO_ADVANCE_FRAMES) {
        game->running = false;
    }
    update_camera(game);
}

static void update_fps_counter(Game *game, double frame_seconds) {
    if (frame_seconds < 0.0) frame_seconds = 0.0;
    game->fps_accum_seconds += frame_seconds;
    game->fps_accum_frames++;
    if (game->fps_accum_seconds >= 0.5) {
        game->fps_value = (game->fps_accum_seconds > 0.0)
            ? (float) ((double) game->fps_accum_frames / game->fps_accum_seconds)
            : 0.0f;
        game->fps_accum_seconds = 0.0;
        game->fps_accum_frames = 0;
    }
}

static SDL_RendererFlip platform_flip(const Platform *platform) {
    return platform->flip_x ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
}

static const TextureAsset *get_airplane_texture(const Game *game) {
    if (game->respawn_sequence.active && game->assets.airplane_high_speed.texture) {
        return &game->assets.airplane_high_speed;
    }
    if (game->assets.airplane.texture) return &game->assets.airplane;
    if (game->assets.airplane_high_speed.texture) return &game->assets.airplane_high_speed;
    return NULL;
}

static float get_airplane_altitude(const Game *game) {
    if (game->respawn_sequence.active) return game->respawn_sequence.flight_altitude;
    return game->config.airplane.altitude;
}

static void get_airplane_size(const Game *game, float *out_width, float *out_height) {
    float height = AIRPLANE_BASE_RENDER_HEIGHT * game->config.airplane.size;
    float width = height * 2.3f;
    const TextureAsset *airplane_tex = get_airplane_texture(game);

    if (airplane_tex && airplane_tex->height > 0) {
        int frame_w = airplane_tex->width / AIRPLANE_SHEET_COLS;
        int frame_h = airplane_tex->height / AIRPLANE_SHEET_ROWS;
        if (frame_w > 0 && frame_h > 0) {
            width = height * ((float) frame_w / (float) frame_h);
        } else {
            width = height * ((float) airplane_tex->width / (float) airplane_tex->height);
        }
    }

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
}

static float get_airplane_offscreen_margin(const Game *game) {
    float width = 0.0f;
    get_airplane_size(game, &width, NULL);
    return fmaxf(36.0f, width * 0.55f);
}

static void reset_airplane_motion(Game *game) {
    float margin = get_airplane_offscreen_margin(game);
    game->airplane_x = -margin;
    game->airplane_facing_right = true;
}

static void update_airplane_motion(Game *game) {
    if (game->respawn_sequence.active) {
        update_respawn_sequence(game);
        return;
    }

    float margin = get_airplane_offscreen_margin(game);
    float left_edge = -margin;
    float right_edge = WORLD_WIDTH + margin;

    if (game->airplane_facing_right) {
        game->airplane_x += AIRPLANE_SPEED;
        if (game->airplane_x >= right_edge) {
            game->airplane_x = right_edge;
            game->airplane_facing_right = false;
        }
    } else {
        game->airplane_x -= AIRPLANE_SPEED;
        if (game->airplane_x <= left_edge) {
            game->airplane_x = left_edge;
            game->airplane_facing_right = true;
        }
    }
}

static void draw_airplane(Game *game) {
    float world_w = 0.0f;
    float world_h = 0.0f;
    const TextureAsset *airplane_tex = get_airplane_texture(game);
    int airplane_fps = game->respawn_sequence.active ? AIRPLANE_HIGH_SPEED_FPS : AIRPLANE_FPS;
    get_airplane_size(game, &world_w, &world_h);

    float sx = world_to_view_x(game, game->airplane_x);
    float sy = world_to_view_y(game, get_airplane_altitude(game));
    float sw = scale_length(game, world_w);
    float sh = scale_height(game, world_h);
    SDL_FRect dst = {sx - sw * 0.5f, sy - sh * 0.5f, sw, sh};
    SDL_RendererFlip flip = game->airplane_facing_right ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    if (airplane_tex && airplane_tex->texture) {
        int frame_w = airplane_tex->width / AIRPLANE_SHEET_COLS;
        int frame_h = airplane_tex->height / AIRPLANE_SHEET_ROWS;
        int step = clampi_local((int) lroundf(60.0f / (float) airplane_fps), 1, 60);
        int frame = (game->time_frames / step) % AIRPLANE_SHEET_FRAMES;

        if (frame_w > 0 && frame_h > 0) {
            SDL_Rect src = {
                (frame % AIRPLANE_SHEET_COLS) * frame_w,
                (frame / AIRPLANE_SHEET_COLS) * frame_h,
                frame_w,
                frame_h
            };
            SDL_RenderCopyExF(game->renderer, airplane_tex->texture, &src, &dst, 0.0, NULL, flip);
        } else {
            SDL_RenderCopyExF(game->renderer, airplane_tex->texture, NULL, &dst, 0.0, NULL, flip);
        }
    } else {
        draw_filled_ellipse(game->renderer, sx, sy, sw * 0.35f, sh * 0.16f, (SDL_Color) {245, 158, 11, 235});
        draw_filled_rect(game->renderer, sx - sw * 0.24f, sy - sh * 0.04f, sw * 0.48f, sh * 0.08f, (SDL_Color) {251, 191, 36, 235});
        draw_filled_ellipse(game->renderer, sx + (game->airplane_facing_right ? -sw * 0.2f : sw * 0.2f), sy, sw * 0.12f, sh * 0.05f, (SDL_Color) {217, 119, 6, 240});
    }

    if (game->admin_mode) {
        draw_rect(game->renderer, dst.x, dst.y, dst.w, dst.h, (SDL_Color) {14, 94, 131, 130});
    }
}

static void draw_level_background(Game *game) {
    if (game->assets.background.texture) {
        SDL_Rect src;
        SDL_Rect dst = {0, 0, VIEW_WIDTH, VIEW_HEIGHT};

        if (game->admin_mode) {
            src.x = 0;
            src.y = 0;
            src.w = game->assets.background.width;
            src.h = game->assets.background.height;
        } else {
            float zoom = get_cinematic_zoom_scale(game);
            src.x = (int) floorf((game->camera_x / WORLD_WIDTH) * game->assets.background.width);
            src.y = 0;
            src.w = (int) ceilf((((float) VIEW_WIDTH / zoom) / WORLD_WIDTH) * game->assets.background.width);
            src.h = game->assets.background.height;
            if (src.w < 1) src.w = 1;
            if (src.x + src.w > game->assets.background.width) src.x = game->assets.background.width - src.w;
            if (src.x < 0) src.x = 0;
        }

        SDL_RenderCopy(game->renderer, game->assets.background.texture, &src, &dst);
    } else {
        draw_filled_rect(game->renderer, 0, 0, VIEW_WIDTH, 250, rgb(134, 215, 255));
        draw_filled_rect(game->renderer, 0, 250, VIEW_WIDTH, FLOOR_Y - 250, rgb(248, 241, 221));
        draw_filled_rect(game->renderer, 0, FLOOR_Y, VIEW_WIDTH, VIEW_HEIGHT - FLOOR_Y, rgb(96, 165, 250));
    }

    draw_filled_rect(game->renderer, 0, 0, VIEW_WIDTH, VIEW_HEIGHT, (SDL_Color) {255, 255, 255, 12});
}

static void draw_pool_signs(Game *game) {
    int scale = game->admin_mode ? 2 : 3;
    draw_text(game->renderer, game->admin_mode ? 12 : 28, game->admin_mode ? 12 : 20, scale, (SDL_Color) {15, 76, 129, 180}, "POOL ZONE");
}

static const TextureAsset *get_current_ad_frame(const Game *game) {
    if (game->assets.ad_frame_count <= 0) return NULL;
    int step = clampi_local((int) lroundf(60.0f / (float) AD_FPS), 1, 60);
    int frame = (game->time_frames / step) % game->assets.ad_frame_count;
    return &game->assets.ad_frames[frame];
}

static void draw_animated_ad(Game *game) {
    float sx = world_to_view_x(game, game->config.ad.x);
    float sy = world_to_view_y(game, game->config.ad.y);
    float sw = scale_length(game, game->config.ad.width);
    float sh = scale_height(game, game->config.ad.height);

    const TextureAsset *frame = get_current_ad_frame(game);
    if (frame && frame->texture) {
        SDL_FRect dst = {sx, sy, sw, sh};
        SDL_RenderCopyF(game->renderer, frame->texture, NULL, &dst);
    } else {
        draw_filled_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {18, 18, 22, 235});
        draw_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {220, 220, 230, 120});
        draw_text_centered(game->renderer, (int) (sx + sw * 0.5f), (int) (sy + sh * 0.45f), 2, (SDL_Color) {220, 220, 230, 230}, "ANIM AD");
    }

    if (game->admin_mode) {
        draw_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {244, 114, 182, 180});
    }
}

static void draw_animated_ad_layer(Game *game, int layer) {
    if (clampi_local(game->config.ad.z_index, TV_Z_MIN, TV_Z_MAX) != layer) return;
    draw_animated_ad(game);
}

static void draw_tv(Game *game) {
    float sx = world_to_view_x(game, game->config.tv.x);
    float sy = world_to_view_y(game, game->config.tv.y);
    float sw = scale_length(game, game->config.tv.width);
    float sh = scale_height(game, game->config.tv.height);

    draw_filled_ellipse(game->renderer, sx + sw * 0.5f, sy + sh + scale_height(game, 4.0f), sw * 0.50f, scale_height(game, 10.0f), (SDL_Color) {0, 0, 0, 38});

    if (game->assets.tv.texture) {
        SDL_FRect dst = {sx, sy, sw, sh};
        SDL_RenderCopyF(game->renderer, game->assets.tv.texture, NULL, &dst);
    } else {
        draw_filled_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {35, 35, 45, 230});
        draw_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {220, 220, 230, 120});
        draw_text_centered(game->renderer, (int) (sx + sw * 0.5f), (int) (sy + sh * 0.45f), 2, (SDL_Color) {220, 220, 230, 230}, "TV");
    }

    if (game->admin_mode) {
        draw_rect(game->renderer, sx, sy, sw, sh, (SDL_Color) {13, 117, 158, 170});
    }
}

static void draw_tv_layer(Game *game, int layer) {
    if (clampi_local(game->config.tv.z_index, TV_Z_MIN, TV_Z_MAX) != layer) return;
    draw_tv(game);
}

static void draw_block(Game *game, const Block *block) {
    float sx = world_to_view_x(game, block->x);
    float sy = world_to_view_y(game, block->y);
    float sw = scale_length(game, block->width);
    float sh = scale_height(game, block->height);
    float bob_y = scale_height(game, sinf(game->time_frames * 0.03f + block->bob_offset) * 6.0f);
    float tilt = sinf(game->time_frames * 0.025f + block->tilt_offset) * 0.035f;
    float draw_y = sy + bob_y;

    draw_filled_ellipse(game->renderer, sx + sw * 0.5f, draw_y + sh + scale_height(game, 2.0f), sw * 0.52f, scale_height(game, 11.0f), (SDL_Color) {0, 0, 0, 42});

    if (game->assets.wall.texture) {
        SDL_FRect dst = {sx, draw_y, sw, sh};
        SDL_FPoint center = {sw * 0.5f, sh * 0.5f};
        SDL_RenderCopyExF(game->renderer, game->assets.wall.texture, NULL, &dst, tilt * (180.0 / M_PI), &center, SDL_FLIP_NONE);
    } else {
        draw_filled_rect(game->renderer, sx, draw_y, sw, sh, block->color);
        draw_rect(game->renderer, sx, draw_y, sw, sh, (SDL_Color) {0, 0, 0, 60});
        for (int i = 1; i < 3; ++i) {
            float ly = draw_y + i * (sh / 3.0f);
            draw_line(game->renderer, sx, ly, sx + sw, ly, (SDL_Color) {0, 0, 0, 50});
        }
    }
}

static void draw_platform(Game *game, const Platform *platform) {
    if (platform->hidden) return;

    float sx = world_to_view_x(game, platform->x);
    float sy = world_to_view_y(game, platform->y);
    float width = scale_length(game, platform->width * 1.22f);
    float height = scale_height(game, platform->height * 2.35f);

    draw_filled_ellipse(game->renderer, sx, sy + scale_height(game, 18.0f), width * 0.44f, scale_height(game, 10.0f), (SDL_Color) {0, 0, 0, 42});

    SpriteSheet *sheet = get_sheet(game, SHEET_INNER_TUBE);
    if (sheet->texture.texture) {
        int frame_width = sheet->texture.width / sheet->columns;
        int frame_height = sheet->texture.height / sheet->rows;
        int frame_index = get_looped_sheet_frame(game, sheet, 10.0f, 4.0f);
        SDL_Rect src = {(frame_index % sheet->columns) * frame_width, (frame_index / sheet->columns) * frame_height, frame_width, frame_height};
        SDL_FRect dst = {sx - width / 2.0f, sy + scale_height(game, 4.0f) - height / 2.0f, width, height};
        SDL_FPoint center = {width / 2.0f, height / 2.0f};
        SDL_RenderCopyExF(game->renderer, sheet->texture.texture, &src, &dst, platform->tilt * 0.55f * (180.0 / M_PI), &center, platform_flip(platform));
    } else {
        draw_filled_ellipse(game->renderer, sx, sy, width * 0.5f, scale_height(game, 22.0f), platform->color1);
        draw_filled_ellipse(game->renderer, sx, sy, width * 0.24f, scale_height(game, 10.0f), (SDL_Color) {255, 255, 255, 220});
        draw_circle_outline(game->renderer, sx, sy, width * 0.24f, platform->color2);
    }
}

static void draw_gun(Game *game, float x, float y, float angle, SDL_Color color) {
    float sx = world_to_view_x(game, x);
    float sy = world_to_view_y(game, y);
    float barrel = scale_length(game, 38.0f);
    draw_thick_line(game->renderer, sx, sy, sx + cosf(angle) * barrel, sy + sinf(angle) * barrel, 6.0f, color);
    draw_filled_rect(game->renderer, sx + cosf(angle) * scale_length(game, 24.0f) - scale_length(game, 7.0f), sy + sinf(angle) * scale_length(game, 24.0f) - scale_height(game, 7.0f), scale_length(game, 14.0f), scale_height(game, 14.0f), rgb(17, 24, 39));
}

static void draw_throw_pose(Game *game, float x, float y) {
    float sx = world_to_view_x(game, x);
    float sy = world_to_view_y(game, y);

    draw_filled_ellipse(game->renderer, sx, sy + scale_height(game, 42.0f), scale_length(game, 26.0f), scale_height(game, 8.0f), (SDL_Color) {0, 0, 0, 36});
    draw_filled_rect(game->renderer, sx - scale_length(game, 16.0f), sy + scale_height(game, 12.0f), scale_length(game, 32.0f), scale_height(game, 24.0f), rgb(185, 28, 28));
    draw_filled_circle(game->renderer, sx, sy, scale_length(game, 20.0f), rgb(255, 107, 107));
    draw_filled_rect(game->renderer, sx - scale_length(game, 16.0f), sy - scale_height(game, 24.0f), scale_length(game, 32.0f), scale_height(game, 8.0f), rgb(127, 29, 29));
    draw_filled_rect(game->renderer, sx - scale_length(game, 10.0f), sy - scale_height(game, 34.0f), scale_length(game, 20.0f), scale_height(game, 12.0f), rgb(127, 29, 29));
    draw_thick_line(game->renderer, sx + scale_length(game, 10.0f), sy + scale_height(game, 18.0f), sx + scale_length(game, 30.0f), sy - scale_height(game, 10.0f), 3.0f, rgb(31, 41, 55));
    draw_thick_line(game->renderer, sx - scale_length(game, 8.0f), sy + scale_height(game, 18.0f), sx - scale_length(game, 22.0f), sy + scale_height(game, 34.0f), 3.0f, rgb(31, 41, 55));
    draw_filled_circle(game->renderer, sx + scale_length(game, 34.0f), sy - scale_height(game, 16.0f), scale_length(game, 5.0f), rgb(96, 165, 250));
}

static void draw_character_fallback(Game *game, const Character *character, Uint8 alpha) {
    float sx = world_to_view_x(game, character->x);
    float sy = world_to_view_y(game, character->y);
    draw_filled_rect(
        game->renderer,
        sx - scale_length(game, 14.0f),
        sy + scale_height(game, 14.0f),
        scale_length(game, 28.0f),
        scale_height(game, 24.0f),
        color_with_alpha(character->body_color, alpha)
    );
    draw_filled_circle(game->renderer, sx, sy, scale_length(game, character->radius), color_with_alpha(character->skin_color, alpha));
}

static void get_enemy_sheet_config(Game *game, const Character *character, const SpriteSheet *sheet, int *columns, int *rows, int *frames) {
    *columns = sheet->columns;
    *rows = sheet->rows;
    *frames = sheet->frame_count;
    for (int i = 0; i < 3; ++i) {
        if (character == &game->enemies[i]) {
            *columns = clampi_local(game->config.enemies[i].sprite_cols, 1, 20);
            *rows = clampi_local(game->config.enemies[i].sprite_rows, 1, 20);
            *frames = clampi_local(game->config.enemies[i].sprite_frames, 1, 200);
            return;
        }
    }
}

static void draw_character(Game *game, Character *character) {
    float sx = world_to_view_x(game, character->x);
    float sy = world_to_view_y(game, character->y);
    int player_index = get_player_index(game, character);
    Uint8 alpha = 255;
    if (player_index >= 0 && character->respawn_fade_frames > 0) {
        float t = 1.0f - (float) character->respawn_fade_frames / (float) PLAYER_RESPAWN_FADE_FRAMES;
        t = clampf_local(t, 0.0f, 1.0f);
        alpha = (Uint8) clampi_local((int) lroundf(70.0f + t * 185.0f), 0, 255);
    }
    draw_filled_ellipse(
        game->renderer,
        sx,
        sy + scale_height(game, 42.0f),
        scale_length(game, 26.0f),
        scale_height(game, 8.0f),
        color_with_alpha((SDL_Color) {0, 0, 0, 36}, (Uint8) lroundf((float) alpha * (36.0f / 255.0f)))
    );

    SheetState player_state = {0};
    SpriteSheet *sheet = NULL;
    int frame_index = 0;

    if (player_index >= 0) {
        player_state = get_player_sheet_state(game, player_index);
        sheet = player_state.sheet;
        frame_index = player_state.frame_index;
    } else {
        sheet = get_sheet(game, character->default_sheet);
        int step = clampi_local((int) lroundf(60.0f / (float) sheet->fps), 1, 60);
        frame_index = (game->time_frames / step) % sheet->frame_count;
    }

    if (character == &game->enemies[0] && game->time_frames < game->kevin_throw_until) {
        draw_throw_pose(game, character->x, character->y);
        return;
    }

    if (player_index >= 0 && character->respawn_airborne) {
        const TextureAsset *respawn_sheet = player_respawn_sheet(game, character);
        if (respawn_sheet && respawn_sheet->texture) {
            int frame_w = respawn_sheet->width / PLAYER_RESPAWN_SHEET_COLS;
            int frame_h = respawn_sheet->height / PLAYER_RESPAWN_SHEET_ROWS;
            int step = clampi_local((int) lroundf(60.0f / (float) PLAYER_RESPAWN_SHEET_FPS), 1, 60);
            int frame = (game->time_frames / step) % PLAYER_RESPAWN_SHEET_FRAMES;
            int render_height = PLAYER_RESPAWN_RENDER_HEIGHT;
            Uint8 previous_alpha = 255;

            if (frame_w > 0 && frame_h > 0) {
                SDL_Rect src = {
                    (frame % PLAYER_RESPAWN_SHEET_COLS) * frame_w,
                    (frame / PLAYER_RESPAWN_SHEET_COLS) * frame_h,
                    frame_w,
                    frame_h
                };
                float target_height = scale_height(game, (float) render_height);
                float aspect = (float) frame_w / (float) frame_h;
                float target_width = target_height * aspect;
                SDL_FRect dst = {sx - target_width / 2.0f, sy - target_height + scale_height(game, 24.0f), target_width, target_height};

                if (alpha < 255) {
                    SDL_GetTextureAlphaMod(respawn_sheet->texture, &previous_alpha);
                    SDL_SetTextureAlphaMod(respawn_sheet->texture, alpha);
                }
                SDL_RenderCopyF(game->renderer, respawn_sheet->texture, &src, &dst);
                if (alpha < 255) SDL_SetTextureAlphaMod(respawn_sheet->texture, previous_alpha);
                return;
            }
        }
    }

    if (sheet && sheet->texture.texture) {
        int columns = sheet->columns;
        int rows = sheet->rows;
        int frame_count = sheet->frame_count;
        int render_height = 64;

        if (player_index == 0) {
            render_height = game->config.player.render_height;
        } else if (player_index == 1) {
            render_height = game->config.player2.render_height;
        } else {
            get_enemy_sheet_config(game, character, sheet, &columns, &rows, &frame_count);
            for (int i = 0; i < 3; ++i) {
                if (character == &game->enemies[i]) {
                    render_height = game->config.enemies[i].render_height;
                    break;
                }
            }
        }

        if (player_index >= 0 &&
            (sheet == get_sheet(game, SHEET_BURGLAR_SMOKE) || sheet == get_sheet(game, SHEET_BURGLAR2_SMOKE))) {
            render_height = 116;
        }

        if (columns > 0 && rows > 0) {
            Uint8 previous_alpha = 255;
            int frame_width = sheet->texture.width / columns;
            int frame_height = sheet->texture.height / rows;
            frame_index = frame_count > 0 ? frame_index % frame_count : 0;
            SDL_Rect src = {(frame_index % columns) * frame_width, (frame_index / columns) * frame_height, frame_width, frame_height};
            float target_height = scale_height(game, (float) render_height);
            float aspect = frame_height > 0 ? (float) frame_width / (float) frame_height : 1.0f;
            float target_width = target_height * aspect;
            SDL_FRect dst = {sx - target_width / 2.0f, sy - target_height + scale_height(game, 24.0f), target_width, target_height};
            if (alpha < 255) {
                SDL_GetTextureAlphaMod(sheet->texture.texture, &previous_alpha);
                SDL_SetTextureAlphaMod(sheet->texture.texture, alpha);
            }
            SDL_RenderCopyF(game->renderer, sheet->texture.texture, &src, &dst);
            if (alpha < 255) {
                SDL_SetTextureAlphaMod(sheet->texture.texture, previous_alpha);
            }
            return;
        }
    }

    draw_character_fallback(game, character, alpha);
}

static void draw_health_bar(Game *game, float x, float y, int health, int max_health, SDL_Color fill) {
    float sx = world_to_view_x(game, x);
    float sy = world_to_view_y(game, y);
    float ratio = max_health > 0 ? (float) health / (float) max_health : 0.0f;
    draw_filled_rect(game->renderer, sx - scale_length(game, 24.0f), sy - scale_height(game, 64.0f), scale_length(game, 48.0f), scale_height(game, 7.0f), rgb(34, 34, 34));
    draw_filled_rect(game->renderer, sx - scale_length(game, 24.0f), sy - scale_height(game, 64.0f), scale_length(game, fmaxf(0.0f, ratio * 48.0f)), scale_height(game, 7.0f), fill);
}

static void draw_projectile(Game *game) {
    if (!game->projectile.active) return;
    float sx = world_to_view_x(game, game->projectile.x);
    float sy = world_to_view_y(game, game->projectile.y);
    draw_filled_circle(game->renderer, sx, sy, scale_length(game, game->projectile.radius), rgb(96, 165, 250));
    draw_circle_outline(game->renderer, sx, sy, scale_length(game, game->projectile.radius - 1.5f), (SDL_Color) {239, 246, 255, 220});
}

static void draw_splash_effects(Game *game) {
    for (int i = 0; i < game->splash_count; ++i) {
        SplashParticle *splash = &game->splashes[i];
        float sx = world_to_view_x(game, splash->x);
        float sy = world_to_view_y(game, splash->y);
        float alpha = clampf_local(splash->life / 34.0f, 0.0f, 1.0f) * splash->alpha;
        draw_filled_circle(game->renderer, sx, sy, scale_length(game, splash->size), (SDL_Color) {96, 165, 250, (Uint8) (alpha * 255.0f)});
        draw_circle_outline(game->renderer, sx, sy, scale_length(game, fmaxf(1.5f, splash->size - 2.0f)), (SDL_Color) {239, 246, 255, (Uint8) (alpha * 192.0f)});
    }
}

static void draw_trajectory(Game *game) {
    Character *active = get_active_player(game);
    if (game->current_turn != TURN_PLAYER || game->game_state != GAME_AIMING || !active || !active->alive || active->defeated) return;

    float sim_x = active->x + cosf(active->angle) * 42.0f;
    float sim_y = active->y + sinf(active->angle) * 42.0f;
    float sim_vx = cosf(active->angle) * active->power;
    float sim_vy = sinf(active->angle) * active->power;

    for (int i = 0; i < 7; ++i) {
        sim_x += sim_vx;
        sim_y += sim_vy;
        sim_vy += GRAVITY;
        sim_vx *= 0.998f;

        if (sim_y >= FLOOR_Y || sim_x < 0.0f || sim_x > WORLD_WIDTH) break;

        draw_filled_circle(game->renderer, world_to_view_x(game, sim_x), world_to_view_y(game, sim_y), scale_length(game, 3.0f), (SDL_Color) {255, 255, 255, 200});

        bool blocked = false;
        for (int j = 0; j < 3; ++j) {
            if (sim_x > game->blocks[j].x && sim_x < game->blocks[j].x + game->blocks[j].width &&
                sim_y > game->blocks[j].y && sim_y < game->blocks[j].y + game->blocks[j].height) {
                blocked = true;
                break;
            }
        }
        if (blocked) break;
    }
}

static void draw_aim_guide(Game *game) {
    Character *active = get_active_player(game);
    if (game->current_turn != TURN_PLAYER || game->game_state != GAME_AIMING || !active || !active->alive || active->defeated) return;

    float sx = world_to_view_x(game, active->x);
    float sy = world_to_view_y(game, active->y);
    float length = scale_length(game, 44.0f);
    draw_thick_line(game->renderer, sx, sy, sx + cosf(active->angle) * length, sy + sinf(active->angle) * length, 3.0f, rgb(17, 17, 17));

    float ratio = (active->power - PLAYER_MIN_POWER) / (PLAYER_MAX_POWER - PLAYER_MIN_POWER);
    draw_arc(game->renderer, sx, sy, scale_length(game, 34.0f), -(float) M_PI * 0.95f, -(float) M_PI * 0.95f + (float) M_PI * 1.9f * ratio, (SDL_Color) {14, 165, 233, 230}, 6.0f);
}

static void draw_mini_map(Game *game) {
    const int map_w = 360;
    const int map_h = 74;
    const int x = VIEW_WIDTH / 2 - map_w / 2;
    const int y = VIEW_HEIGHT - map_h - 16;

    draw_filled_rect(game->renderer, (float) x, (float) y, (float) map_w, (float) map_h, (SDL_Color) {59, 23, 13, 210});
    draw_rect(game->renderer, (float) x, (float) y, (float) map_w, (float) map_h, (SDL_Color) {255, 239, 220, 55});
    draw_filled_rect(game->renderer, (float) x + 10.0f, (float) y + 18.0f, (float) map_w - 20.0f, 16.0f, (SDL_Color) {230, 195, 132, 72});

    for (int i = 0; i < configured_player_count(game); ++i) {
        Character *player = get_player_by_index(game, i);
        if (!player || !player->alive) continue;
        float px = x + 10.0f + (player->x / WORLD_WIDTH) * (map_w - 20.0f);
        SDL_Color color = player_ui_color(i);
        draw_filled_circle(game->renderer, px, y + 26.0f, 7.0f, color);
        char text[16];
        int shown_health = 0;
        int shown_max = 1;
        get_character_display_health(game, player, &shown_health, &shown_max);
        (void) shown_max;
        SDL_snprintf(text, sizeof(text), "%d", shown_health);
        draw_text_centered(game->renderer, (int) px, y + 40, 2, rgb(255, 255, 255), text);
    }

    for (int i = 0; i < 3; ++i) {
        if (!game->enemies[i].alive) continue;
        float ex = x + 10.0f + (game->enemies[i].x / WORLD_WIDTH) * (map_w - 20.0f);
        draw_filled_circle(game->renderer, ex, y + 26.0f, 7.0f, rgb(239, 68, 68));
        char text[16];
        SDL_snprintf(text, sizeof(text), "%d", game->enemies[i].health > 0 ? game->enemies[i].health : 0);
        draw_text_centered(game->renderer, (int) ex, y + 40, 2, rgb(255, 255, 255), text);
    }
}

static void draw_turn_banner(Game *game) {
    const int banner_w = 340;
    const int banner_h = 74;
    const int banner_x = VIEW_WIDTH - banner_w - 30;

    draw_filled_rect(game->renderer, banner_x, 16, banner_w, banner_h, (SDL_Color) {255, 246, 233, 230});
    draw_rect(game->renderer, banner_x, 16, banner_w, banner_h, (SDL_Color) {76, 33, 18, 38});
    Character *active = get_active_player(game);
    char title[64];
    if (game->current_turn == TURN_PLAYER) {
        SDL_snprintf(title, sizeof(title), "%s TURN", active ? active->name : "PLAYER");
    } else {
        SDL_snprintf(title, sizeof(title), "KEVIN TEAM TURN");
    }
    draw_text(game->renderer, banner_x + 22, 24, 3, rgb(15, 76, 129), title);

    char line[64];
    if (game->current_turn == TURN_PLAYER && active) {
        SDL_snprintf(line, sizeof(line), "ANGLE %.2f  POWER %.1f", active->angle, active->power);
    } else {
        Character *shooter = get_current_enemy_shooter(game);
        int countdown = game->enemy_shoot_timer > 0 ? (int) ceilf(game->enemy_shoot_timer / 10.0f) : 0;
        SDL_snprintf(line, sizeof(line), "%s  IN %d", shooter ? shooter->name : "NONE", countdown);
    }
    draw_text(game->renderer, banner_x + 22, 56, 2, rgb(49, 82, 102), line);
}

static void draw_power_meter(Game *game) {
    Character *active = get_active_player(game);
    if (game->current_turn != TURN_PLAYER || !active || !active->alive || active->defeated) return;

    const int x = 24;
    const int y = VIEW_HEIGHT - 116;
    const char *hint_text = NULL;
    char hint_buffer[64];
    int w = 220;
    const int panel_h = 82;
    const int h = 16;
    float ratio = clampf_local((active->power - PLAYER_MIN_POWER) / (PLAYER_MAX_POWER - PLAYER_MIN_POWER), 0.0f, 1.0f);

    if (game->duo_mode) {
        int player_index = game->active_player_index;
        SDL_snprintf(hint_buffer,
                     sizeof(hint_buffer),
                     "P%d: %s + %s TO THROW",
                     player_index + 1,
                     session_control_scheme_label(level3ControlSchemeForPlayer(game, player_index)),
                     session_interact_bind_label(level3InteractBindForPlayer(game, player_index)));
        hint_text = hint_buffer;
    } else {
        hint_text = game->mouse_aim.active ? "RELEASE TO THROW" : "DRAG FROM PLAYER TO AIM";
    }

    int title_width = text_width_px("THROW POWER", 2) + 24;
    int hint_width = text_width_px(hint_text, 2) + 24;
    w = clampi_local((title_width > hint_width ? title_width : hint_width), 220, 360);

    draw_filled_rect(game->renderer, x, y, w, panel_h, (SDL_Color) {255, 246, 233, 235});
    draw_rect(game->renderer, x, y, w, panel_h, (SDL_Color) {76, 33, 18, 38});
    draw_text(game->renderer, x + 12, y + 8, 2, rgb(15, 76, 129), "THROW POWER");
    draw_filled_rect(game->renderer, x + 12, y + 29, w - 24, h, (SDL_Color) {15, 76, 129, 36});
    draw_filled_rect(game->renderer, x + 12, y + 29, (w - 24) * ratio, h, rgb(14, 165, 233));
    draw_rect(game->renderer, x + 12, y + 29, w - 24, h, (SDL_Color) {12, 74, 110, 90});
    draw_text(game->renderer, x + 12, y + 58, 2, rgb(49, 82, 102), hint_text);
}

static const char *editor_target_name(int target) {
    static const char *names[TARGET_COUNT] = {
        "PLAYER 1 FLOAT", "PLAYER 2 FLOAT", "KEVIN FLOAT", "FRIEND FLOAT", "SCOUT FLOAT",
        "WALL 1", "WALL 2", "WALL 3", "TV", "AD", "AIRPLANE"
    };
    return names[clampi_local(target, 0, TARGET_COUNT - 1)];
}

static int get_editor_field_ref(Game *game, int target, int field_index, EditorFieldRef *out) {
    memset(out, 0, sizeof(*out));

    switch (target) {
        case 0: {
            EditorFieldRef fields[] = {
                {"X", &game->config.player.x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.player.float_y, false, 220.0, WORLD_HEIGHT, 1.0},
                {"SPRITE H", &game->config.player.render_height, true, 40.0, 180.0, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 1: {
            EditorFieldRef fields[] = {
                {"X", &game->config.player2.x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.player2.float_y, false, 220.0, WORLD_HEIGHT, 1.0},
                {"SPRITE H", &game->config.player2.render_height, true, 40.0, 180.0, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 2:
        case 3:
        case 4: {
            int enemy = target - 2;
            EditorFieldRef fields[] = {
                {"X", &game->config.enemies[enemy].x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.enemies[enemy].float_y, false, 220.0, WORLD_HEIGHT, 1.0},
                {"SPRITE H", &game->config.enemies[enemy].render_height, true, 32.0, 220.0, 1.0},
                {"COLS", &game->config.enemies[enemy].sprite_cols, true, 1.0, 20.0, 1.0},
                {"ROWS", &game->config.enemies[enemy].sprite_rows, true, 1.0, 20.0, 1.0},
                {"FRAMES", &game->config.enemies[enemy].sprite_frames, true, 1.0, 200.0, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 5:
        case 6:
        case 7: {
            int block = target - 5;
            EditorFieldRef fields[] = {
                {"X", &game->config.blocks[block].x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.blocks[block].y, false, 120.0, WORLD_HEIGHT, 1.0},
                {"WIDTH", &game->config.blocks[block].width, false, 20.0, 220.0, 1.0},
                {"HEIGHT", &game->config.blocks[block].height, false, 20.0, 320.0, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 8: {
            EditorFieldRef fields[] = {
                {"X", &game->config.tv.x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.tv.y, false, 0.0, WORLD_HEIGHT, 1.0},
                {"WIDTH", &game->config.tv.width, false, 24.0, 900.0, 1.0},
                {"HEIGHT", &game->config.tv.height, false, 24.0, 520.0, 1.0},
                {"Z INDEX", &game->config.tv.z_index, true, TV_Z_MIN, TV_Z_MAX, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 9: {
            EditorFieldRef fields[] = {
                {"X", &game->config.ad.x, false, 0.0, WORLD_WIDTH, 1.0},
                {"Y", &game->config.ad.y, false, 0.0, WORLD_HEIGHT, 1.0},
                {"WIDTH", &game->config.ad.width, false, 24.0, 900.0, 1.0},
                {"HEIGHT", &game->config.ad.height, false, 24.0, 520.0, 1.0},
                {"Z INDEX", &game->config.ad.z_index, true, TV_Z_MIN, TV_Z_MAX, 1.0},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        case 10: {
            EditorFieldRef fields[] = {
                {"ALTITUDE", &game->config.airplane.altitude, false, 24.0, 260.0, 1.0},
                {"SIZE", &game->config.airplane.size, false, 0.2, 3.0, 0.1},
            };
            int count = (int) SDL_arraysize(fields);
            if (field_index >= 0 && field_index < count) *out = fields[field_index];
            return count;
        }
        default:
            return 0;
    }
}

static double editor_field_get_value(const EditorFieldRef *field) {
    return field->is_integer ? (double) (*(int *) field->value_ptr) : (double) (*(float *) field->value_ptr);
}

static void editor_field_set_value(const EditorFieldRef *field, double value) {
    if (value < field->min_value) value = field->min_value;
    if (value > field->max_value) value = field->max_value;
    if (field->is_integer) *(int *) field->value_ptr = round_to_int(value);
    else *(float *) field->value_ptr = (float) value;
}

static void draw_editor_overlay(Game *game) {
    EditorFieldRef field;
    int field_count = get_editor_field_ref(game, game->editor_target, game->editor_field, &field);
    if (field_count <= 0) return;
    game->editor_field = clampi_local(game->editor_field, 0, field_count - 1);

    const int panel_x = 20;
    const int panel_y = 20;
    const int panel_w = 460;
    const int fields_y = 114;
    const int row_h = 18;
    int info_y = fields_y + field_count * row_h + 10;
    int status_y = info_y + 54;
    int panel_h = status_y + 22 - panel_y;

    draw_filled_rect(game->renderer, panel_x, panel_y, panel_w, panel_h, (SDL_Color) {242, 251, 255, 247});
    draw_rect(game->renderer, panel_x, panel_y, panel_w, panel_h, (SDL_Color) {14, 94, 131, 90});
    draw_text(game->renderer, 36, 34, 3, rgb(16, 55, 77), "EDITOR MODE");
    draw_text(game->renderer, 36, 60, 2, rgb(49, 82, 102), "GAMEPLAY IS PAUSED  FULL MAP IS VISIBLE");
    draw_text(game->renderer, 36, 88, 2, rgb(16, 55, 77), editor_target_name(game->editor_target));

    for (int i = 0; i < field_count; ++i) {
        EditorFieldRef row_field;
        get_editor_field_ref(game, game->editor_target, i, &row_field);

        char row_text[128];
        if (row_field.is_integer) {
            SDL_snprintf(row_text, sizeof(row_text), "%c %s: %d", i == game->editor_field ? '>' : ' ', row_field.label, *(int *) row_field.value_ptr);
        } else {
            SDL_snprintf(row_text, sizeof(row_text), "%c %s: %.1f", i == game->editor_field ? '>' : ' ', row_field.label, *(float *) row_field.value_ptr);
        }

        draw_text(game->renderer,
                  36,
                  fields_y + i * row_h,
                  2,
                  i == game->editor_field ? rgb(13, 117, 158) : rgb(49, 82, 102),
                  row_text);
    }

    draw_text(game->renderer, 36, info_y, 2, rgb(49, 82, 102), "Q E TARGET   TAB FIELD   ARROWS CHANGE");
    draw_text(game->renderer, 36, info_y + 18, 2, rgb(49, 82, 102), "SHIFT + ARROWS = X10   P SAVE   L LOAD");
    draw_text(game->renderer, 36, info_y + 36, 2, rgb(49, 82, 102), "BACKSPACE DEFAULTS   R MATCH RESET   M CLOSE");

    if (game->status_text[0] != '\0' && game->time_frames < game->status_until) {
        draw_text(game->renderer, 36, status_y, 2, rgb(13, 117, 158), game->status_text);
    }
}

static void draw_hud(Game *game) {
    if (game->game_state == GAME_WON) {
        draw_text_centered(game->renderer, VIEW_WIDTH / 2, 80, 5, rgb(20, 83, 45), "POOL CLEARED!");
        draw_text_centered(game->renderer, VIEW_WIDTH / 2, 122, 3, rgb(20, 83, 45), "PRESS ANY KEY TO CONTINUE");
    } else if (game->game_state == GAME_LOST) {
        draw_text_centered(game->renderer, VIEW_WIDTH / 2, 80, 5, rgb(127, 29, 29), "KEVIN DEFENDED THE POOL!");
        draw_text_centered(game->renderer, VIEW_WIDTH / 2, 122, 3, rgb(127, 29, 29), "PRESS ANY KEY TO CONTINUE");
    }

    draw_turn_banner(game);
    draw_power_meter(game);
    draw_mini_map(game);

    if (game->admin_mode) {
        draw_editor_overlay(game);
    } else if (game->status_text[0] != '\0' && game->time_frames < game->status_until) {
        int width = text_width_px(game->status_text, 2) + 24;
        draw_filled_rect(game->renderer, VIEW_WIDTH - width - 20, VIEW_HEIGHT - 36, width, 24, (SDL_Color) {242, 251, 255, 235});
        draw_rect(game->renderer, VIEW_WIDTH - width - 20, VIEW_HEIGHT - 36, width, 24, (SDL_Color) {14, 94, 131, 70});
        draw_text(game->renderer, VIEW_WIDTH - width - 8, VIEW_HEIGHT - 30, 2, rgb(49, 82, 102), game->status_text);
    }

    {
        char fps_line[32];
        snprintf(fps_line, sizeof(fps_line), "FPS %.1f", game->fps_value);
        draw_text(game->renderer,
                  game->admin_mode ? 12 : 28,
                  game->admin_mode ? 34 : 44,
                  2,
                  rgb(15, 76, 129),
                  fps_line);
    }
}

static void draw_intro_overlay(Game *game) {
    if (game->intro_state == INTRO_DONE) return;

    if (game->intro_white_alpha > 0.0f) {
        draw_filled_rect(game->renderer, 0, 0, VIEW_WIDTH, VIEW_HEIGHT,
                         (SDL_Color) {255, 255, 255, (Uint8) clampf_local(game->intro_white_alpha, 0.0f, 255.0f)});
    }
    if (game->intro_title_alpha > 0.0f) {
        Uint8 alpha = (Uint8) clampf_local(game->intro_title_alpha, 0.0f, 255.0f);
        if (game->assets.chapter_title_tex && game->assets.chapter_title_w > 0 && game->assets.chapter_title_h > 0) {
            SDL_Rect dst = {
                VIEW_WIDTH / 2 - game->assets.chapter_title_w / 2,
                VIEW_HEIGHT / 2 - game->assets.chapter_title_h / 2,
                game->assets.chapter_title_w,
                game->assets.chapter_title_h
            };
            SDL_SetTextureAlphaMod(game->assets.chapter_title_tex, alpha);
            SDL_RenderCopy(game->renderer, game->assets.chapter_title_tex, NULL, &dst);
            SDL_SetTextureAlphaMod(game->assets.chapter_title_tex, 255);
        } else {
            draw_text_centered(game->renderer,
                               VIEW_WIDTH / 2,
                               VIEW_HEIGHT / 2 - 18,
                               5,
                               (SDL_Color) {24, 34, 58, alpha},
                               "Chapter 4 : The Pool");
        }
    }
}

static void render_game(Game *game, bool present) {
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 203, 238, 255, 255);
    SDL_RenderClear(game->renderer);

    draw_level_background(game);
    draw_animated_ad_layer(game, 0);
    draw_tv_layer(game, 0);
    draw_pool_signs(game);
    draw_airplane(game);
    for (int i = 0; i < 3; ++i) draw_block(game, &game->blocks[i]);
    draw_trajectory(game);
    draw_aim_guide(game);
    draw_animated_ad_layer(game, 1);
    draw_tv_layer(game, 1);

    for (int i = 0; i < configured_player_count(game); ++i) {
        Character *player = get_player_by_index(game, i);
        if (!player || !player->alive) continue;
        draw_character(game, player);
        if (!player->defeated) {
            int shown_health = 0;
            int shown_max = 1;
            SDL_Color health_color = player_ui_color(i);
            SDL_Color gun_color = i == 0 ? rgb(31, 41, 55) : rgb(30, 64, 175);
            get_character_display_health(game, player, &shown_health, &shown_max);
            if (!player->respawn_airborne) {
                draw_gun(game, player->x + 5.0f, player->y + 2.0f, player->angle, gun_color);
            }
            draw_health_bar(game, player->x, player->y, shown_health, shown_max, health_color);
        }
    }

    for (int i = 0; i < 3; ++i) {
        if (!game->enemies[i].alive) continue;
        draw_character(game, &game->enemies[i]);
        if (!game->enemies[i].defeated) {
            SDL_Color health_color = game->enemies[i].health > game->enemies[i].max_health * 0.4f
                ? rgb(34, 197, 94)
                : rgb(239, 68, 68);
            draw_gun(game, game->enemies[i].x - 8.0f, game->enemies[i].y + 3.0f, game->enemies[i].display_angle, rgb(71, 85, 105));
            draw_health_bar(game, game->enemies[i].x, game->enemies[i].y, game->enemies[i].health, game->enemies[i].max_health, health_color);
        }
    }
    draw_animated_ad_layer(game, 2);
    draw_tv_layer(game, 2);

    if (game->player.alive ||
        (game->respawn_sequence.active && game->respawn_sequence.player_index == 0)) {
        draw_platform(game, &game->player.platform);
    }
    if ((game->duo_mode && game->player2.alive) ||
        (game->respawn_sequence.active && game->respawn_sequence.player_index == 1)) {
        draw_platform(game, &game->player2.platform);
    }
    for (int i = 0; i < 3; ++i) draw_platform(game, &game->enemies[i].platform);
    draw_animated_ad_layer(game, 3);
    draw_tv_layer(game, 3);

    draw_projectile(game);
    draw_splash_effects(game);
    draw_animated_ad_layer(game, 4);
    draw_tv_layer(game, 4);
    if (game->intro_state == INTRO_DONE) draw_hud(game);
    draw_animated_ad_layer(game, 5);
    draw_tv_layer(game, 5);
    draw_intro_overlay(game);
    if (!game->pause_menu_active) {
        options_scene_render_global_brightness_overlay(game->renderer);
    }
    online_client_submit_frame(game->renderer, 3);
    if (present) SDL_RenderPresent(game->renderer);
}

static void set_admin_mode(Game *game, bool enabled) {
    game->admin_mode = enabled;
    game->mouse_aim.active = false;
    if (enabled) {
        apply_level_config_to_scene(game);
        set_status(game, "EDITING LAYOUT", 300);
    } else {
        set_status(game, "", 0);
    }
}

static void window_to_logical(Game *game, int window_x, int window_y, float *logical_x, float *logical_y) {
    int ww = 1;
    int wh = 1;
    SDL_GetWindowSize(game->window, &ww, &wh);

    float scale = fminf((float) ww / VIEW_WIDTH, (float) wh / VIEW_HEIGHT);
    float viewport_w = VIEW_WIDTH * scale;
    float viewport_h = VIEW_HEIGHT * scale;
    float offset_x = ((float) ww - viewport_w) * 0.5f;
    float offset_y = ((float) wh - viewport_h) * 0.5f;

    *logical_x = ((float) window_x - offset_x) / scale;
    *logical_y = ((float) window_y - offset_y) / scale;
}

static void reload_config(Game *game) {
    if (load_level_config(game->config_path, &game->config)) {
        sanitize_level_config(&game->config);
        apply_level_config_to_scene(game);
        reset_game(game);
        set_status(game, "LAYOUT LOADED", 180);
    } else {
        set_status(game, "LAYOUT FILE NOT FOUND", 180);
    }
}

static void adjust_editor_field(Game *game, double direction, bool accelerated) {
    EditorFieldRef field;
    int field_count = get_editor_field_ref(game, game->editor_target, game->editor_field, &field);
    if (field_count <= 0) return;
    double value = editor_field_get_value(&field) + direction * field.step * (accelerated ? 10.0 : 1.0);
    editor_field_set_value(&field, value);
    sanitize_level_config(&game->config);
    apply_level_config_to_scene(game);
    set_status(game, "LAYOUT UPDATED", 120);
}

static ControlScheme level3ControlSchemeForPlayer(const Game *game, int player_index) {
    const GameSession *session = game ? game->session : NULL;

    if (!session || player_index < 0 || player_index > 1) {
        return player_index == 1 ? CONTROL_SCHEME_WASD : CONTROL_SCHEME_ARROWS;
    }

    if (session->player_control_scheme[player_index] == CONTROL_SCHEME_WASD ||
        session->player_control_scheme[player_index] == CONTROL_SCHEME_ARROWS) {
        return session->player_control_scheme[player_index];
    }

    if (player_index == 0 && session->control_scheme == CONTROL_SCHEME_WASD) {
        return CONTROL_SCHEME_WASD;
    }

    return player_index == 1 ? CONTROL_SCHEME_WASD : CONTROL_SCHEME_ARROWS;
}

static InteractBind level3InteractBindForPlayer(const Game *game, int player_index) {
    const GameSession *session = game ? game->session : NULL;
    InteractBind bind;

    if (!session || player_index < 0 || player_index > 1) {
        return player_index == 1 ? INTERACT_BIND_F : INTERACT_BIND_0;
    }

    bind = session->player_interact_bind[player_index];
    if (bind == INTERACT_BIND_E || bind == INTERACT_BIND_F || bind == INTERACT_BIND_0) {
        return bind;
    }

    return player_index == 1 ? INTERACT_BIND_F : INTERACT_BIND_0;
}

static bool level3KeyMatchesInteractBind(SDL_Keycode key, InteractBind bind) {
    switch (bind) {
        case INTERACT_BIND_E:
            return key == SDLK_e;
        case INTERACT_BIND_F:
            return key == SDLK_f;
        case INTERACT_BIND_0:
            return key == SDLK_0 || key == SDLK_KP_0;
        default:
            return false;
    }
}

static bool level3KeyMatchesLeft(SDL_Keycode key, ControlScheme scheme) {
    if (scheme == CONTROL_SCHEME_WASD) return key == SDLK_a;
    return key == SDLK_LEFT;
}

static bool level3KeyMatchesRight(SDL_Keycode key, ControlScheme scheme) {
    if (scheme == CONTROL_SCHEME_WASD) return key == SDLK_d;
    return key == SDLK_RIGHT;
}

static bool level3KeyMatchesUp(SDL_Keycode key, ControlScheme scheme) {
    if (scheme == CONTROL_SCHEME_WASD) return key == SDLK_w;
    return key == SDLK_UP;
}

static bool level3KeyMatchesDown(SDL_Keycode key, ControlScheme scheme) {
    if (scheme == CONTROL_SCHEME_WASD) return key == SDLK_s;
    return key == SDLK_DOWN;
}

static bool level4OnlineHostActive(const Game *game) {
    return game && game->duo_mode &&
           online_client_is_connected() &&
           online_client_is_host();
}

static bool level4OnlineRemoteKeyEvent(const SDL_KeyboardEvent *event) {
    return event && event->windowID == LEVEL4_ONLINE_REMOTE_WINDOW_ID;
}

static bool level4RemoteKeyMatchesLeft(SDL_Keycode key) {
    return key == SDLK_a;
}

static bool level4RemoteKeyMatchesRight(SDL_Keycode key) {
    return key == SDLK_d;
}

static bool level4RemoteKeyMatchesUp(SDL_Keycode key) {
    return key == SDLK_w || key == SDLK_SPACE;
}

static bool level4RemoteKeyMatchesDown(SDL_Keycode key) {
    return key == SDLK_s;
}

static bool level4RemoteKeyMatchesShoot(SDL_Keycode key) {
    return key == SDLK_f || key == SDLK_0 || key == SDLK_KP_0;
}

static void handle_keydown(Game *game, const SDL_KeyboardEvent *event) {
    SDL_Keycode key = event->keysym.sym;
    bool remote_event = level4OnlineRemoteKeyEvent(event);
    bool alt = (event->keysym.mod & KMOD_ALT) != 0;
    bool ctrl = (event->keysym.mod & KMOD_CTRL) != 0;
    bool shift = (event->keysym.mod & KMOD_SHIFT) != 0;
    char action_message[48];

    if (key == SDLK_ESCAPE) {
        if (game->session) game->session->quit_requested = 1;
        game->running = false;
        return;
    }

    if (key == SDLK_a && shift && !event->repeat) {
        set_status(game, "SKIP LEVEL 4", 90);
        game->game_state = GAME_WON;
        if (game->win_animation_start < 0) game->win_animation_start = game->time_frames;
        game->running = false;
        return;
    }

    if (key == SDLK_n) {
        game->skip_key_count++;
        if (game->skip_key_count >= 3) {
            game->game_state = GAME_WON;
            if (game->win_animation_start < 0) game->win_animation_start = game->time_frames;
            game->running = false;
            return;
        }
        SDL_snprintf(action_message, sizeof(action_message), "SKIP LEVEL 4 %d/3", game->skip_key_count);
        set_status(game, action_message, 120);
        return;
    }

    if (key == SDLK_p && alt && shift && !ctrl && !event->repeat) {
        if (game->session) game->session->dev_jump_to_final_cutscene = 1;
        game->game_state = GAME_WON;
        if (game->win_animation_start < 0) game->win_animation_start = game->time_frames;
        game->running = false;
        return;
    }

    if (game->intro_state != INTRO_DONE) {
        return;
    }

    if (key == SDLK_k && !game->admin_mode && !game->respawn_sequence.active &&
        game->game_state != GAME_WON && game->game_state != GAME_LOST) {
        game->throw_key_count++;
        if (game->throw_key_count >= 3) {
            Character *target = get_active_player(game);
            game->throw_key_count = 0;

            if (target && target->alive && !target->defeated && !target->respawn_airborne) {
                float throw_x = target->x < WORLD_WIDTH * 0.5f ? 10.2f : -10.2f;
                begin_character_fall(game, target, target->vx + throw_x, target->vy - 8.9f, true);
                set_status(game, "THROWN! RESPAWN STARTING", 120);
            } else {
                set_status(game, "NO PLAYER AVAILABLE TO THROW", 120);
            }
            return;
        }

        SDL_snprintf(action_message, sizeof(action_message), "THROW PLAYER %d/3", game->throw_key_count);
        set_status(game, action_message, 120);
        return;
    }

    if (key == SDLK_m) {
        set_admin_mode(game, !game->admin_mode);
        return;
    }

    if (game->admin_mode) {
        switch (key) {
            case SDLK_r: reset_game(game); set_status(game, "MATCH RESET", 180); return;
            case SDLK_q: game->editor_target = (game->editor_target + TARGET_COUNT - 1) % TARGET_COUNT; game->editor_field = 0; set_status(game, "TARGET CHANGED", 120); return;
            case SDLK_e: game->editor_target = (game->editor_target + 1) % TARGET_COUNT; game->editor_field = 0; set_status(game, "TARGET CHANGED", 120); return;
            case SDLK_TAB: {
                EditorFieldRef field;
                int count = get_editor_field_ref(game, game->editor_target, game->editor_field, &field);
                if (count > 0) {
                    game->editor_field = (game->editor_field + 1) % count;
                    set_status(game, "FIELD CHANGED", 120);
                }
                return;
            }
            case SDLK_LEFT: adjust_editor_field(game, -1.0, shift); return;
            case SDLK_RIGHT: adjust_editor_field(game, 1.0, shift); return;
            case SDLK_UP: adjust_editor_field(game, 1.0, shift); return;
            case SDLK_DOWN: adjust_editor_field(game, -1.0, shift); return;
            case SDLK_p: if (save_level_config(game)) set_status(game, "LAYOUT SAVED", 180); else set_status(game, "SAVE FAILED", 180); return;
            case SDLK_l: reload_config(game); return;
            case SDLK_BACKSPACE: game->config = DEFAULT_LEVEL_CONFIG; sanitize_level_config(&game->config); apply_level_config_to_scene(game); reset_game(game); set_status(game, "LAYOUT RESET TO DEFAULTS", 180); return;
            default: return;
        }
    }

    if (game->game_state == GAME_WON || game->game_state == GAME_LOST) {
        if (remote_event) return;
        game->running = false;
        return;
    }

    if (game->respawn_sequence.active) {
        if (remote_event) return;
        if (key == SDLK_r) reset_game(game);
        return;
    }

    Character *active = get_active_player(game);
    if (game->current_turn == TURN_PLAYER && game->game_state == GAME_AIMING && active && active->alive && !active->defeated) {
        if (game->duo_mode) {
            ControlScheme control_scheme = level3ControlSchemeForPlayer(game, game->active_player_index);
            InteractBind interact_bind = level3InteractBindForPlayer(game, game->active_player_index);

            if (level4OnlineHostActive(game)) {
                if (remote_event) {
                    if (game->active_player_index != 1) return;
                    if (level4RemoteKeyMatchesLeft(key)) active->angle -= 0.05f;
                    if (level4RemoteKeyMatchesRight(key)) active->angle += 0.05f;
                    if (level4RemoteKeyMatchesUp(key)) active->power = fminf(PLAYER_MAX_POWER, active->power + 0.4f);
                    if (level4RemoteKeyMatchesDown(key)) active->power = fmaxf(PLAYER_MIN_POWER, active->power - 0.4f);
                    if (level4RemoteKeyMatchesShoot(key)) shoot_player(game);
                } else {
                    if (game->active_player_index != 0) return;
                    if (level3KeyMatchesLeft(key, control_scheme)) active->angle -= 0.05f;
                    if (level3KeyMatchesRight(key, control_scheme)) active->angle += 0.05f;
                    if (level3KeyMatchesUp(key, control_scheme)) active->power = fminf(PLAYER_MAX_POWER, active->power + 0.4f);
                    if (level3KeyMatchesDown(key, control_scheme)) active->power = fmaxf(PLAYER_MIN_POWER, active->power - 0.4f);
                    if (level3KeyMatchesInteractBind(key, interact_bind)) shoot_player(game);
                }
            } else {
                if (level3KeyMatchesLeft(key, control_scheme)) active->angle -= 0.05f;
                if (level3KeyMatchesRight(key, control_scheme)) active->angle += 0.05f;
                if (level3KeyMatchesUp(key, control_scheme)) active->power = fminf(PLAYER_MAX_POWER, active->power + 0.4f);
                if (level3KeyMatchesDown(key, control_scheme)) active->power = fmaxf(PLAYER_MIN_POWER, active->power - 0.4f);
                if (level3KeyMatchesInteractBind(key, interact_bind)) shoot_player(game);
            }
        } else {
            if (key == SDLK_a || key == SDLK_LEFT) active->angle -= 0.05f;
            if (key == SDLK_d || key == SDLK_RIGHT) active->angle += 0.05f;
            if (key == SDLK_w || key == SDLK_UP) active->power = fminf(PLAYER_MAX_POWER, active->power + 0.4f);
            if (key == SDLK_s || key == SDLK_DOWN) active->power = fmaxf(PLAYER_MIN_POWER, active->power - 0.4f);
            if (key == SDLK_SPACE ||
                level3KeyMatchesInteractBind(key, level3InteractBindForPlayer(game, 0)) ||
                level3KeyMatchesInteractBind(key, level3InteractBindForPlayer(game, 1))) shoot_player(game);
        }
        active->angle = clampf_local(active->angle, PLAYER_AIM_MIN_ANGLE, PLAYER_AIM_MAX_ANGLE);
    }

    if (key == SDLK_r) reset_game(game);
}

static void process_events(Game *game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        arcade_input_handle_event(&event);
        if (game->pause_menu_active) {
            OptionsSceneResult result = {0};
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                level4OnlineHostActive(game) &&
                level4OnlineRemoteKeyEvent(&event.key)) {
                continue;
            }
            options_scene_handle_event(&event, &result);
            if (event.type == SDL_QUIT) {
                if (game->session) game->session->quit_requested = 1;
                game->running = false;
            } else if (result.quit_to_menu) {
                game->pause_menu_active = false;
                options_scene_leave();
                if (game->session) game->session->quit_requested = 1;
                game->running = false;
            } else if (result.return_to_main) {
                game->pause_menu_active = false;
                options_scene_leave();
                online_client_send_pause_state(0);
            }
            continue;
        }
        switch (event.type) {
            case SDL_QUIT:
                if (game->session) game->session->quit_requested = 1;
                game->running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat == 0 &&
                    event.key.keysym.sym == SDLK_ESCAPE &&
                    game->pause_menu_ready) {
                    game->pause_menu_active = true;
                    game->mouse_aim.active = false;
                    options_scene_enter();
                    online_client_send_pause_state(1);
                } else if (event.key.repeat == 0) {
                    handle_keydown(game, &event.key);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT && can_use_mouse_aim(game)) {
                    float lx = 0.0f;
                    float ly = 0.0f;
                    window_to_logical(game, event.button.x, event.button.y, &lx, &ly);
                    game->mouse_aim.active = true;
                    game->mouse_aim.x = lx;
                    game->mouse_aim.y = ly;
                    update_player_aim(game, lx, ly);
                }
                break;
            case SDL_MOUSEMOTION:
                if (game->mouse_aim.active && can_use_mouse_aim(game)) {
                    float lx = 0.0f;
                    float ly = 0.0f;
                    window_to_logical(game, event.motion.x, event.motion.y, &lx, &ly);
                    game->mouse_aim.x = lx;
                    game->mouse_aim.y = ly;
                    update_player_aim(game, lx, ly);
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT && game->mouse_aim.active) {
                    game->mouse_aim.active = false;
                    if (can_use_mouse_aim(game)) shoot_player(game);
                }
                break;
        }
    }
}

static void init_config(Game *game) {
    game->config = DEFAULT_LEVEL_CONFIG;
    sanitize_level_config(&game->config);
    if (!load_level_config(game->config_path, &game->config)) save_level_config(game);
    sanitize_level_config(&game->config);
}

static void init_game(Game *game) {
    memset(&game->projectile, 0, sizeof(game->projectile));
    game->projectile.radius = 8.0f;
    game->camera_x = 0.0f;
    game->game_state = GAME_AIMING;
    game->current_turn = TURN_PLAYER;
    game->enemy_shoot_timer = 0;
    game->enemy_turn_index = 0;
    game->enemy_miss_streak = 0;
    game->enemy_shot_hit_current = false;
    game->duo_mode = game->session && game->session->mode == GAME_MODE_DUO;
    game->active_player_index = 0;
    game->player_skin_number[0] = 1;
    game->player_skin_number[1] = 2;
    if (game->session) {
        int p1_skin = game->session->player_skin_number[0];
        if (p1_skin != 1 && p1_skin != 2) p1_skin = game->session->skin_number;
        int p2_skin = game->session->player_skin_number[1];
        if (p2_skin != 1 && p2_skin != 2) p2_skin = (p1_skin == 1 ? 2 : 1);
        game->player_skin_number[0] = clamp_skin_number(p1_skin);
        game->player_skin_number[1] = clamp_skin_number(p2_skin);
    }
    game->time_frames = 0;
    game->fps_accum_seconds = 0.0;
    game->fps_accum_frames = 0;
    game->fps_value = 0.0f;
    game->admin_mode = false;
    game->win_animation_start = -1;
    game->editor_target = 0;
    game->editor_field = 0;
    game->running = true;
    game->status_text[0] = '\0';
    game->status_until = 0;
    game->outcome_frame = -1;
    game->skip_key_count = 0;
    game->throw_key_count = 0;
    game->intro_state = INTRO_FADE_IN;
    game->intro_state_start_frame = 0;
    game->intro_white_alpha = 255.0f;
    game->intro_title_alpha = 0.0f;
    resolve_player_start_health(game);

    init_characters(game);
    init_config(game);
    reset_airplane_motion(game);
    reset_game(game);
}

int runLevel4(GameSession *session, SDL_Window *window, SDL_Renderer *renderer) {
    srand((unsigned int) time(NULL));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (!window || !renderer) {
        fprintf(stderr, "Level 4 requires a shared SDL window and renderer.\n");
        return 1;
    }

    SDL_RenderSetLogicalSize(renderer, VIEW_WIDTH, VIEW_HEIGHT);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetWindowTitle(window, "Chapter 4 : The Pool");

    Game game;
    memset(&game, 0, sizeof(game));
    game.window = window;
    game.renderer = renderer;
    game.session = session;
    game.smoke_test = false;

    arcade_input_init();
    resolve_base_paths(&game);
    load_assets(&game);
    init_game(&game);
    game.pause_menu_ready = options_scene_init(window, renderer);
    if (game.pause_menu_ready) options_scene_set_audio_enabled(0);

    Uint64 previous = SDL_GetPerformanceCounter();
    double accumulator = 0.0;
    double frequency = (double) SDL_GetPerformanceFrequency();
    Uint32 lastAutosaveTick = SDL_GetTicks();
    const Uint32 autosaveIntervalMs = 5000;

    while (game.running) {
        Uint64 now = SDL_GetPerformanceCounter();
        double delta = (double) (now - previous) / frequency;
        previous = now;
        if (delta > 0.25) delta = 0.25;
        accumulator += delta;
        update_fps_counter(&game, delta);

        arcade_input_begin_frame();
        online_client_pump();
        if (online_client_should_abort_host_gameplay()) {
            if (session) session->quit_requested = 1;
            game.running = false;
        }
        {
            int syncedPause = 0;
            while (online_client_consume_pause_state_change(&syncedPause)) {
                if (!game.pause_menu_ready) continue;
                if (syncedPause) {
                    if (!game.pause_menu_active) {
                        game.pause_menu_active = true;
                        game.mouse_aim.active = false;
                        options_scene_enter();
                    }
                    online_client_send_pause_state(1);
                } else {
                    if (game.pause_menu_active) {
                        game.pause_menu_active = false;
                        options_scene_leave();
                    }
                    online_client_send_pause_state(0);
                }
                accumulator = 0.0;
                previous = SDL_GetPerformanceCounter();
            }
        }
        process_events(&game);
        if (!game.pause_menu_active) ensure_background_music(&game);

        while (!game.pause_menu_active && accumulator >= FRAME_TIME) {
            update_game_step(&game);
            accumulator -= FRAME_TIME;
        }

        if (game.pause_menu_active) {
            accumulator = 0.0;
            previous = SDL_GetPerformanceCounter();
            options_scene_update((float) FRAME_TIME);
            render_game(&game, false);
            online_client_submit_frame(game.renderer, 4);
            options_scene_render();
            SDL_RenderPresent(game.renderer);
        } else {
            render_game(&game, true);
        }

        if (session && session->save_enabled && game.running &&
            game.game_state != GAME_WON &&
            game.game_state != GAME_LOST) {
            Uint32 autosaveNow = SDL_GetTicks();
            if (autosaveNow - lastAutosaveTick >= autosaveIntervalMs) {
                int p1Lives = 0;
                int p1Max = 0;
                int p2Lives = 0;
                int p2Max = 0;
                get_character_display_health(&game, &game.player, &p1Lives, &p1Max);
                if (game.duo_mode) {
                    get_character_display_health(&game, &game.player2, &p2Lives, &p2Max);
                }
                session->level5.completed = 0;
                session_autosave_progress(session, 4, p1Lives, game.duo_mode ? p2Lives : 0);
                lastAutosaveTick = autosaveNow;
            }
        }
    }

    if (session) {
        session->level5.completed = (!session->quit_requested && game.game_state == GAME_WON) ? 1 : 0;
        session->level5.points = session->level5.completed ? calculate_level5_points(&game) : 0;
        session_calculate_total_points(session);
    }

    cleanup_assets(&game.assets);
    if (game.pause_menu_ready) options_scene_cleanup();
    if (game.assets.audio_device_owned) Mix_CloseAudio();
    if (game.assets.mixer_owned) Mix_Quit();
    if (game.assets.image_owned) IMG_Quit();
    arcade_input_shutdown();
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_SetWindowTitle(window, "Home Alone - Merged");
    return 0;
}
