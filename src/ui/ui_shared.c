#include "ui_shared.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define UI_TEXT_CACHE_CAPACITY 512
#define UI_TEXT_CACHE_MAX_LEN 127
#define UI_NUMERIC_FONT_CACHE_CAPACITY 16

typedef struct {
    int used;
    SDL_Renderer* renderer;
    TTF_Font* font;
    SDL_Color color;
    int solid;
    char text[UI_TEXT_CACHE_MAX_LEN + 1];
    SDL_Texture* texture;
    int w;
    int h;
    Uint32 last_used;
} UiTextCacheEntry;

typedef struct {
    int used;
    int point_size;
    TTF_Font* font;
} UiNumericFontCacheEntry;

static UiTextCacheEntry g_text_cache[UI_TEXT_CACHE_CAPACITY];
static UiNumericFontCacheEntry g_numeric_font_cache[UI_NUMERIC_FONT_CACHE_CAPACITY];
static char g_ui_project_root[PATH_MAX];
static int g_ui_project_root_ready = 0;

static int ui_path_is_absolute(const char* path)
{
    if (!path || !path[0]) return 0;
    if (path[0] == '/') return 1;
    if (isalpha((unsigned char)path[0]) && path[1] == ':') return 1;
    return 0;
}

static int ui_file_exists(const char* path)
{
    FILE* f = NULL;

    if (!path) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int ui_detect_project_root(char* out_path, size_t out_path_size)
{
    char exe_path[PATH_MAX];
    ssize_t len = 0;
    char* slash = NULL;
    static const char* cwd_candidates[] = {
        ".",
        "..",
        "../..",
        "../../..",
        "../../../..",
        "../../../../.."
    };

    if (!out_path || out_path_size == 0) return 0;

    len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
        char marker_path[PATH_MAX];

        exe_path[len] = '\0';
        slash = strrchr(exe_path, '/');
        if (slash) {
            *slash = '\0';
            if (snprintf(marker_path, sizeof(marker_path), "%s/src/main_menu/main.c", exe_path) < (int)sizeof(marker_path) &&
                ui_file_exists(marker_path)) {
                snprintf(out_path, out_path_size, "%s", exe_path);
                return 1;
            }
        }
    }

    for (size_t i = 0; i < sizeof(cwd_candidates) / sizeof(cwd_candidates[0]); ++i) {
        char marker_path[PATH_MAX];
        if (snprintf(marker_path, sizeof(marker_path), "%s/src/main_menu/main.c", cwd_candidates[i]) >= (int)sizeof(marker_path)) {
            continue;
        }
        if (ui_file_exists(marker_path)) {
            snprintf(out_path, out_path_size, "%s", cwd_candidates[i]);
            return 1;
        }
    }

    return 0;
}

int ui_resolve_asset_path(const char* relative_path, char* out_path, size_t out_path_size)
{
    if (!relative_path || !out_path || out_path_size == 0) return 0;

    if (ui_path_is_absolute(relative_path)) {
        snprintf(out_path, out_path_size, "%s", relative_path);
        return 1;
    }

    if (!g_ui_project_root_ready) {
        if (ui_detect_project_root(g_ui_project_root, sizeof(g_ui_project_root))) {
            g_ui_project_root_ready = 1;
        }
    }

    if (g_ui_project_root_ready &&
        snprintf(out_path, out_path_size, "%s/%s", g_ui_project_root, relative_path) < (int)out_path_size) {
        return 1;
    }

    snprintf(out_path, out_path_size, "%s", relative_path);
    return 1;
}

SDL_Texture* ui_load_texture(SDL_Renderer* renderer, const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!renderer || !relative_path) return NULL;
    ui_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return IMG_LoadTexture(renderer, asset_path);
}

Mix_Chunk* ui_load_wav(const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!relative_path) return NULL;
    ui_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return Mix_LoadWAV(asset_path);
}

Mix_Music* ui_load_music(const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!relative_path) return NULL;
    ui_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return Mix_LoadMUS(asset_path);
}

FILE* ui_open_asset_file(const char* relative_path, const char* mode)
{
    char asset_path[PATH_MAX];

    if (!relative_path || !mode) return NULL;
    ui_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return fopen(asset_path, mode);
}

