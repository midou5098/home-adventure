#include "top_scores_scene.h"
#include "asset_paths.h"
#include "mainmenu_headers.h"
#include "ui_shared.h"
#include "game_progress.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOP_SNOW_MAX 100
#define TOP_WINNER_COUNT 3
#define TOP_WINNER_PAUSE_SECONDS 2.0f
#define TOP_WINNER_REPLAY_COUNT 4

typedef enum {
    TOP_FADE_NONE = 0,
    TOP_FADE_OUT
} TopFadeState;

typedef enum {
    TOP_WINNER_ANIM_PAUSED = 0,
    TOP_WINNER_ANIM_PLAYING
} TopWinnerAnimState;

typedef struct {
    float x;
    float y;
    float speed;
    float drift;
    int size;
} TopSnowflake;

typedef struct {
    SDL_Texture* texture;
    float x;
    float y;
    int frames;
    int columns;
    int frameW;
    int frameH;
    int displayW;
    int displayH;
    int currentFrame;
    float frameTimer;
    float frameDelay;
    float speed;
} TopSprite;

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static int g_initialized = 0;
static int g_active = 0;
static int g_return_requested = 0;

static SDL_Texture* g_background = NULL;
static SDL_Texture* g_snow = NULL;
static SDL_Texture* g_panel = NULL;
static SDL_Texture* g_podium[3] = {NULL, NULL, NULL};

static TTF_Font* g_title_font = NULL;
static TTF_Font* g_entry_font = NULL;
static TTF_Font* g_number_font = NULL;
static TTF_Font* g_button_font = NULL;

static Mix_Chunk* g_click_sfx = NULL;
static Mix_Chunk* g_menu_music = NULL;
static int g_menu_music_channel = -1;
static int g_music_volume = MIX_MAX_VOLUME;
static GameLeaderboardEntry g_leaderboard[3];
static size_t g_leaderboard_count = 0;

static TopSnowflake g_snowflakes[TOP_SNOW_MAX];
static TopSprite g_kid = {0};
static TopSprite g_car = {0};
static TopSprite g_heli = {0};
static TopSprite g_winner[3] = {{0}, {0}, {0}};
static TopWinnerAnimState g_winner_anim_state[TOP_WINNER_COUNT] = {
    TOP_WINNER_ANIM_PAUSED,
    TOP_WINNER_ANIM_PAUSED,
    TOP_WINNER_ANIM_PAUSED
};
static float g_winner_pause_timer[TOP_WINNER_COUNT] = {0.0f, 0.0f, 0.0f};
static int g_winner_completed_replays[TOP_WINNER_COUNT] = {0, 0, 0};

static SDL_Rect g_panel_rect = {0, 0, 0, 0};
static SDL_Rect g_back_rect = {0, 0, 0, 0};
static SDL_Rect g_podium_rect[3] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};
static Button g_back_button = {0};

static float g_fade_alpha = 0.0f;
static TopFadeState g_fade_state = TOP_FADE_NONE;

static void top_get_render_size(int* out_w, int* out_h)
{
    int w = 1280;
    int h = 720;

    if (g_renderer) {
        int logical_w = 0;
        int logical_h = 0;
        SDL_RenderGetLogicalSize(g_renderer, &logical_w, &logical_h);

        if (logical_w > 0 && logical_h > 0) {
            w = logical_w;
            h = logical_h;
        } else if (SDL_GetRendererOutputSize(g_renderer, &w, &h) != 0 && g_window) {
            SDL_GetWindowSize(g_window, &w, &h);
        }
    } else if (g_window) {
        SDL_GetWindowSize(g_window, &w, &h);
    }

    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

static SDL_Texture* top_load_texture(const char* path)
{
    if (!g_renderer || !path) return NULL;

    SDL_Texture* tex = ui_load_texture(g_renderer, path);
    if (!tex) {
        SDL_Log("TopScores: failed to load %s (%s)", path, IMG_GetError());
    }
    return tex;
}

static int top_point_in_rect(int x, int y, const SDL_Rect* r)
{
    if (!r) return 0;
    return (x >= r->x && x <= r->x + r->w &&
            y >= r->y && y <= r->y + r->h);
}

static int top_touch_to_logical(float nx, float ny, int* lx, int* ly)
{
    if (!lx || !ly) return 0;
    if (nx < 0.0f || ny < 0.0f || nx > 1.0f || ny > 1.0f) return 0;

    int render_w = 0;
    int render_h = 0;
    top_get_render_size(&render_w, &render_h);
    if (render_w <= 0 || render_h <= 0) return 0;

    int x = (int)(nx * (float)render_w);
    int y = (int)(ny * (float)render_h);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= render_w) x = render_w - 1;
    if (y >= render_h) y = render_h - 1;

    *lx = x;
    *ly = y;
    return 1;
}

