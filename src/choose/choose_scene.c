#include "choose_scene.h"
#include "asset_paths.h"
#include "mainmenu_headers.h"
#include "ui_shared.h"
#include "debug_log.h"
#include "game_progress.h"
#include "skin_registry.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#define CHOOSE_SCREEN_W 1280
#define CHOOSE_SCREEN_H 720
#define CHOOSE_FRAME_COLS 6
#define CHOOSE_FRAME_ROWS 6
#define CHOOSE_TOTAL_FRAMES 36
#define CHOOSE_SNOW_MAX 100
#define CHOOSE_MAX_NAME_LENGTH 18
#define CHOOSE_CONTROL_COUNT 3
#define CHOOSE_FADE_SPEED 520.0f
#define CHOOSE_NAME_PANEL_W 360
#define CHOOSE_NAME_PANEL_H 360

typedef enum {
    CHOOSE_STATE_FADE_IN = 0,
    CHOOSE_STATE_SELECT,
    CHOOSE_STATE_NAME_INPUT,
    CHOOSE_STATE_FADE_OUT,
    CHOOSE_STATE_EXIT
} ChooseGameState;

typedef enum {
    CHOOSE_BUTTON_STYLE_TEXTURE = 0,
    CHOOSE_BUTTON_STYLE_TEXT = 1
} ChooseButtonStyle;

typedef struct {
    float x;
    float y;
    float speed;
    float drift;
    int size;
} ChooseSnowflake;

typedef struct {
    SDL_Texture* texture;
    int frame_width;
    int frame_height;
    int current_frame;
    float frame_time;
    float frame_delay;
    int loop;
    int finished;
} ChooseAnimation;

typedef struct {
    ChooseAnimation idle;
    ChooseAnimation select;
    int skin_id;
    char name[64];
    char desc1[64];
} ChooseCharacter;

typedef struct {
    ChooseAnimation idle;
    ChooseAnimation pressed;
    int is_pressed;
    SDL_Rect rect;
    float scale;
    float target_scale;
    float bounce_offset;
    int is_static;
    float press_timer;
    ChooseButtonStyle style;
    char label[32];
} ChooseButton;

typedef struct {
    SDL_Renderer* renderer;

    SDL_Texture* background;
    SDL_Texture* container_tex;
    SDL_Texture* enter_name_tex;
    SDL_Texture* left_arrow_tex;
    SDL_Texture* right_arrow_tex;
    SDL_Texture* snow_tex;

    Mix_Chunk* click_sound;
    Mix_Chunk* select_sound;
    Mix_Chunk* menu_music;
    int menu_music_channel;
    int music_volume;

    TTF_Font* font_big;
    TTF_Font* font_small;
    TTF_Font* button_font;
    TTF_Font* card_id_font;

    SkinDefinition skins[SKIN_REGISTRY_MAX];
    int skin_count;
    ChooseCharacter chars[SKIN_REGISTRY_MAX];
    ChooseSnowflake snowflakes[CHOOSE_SNOW_MAX];

    ChooseButton confirm_btn;
    ChooseButton back_btn;
    ChooseButton name_confirm_btn;
    ChooseButton name_return_btn;

    SDL_Rect player_card_rects[2];
    SDL_Rect left_arrow_rects[2];
    SDL_Rect right_arrow_rects[2];
    SDL_Rect input_select_rects[2];
    SDL_Rect name_panel_rect;

    int selected_mode;
    int duo_mode;
    int selected_chars[2];
    int current_chars[2];
    int selected_controls[2];
    int active_player;
    int focused_button;
    int focus_count;

    char player_name[CHOOSE_MAX_NAME_LENGTH + 1];
    int name_length;

    ChooseGameState state;
    float fade_alpha;
    float last_delta;

    int initialized;
    int active;
    int return_requested;
    int start_game_requested;
    int resume_from_save;
    int save_enabled;
    GameSelection start_selection;
} ChooseSceneContext;

static ChooseSceneContext g_ctx = {
    .menu_music_channel = -1,
    .music_volume = MIX_MAX_VOLUME,
    .selected_mode = 1,
    .selected_chars = {0, 1},
    .current_chars = {0, 1},
    .selected_controls = {0, 1},
    .focus_count = 3,
    .state = CHOOSE_STATE_FADE_IN,
    .fade_alpha = 255.0f,
    .last_delta = 0.016f
};

#define g_renderer g_ctx.renderer
#define g_background g_ctx.background
#define g_container_tex g_ctx.container_tex
#define g_enter_name_tex g_ctx.enter_name_tex
#define g_left_arrow_tex g_ctx.left_arrow_tex
#define g_right_arrow_tex g_ctx.right_arrow_tex
#define g_snow_tex g_ctx.snow_tex
#define g_click_sound g_ctx.click_sound
#define g_select_sound g_ctx.select_sound
#define g_menu_music g_ctx.menu_music
#define g_menu_music_channel g_ctx.menu_music_channel
#define g_music_volume g_ctx.music_volume
#define g_font_big g_ctx.font_big
#define g_font_small g_ctx.font_small
#define g_button_font g_ctx.button_font
#define g_card_id_font g_ctx.card_id_font
#define g_skins g_ctx.skins
#define g_skin_count g_ctx.skin_count
#define g_chars g_ctx.chars
#define g_snowflakes g_ctx.snowflakes
#define g_confirm_btn g_ctx.confirm_btn
#define g_back_btn g_ctx.back_btn
#define g_name_confirm_btn g_ctx.name_confirm_btn
#define g_name_return_btn g_ctx.name_return_btn
#define g_player_card_rects g_ctx.player_card_rects
#define g_left_arrow_rects g_ctx.left_arrow_rects
#define g_right_arrow_rects g_ctx.right_arrow_rects
#define g_input_select_rects g_ctx.input_select_rects
#define g_name_panel_rect g_ctx.name_panel_rect
#define g_selected_mode g_ctx.selected_mode
#define g_duo_mode g_ctx.duo_mode
#define g_selected_chars g_ctx.selected_chars
#define g_current_chars g_ctx.current_chars
#define g_selected_controls g_ctx.selected_controls
#define g_active_player g_ctx.active_player
#define g_focused_button g_ctx.focused_button
#define g_focus_count g_ctx.focus_count
#define g_player_name g_ctx.player_name
#define g_name_length g_ctx.name_length
#define g_state g_ctx.state
#define g_fade_alpha g_ctx.fade_alpha
#define g_last_delta g_ctx.last_delta
#define g_initialized g_ctx.initialized
#define g_active g_ctx.active
#define g_return_requested g_ctx.return_requested
#define g_start_game_requested g_ctx.start_game_requested
#define g_resume_from_save g_ctx.resume_from_save
#define g_save_enabled g_ctx.save_enabled
#define g_start_selection g_ctx.start_selection

static SDL_Texture* choose_load_texture(const char* path)
{
    if (!g_renderer || !path) return NULL;

    SDL_Texture* tex = ui_load_texture(g_renderer, path);
    if (!tex) {
        SDL_Log("Choose texture load failed (%s): %s", path, IMG_GetError());
        debug_logf("choose: texture load failed path=%s err=%s", path, IMG_GetError());
        return NULL;
    }
    return tex;
}

static SDL_Texture* choose_load_optional_texture(const char* path)
{
    if (!g_renderer || !path) return NULL;
    return ui_load_texture(g_renderer, path);
}

static void choose_render_text(TTF_Font* font, const char* text, SDL_Color color, int x, int y)
{
    ui_draw_text_left(g_renderer, font, text, x, y, color);
}