static int ui_color_equal(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int ui_text_has_digit(const char* text)
{
    if (!text) return 0;

    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (isdigit((unsigned char)text[i])) return 1;
    }

    return 0;
}

static SDL_Texture* ui_get_cached_text_texture(SDL_Renderer* renderer,
                                               TTF_Font* font,
                                               const char* text,
                                               SDL_Color color,
                                               int solid,
                                               int* out_w,
                                               int* out_h)
{
    size_t text_len = 0;
    Uint32 now = 0;
    int free_slot = -1;
    int lru_slot = 0;
    Uint32 lru_tick = 0xFFFFFFFFu;

    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;

    font = ui_font_for_text(font, text);
    if (!renderer || !font || !text || !text[0]) return NULL;

    text_len = strlen(text);
    if (text_len > UI_TEXT_CACHE_MAX_LEN) return NULL;

    now = SDL_GetTicks();
    for (int i = 0; i < UI_TEXT_CACHE_CAPACITY; ++i) {
        UiTextCacheEntry* e = &g_text_cache[i];
        if (!e->used) {
            if (free_slot < 0) free_slot = i;
            continue;
        }

        if (e->renderer == renderer &&
            e->font == font &&
            e->solid == solid &&
            ui_color_equal(e->color, color) &&
            strcmp(e->text, text) == 0) {
            e->last_used = now;
            if (out_w) *out_w = e->w;
            if (out_h) *out_h = e->h;
            return e->texture;
        }

        if (e->last_used < lru_tick) {
            lru_tick = e->last_used;
            lru_slot = i;
        }
    }

    {
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
        SDL_Texture* tex = NULL;
        UiTextCacheEntry* slot = NULL;
        int slot_index = (free_slot >= 0) ? free_slot : lru_slot;

        (void)solid;
        if (!surf) return NULL;

        tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (!tex) {
            SDL_FreeSurface(surf);
            return NULL;
        }
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
#endif

        slot = &g_text_cache[slot_index];
        if (slot->used && slot->texture) {
            SDL_DestroyTexture(slot->texture);
        }

        slot->used = 1;
        slot->renderer = renderer;
        slot->font = font;
        slot->color = color;
        slot->solid = solid;
        memcpy(slot->text, text, text_len);
        slot->text[text_len] = '\0';
        slot->texture = tex;
        slot->w = surf->w;
        slot->h = surf->h;
        slot->last_used = now;

        if (out_w) *out_w = slot->w;
        if (out_h) *out_h = slot->h;

        SDL_FreeSurface(surf);
        return slot->texture;
    }
}

TTF_Font* ui_open_font_from_candidates(const char* const* candidates,
                                       int count,
                                       int point_size,
                                       int disable_kerning)
{
    TTF_Font* font = NULL;

    if (!candidates || count <= 0 || point_size <= 0) return NULL;

    for (int i = 0; i < count && !font; ++i) {
        if (!candidates[i] || !candidates[i][0]) continue;
        {
            char font_path[PATH_MAX];
            ui_resolve_asset_path(candidates[i], font_path, sizeof(font_path));
            font = TTF_OpenFont(font_path, point_size);
        }
    }

    if (font) {
        ui_apply_font_quality(font);
        if (disable_kerning) {
            TTF_SetFontKerning(font, 0);
        }
    }

    return font;
}