static void top_draw_text_center(TTF_Font* font, const char* text, int center_x, int y, SDL_Color color)
{
    ui_draw_text_center(g_renderer, font, text, center_x, y, color);
}

static void top_draw_text_left(TTF_Font* font, const char* text, int x, int y, SDL_Color color)
{
    ui_draw_text_left(g_renderer, font, text, x, y, color);
}

static void top_ensure_button_font(void)
{
    if (g_button_font) return;

    g_button_font = ui_open_arial_font(24, 0);

    const char* candidates[] = {
        ASSET_BUTTON_FONT,
        ASSET_CHOOSE_FONT,
        ASSET_OPTIONS_FONT,
        ASSET_MAIN_MENU_FONT_TEXT
    };
    const int count = (int)(sizeof(candidates) / sizeof(candidates[0]));

    if (!g_button_font) {
        g_button_font = ui_open_font_from_candidates(candidates, count, 24, 0);
    }
    if (g_button_font) {
        ui_apply_font_quality(g_button_font);
    }
}

static void top_render_score_line(int rank, const GameLeaderboardEntry* entry, int y, SDL_Color color)
{
    char rank_text[8];
    char name_text[32];
    char score_text[32];
    const char* name = "---";
    int score = 0;
    TTF_Font* number_font = NULL;
    TTF_Font* rank_font = NULL;
    TTF_Font* score_font = NULL;
    int rank_w = 0;
    int name_w = 0;
    int score_w = 0;
    int row_h = 0;
    int rank_h = 0;
    int name_h = 0;
    int score_h = 0;
    int gap_rank_name = 0;
    int gap_name_score = 0;
    int total_w = 0;
    int start_x = 0;
    int rank_x = 0;
    int name_x = 0;
    int score_x = 0;
    int rank_y = y;
    int name_y = y;
    int score_y = y;

    if (entry) {
        name = entry->player_name[0] ? entry->player_name : "---";
        score = entry->score;
    }

    snprintf(name_text, sizeof(name_text), "%s", name);
    snprintf(score_text, sizeof(score_text), "%d", score);

    if (!g_entry_font) return;
    number_font = g_number_font ? g_number_font : g_entry_font;

    snprintf(rank_text, sizeof(rank_text), "%d.", rank);
    rank_font = ui_font_for_text(number_font, rank_text);
    score_font = ui_font_for_text(number_font, score_text);
    if (!rank_font) rank_font = number_font;
    if (!score_font) score_font = number_font;

    gap_rank_name = (g_panel_rect.w >= 430) ? 14 : 10;
    gap_name_score = (g_panel_rect.w >= 430) ? 34 : 22;
    TTF_SizeUTF8(rank_font, rank_text, &rank_w, NULL);
    TTF_SizeUTF8(g_entry_font, name_text, &name_w, NULL);
    TTF_SizeUTF8(score_font, score_text, &score_w, NULL);

    rank_h = TTF_FontHeight(rank_font);
    name_h = TTF_FontHeight(g_entry_font);
    score_h = TTF_FontHeight(score_font);
    row_h = rank_h;
    if (name_h > row_h) row_h = name_h;
    if (score_h > row_h) row_h = score_h;

    total_w = rank_w + gap_rank_name + name_w + gap_name_score + score_w;
    start_x = g_panel_rect.x + (g_panel_rect.w - total_w) / 2;
    rank_x = start_x;
    name_x = rank_x + rank_w + gap_rank_name;
    score_x = name_x + name_w + gap_name_score;
    rank_y = y + (row_h - rank_h) / 2;
    name_y = y + (row_h - name_h) / 2;
    score_y = y + (row_h - score_h) / 2;

    top_draw_text_left(rank_font, rank_text, rank_x, rank_y, color);
    top_draw_text_left(g_entry_font, name_text, name_x, name_y, color);
    top_draw_text_left(score_font, score_text, score_x, score_y, color);
}