static TTF_Font* choose_open_menu_font(int point_size)
{
    const char* font_candidates[] = {
        ASSET_BUTTON_FONT,
        ASSET_CHOOSE_FONT,
        ASSET_OPTIONS_FONT,
        ASSET_MAIN_MENU_FONT_TEXT,
        ASSET_MAIN_MENU_FONT_OPTIONS,
        ASSET_MAIN_MENU_FONT_TITLE
    };
    const int count = (int)(sizeof(font_candidates) / sizeof(font_candidates[0]));
    TTF_Font* font = ui_open_font_from_candidates(font_candidates, count, point_size, 0);
    if (font) return font;

    return ui_open_arial_font(point_size, 0);
}

static const char* choose_get_control_name(int control_index)
{
    static const char* control_names[CHOOSE_CONTROL_COUNT] = {
        "WASD",
        "ARROWS",
        "CONTROLLER"
    };

    if (control_index < 0 || control_index >= CHOOSE_CONTROL_COUNT) {
        return control_names[0];
    }

    return control_names[control_index];
}

static int choose_controls_conflict(int a, int b)
{
    if (a == b) return 1;
    if ((a == 1 || a == 2) && (b == 1 || b == 2)) return 1;
    return 0;
}

static int choose_character_count(void)
{
    if (g_skin_count <= 0) return 1;
    if (g_skin_count > SKIN_REGISTRY_MAX) return SKIN_REGISTRY_MAX;
    return g_skin_count;
}

static int choose_clamp_character_index(int idx)
{
    int count = choose_character_count();
    if (count <= 0) return 0;

    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    return idx;
}

static int choose_skin_id_from_index(int idx)
{
    idx = choose_clamp_character_index(idx);

    if (g_skin_count <= 0) return 1;
    if (idx < 0 || idx >= g_skin_count) return g_skins[0].id;
    if (g_chars[idx].skin_id > 0) return g_chars[idx].skin_id;
    return g_skins[idx].id > 0 ? g_skins[idx].id : 1;
}

static int choose_index_from_skin_id(int skin_id)
{
    if (g_skin_count <= 0) return 0;
    if (skin_id <= 0) return 0;

    for (int i = 0; i < g_skin_count; ++i) {
        if (g_chars[i].skin_id == skin_id || g_skins[i].id == skin_id) {
            return i;
        }
    }

    return 0;
}

static void choose_trigger_character_select(int char_index, int* selected_char);

static int choose_next_different_character_index(int used_index)
{
    int count = choose_character_count();
    int used_skin = choose_skin_id_from_index(used_index);

    for (int i = 0; i < count; ++i) {
        if (i != used_index && choose_skin_id_from_index(i) != used_skin) {
            return i;
        }
    }

    return choose_clamp_character_index(used_index);
}

static void choose_enforce_unique_duo_skins(int changed_player)
{
    int other_player = changed_player == 0 ? 1 : 0;

    if (!g_duo_mode || choose_character_count() < 2) return;
    if (changed_player < 0 || changed_player > 1) changed_player = 0;

    g_selected_chars[changed_player] = choose_clamp_character_index(g_selected_chars[changed_player]);
    g_current_chars[changed_player] = choose_clamp_character_index(g_current_chars[changed_player]);
    g_selected_chars[other_player] = choose_clamp_character_index(g_selected_chars[other_player]);
    g_current_chars[other_player] = choose_clamp_character_index(g_current_chars[other_player]);

    if (choose_skin_id_from_index(g_selected_chars[changed_player]) ==
        choose_skin_id_from_index(g_selected_chars[other_player])) {
        int replacement = choose_next_different_character_index(g_selected_chars[changed_player]);
        g_selected_chars[other_player] = replacement;
        g_current_chars[other_player] = replacement;
    }
}

static void choose_set_player_character(int player_index, int char_index)
{
    if (player_index < 0 || player_index > 1) return;
    char_index = choose_clamp_character_index(char_index);
    choose_trigger_character_select(char_index, &g_selected_chars[player_index]);
    g_current_chars[player_index] = char_index;
    choose_enforce_unique_duo_skins(player_index);
}

static int choose_init_animation(ChooseAnimation* anim, const char* path, float delay, int loop)
{
    if (!anim || !path) return 0;

    memset(anim, 0, sizeof(*anim));
    anim->texture = choose_load_texture(path);
    if (!anim->texture) return 0;

    int w = 0;
    int h = 0;
    SDL_QueryTexture(anim->texture, NULL, NULL, &w, &h);

    anim->frame_width = (CHOOSE_FRAME_COLS > 0) ? (w / CHOOSE_FRAME_COLS) : w;
    anim->frame_height = (CHOOSE_FRAME_ROWS > 0) ? (h / CHOOSE_FRAME_ROWS) : h;
    anim->current_frame = 0;
    anim->frame_time = 0.0f;
    anim->frame_delay = delay;
    anim->loop = loop;
    anim->finished = 0;

    return 1;
}

static void choose_destroy_animation(ChooseAnimation* anim)
{
    if (!anim) return;

    if (anim->texture) {
        SDL_DestroyTexture(anim->texture);
        anim->texture = NULL;
    }

    anim->frame_width = 0;
    anim->frame_height = 0;
    anim->current_frame = 0;
    anim->frame_time = 0.0f;
    anim->frame_delay = 0.0f;
    anim->loop = 0;
    anim->finished = 0;
}

static void choose_update_animation(ChooseAnimation* anim, float delta)
{
    if (!anim || !anim->texture || anim->finished) return;

    anim->frame_time += delta;
    if (anim->frame_time < anim->frame_delay) return;

    anim->frame_time = 0.0f;
    anim->current_frame++;

    if (anim->current_frame >= CHOOSE_TOTAL_FRAMES) {
        if (anim->loop) {
            anim->current_frame = 0;
        } else {
            anim->current_frame = CHOOSE_TOTAL_FRAMES - 1;
            anim->finished = 1;
        }
    }
}

static void choose_render_animation(const ChooseAnimation* anim, int x, int y, int w, int h)
{
    if (!g_renderer || !anim || !anim->texture) return;

    SDL_Rect src = {
        (anim->current_frame % CHOOSE_FRAME_COLS) * anim->frame_width,
        (anim->current_frame / CHOOSE_FRAME_COLS) * anim->frame_height,
        anim->frame_width,
        anim->frame_height
    };

    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(g_renderer, anim->texture, &src, &dst);
}

static void choose_init_text_button(ChooseButton* btn, SDL_Rect rect, const char* label)
{
    if (!btn || !label) return;

    memset(btn, 0, sizeof(*btn));
    btn->rect = rect;
    btn->scale = 1.0f;
    btn->target_scale = 1.0f;
    btn->bounce_offset = 0.0f;
    btn->is_static = 1;
    btn->style = CHOOSE_BUTTON_STYLE_TEXT;
    snprintf(btn->label, sizeof(btn->label), "%s", label);
}

static int choose_handle_button_event(ChooseButton* btn, const SDL_Event* e)
{
    if (!btn || !e) return 0;

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x;
        int my = e->button.y;

        if (SDL_PointInRect(&(SDL_Point){mx, my}, &btn->rect)) {
            btn->is_pressed = 1;
            btn->press_timer = 0.13f;
            return 1;
        }
    }

    return 0;
}

static void choose_ensure_button_font(void)
{
    if (g_button_font) return;

    g_button_font = choose_open_menu_font(20);

    if (!g_button_font) {
        SDL_Log("Choose button font load warning: %s", TTF_GetError());
        debug_logf("choose: button font load warning err=%s", TTF_GetError());
    }
}

static void choose_render_styled_button(ChooseButton* btn, const SDL_Rect* rect, int hovered, int pressed)
{
    if (!g_renderer || !btn || !rect) return;
    choose_ensure_button_font();
    if (!g_button_font) return;

    render_main_menu_style_button(g_renderer, g_button_font, rect, btn->label, hovered, pressed);
}