static TTF_Font* ui_open_numeric_arial_font(int point_size)
{
    const char* arial_candidates[] = {
        "assets/fonts/arial.ttf",
        "assets/font/arial.ttf",
        "assets/Arial.ttf",
        "assets/arial.ttf",
        "./assets/fonts/arial.ttf",
        "./assets/font/arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/arial.ttf",
        "/usr/share/fonts/truetype/microsoft/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Helvetica.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/ARIAL.TTF",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    const int count = (int)(sizeof(arial_candidates) / sizeof(arial_candidates[0]));
    return ui_open_font_from_candidates(arial_candidates, count, point_size, 0);
}

TTF_Font* ui_open_arial_font(int point_size, int disable_kerning)
{
    const char* arial_candidates[] = {
        "assets/main_menu/fonts/christmas_comeback_2.otf",
        "assets/fonts/arial.ttf",
        "assets/font/arial.ttf",
        "assets/Arial.ttf",
        "assets/arial.ttf",
        "./assets/fonts/arial.ttf",
        "./assets/font/arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/arial.ttf",
        "/usr/share/fonts/truetype/microsoft/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/opentype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Helvetica.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/ARIAL.TTF",
        "C:/Windows/Fonts/segoeui.ttf"
    };
    const int count = (int)(sizeof(arial_candidates) / sizeof(arial_candidates[0]));
    return ui_open_font_from_candidates(arial_candidates, count, point_size, disable_kerning);
}

TTF_Font* ui_font_for_text(TTF_Font* preferred_font, const char* text)
{
    int point_size = 0;
    int free_slot = -1;

    if (!preferred_font || !text || !ui_text_has_digit(text)) return preferred_font;

    point_size = TTF_FontHeight(preferred_font);
    if (point_size < 8) point_size = 8;

    for (int i = 0; i < UI_NUMERIC_FONT_CACHE_CAPACITY; ++i) {
        if (!g_numeric_font_cache[i].used) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (g_numeric_font_cache[i].point_size == point_size && g_numeric_font_cache[i].font) {
            return g_numeric_font_cache[i].font;
        }
    }

    if (free_slot < 0) return preferred_font;

    g_numeric_font_cache[free_slot].font = ui_open_numeric_arial_font(point_size);
    if (!g_numeric_font_cache[free_slot].font) return preferred_font;

    g_numeric_font_cache[free_slot].used = 1;
    g_numeric_font_cache[free_slot].point_size = point_size;
    return g_numeric_font_cache[free_slot].font;
}

void ui_apply_font_quality(TTF_Font* font)
{
    if (!font) return;

#ifdef TTF_HINTING_LIGHT
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
#else
    TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
#endif
    TTF_SetFontKerning(font, 1);
}

void ui_draw_text_center(SDL_Renderer* renderer,
                         TTF_Font* font,
                         const char* text,
                         int center_x,
                         int y,
                         SDL_Color color)
{
    SDL_Texture* tex = NULL;
    SDL_Rect dst = {0, 0, 0, 0};
    int text_w = 0;
    int text_h = 0;

    if (!renderer || !font || !text || !text[0]) return;

    tex = ui_get_cached_text_texture(renderer, font, text, color, 0, &text_w, &text_h);
    if (tex) {
        dst.x = center_x - (text_w / 2);
        dst.y = y;
        dst.w = text_w;
        dst.h = text_h;
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}

void ui_draw_text_left(SDL_Renderer* renderer,
                       TTF_Font* font,
                       const char* text,
                       int x,
                       int y,
                       SDL_Color color)
{
    SDL_Texture* tex = NULL;
    SDL_Rect dst = {0, 0, 0, 0};
    int text_w = 0;
    int text_h = 0;

    if (!renderer || !font || !text || !text[0]) return;

    tex = ui_get_cached_text_texture(renderer, font, text, color, 0, &text_w, &text_h);
    if (tex) {
        dst.x = x;
        dst.y = y;
        dst.w = text_w;
        dst.h = text_h;
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}

static void ui_set_draw_color(SDL_Renderer* renderer, SDL_Color color)
{
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void ui_fill_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color)
{
    if (!renderer || !rect) return;
    ui_set_draw_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

void ui_draw_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color)
{
    if (!renderer || !rect) return;
    ui_set_draw_color(renderer, color);
    SDL_RenderDrawRect(renderer, rect);
}

static void ui_fill_warm_face(SDL_Renderer* renderer, const SDL_Rect* face, int hovered, int pressed)
{
    SDL_Color bands[5];
    SDL_Color center_glow;
    int base = 0;
    int y = 0;

    if (!renderer || !face || face->w <= 0 || face->h <= 0) return;

    if (pressed) {
        bands[0] = (SDL_Color){0xFF, 0xD3, 0x6B, 0xFF};
        bands[1] = (SDL_Color){0xFF, 0xC8, 0x57, 0xFF};
        bands[2] = (SDL_Color){0xFF, 0xB3, 0x47, 0xFF};
        bands[3] = (SDL_Color){0xFF, 0x9E, 0x57, 0xFF};
        bands[4] = (SDL_Color){0xE8, 0x7F, 0x3E, 0xFF};
        center_glow = (SDL_Color){0xFF, 0xF1, 0xA8, 0x28};
    } else if (hovered) {
        bands[0] = (SDL_Color){0xFF, 0xF1, 0xA8, 0xFF};
        bands[1] = (SDL_Color){0xFF, 0xE2, 0x8C, 0xFF};
        bands[2] = (SDL_Color){0xFF, 0xD3, 0x6B, 0xFF};
        bands[3] = (SDL_Color){0xFF, 0xC8, 0x57, 0xFF};
        bands[4] = (SDL_Color){0xFF, 0xB3, 0x47, 0xFF};
        center_glow = (SDL_Color){0xFF, 0xF1, 0xA8, 0x55};
    } else {
        bands[0] = (SDL_Color){0xFF, 0xE9, 0xA5, 0xFF};
        bands[1] = (SDL_Color){0xFF, 0xD9, 0x78, 0xFF};
        bands[2] = (SDL_Color){0xFF, 0xC8, 0x57, 0xFF};
        bands[3] = (SDL_Color){0xFF, 0xB3, 0x47, 0xFF};
        bands[4] = (SDL_Color){0xFF, 0x9E, 0x57, 0xFF};
        center_glow = (SDL_Color){0xFF, 0xF1, 0xA8, 0x35};
    }

    base = face->h / 5;
    y = face->y;

    for (int i = 0; i < 5; ++i) {
        int h = (i == 4) ? (face->y + face->h - y) : base;
        SDL_Rect band = {face->x, y, face->w, h};
        ui_fill_rect(renderer, &band, bands[i]);
        y += h;
    }

    if (face->w > 10 && face->h > 6) {
        SDL_Rect center = {face->x + 4, face->y + face->h / 3, face->w - 8, face->h / 3};
        ui_fill_rect(renderer, &center, center_glow);
    }
}

static void ui_render_warm_glow(SDL_Renderer* renderer, const SDL_Rect* rect)
{
    const SDL_Color layers[] = {
        {0xFF, 0xA0, 0x7A, 0x14},
        {0xFF, 0x9E, 0x57, 0x20},
        {0xFF, 0xB3, 0x47, 0x30},
        {0xFF, 0xC8, 0x57, 0x40},
        {0xFF, 0xD3, 0x6B, 0x54},
        {0xFF, 0xF1, 0xA8, 0x46}
    };
    const int pads[] = {11, 9, 7, 5, 3, 2};
    float pulse = 0.0f;

    if (!renderer || !rect) return;

    pulse = 0.86f + 0.14f * sinf(SDL_GetTicks() * 0.012f);

    for (int i = 0; i < 6; ++i) {
        SDL_Rect g = {
            rect->x - pads[i],
            rect->y - pads[i],
            rect->w + pads[i] * 2,
            rect->h + pads[i] * 2
        };
        SDL_Color c = layers[i];
        int alpha = (int)(c.a * pulse);
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        c.a = (Uint8)alpha;
        ui_fill_rect(renderer, &g, c);
    }
}

static void ui_render_selection_arrows(SDL_Renderer* renderer, const SDL_Rect* rect)
{
    int center_y = 0;
    int arrow_size = 0;
    int pulse = 0;
    int left_tip_x = 0;
    int right_tip_x = 0;

    if (!renderer || !rect || rect->h <= 0) return;

    ui_set_draw_color(renderer, (SDL_Color){0x00, 0x00, 0x00, 0xFF});

    center_y = rect->y + rect->h / 2;
    arrow_size = (rect->h >= 18) ? 5 : 4;
    pulse = (int)lroundf(1.5f * (sinf(SDL_GetTicks() * 0.012f) + 1.0f));
    left_tip_x = rect->x - (7 + pulse);
    right_tip_x = rect->x + rect->w + (7 + pulse);

    for (int i = 0; i <= arrow_size; ++i) {
        SDL_RenderDrawLine(renderer,
                           left_tip_x + i,
                           center_y - i,
                           left_tip_x + i,
                           center_y + i);
        SDL_RenderDrawLine(renderer,
                           right_tip_x - i,
                           center_y - i,
                           right_tip_x - i,
                           center_y + i);
    }
}

static void ui_render_button_label(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const SDL_Rect* rect,
                                   const char* label,
                                   SDL_Color text_color,
                                   int pressed)
{
    TTF_Font* label_font = NULL;
    int center_x = 0;
    int text_h = 0;
    int text_y = 0;

    if (!renderer || !font || !rect || !label || !label[0]) return;

    label_font = ui_font_for_text(font, label);
    if (!label_font) label_font = font;
    center_x = rect->x + rect->w / 2;
    text_h = TTF_FontHeight(label_font);
    text_y = rect->y + (rect->h - text_h) / 2 + (pressed ? 1 : 0);

    ui_draw_text_center(renderer, label_font, label, center_x, text_y, text_color);
}

void ui_render_styled_button(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const SDL_Rect* rect,
                             const char* label,
                             int hovered,
                             int pressed,
                             int enable_hover_glow)
{
    const SDL_Color outer_border = {0x2A, 0x15, 0x07, 0xFF};
    const SDL_Color inner_border = {0xA4, 0x6A, 0x2D, 0xFF};
    const SDL_Color glow_border = {0xFF, 0xD3, 0x6B, 0xFF};
    const SDL_Color pressed_border = {0xFF, 0xB3, 0x47, 0xFF};
    const SDL_Color shell_fill = {0x1B, 0x2A, 0x49, 0xFF};
    const SDL_Color top_highlight = {0xFF, 0xD8, 0x83, 0xFF};
    const SDL_Color bottom_shadow = {0x2A, 0x15, 0x07, 0xFF};
    const SDL_Color text_default = {0x13, 0x0F, 0x07, 0xFF};
    const SDL_Color text_hover = {0xFF, 0xF7, 0xD1, 0xFF};
    const SDL_Color text_pressed = {0x21, 0x12, 0x03, 0xFF};
    const SDL_Color inset_shadow = {0x3A, 0x2A, 0x1A, 0x30};
    const SDL_Color side_highlight = {0xFF, 0xF1, 0xA8, 0x24};
    SDL_Rect inner = {0, 0, 0, 0};
    SDL_Rect face = {0, 0, 0, 0};
    SDL_Color current_outer = outer_border;
    SDL_Color current_border = inner_border;
    SDL_Color current_text = text_default;
    SDL_Rect top = {0, 0, 0, 0};
    SDL_Rect bottom = {0, 0, 0, 0};

    if (!renderer || !font || !rect || rect->w <= 0 || rect->h <= 0 || !label || !label[0]) return;

    inner = (SDL_Rect){rect->x + 1, rect->y + 1, rect->w - 2, rect->h - 2};
    face = (SDL_Rect){rect->x + 2, rect->y + 2, rect->w - 4, rect->h - 4};
    if (inner.w <= 0 || inner.h <= 0 || face.w <= 0 || face.h <= 0) return;

    if (hovered) {
        current_outer = glow_border;
        current_border = glow_border;
        current_text = text_hover;
    }
    if (pressed) {
        current_border = pressed_border;
        current_text = text_pressed;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    if (hovered && enable_hover_glow) {
        ui_render_warm_glow(renderer, rect);
    }

    ui_fill_rect(renderer, rect, current_outer);
    ui_fill_rect(renderer, &inner, shell_fill);
    ui_fill_warm_face(renderer, &face, hovered, pressed);
    ui_draw_rect(renderer, &inner, current_border);

    if (hovered) {
        SDL_Rect ring = {rect->x - 1, rect->y - 1, rect->w + 2, rect->h + 2};
        ui_draw_rect(renderer, &ring, glow_border);
    }

    top = (SDL_Rect){inner.x, inner.y, inner.w, 1};
    bottom = (SDL_Rect){inner.x, inner.y + inner.h - 1, inner.w, 1};
    if (!pressed) {
        ui_fill_rect(renderer, &top, top_highlight);
    }
    ui_fill_rect(renderer, &bottom, bottom_shadow);

    if (face.w > 6 && face.h > 4) {
        SDL_Rect left_high = {face.x + 1, face.y + 1, 1, face.h - 2};
        SDL_Rect right_shadow = {face.x + face.w - 2, face.y + 1, 1, face.h - 2};
        ui_fill_rect(renderer, &left_high, side_highlight);
        ui_fill_rect(renderer, &right_shadow, inset_shadow);
    }

    if (hovered) {
        ui_render_selection_arrows(renderer, rect);
    }

    ui_render_button_label(renderer, font, rect, label, current_text, pressed);
}