static int top_init_sprite(TopSprite* s,
                           const char* path,
                           float y,
                           float speed,
                           int display_w,
                           int display_h,
                           int frames,
                           int columns,
                           float frame_delay,
                           float start_x)
{
    if (!s || !path) return 0;

    memset(s, 0, sizeof(*s));
    s->texture = top_load_texture(path);
    if (!s->texture) return 0;

    int tex_w = 0;
    int tex_h = 0;
    SDL_QueryTexture(s->texture, NULL, NULL, &tex_w, &tex_h);
    if (tex_w <= 0 || tex_h <= 0) return 0;

    s->frames = (frames > 0) ? frames : 1;
    s->columns = (columns > 0) ? columns : 1;
    {
        int rows = (s->frames + s->columns - 1) / s->columns;
        if (rows < 1) rows = 1;
        s->frameW = tex_w / s->columns;
        s->frameH = tex_h / rows;
    }

    s->displayW = display_w;
    s->displayH = display_h;
    s->frameDelay = (frame_delay > 0.0f) ? frame_delay : 0.05f;
    s->speed = speed;
    s->x = start_x;
    s->y = y;
    s->currentFrame = 0;
    s->frameTimer = 0.0f;
    return 1;
}

static void top_destroy_sprite(TopSprite* s)
{
    if (!s) return;
    if (s->texture) {
        SDL_DestroyTexture(s->texture);
        s->texture = NULL;
    }
}

static void top_update_sprite(TopSprite* s, float dt)
{
    int render_w = 1280;
    if (!s || !s->texture) return;

    top_get_render_size(&render_w, NULL);
    s->x += s->speed * dt;
    if (s->x > (float)render_w) {
        s->x = -((float)s->displayW);
    }

    s->frameTimer += dt;
    while (s->frameTimer >= s->frameDelay) {
        s->frameTimer -= s->frameDelay;
        s->currentFrame = (s->currentFrame + 1) % s->frames;
    }
}

static void top_reset_winner_animations(void)
{
    for (int i = 0; i < TOP_WINNER_COUNT; ++i) {
        g_winner_anim_state[i] = TOP_WINNER_ANIM_PLAYING;
        g_winner_pause_timer[i] = 0.0f;
        g_winner_completed_replays[i] = 0;
        g_winner[i].currentFrame = 0;
        g_winner[i].frameTimer = 0.0f;
    }
}

static void top_update_winner_animation(int idx, float dt)
{
    TopSprite* s = NULL;
    float delay = 0.05f;

    if (idx < 0 || idx >= TOP_WINNER_COUNT) return;
    s = &g_winner[idx];
    if (!s->texture || s->frames <= 1) return;

    if (g_winner_anim_state[idx] == TOP_WINNER_ANIM_PAUSED) {
        g_winner_pause_timer[idx] -= dt;
        if (g_winner_pause_timer[idx] <= 0.0f) {
            g_winner_anim_state[idx] = TOP_WINNER_ANIM_PLAYING;
            s->currentFrame = 0;
            s->frameTimer = 0.0f;
        }
        return;
    }

    delay = (s->frameDelay > 0.0f) ? s->frameDelay : 0.05f;
    s->frameTimer += dt;

    while (s->frameTimer >= delay) {
        s->frameTimer -= delay;
        s->currentFrame++;

        if (s->currentFrame >= s->frames) {
            g_winner_completed_replays[idx]++;
            s->currentFrame = 0;
            s->frameTimer = 0.0f;

            if (g_winner_completed_replays[idx] >= TOP_WINNER_REPLAY_COUNT) {
                g_winner_anim_state[idx] = TOP_WINNER_ANIM_PAUSED;
                g_winner_pause_timer[idx] = TOP_WINNER_PAUSE_SECONDS;
                g_winner_completed_replays[idx] = 0;
            }
        }
    }
}