static void choose_render_button(ChooseButton* btn, float delta)
{
    if (!btn) return;

    int hovered = 0;
    if (btn->style == CHOOSE_BUTTON_STYLE_TEXT) {
        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        hovered = SDL_PointInRect(&(SDL_Point){mx, my}, &btn->rect);
        btn->target_scale = hovered ? 1.07f : 1.0f;
        btn->bounce_offset = hovered ? -1.0f : 0.0f;
    }

    {
        float diff = btn->target_scale - btn->scale;
        btn->scale += diff * 11.0f * delta;
        if (fabsf(diff) < 0.001f) {
            btn->scale = btn->target_scale;
        }
    }

    SDL_Rect scaled_rect = {
        btn->rect.x + (int)((btn->rect.w * (1.0f - btn->scale)) / 2.0f),
        btn->rect.y + (int)((btn->rect.h * (1.0f - btn->scale)) / 2.0f),
        (int)(btn->rect.w * btn->scale),
        (int)(btn->rect.h * btn->scale)
    };
    scaled_rect.y += (int)btn->bounce_offset;

    if (btn->style == CHOOSE_BUTTON_STYLE_TEXT) {
        if (btn->is_pressed) scaled_rect.y += 1;
        choose_render_styled_button(btn, &scaled_rect, hovered, btn->is_pressed);
        if (btn->is_pressed) {
            btn->press_timer -= delta;
            if (btn->press_timer <= 0.0f) {
                btn->is_pressed = 0;
                btn->press_timer = 0.0f;
            }
        }
        return;
    }

    if (btn->is_pressed) {
        if (btn->pressed.texture) {
            choose_render_animation(&btn->pressed, scaled_rect.x, scaled_rect.y, scaled_rect.w, scaled_rect.h);
        }
        btn->press_timer -= delta;
        if (btn->press_timer <= 0.0f) {
            btn->is_pressed = 0;
            btn->target_scale = 1.0f;
            btn->press_timer = 0.0f;
        }
    } else if (btn->idle.texture) {
        choose_render_animation(&btn->idle, scaled_rect.x, scaled_rect.y, scaled_rect.w, scaled_rect.h);
    }
}

static void choose_init_snow(void)
{
    for (int i = 0; i < CHOOSE_SNOW_MAX; ++i) {
        g_snowflakes[i].x = (float)(rand() % CHOOSE_SCREEN_W);
        g_snowflakes[i].y = (float)(rand() % CHOOSE_SCREEN_H);
        g_snowflakes[i].speed = 40.0f + (float)(rand() % 80);
        g_snowflakes[i].drift = -30.0f + (float)(rand() % 60);
        g_snowflakes[i].size = 10 + (rand() % 3);
    }
}

static void choose_update_snow(float delta)
{
    for (int i = 0; i < CHOOSE_SNOW_MAX; ++i) {
        g_snowflakes[i].y += g_snowflakes[i].speed * delta;
        g_snowflakes[i].x += g_snowflakes[i].drift * delta;

        if (g_snowflakes[i].y > CHOOSE_SCREEN_H) {
            g_snowflakes[i].y = (float)(-g_snowflakes[i].size);
            g_snowflakes[i].x = (float)(rand() % CHOOSE_SCREEN_W);
        }

        if (g_snowflakes[i].x > CHOOSE_SCREEN_W) g_snowflakes[i].x = 0.0f;
        if (g_snowflakes[i].x < 0.0f) g_snowflakes[i].x = (float)CHOOSE_SCREEN_W;
    }
}

static void choose_render_snow(void)
{
    if (!g_renderer || !g_snow_tex) return;

    for (int i = 0; i < CHOOSE_SNOW_MAX; ++i) {
        SDL_Rect dst = {
            (int)g_snowflakes[i].x,
            (int)g_snowflakes[i].y,
            g_snowflakes[i].size + 10,
            g_snowflakes[i].size + 10
        };
        SDL_RenderCopy(g_renderer, g_snow_tex, NULL, &dst);
    }
}

static void choose_trigger_character_select(int char_index, int* selected_char)
{
    if (!selected_char) return;
    if (char_index < 0 || char_index >= choose_character_count()) return;

    if (g_select_sound) Mix_PlayChannel(-1, g_select_sound, 0);
    *selected_char = char_index;
    if (g_chars[char_index].select.texture) {
        g_chars[char_index].select.current_frame = 0;
        g_chars[char_index].select.finished = 0;
    }
}

static void choose_render_arrow_placeholder(SDL_Rect rect, int is_hovered)
{
    if (!g_renderer) return;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, is_hovered ? 255 : 220);
    SDL_RenderDrawLine(g_renderer, rect.x, rect.y + rect.h / 2, rect.x + rect.w, rect.y + rect.h / 2);
    SDL_RenderDrawLine(g_renderer, rect.x + rect.w / 2, rect.y + rect.h / 4, rect.x + rect.w, rect.y + rect.h / 2);
    SDL_RenderDrawLine(g_renderer, rect.x + rect.w / 2, rect.y + (rect.h * 3) / 4, rect.x + rect.w, rect.y + rect.h / 2);
}

static void choose_render_arrow_texture(SDL_Texture* texture, const SDL_Rect* rect, SDL_RendererFlip flip)
{
    if (!g_renderer || !texture || !rect) return;
    SDL_RenderCopyEx(g_renderer, texture, NULL, rect, 0.0, NULL, flip);
}

static void choose_render_focus_outline(SDL_Rect rect)
{
    if (!g_renderer) return;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 255, 230, 120, 255);
    SDL_RenderDrawRect(g_renderer, &rect);

    SDL_Rect outer = {rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
    SDL_RenderDrawRect(g_renderer, &outer);
}

static void choose_render_input_selector(int player_index, int hovered, int focused)
{
    SDL_Rect rect = {0, 0, 0, 0};
    char control_label[64];
    int active = hovered || focused;

    if (!g_renderer) return;
    if (player_index < 0 || player_index > 1) return;

    rect = g_input_select_rects[player_index];
    if (active) {
        int scaled_w = (int)(rect.w * 1.07f);
        int scaled_h = (int)(rect.h * 1.07f);
        rect.x -= (scaled_w - rect.w) / 2;
        rect.y -= (scaled_h - rect.h) / 2;
        rect.w = scaled_w;
        rect.h = scaled_h;
        rect.y -= 1;
    }

    choose_ensure_button_font();
    if (g_button_font) {
        snprintf(control_label, sizeof(control_label), "%s", choose_get_control_name(g_selected_controls[player_index]));
        render_main_menu_style_button(g_renderer, g_button_font, &rect, control_label, active, 0);
        return;
    }

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 24, 36, 58, 230);
    SDL_RenderFillRect(g_renderer, &rect);
    SDL_SetRenderDrawColor(g_renderer, 255, 210, 120, active ? 255 : 200);
    SDL_RenderDrawRect(g_renderer, &rect);

    if (g_font_small) {
        SDL_Color white = {255, 255, 255, 255};
        int text_w = 0;
        int text_h = 0;
        int text_x = rect.x + 10;
        int text_y = rect.y + 8;
        const char* control_name = choose_get_control_name(g_selected_controls[player_index]);
        if (TTF_SizeUTF8(g_font_small, control_name, &text_w, &text_h) == 0) {
            text_x = rect.x + (rect.w - text_w) / 2;
            text_y = rect.y + (rect.h - text_h) / 2;
        }
        choose_render_text(g_font_small, control_name, white, text_x, text_y);
    }
}

static void choose_render_fade(int alpha)
{
    if (!g_renderer) return;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, alpha);
    SDL_Rect r = {0, 0, CHOOSE_SCREEN_W, CHOOSE_SCREEN_H};
    SDL_RenderFillRect(g_renderer, &r);
}

static void choose_stop_music(void)
{
    if (g_menu_music_channel >= 0) {
        Mix_HaltChannel(g_menu_music_channel);
        g_menu_music_channel = -1;
    }
}

static void choose_start_music(void)
{
    choose_stop_music();

    if (!g_menu_music) return;

    g_menu_music_channel = Mix_PlayChannel(-1, g_menu_music, -1);
    if (g_menu_music_channel >= 0) {
        Mix_Volume(g_menu_music_channel, g_music_volume);
    }
}

static void choose_reset_runtime_state(void)
{
    int second_index = (choose_character_count() > 1) ? 1 : 0;

    g_state = CHOOSE_STATE_FADE_IN;
    g_fade_alpha = 255.0f;
    g_last_delta = 0.016f;

    g_player_name[0] = '\0';
    g_name_length = 0;

    g_selected_chars[0] = 0;
    g_selected_chars[1] = second_index;
    g_current_chars[0] = 0;
    g_current_chars[1] = second_index;
    g_selected_controls[0] = 0;
    g_selected_controls[1] = 1;
    g_active_player = 0;
    g_focused_button = 0;
    g_focus_count = g_duo_mode ? 6 : 4;

    g_return_requested = 0;
    g_start_game_requested = 0;
    g_resume_from_save = 0;
    g_start_selection = (GameSelection){0};
}

static SDL_Rect choose_get_name_input_rect(void)
{
    SDL_Rect r = {0, 0, 0, 0};

    r.x = g_name_panel_rect.x + (g_name_panel_rect.w * 8) / 100;
    r.y = g_name_panel_rect.y + (g_name_panel_rect.h * 48) / 100;
    r.w = (g_name_panel_rect.w * 84) / 100;
    r.h = (g_name_panel_rect.h * 23) / 100;

    if (r.w < 120) r.w = 120;
    if (r.h < 34) r.h = 34;
    return r;
}

static void choose_setup_layout(void)
{
    const int solo_card_w = 292;
    const int solo_card_h = 372;
    const int solo_card_y = 56;
    SDL_Rect solo_card = {(CHOOSE_SCREEN_W - solo_card_w) / 2, solo_card_y, solo_card_w, solo_card_h};

    const int duo_card_w = 258;
    const int duo_card_h = 356;
    const int duo_card_y = 60;
    const int duo_gap = 118;
    const int duo_total_w = duo_card_w * 2 + duo_gap;
    const int duo_left_x = (CHOOSE_SCREEN_W - duo_total_w) / 2;
    SDL_Rect duo_cards[2] = {
        {duo_left_x, duo_card_y, duo_card_w, duo_card_h},
        {duo_left_x + duo_card_w + duo_gap, duo_card_y, duo_card_w, duo_card_h}
    };

    if (g_duo_mode) {
        g_player_card_rects[0] = duo_cards[0];
        g_player_card_rects[1] = duo_cards[1];
    } else {
        g_player_card_rects[0] = solo_card;
        g_player_card_rects[1] = solo_card;
    }

    for (int p = 0; p < 2; ++p) {
        SDL_Rect card = g_player_card_rects[p];
        int arrow_w = g_duo_mode ? 44 : 50;
        int arrow_h = g_duo_mode ? 62 : 70;
        int input_w = (card.w * 74) / 100;
        int input_h = 48;
        int input_x = 0;
        int input_y = card.y + card.h + 14;
        if (input_w < 164) input_w = 164;
        input_x = card.x + (card.w - input_w) / 2;
        g_left_arrow_rects[p] = (SDL_Rect){card.x - (arrow_w + 14), card.y + card.h / 2 - arrow_h / 2, arrow_w, arrow_h};
        g_right_arrow_rects[p] = (SDL_Rect){card.x + card.w + 14, card.y + card.h / 2 - arrow_h / 2, arrow_w, arrow_h};
        g_input_select_rects[p] = (SDL_Rect){input_x, input_y, input_w, input_h};
    }

    const int menu_button_w = 330;
    const int menu_button_h = 58;
    const int menu_button_gap = 10;
    const int menu_button_x = (CHOOSE_SCREEN_W - menu_button_w) / 2;
    const int confirm_button_y = CHOOSE_SCREEN_H - (menu_button_h * 2 + menu_button_gap + 18);
    const int return_button_y = confirm_button_y + menu_button_h + menu_button_gap;

    choose_init_text_button(&g_confirm_btn,
                            (SDL_Rect){menu_button_x, confirm_button_y, menu_button_w, menu_button_h},
                            "CONFIRM");

    choose_init_text_button(&g_back_btn,
                            (SDL_Rect){menu_button_x, return_button_y, menu_button_w, menu_button_h},
                            "RETURN");

    const int name_panel_w = CHOOSE_NAME_PANEL_W;
    const int name_panel_h = CHOOSE_NAME_PANEL_H;
    const int name_panel_x = (CHOOSE_SCREEN_W - name_panel_w) / 2;
    const int name_button_w = name_panel_w - 84;
    const int name_button_h = 50;
    const int name_button_gap = 10;
    const int name_gap_after_panel = 12;
    const int name_group_h = name_panel_h + name_gap_after_panel + (name_button_h * 2) + name_button_gap;
    int name_panel_y = (CHOOSE_SCREEN_H - name_group_h) / 2;
    const int name_button_x = name_panel_x + (name_panel_w - name_button_w) / 2;
    int name_confirm_y = 0;
    int name_return_y = 0;

    if (name_panel_y < 16) name_panel_y = 16;

    name_confirm_y = name_panel_y + name_panel_h + name_gap_after_panel;
    name_return_y = name_confirm_y + name_button_h + name_button_gap;

    g_name_panel_rect = (SDL_Rect){name_panel_x, name_panel_y, name_panel_w, name_panel_h};

    choose_init_text_button(&g_name_confirm_btn,
                            (SDL_Rect){name_button_x, name_confirm_y, name_button_w, name_button_h},
                            "CONFIRM");

    choose_init_text_button(&g_name_return_btn,
                            (SDL_Rect){name_button_x, name_return_y, name_button_w, name_button_h},
                            "RETURN");
}

static int choose_is_allowed_name_char(unsigned char c)
{
    return isalnum(c) || c == '-' || c == '_';
}

static int choose_sanitize_name(const char* input, char* output, size_t output_size)
{
    size_t out_len = 0;

    if (!output || output_size == 0) return 0;
    output[0] = '\0';
    if (!input) return 0;

    for (size_t i = 0; input[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)input[i];

        if (!choose_is_allowed_name_char(c)) continue;
        if (out_len + 1 >= output_size || out_len >= CHOOSE_MAX_NAME_LENGTH) break;
        output[out_len++] = (char)c;
    }

    output[out_len] = '\0';
    return out_len > 0;
}

static void choose_ensure_default_player_name(void)
{
    if (g_name_length > 0) return;

    snprintf(g_player_name, sizeof(g_player_name), "Player");
    g_name_length = (int)strlen(g_player_name);
}

static void choose_stage_start_game_selection(void)
{
    GameProgress progress = {0};

    choose_enforce_unique_duo_skins(g_active_player);

    g_start_selection.duo_mode = g_duo_mode ? 1 : 0;
    g_start_selection.player_count = g_duo_mode ? 2 : 1;
    g_start_selection.selected_skin[0] = choose_skin_id_from_index(g_selected_chars[0]);
    g_start_selection.selected_skin[1] = g_duo_mode ? choose_skin_id_from_index(g_selected_chars[1]) : 0;
    g_start_selection.control_scheme[0] = g_selected_controls[0] + 1;
    g_start_selection.control_scheme[1] = g_duo_mode ? (g_selected_controls[1] + 1) : 0;
    g_start_selection.resume_from_save = g_resume_from_save ? 1 : 0;
    g_start_selection.save_enabled = g_save_enabled ? 1 : 0;
    game_progress_set_player_name(g_player_name);
    game_progress_set_selection(&g_start_selection);
    game_progress_get(&progress);
    progress.save_enabled = g_start_selection.save_enabled ? 1 : 0;
    game_progress_set(&progress);
    game_progress_mark_started_game(1);
    g_start_game_requested = 1;
    /* Force a deterministic handoff to gameplay on the next main-loop tick. */
    g_state = CHOOSE_STATE_EXIT;
    g_return_requested = 1;
    g_fade_alpha = 255.0f;
    debug_logf("choose: start requested name='%s' duo=%d players=%d skin1=%d skin2=%d ctrl1=%d ctrl2=%d",
               g_player_name,
               g_start_selection.duo_mode,
               g_start_selection.player_count,
               g_start_selection.selected_skin[0],
               g_start_selection.selected_skin[1],
               g_start_selection.control_scheme[0],
               g_start_selection.control_scheme[1]);
}