static void top_render_sprite(TopSprite* s)
{
    if (!s || !s->texture || !g_renderer) return;

    SDL_Rect src = {
        (s->currentFrame % s->columns) * s->frameW,
        (s->currentFrame / s->columns) * s->frameH,
        s->frameW,
        s->frameH
    };
    SDL_Rect dst = {
        (int)s->x,
        (int)s->y,
        s->displayW,
        s->displayH
    };
    SDL_RenderCopy(g_renderer, s->texture, &src, &dst);
}

static void top_init_snow(void)
{
    int render_w = 1280;
    int render_h = 720;
    top_get_render_size(&render_w, &render_h);

    for (int i = 0; i < TOP_SNOW_MAX; ++i) {
        g_snowflakes[i].x = (float)(rand() % ((render_w > 0) ? render_w : 1));
        g_snowflakes[i].y = (float)(rand() % ((render_h > 0) ? render_h : 1));
        g_snowflakes[i].speed = 40.0f + (float)(rand() % 80);
        g_snowflakes[i].drift = -30.0f + (float)(rand() % 60);
        g_snowflakes[i].size = 10 + (rand() % 3);
    }
}

static void top_update_snow(float dt)
{
    int render_w = 1280;
    int render_h = 720;
    top_get_render_size(&render_w, &render_h);
    if (render_w < 1) render_w = 1;
    if (render_h < 1) render_h = 1;

    for (int i = 0; i < TOP_SNOW_MAX; ++i) {
        g_snowflakes[i].y += g_snowflakes[i].speed * dt;
        g_snowflakes[i].x += g_snowflakes[i].drift * dt;

        if (g_snowflakes[i].y > (float)render_h) {
            g_snowflakes[i].y = (float)(-g_snowflakes[i].size);
            g_snowflakes[i].x = (float)(rand() % render_w);
        }

        if (g_snowflakes[i].x > (float)render_w) g_snowflakes[i].x = 0.0f;
        if (g_snowflakes[i].x < 0.0f) g_snowflakes[i].x = (float)render_w;
    }
}

static void top_render_snow(void)
{
    if (!g_renderer || !g_snow) return;

    for (int i = 0; i < TOP_SNOW_MAX; ++i) {
        SDL_Rect dst = {
            (int)g_snowflakes[i].x,
            (int)g_snowflakes[i].y,
            g_snowflakes[i].size + 10,
            g_snowflakes[i].size + 10
        };
        SDL_RenderCopy(g_renderer, g_snow, NULL, &dst);
    }
}

static void top_update_layout(void)
{
    int render_w = 1280;
    int render_h = 720;
    top_get_render_size(&render_w, &render_h);

    {
        int tex_w = 1024;
        int tex_h = 1536;
        int max_w = render_w - 80;
        int max_h = render_h - 20;
        float aspect = (float)tex_w / (float)tex_h;
        float scale_w = 1.0f;
        float scale_h = 1.0f;
        float scale = 1.0f;

        if (g_panel) {
            int query_w = 0;
            int query_h = 0;
            SDL_QueryTexture(g_panel, NULL, NULL, &query_w, &query_h);
            if (query_w > 0 && query_h > 0) {
                tex_w = query_w;
                tex_h = query_h;
                aspect = (float)tex_w / (float)tex_h;
            }
        }

        if (max_w < render_w - 20) max_w = render_w - 20;
        if (max_h < render_h - 20) max_h = render_h - 20;
        if (max_w < 1) max_w = 1;
        if (max_h < 1) max_h = 1;

        scale_w = (float)max_w / (float)tex_w;
        scale_h = (float)max_h / (float)tex_h;
        scale = (scale_w < scale_h) ? scale_w : scale_h;

        g_panel_rect.w = (int)(tex_w * scale);
        g_panel_rect.h = (int)(tex_h * scale);
        if (g_panel_rect.w < 360) g_panel_rect.w = 360;
        g_panel_rect.h = (int)((float)g_panel_rect.w / aspect);
        if (g_panel_rect.h > render_h - 16) {
            g_panel_rect.h = render_h - 16;
            g_panel_rect.w = (int)((float)g_panel_rect.h * aspect);
        }

        g_panel_rect.x = (render_w - g_panel_rect.w) / 2;
        g_panel_rect.y = (render_h - g_panel_rect.h) / 2;
        if (g_panel_rect.y < 8) g_panel_rect.y = 8;
        if (g_panel_rect.x < 8) g_panel_rect.x = 8;
    }

    g_back_rect.w = (g_panel_rect.w * 50) / 100;
    if (g_back_rect.w < 210) g_back_rect.w = 210;
    if (g_back_rect.w > 360) g_back_rect.w = 360;
    g_back_rect.h = (g_panel_rect.h >= 620) ? 58 : 54;
    g_back_rect.x = g_panel_rect.x + (g_panel_rect.w - g_back_rect.w) / 2;
    g_back_rect.y = g_panel_rect.y + g_panel_rect.h - g_back_rect.h - ((g_panel_rect.h >= 620) ? 18 : 14);
    g_back_button.rect = g_back_rect;

    {
        int podium_w = g_panel_rect.w / 7;
        int podium_gap = g_panel_rect.w / 26;
        int podium_bottom = g_back_rect.y - 24;
        int center_h = g_panel_rect.h / 6;
        int left_h = center_h - 14;
        int right_h = center_h - 26;
        int center_x = 0;
        int left_x = 0;
        int right_x = 0;

        if (podium_w < 62) podium_w = 62;
        if (podium_w > 84) podium_w = 84;
        if (podium_gap < 8) podium_gap = 8;
        if (podium_gap > 16) podium_gap = 16;

        if (center_h < 78) center_h = 78;
        if (center_h > 118) center_h = 118;
        if (left_h < 54) left_h = 54;
        if (right_h < 46) right_h = 46;

        center_x = g_panel_rect.x + (g_panel_rect.w - podium_w) / 2;
        left_x = center_x - podium_w - podium_gap;
        right_x = center_x + podium_w + podium_gap;

        g_podium_rect[0] = (SDL_Rect){left_x, podium_bottom - left_h, podium_w, left_h};
        g_podium_rect[1] = (SDL_Rect){center_x, podium_bottom - center_h, podium_w, center_h};
        g_podium_rect[2] = (SDL_Rect){right_x, podium_bottom - right_h, podium_w, right_h};
    }

    {
        int left_h = g_podium_rect[0].h + 4;
        int center_h = g_podium_rect[1].h + 8;
        int right_h = g_podium_rect[2].h + 2;
        const int foot_overlap[3] = {30, 34, 28};

        g_winner[0].displayH = left_h;
        g_winner[1].displayH = center_h;
        g_winner[2].displayH = right_h;

        for (int i = 0; i < 3; ++i) {
            if (g_winner[i].displayH < 52) g_winner[i].displayH = 52;
            g_winner[i].displayW = (g_winner[i].displayH * 3) / 4;
            g_winner[i].x = (float)(g_podium_rect[i].x + (g_podium_rect[i].w - g_winner[i].displayW) / 2);
            g_winner[i].y = (float)(g_podium_rect[i].y - g_winner[i].displayH + foot_overlap[i]);
        }
    }
}

static void top_reset_sprite_positions(void)
{
    int render_h = 720;
    top_get_render_size(NULL, &render_h);

    g_kid.y = (float)(render_h - g_kid.displayH - 10);
    g_car.y = g_kid.y + 10.0f;
    g_heli.y = 20.0f;

    g_kid.x = 140.0f;
    g_car.x = -120.0f;
    g_heli.x = -260.0f;
}

static void top_stop_music(void)
{
    if (g_menu_music_channel >= 0) {
        Mix_HaltChannel(g_menu_music_channel);
        g_menu_music_channel = -1;
    }
}

static void top_start_music(void)
{
    top_stop_music();

    if (!g_menu_music) return;
    g_menu_music_channel = Mix_PlayChannel(-1, g_menu_music, -1);
    if (g_menu_music_channel >= 0) {
        Mix_Volume(g_menu_music_channel, g_music_volume);
    }
}

static void top_request_exit(void)
{
    if (!g_active) return;
    if (g_fade_state == TOP_FADE_OUT) return;

    if (g_click_sfx) {
        Mix_PlayChannel(-1, g_click_sfx, 0);
    }
    g_fade_state = TOP_FADE_OUT;
}