static int choose_init_characters(void)
{
    int loaded_count = 0;

    memset(g_skins, 0, sizeof(g_skins));
    memset(g_chars, 0, sizeof(g_chars));
    g_skin_count = skin_registry_load(g_skins, SKIN_REGISTRY_MAX);
    if (g_skin_count <= 0) {
        debug_logf("choose: no skins discovered");
        return 0;
    }

    loaded_count = 0;
    for (int i = 0; i < g_skin_count; ++i) {
        const float anim_delay = 0.05f;
        int idle_ok = 0;
        int select_ok = 0;
        int idx = loaded_count;

        /* The merged gameplay only supports skins 1 and 2, so keep the menu aligned. */
        if (g_skins[i].id < 1 || g_skins[i].id > 2) {
            continue;
        }

        idle_ok = choose_init_animation(&g_chars[idx].idle, g_skins[i].choose_idle_path, anim_delay, 1);
        if (!idle_ok) {
            debug_logf("choose: failed loading skin preview idle id=%d path=%s",
                       g_skins[i].id,
                       g_skins[i].choose_idle_path);
            continue;
        }

        select_ok = choose_init_animation(&g_chars[idx].select, g_skins[i].choose_select_path, anim_delay, 0);
        if (!select_ok) {
            /* Fallback to idle animation when no dedicated select sheet exists. */
            choose_init_animation(&g_chars[idx].select, g_skins[i].choose_idle_path, anim_delay, 0);
        }

        g_chars[idx].skin_id = g_skins[i].id;
        snprintf(g_chars[idx].name,
                 sizeof(g_chars[idx].name),
                 "%s",
                 g_skins[i].display_name[0] ? g_skins[i].display_name : "Unknown");
        snprintf(g_chars[idx].desc1, sizeof(g_chars[idx].desc1), "ID %03d", g_skins[i].id);
        debug_logf("choose: skin id=%d name='%s' idle='%s' select='%s'",
                   g_skins[i].id,
                   g_skins[i].display_name,
                   g_skins[i].choose_idle_path,
                   g_skins[i].choose_select_path);
        loaded_count++;
    }

    g_skin_count = loaded_count;
    debug_logf("choose: loaded %d selectable skin(s)", g_skin_count);
    return g_skin_count > 0;
}

static void choose_destroy_characters(void)
{
    for (int i = 0; i < g_skin_count; ++i) {
        choose_destroy_animation(&g_chars[i].idle);
        choose_destroy_animation(&g_chars[i].select);
        g_chars[i].skin_id = 0;
        g_chars[i].name[0] = '\0';
        g_chars[i].desc1[0] = '\0';
    }
    g_skin_count = 0;
}

static void choose_update_character_animations(float delta)
{
    for (int i = 0; i < g_skin_count; ++i) {
        int selected_for_any = g_duo_mode
                                   ? ((g_selected_chars[0] == i) || (g_selected_chars[1] == i))
                                   : (g_selected_chars[0] == i);

        if (selected_for_any && g_chars[i].select.texture && !g_chars[i].select.finished) {
            choose_update_animation(&g_chars[i].select, delta);
        } else {
            choose_update_animation(&g_chars[i].idle, delta);
        }
    }
}

static int choose_get_confirm_focus_index(void)
{
    return g_duo_mode ? 4 : 2;
}

static int choose_get_return_focus_index(void)
{
    return g_duo_mode ? 5 : 3;
}

static void choose_apply_button_action_select(int which)
{
    if (which == 0) {
        g_focused_button = choose_get_confirm_focus_index();
        if (g_click_sound) Mix_PlayChannel(-1, g_click_sound, 0);
        g_state = CHOOSE_STATE_NAME_INPUT;
    } else {
        g_focused_button = choose_get_return_focus_index();
        if (g_click_sound) Mix_PlayChannel(-1, g_click_sound, 0);
        g_state = CHOOSE_STATE_FADE_OUT;
    }
}

static void choose_play_click(void)
{
    if (g_click_sound) Mix_PlayChannel(-1, g_click_sound, 0);
}

static int choose_can_change_focused_card(void)
{
    return (!g_duo_mode && g_focused_button == 0) || (g_duo_mode && g_focused_button <= 1);
}

static int choose_can_change_focused_input(void)
{
    return (!g_duo_mode && g_focused_button == 1) ||
           (g_duo_mode && g_focused_button >= 2 && g_focused_button <= 3);
}

static int choose_get_focused_player(void)
{
    return g_duo_mode ? g_focused_button : 0;
}

static int choose_get_focused_input_player(void)
{
    return g_duo_mode ? (g_focused_button - 2) : 0;
}

static void choose_focus_player(int player_index)
{
    g_focused_button = player_index;
    g_active_player = player_index;
}

static void choose_focus_input(int player_index)
{
    g_focused_button = g_duo_mode ? (player_index + 2) : 1;
    g_active_player = player_index;
}

static void choose_set_player_control(int player_index, int control_index)
{
    int other_player = 0;

    if (player_index < 0 || player_index > 1) return;
    if (control_index < 0 || control_index >= CHOOSE_CONTROL_COUNT) return;

    g_selected_controls[player_index] = control_index;

    if (!g_duo_mode) return;

    other_player = (player_index == 0) ? 1 : 0;
    if (!choose_controls_conflict(g_selected_controls[other_player], control_index)) return;

    for (int i = 0; i < CHOOSE_CONTROL_COUNT; ++i) {
        if (choose_controls_conflict(i, control_index)) continue;
        g_selected_controls[other_player] = i;
        break;
    }
}

static void choose_shift_player_control(int player_index, int step)
{
    int next_control = 0;
    if (player_index < 0 || player_index > 1 || step == 0) return;

    next_control = (g_selected_controls[player_index] + step) % CHOOSE_CONTROL_COUNT;
    if (next_control < 0) next_control += CHOOSE_CONTROL_COUNT;
    choose_set_player_control(player_index, next_control);
}

static void choose_trigger_focused_selection(void)
{
    int p = 0;
    if (!choose_can_change_focused_card()) return;

    p = choose_get_focused_player();
    g_active_player = p;
    choose_set_player_character(p, g_current_chars[p]);
}

static void choose_shift_focused_selection(int step)
{
    int count = choose_character_count();
    int p = 0;
    if (!choose_can_change_focused_card()) return;

    p = choose_get_focused_player();
    g_active_player = p;
    g_current_chars[p] = (g_current_chars[p] + step + count) % count;
    choose_set_player_character(p, g_current_chars[p]);
}

static void choose_shift_focused_control(int step)
{
    int p = 0;
    if (!choose_can_change_focused_input()) return;

    p = choose_get_focused_input_player();
    g_active_player = p;
    choose_shift_player_control(p, step);
}

static void choose_return_to_select_state(void)
{
    g_state = CHOOSE_STATE_SELECT;
    g_focused_button = g_duo_mode ? g_active_player : 0;
}