int top_scores_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer)
{
    if (!shared_window || !shared_renderer) return 0;
    if (g_initialized) return 1;

    g_window = shared_window;
    g_renderer = shared_renderer;

    g_background = top_load_texture(ASSET_TOP_SCORES_BACKGROUND);
    g_snow = top_load_texture(ASSET_TOP_SCORES_SNOW);
    g_panel = top_load_texture(ASSET_TOP_SCORES_PANEL);
    /* Standard podium order: left(2nd), center(1st), right(3rd). */
    g_podium[0] = top_load_texture(ASSET_TOP_SCORES_PODIUM_2);
    g_podium[1] = top_load_texture(ASSET_TOP_SCORES_PODIUM_1);
    g_podium[2] = top_load_texture(ASSET_TOP_SCORES_PODIUM_3);

    {
        const char* title_candidates[] = {
            ASSET_MAIN_MENU_FONT_TITLE,
            ASSET_MAIN_MENU_FONT_TEXT,
            ASSET_MAIN_MENU_FONT_OPTIONS
        };
        g_title_font = ui_open_font_from_candidates(title_candidates,
                                                    (int)(sizeof(title_candidates) / sizeof(title_candidates[0])),
                                                    34,
                                                    0);
    }
    if (!g_title_font) g_title_font = ui_open_arial_font(34, 0);

    {
        const char* entry_candidates[] = {
            ASSET_MAIN_MENU_FONT_TEXT,
            ASSET_MAIN_MENU_FONT_OPTIONS,
            ASSET_MAIN_MENU_FONT_TITLE
        };
        g_entry_font = ui_open_font_from_candidates(entry_candidates,
                                                    (int)(sizeof(entry_candidates) / sizeof(entry_candidates[0])),
                                                    24,
                                                    0);
    }
    if (!g_entry_font) g_entry_font = ui_open_arial_font(24, 0);

    {
        const char* number_candidates[] = {
            ASSET_MAIN_MENU_FONT_TEXT,
            ASSET_MAIN_MENU_FONT_OPTIONS,
            ASSET_MAIN_MENU_FONT_TITLE
        };
        g_number_font = ui_open_font_from_candidates(number_candidates,
                                                     (int)(sizeof(number_candidates) / sizeof(number_candidates[0])),
                                                     24,
                                                     0);
    }
    if (!g_number_font) g_number_font = ui_open_arial_font(24, 0);

    g_click_sfx = ui_load_wav(ASSET_TOP_SCORES_CLICK);
    g_menu_music = ui_load_wav(ASSET_TOP_SCORES_MUSIC);

    {
        int render_h = 720;
        top_get_render_size(NULL, &render_h);
        {
            float kid_y = (float)(render_h - 100 - 10);
            float car_y = kid_y + 10.0f;
            float heli_y = 20.0f;

            top_init_sprite(&g_kid, ASSET_TOP_SCORES_KID, kid_y, 70.0f, 60, 100, 36, 6, 0.05f, 140.0f);
            top_init_sprite(&g_car, ASSET_TOP_SCORES_CAR, car_y, 60.0f, 180, 70, 36, 6, 0.05f, -120.0f);
            top_init_sprite(&g_heli, ASSET_TOP_SCORES_HELI, heli_y, 60.0f, 110, 80, 36, 6, 0.05f, -260.0f);
            top_init_sprite(&g_winner[0], ASSET_TOP_SCORES_CELEB_2, 0.0f, 0.0f, 76, 112, 36, 6, 0.08f, 0.0f);
            top_init_sprite(&g_winner[1], ASSET_TOP_SCORES_CELEB_1, 0.0f, 0.0f, 84, 128, 36, 6, 0.08f, 0.0f);
            top_init_sprite(&g_winner[2], ASSET_TOP_SCORES_CELEB_3, 0.0f, 0.0f, 76, 112, 36, 6, 0.08f, 0.0f);
        }
    }

    top_init_snow();
    top_update_layout();
    prototype_button_init(&g_back_button, g_back_rect.x, g_back_rect.y, g_back_rect.w, g_back_rect.h, "RETURN");
    top_reset_winner_animations();
    game_progress_load_leaderboard(GAME_PROGRESS_LEADERBOARD_PATH,
                                   g_leaderboard,
                                   3,
                                   &g_leaderboard_count);

    g_initialized = 1;
    g_active = 0;
    g_return_requested = 0;
    return 1;
}

void top_scores_scene_enter(void)
{
    if (!g_initialized) return;

    g_active = 1;
    g_return_requested = 0;
    g_fade_alpha = 0.0f;
    g_fade_state = TOP_FADE_NONE;

    game_progress_load_leaderboard(GAME_PROGRESS_LEADERBOARD_PATH,
                                   g_leaderboard,
                                   3,
                                   &g_leaderboard_count);
    top_update_layout();
    prototype_button_init(&g_back_button, g_back_rect.x, g_back_rect.y, g_back_rect.w, g_back_rect.h, "RETURN");
    top_reset_sprite_positions();
    top_init_snow();
    top_reset_winner_animations();
    top_start_music();
}

void top_scores_scene_leave(void)
{
    if (!g_initialized) return;
    g_active = 0;
    top_stop_music();
}

void top_scores_scene_set_music_volume(int sdl_volume)
{
    if (sdl_volume < 0) sdl_volume = 0;
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;

    g_music_volume = sdl_volume;
    if (g_menu_music_channel >= 0) {
        Mix_Volume(g_menu_music_channel, g_music_volume);
    }
}

void top_scores_scene_handle_event(const SDL_Event* e)
{
    if (!g_initialized || !g_active || !e) return;
    if (g_fade_state == TOP_FADE_OUT) return;

    if (e->type == SDL_WINDOWEVENT &&
        (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         e->window.event == SDL_WINDOWEVENT_RESIZED)) {
        top_update_layout();
        top_reset_sprite_positions();
        return;
    }

    if (e->type == SDL_MOUSEMOTION) {
        prototype_button_handle_event(&g_back_button, e);
        return;
    }

    if (e->type == SDL_FINGERMOTION) {
        int lx = 0;
        int ly = 0;
        if (top_touch_to_logical(e->tfinger.x, e->tfinger.y, &lx, &ly)) {
            SDL_Rect dest = get_button_dest(&g_back_button);
            g_back_button.selected = top_point_in_rect(lx, ly, &dest);
        }
        return;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (prototype_button_handle_event(&g_back_button, e)) {
            top_request_exit();
        }
        return;
    }

    if (e->type == SDL_FINGERDOWN) {
        int lx = 0;
        int ly = 0;
        SDL_Rect dest = get_button_dest(&g_back_button);
        if (top_touch_to_logical(e->tfinger.x, e->tfinger.y, &lx, &ly) &&
            top_point_in_rect(lx, ly, &dest)) {
            g_back_button.pressedTimer = 0.13f;
            top_request_exit();
        }
        return;
    }

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if (k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_RETURN || k == SDLK_SPACE) {
            top_request_exit();
        }
    }
}

void top_scores_scene_update(float dt)
{
    if (!g_initialized || !g_active) return;

    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;

    top_update_snow(dt);
    top_update_sprite(&g_car, dt);
    top_update_sprite(&g_heli, dt);
    top_update_sprite(&g_kid, dt);
    for (int i = 0; i < TOP_WINNER_COUNT; ++i) {
        top_update_winner_animation(i, dt);
    }
    prototype_button_update(&g_back_button, dt);

    if (g_fade_state == TOP_FADE_OUT) {
        g_fade_alpha += 520.0f * dt;
        if (g_fade_alpha >= 255.0f) {
            g_fade_alpha = 255.0f;
            g_return_requested = 1;
        }
    }
}