int choose_scene_init(SDL_Renderer* shared_renderer)
{
    if (!shared_renderer) return 0;

    if (g_initialized) return 1;

    g_renderer = shared_renderer;

    g_background = choose_load_texture(ASSET_CHOOSE_BACKGROUND);
    g_container_tex = choose_load_texture(ASSET_CHOOSE_UI_CONTAINER);
    g_enter_name_tex = choose_load_texture(ASSET_CHOOSE_UI_ENTER_NAME);
    g_left_arrow_tex = choose_load_optional_texture(ASSET_CHOOSE_UI_ARROW_LEFT);
    g_right_arrow_tex = choose_load_optional_texture(ASSET_CHOOSE_UI_ARROW_RIGHT);
    g_snow_tex = choose_load_texture(ASSET_CHOOSE_UI_SNOW);

    g_click_sound = ui_load_wav(ASSET_CHOOSE_SOUND_CLICK);
    if (!g_click_sound) {
        SDL_Log("Choose click sound load warning (%s): %s", ASSET_CHOOSE_SOUND_CLICK, Mix_GetError());
        debug_logf("choose: click sound load warning path=%s err=%s", ASSET_CHOOSE_SOUND_CLICK, Mix_GetError());
    }

    g_select_sound = ui_load_wav(ASSET_CHOOSE_SOUND_SELECT);
    if (!g_select_sound) {
        SDL_Log("Choose select sound load warning (%s): %s", ASSET_CHOOSE_SOUND_SELECT, Mix_GetError());
        debug_logf("choose: select sound load warning path=%s err=%s", ASSET_CHOOSE_SOUND_SELECT, Mix_GetError());
    }

    g_menu_music = ui_load_wav(ASSET_CHOOSE_SOUND_MENU);
    if (!g_menu_music) {
        SDL_Log("Choose menu music load warning (%s): %s", ASSET_CHOOSE_SOUND_MENU, Mix_GetError());
        debug_logf("choose: menu music load warning path=%s err=%s", ASSET_CHOOSE_SOUND_MENU, Mix_GetError());
    }

    g_font_big = choose_open_menu_font(28);
    if (!g_font_big) {
        SDL_Log("Choose font (28) load warning: %s", TTF_GetError());
        debug_logf("choose: font_big load warning err=%s", TTF_GetError());
    }

    g_font_small = choose_open_menu_font(20);
    if (!g_font_small) {
        SDL_Log("Choose font (20) load warning: %s", TTF_GetError());
        debug_logf("choose: font_small load warning err=%s", TTF_GetError());
    }

    g_card_id_font = choose_open_menu_font(20);
    if (!g_card_id_font) {
        SDL_Log("Choose card ID font load warning: %s", TTF_GetError());
        debug_logf("choose: card id font load warning err=%s", TTF_GetError());
    }

    choose_init_snow();
    if (!choose_init_characters()) {
        debug_logf("choose: init failed (no valid skins)");
        g_initialized = 1;
        choose_scene_cleanup();
        return 0;
    }

    g_initialized = 1;
    g_active = 0;
    g_return_requested = 0;

    return 1;
}

void choose_scene_enter(int mode, int start_choice, int save_enabled)
{
    GameProgress progress = {0};
    char save_path[256];
    int load_requested = (start_choice == 0);

    if (!g_initialized) return;

    g_selected_mode = (mode == 2) ? 2 : 1;
    g_duo_mode = (g_selected_mode == 2);
    g_resume_from_save = 0;
    g_save_enabled = save_enabled ? 1 : 0;
    game_progress_copy_last_save_path_for_mode(g_selected_mode, save_path, sizeof(save_path));

    g_active = 1;
    choose_reset_runtime_state();
    if (load_requested && game_progress_load_global_from_path(save_path)) {
        game_progress_get(&progress);
        if (game_progress_is_resumable(&progress)) {
            g_resume_from_save = 1;
            g_save_enabled = 1;
            g_selected_mode = progress.selection.duo_mode ? 2 : 1;
            g_duo_mode = progress.selection.duo_mode ? 1 : 0;

            g_selected_chars[0] = choose_index_from_skin_id(progress.selection.selected_skin[0]);
            g_current_chars[0] = g_selected_chars[0];
            g_selected_controls[0] = progress.selection.control_scheme[0] - 1;
            if (g_selected_controls[0] < 0 || g_selected_controls[0] >= CHOOSE_CONTROL_COUNT) g_selected_controls[0] = 0;

            g_selected_chars[1] = choose_index_from_skin_id(progress.selection.selected_skin[1]);
            g_current_chars[1] = g_selected_chars[1];
            g_selected_controls[1] = progress.selection.control_scheme[1] - 1;
            if (g_selected_controls[1] < 0 || g_selected_controls[1] >= CHOOSE_CONTROL_COUNT) g_selected_controls[1] = 1;
            choose_enforce_unique_duo_skins(0);

            if (progress.player_name[0] != '\0') {
                snprintf(g_player_name, sizeof(g_player_name), "%s", progress.player_name);
                g_name_length = (int)strlen(g_player_name);
            }
            g_focus_count = g_duo_mode ? 6 : 4;
        }
    }
    choose_setup_layout();
    g_start_game_requested = 0;
    g_start_selection = (GameSelection){0};
    debug_logf("choose: enter mode=%d start=%d save_enabled=%d duo=%d", mode, start_choice, g_save_enabled, g_duo_mode);

    if (load_requested && g_resume_from_save) {
        choose_stage_start_game_selection();
        return;
    }

    choose_start_music();
    SDL_StartTextInput();
}

void choose_scene_leave(void)
{
    if (!g_initialized) return;

    g_active = 0;
    choose_stop_music();
    g_start_game_requested = 0;
    debug_logf("choose: leave");
    SDL_StopTextInput();
}

void choose_scene_set_music_volume(int sdl_volume)
{
    if (sdl_volume < 0) sdl_volume = 0;
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;

    g_music_volume = sdl_volume;
    if (g_menu_music_channel >= 0) {
        Mix_Volume(g_menu_music_channel, g_music_volume);
    }
}

static void choose_handle_select_state_event(const SDL_Event* e)
{
    if (!e) return;

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x;
        int my = e->button.y;
        int player_count = g_duo_mode ? 2 : 1;

        for (int p = 0; p < player_count; ++p) {
            if (SDL_PointInRect(&(SDL_Point){mx, my}, &g_left_arrow_rects[p])) {
                choose_focus_player(p);
                choose_shift_focused_selection(-1);
                break;
            }

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &g_right_arrow_rects[p])) {
                choose_focus_player(p);
                choose_shift_focused_selection(1);
                break;
            }

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &g_player_card_rects[p])) {
                choose_focus_player(p);
                choose_trigger_focused_selection();
                break;
            }

            if (SDL_PointInRect(&(SDL_Point){mx, my}, &g_input_select_rects[p])) {
                choose_focus_input(p);
                choose_shift_focused_control(1);
                break;
            }
        }
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_LEFT || e->key.keysym.sym == SDLK_a) {
            if (choose_can_change_focused_input()) {
                choose_shift_focused_control(-1);
            } else {
                choose_shift_focused_selection(-1);
            }
        } else if (e->key.keysym.sym == SDLK_RIGHT || e->key.keysym.sym == SDLK_d) {
            if (choose_can_change_focused_input()) {
                choose_shift_focused_control(1);
            } else {
                choose_shift_focused_selection(1);
            }
        } else if (e->key.keysym.sym == SDLK_SPACE) {
            if (choose_can_change_focused_input()) {
                choose_shift_focused_control(1);
            } else {
                choose_shift_focused_selection(1);
            }
        } else if (e->key.keysym.sym == SDLK_UP || e->key.keysym.sym == SDLK_w) {
            g_focused_button = (g_focused_button + g_focus_count - 1) % g_focus_count;
        } else if (e->key.keysym.sym == SDLK_DOWN || e->key.keysym.sym == SDLK_s) {
            g_focused_button = (g_focused_button + 1) % g_focus_count;
        } else if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) {
            int confirm_focus = choose_get_confirm_focus_index();
            int return_focus = choose_get_return_focus_index();

            if (choose_can_change_focused_card()) {
                choose_trigger_focused_selection();
            } else if (choose_can_change_focused_input()) {
                choose_shift_focused_control(1);
            } else if (g_focused_button == confirm_focus) {
                choose_play_click();
                g_state = CHOOSE_STATE_NAME_INPUT;
            } else if (g_focused_button == return_focus) {
                choose_play_click();
                g_state = CHOOSE_STATE_FADE_OUT;
            }
        } else if (e->key.keysym.sym == SDLK_ESCAPE) {
            g_state = CHOOSE_STATE_FADE_OUT;
        }
    }

    if (choose_handle_button_event(&g_confirm_btn, e)) {
        choose_apply_button_action_select(0);
    }

    if (choose_handle_button_event(&g_back_btn, e)) {
        choose_apply_button_action_select(1);
    }
}