void top_scores_scene_render(void)
{
    if (!g_initialized || !g_active || !g_renderer) return;

    {
        int render_w = 1280;
        int render_h = 720;
        top_get_render_size(&render_w, &render_h);

        if (g_background) {
            SDL_Rect bg_rect = {0, 0, render_w, render_h};
            SDL_RenderCopy(g_renderer, g_background, NULL, &bg_rect);
        }
    }

    top_render_snow();
    top_render_sprite(&g_car);
    top_render_sprite(&g_heli);
    top_render_sprite(&g_kid);

    if (g_panel) {
        SDL_RenderCopy(g_renderer, g_panel, NULL, &g_panel_rect);
    } else {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 18, 34, 52, 220);
        SDL_RenderFillRect(g_renderer, &g_panel_rect);
        SDL_SetRenderDrawColor(g_renderer, 220, 236, 255, 255);
        SDL_RenderDrawRect(g_renderer, &g_panel_rect);
    }

    if (g_podium[0]) SDL_RenderCopy(g_renderer, g_podium[0], NULL, &g_podium_rect[0]);
    if (g_podium[1]) SDL_RenderCopy(g_renderer, g_podium[1], NULL, &g_podium_rect[1]);
    if (g_podium[2]) SDL_RenderCopy(g_renderer, g_podium[2], NULL, &g_podium_rect[2]);

    for (int i = 0; i < 3; ++i) {
        top_render_sprite(&g_winner[i]);
    }

    top_draw_text_center(g_title_font, "TOP SCORES", g_panel_rect.x + g_panel_rect.w / 2, g_panel_rect.y + 22,
                         (SDL_Color){255, 236, 180, 255});

    {
        int score_y = g_panel_rect.y + g_panel_rect.h / 6 + 24;
        int score_gap = g_panel_rect.h / 16;
        if (score_y < g_panel_rect.y + 118) score_y = g_panel_rect.y + 118;
        if (score_gap < 42) score_gap = 42;
        if (score_gap > 56) score_gap = 56;
        top_render_score_line(1,
                              g_leaderboard_count > 0 ? &g_leaderboard[0] : NULL,
                              score_y,
                              (SDL_Color){245, 247, 255, 255});
        top_render_score_line(2,
                              g_leaderboard_count > 1 ? &g_leaderboard[1] : NULL,
                              score_y + score_gap,
                              (SDL_Color){245, 247, 255, 255});
        top_render_score_line(3,
                              g_leaderboard_count > 2 ? &g_leaderboard[2] : NULL,
                              score_y + score_gap * 2,
                              (SDL_Color){245, 247, 255, 255});
    }

    top_ensure_button_font();
    if (g_button_font) {
        prototype_button_render(&g_back_button, g_renderer, g_button_font);
    }

    if (g_fade_alpha > 0.0f) {
        int render_w = 1280;
        int render_h = 720;
        top_get_render_size(&render_w, &render_h);

        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0x0D, 0x18, 0x28, (Uint8)g_fade_alpha);
        SDL_Rect fade_rect = {0, 0, render_w, render_h};
        SDL_RenderFillRect(g_renderer, &fade_rect);
    }
}

int top_scores_scene_consume_return_request(void)
{
    int out = g_return_requested;
    g_return_requested = 0;
    return out;
}

void top_scores_scene_cleanup(void)
{
    if (!g_initialized) return;

    top_scores_scene_leave();

    if (g_background) {
        SDL_DestroyTexture(g_background);
        g_background = NULL;
    }
    if (g_snow) {
        SDL_DestroyTexture(g_snow);
        g_snow = NULL;
    }
    if (g_panel) {
        SDL_DestroyTexture(g_panel);
        g_panel = NULL;
    }
    for (int i = 0; i < 3; ++i) {
        if (g_podium[i]) {
            SDL_DestroyTexture(g_podium[i]);
            g_podium[i] = NULL;
        }
    }
    top_destroy_sprite(&g_kid);
    top_destroy_sprite(&g_car);
    top_destroy_sprite(&g_heli);
    for (int i = 0; i < 3; ++i) {
        top_destroy_sprite(&g_winner[i]);
    }

    if (g_title_font) {
        TTF_CloseFont(g_title_font);
        g_title_font = NULL;
    }
    if (g_entry_font) {
        TTF_CloseFont(g_entry_font);
        g_entry_font = NULL;
    }
    if (g_number_font) {
        TTF_CloseFont(g_number_font);
        g_number_font = NULL;
    }
    if (g_button_font) {
        TTF_CloseFont(g_button_font);
        g_button_font = NULL;
    }

    if (g_click_sfx) {
        Mix_FreeChunk(g_click_sfx);
        g_click_sfx = NULL;
    }
    if (g_menu_music) {
        Mix_FreeChunk(g_menu_music);
        g_menu_music = NULL;
    }

    g_initialized = 0;
    g_active = 0;
    g_return_requested = 0;
    g_back_button = (Button){0};
    g_renderer = NULL;
    g_window = NULL;
}