static void choose_handle_name_input_state_event(const SDL_Event* e)
{
    if (!e) return;

    if (e->type == SDL_TEXTINPUT && g_name_length < CHOOSE_MAX_NAME_LENGTH) {
        unsigned char c = (unsigned char)e->text.text[0];
        if (c != '\0' && choose_is_allowed_name_char(c)) {
            g_player_name[g_name_length++] = (char)c;
            g_player_name[g_name_length] = '\0';
        }
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKSPACE && g_name_length > 0) {
            g_player_name[g_name_length - 1] = '\0';
            g_name_length--;
        }

        if ((e->key.keysym.sym == SDLK_RETURN ||
             e->key.keysym.sym == SDLK_KP_ENTER ||
             e->key.keysym.sym == SDLK_SPACE) &&
            g_selected_chars[0] != -1) {
            choose_ensure_default_player_name();
            choose_play_click();
            choose_stage_start_game_selection();
        }

        if (e->key.keysym.sym == SDLK_ESCAPE) {
            choose_return_to_select_state();
        }
    }

    if (choose_handle_button_event(&g_name_confirm_btn, e) &&
        g_selected_chars[0] != -1) {
        choose_ensure_default_player_name();
        choose_play_click();
        choose_stage_start_game_selection();
    }

    if (choose_handle_button_event(&g_name_return_btn, e)) {
        choose_play_click();
        choose_return_to_select_state();
    }
}

void choose_scene_handle_event(const SDL_Event* e, ChooseSceneResult* result)
{
    if (result) result->return_to_main = 0;
    if (!g_initialized || !g_active || !e) return;

    if (g_state == CHOOSE_STATE_EXIT) {
        g_return_requested = 1;
        if (result) result->return_to_main = 1;
        return;
    }

    if (g_state == CHOOSE_STATE_SELECT) {
        choose_handle_select_state_event(e);
    } else if (g_state == CHOOSE_STATE_NAME_INPUT) {
        choose_handle_name_input_state_event(e);
    }
}

void choose_scene_update(float delta)
{
    if (!g_initialized || !g_active) return;

    if (delta > 0.1f) delta = 0.1f;
    g_last_delta = delta;

    if (g_state == CHOOSE_STATE_FADE_IN) {
        g_fade_alpha -= CHOOSE_FADE_SPEED * delta;
        if (g_fade_alpha <= 0.0f) {
            g_fade_alpha = 0.0f;
            g_state = CHOOSE_STATE_SELECT;
        }
    }

    if (g_state == CHOOSE_STATE_FADE_OUT) {
        g_fade_alpha += CHOOSE_FADE_SPEED * delta;
        if (g_fade_alpha >= 255.0f) {
            g_fade_alpha = 255.0f;
            g_state = CHOOSE_STATE_EXIT;
            g_return_requested = 1;
        }
    }

    choose_update_snow(delta);
    choose_update_character_animations(delta);
}

static void choose_render_select_state(void)
{
    int mouse_x = 0;
    int mouse_y = 0;
    int player_count = g_duo_mode ? 2 : 1;
    SDL_Color white = {255, 255, 255, 255};

    SDL_GetMouseState(&mouse_x, &mouse_y);

    for (int p = 0; p < player_count; ++p) {
        int i = choose_clamp_character_index(g_current_chars[p]);
        SDL_Rect card = g_player_card_rects[p];
        int left_hovered = SDL_PointInRect(&(SDL_Point){mouse_x, mouse_y}, &g_left_arrow_rects[p]);
        int right_hovered = SDL_PointInRect(&(SDL_Point){mouse_x, mouse_y}, &g_right_arrow_rects[p]);
        int input_hovered = SDL_PointInRect(&(SDL_Point){mouse_x, mouse_y}, &g_input_select_rects[p]);
        int input_focused = choose_can_change_focused_input() && (choose_get_focused_input_player() == p);

        if (g_left_arrow_tex) {
            choose_render_arrow_texture(g_left_arrow_tex, &g_left_arrow_rects[p], SDL_FLIP_HORIZONTAL);
        } else {
            choose_render_arrow_placeholder(g_left_arrow_rects[p], left_hovered);
        }

        if (g_right_arrow_tex) {
            choose_render_arrow_texture(g_right_arrow_tex, &g_right_arrow_rects[p], SDL_FLIP_NONE);
        } else {
            choose_render_arrow_placeholder(g_right_arrow_rects[p], right_hovered);
        }

        if (g_container_tex) {
            SDL_RenderCopy(g_renderer, g_container_tex, NULL, &card);
        }

        int select_w = (card.w * (g_duo_mode ? 36 : 38)) / 100;
        int select_h = (card.h * 31) / 100;
        int idle_w = (select_w * 78) / 100;
        int idle_h = (select_h * 88) / 100;
        if (select_w < 92) select_w = 92;
        if (select_h < 92) select_h = 92;
        if (idle_w < 74) idle_w = 74;
        if (idle_h < 80) idle_h = 80;
        int select_x = card.x + (card.w - select_w) / 2;
        int select_y = card.y + (card.h * 34) / 100;
        int idle_x = card.x + (card.w - idle_w) / 2;
        int idle_y = select_y + (select_h - idle_h) / 2;

        if (g_selected_chars[p] == i && g_chars[i].select.texture && !g_chars[i].select.finished) {
            choose_render_animation(&g_chars[i].select, select_x, select_y, select_w, select_h);
        } else {
            choose_render_animation(&g_chars[i].idle, idle_x, idle_y, idle_w, idle_h);
        }

        if (g_duo_mode && (g_card_id_font || g_font_small)) {
            TTF_Font* card_id_font = g_card_id_font ? g_card_id_font : g_font_small;
            const char* player_label = (p == 0) ? "PLAYER 1" : "PLAYER 2";
            int label_w = 0;
            int label_h = 0;
            int label_x = card.x + 20;
            if (TTF_SizeUTF8(card_id_font, player_label, &label_w, &label_h) == 0) {
                label_x = card.x + (card.w - label_w) / 2;
            }
            choose_render_text(card_id_font, player_label, white, label_x, card.y - 30);
        }

        if (g_font_big) {
            int name_w = 0;
            int name_h = 0;
            int name_x = card.x + 20;
            if (TTF_SizeUTF8(g_font_big, g_chars[i].name, &name_w, &name_h) == 0) {
                name_x = card.x + (card.w - name_w) / 2;
            }
            choose_render_text(g_font_big, g_chars[i].name, white, name_x, card.y + 28);
        }

        if (g_font_small) {
            int desc_w = 0;
            int desc_h = 0;
            int desc_x = card.x + 22;
            if (TTF_SizeUTF8(g_font_small, g_chars[i].desc1, &desc_w, &desc_h) == 0) {
                desc_x = card.x + (card.w - desc_w) / 2;
            }
            choose_render_text(g_font_small, g_chars[i].desc1, white, desc_x, card.y + card.h - 68);
        }

        if (g_font_small) {
            const char* input_label = "INPUT";
            int label_w = 0;
            int label_h = 0;
            int label_x = g_input_select_rects[p].x + 8;
            int label_y = g_input_select_rects[p].y - 24;
            if (TTF_SizeUTF8(g_font_small, input_label, &label_w, &label_h) == 0) {
                label_x = g_input_select_rects[p].x + (g_input_select_rects[p].w - label_w) / 2;
                label_y = g_input_select_rects[p].y - label_h - 4;
            }
            choose_render_text(g_font_small, input_label, white, label_x, label_y);
        }

        choose_render_input_selector(p, input_hovered, input_focused);
    }

    choose_render_button(&g_confirm_btn, g_last_delta);
    choose_render_button(&g_back_btn, g_last_delta);

    if (choose_can_change_focused_card()) choose_render_focus_outline(g_player_card_rects[choose_get_focused_player()]);
    if (choose_can_change_focused_input()) choose_render_focus_outline(g_input_select_rects[choose_get_focused_input_player()]);
    if (g_focused_button == choose_get_confirm_focus_index()) choose_render_focus_outline(g_confirm_btn.rect);
    if (g_focused_button == choose_get_return_focus_index()) choose_render_focus_outline(g_back_btn.rect);
}

static void choose_render_name_input_state(void)
{
    const int name_panel_w = g_name_panel_rect.w;
    const int name_panel_h = g_name_panel_rect.h;
    const int name_panel_x = g_name_panel_rect.x;
    const int name_panel_y = g_name_panel_rect.y;
    const int offset_2vh = (CHOOSE_SCREEN_H * 2) / 100;
    SDL_Rect input_rect = choose_get_name_input_rect();
    SDL_Color white = {255, 255, 255, 255};

    if (g_enter_name_tex) {
        SDL_Rect dst = {name_panel_x, name_panel_y, name_panel_w, name_panel_h};
        SDL_RenderCopy(g_renderer, g_enter_name_tex, NULL, &dst);
    } else {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 16, 28, 48, 228);
        SDL_RenderFillRect(g_renderer, &g_name_panel_rect);
        SDL_SetRenderDrawColor(g_renderer, 220, 236, 255, 255);
        SDL_RenderDrawRect(g_renderer, &g_name_panel_rect);
    }

    if (g_duo_mode && g_font_small) {
        char duo_info[168];
        int idx1 = choose_clamp_character_index(g_selected_chars[0]);
        int idx2 = choose_clamp_character_index(g_selected_chars[1]);
        snprintf(duo_info, sizeof(duo_info), "P1: %s [%s]  |  P2: %s [%s]",
                 g_chars[idx1].name, choose_get_control_name(g_selected_controls[0]),
                 g_chars[idx2].name, choose_get_control_name(g_selected_controls[1]));
        int info_w = 0;
        int info_h = 0;
        int info_y = 0;
        int info_x = name_panel_x + 14;
        if (TTF_SizeUTF8(g_font_small, duo_info, &info_w, &info_h) == 0) {
            info_x = name_panel_x + (name_panel_w - info_w) / 2;
        }
        info_y = name_panel_y - info_h - offset_2vh;
        if (info_y < 6) info_y = 6;
        choose_render_text(g_font_small, duo_info, white, info_x, info_y);
    }

    if (g_font_big) {
        int typed_w = 0;
        int typed_h = 0;
        int text_x = name_panel_x + name_panel_w / 2;
        int text_y = input_rect.y + 8;
        if (TTF_SizeUTF8(g_font_big, g_player_name, &typed_w, &typed_h) != 0) {
            typed_w = 0;
            typed_h = 0;
        }
        if (typed_h <= 0 && TTF_SizeUTF8(g_font_big, "A", NULL, &typed_h) != 0) {
            typed_h = 22;
        }
        text_x -= typed_w / 2;
        text_y = input_rect.y + (input_rect.h - typed_h) / 2 - 10 - offset_2vh;
        choose_render_text(g_font_big, g_player_name, white, text_x, text_y);

        if (((SDL_GetTicks() / 450U) % 2U) == 0U) {
            int cursor_x = text_x + typed_w + 2;
            int cursor_top = text_y + 3;
            int cursor_bottom = text_y + typed_h - 3;
            if (cursor_bottom < cursor_top) cursor_bottom = cursor_top + 14;
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 220);
            SDL_RenderDrawLine(g_renderer, cursor_x, cursor_top, cursor_x, cursor_bottom);
        }
    }

    choose_render_button(&g_name_confirm_btn, g_last_delta);
    choose_render_button(&g_name_return_btn, g_last_delta);
}

void choose_scene_render(void)
{
    if (!g_initialized || !g_active) return;

    if (g_background) {
        SDL_RenderCopy(g_renderer, g_background, NULL, NULL);
    }

    choose_render_snow();

    if (g_state == CHOOSE_STATE_SELECT || g_state == CHOOSE_STATE_FADE_IN) {
        choose_render_select_state();
    }

    if (g_state == CHOOSE_STATE_NAME_INPUT) {
        choose_render_name_input_state();
    }

    choose_render_fade((int)g_fade_alpha);
}

int choose_scene_consume_return_request(void)
{
    int out = g_return_requested;
    g_return_requested = 0;
    return out;
}

int choose_scene_consume_start_game_request(GameSelection* out_selection)
{
    int out = g_start_game_requested;
    if (out && out_selection) {
        *out_selection = g_start_selection;
    }
    if (out) {
        g_start_game_requested = 0;
        g_return_requested = 0;
        debug_logf("choose: consume start request");
    }
    return out;
}

void choose_scene_debug_force_start(const char* name)
{
    if (!g_initialized || !g_active) return;

    if (name && choose_sanitize_name(name, g_player_name, sizeof(g_player_name))) {
        g_name_length = (int)strlen(g_player_name);
    } else {
        snprintf(g_player_name, sizeof(g_player_name), "Player");
        g_name_length = (int)strlen(g_player_name);
    }

    choose_stage_start_game_selection();
}

void choose_scene_cleanup(void)
{
    if (!g_initialized) return;

    choose_scene_leave();

    if (g_background) {
        SDL_DestroyTexture(g_background);
        g_background = NULL;
    }
    if (g_container_tex) {
        SDL_DestroyTexture(g_container_tex);
        g_container_tex = NULL;
    }
    if (g_enter_name_tex) {
        SDL_DestroyTexture(g_enter_name_tex);
        g_enter_name_tex = NULL;
    }
    if (g_left_arrow_tex) {
        SDL_DestroyTexture(g_left_arrow_tex);
        g_left_arrow_tex = NULL;
    }
    if (g_right_arrow_tex) {
        SDL_DestroyTexture(g_right_arrow_tex);
        g_right_arrow_tex = NULL;
    }
    if (g_snow_tex) {
        SDL_DestroyTexture(g_snow_tex);
        g_snow_tex = NULL;
    }

    choose_destroy_characters();

    if (g_click_sound) {
        Mix_FreeChunk(g_click_sound);
        g_click_sound = NULL;
    }
    if (g_select_sound) {
        Mix_FreeChunk(g_select_sound);
        g_select_sound = NULL;
    }
    if (g_menu_music) {
        Mix_FreeChunk(g_menu_music);
        g_menu_music = NULL;
    }

    if (g_font_big) {
        TTF_CloseFont(g_font_big);
        g_font_big = NULL;
    }
    if (g_font_small) {
        TTF_CloseFont(g_font_small);
        g_font_small = NULL;
    }
    if (g_button_font) {
        TTF_CloseFont(g_button_font);
        g_button_font = NULL;
    }
    if (g_card_id_font) {
        TTF_CloseFont(g_card_id_font);
        g_card_id_font = NULL;
    }

    g_initialized = 0;
    g_active = 0;
    g_return_requested = 0;
    g_renderer = NULL;
}
