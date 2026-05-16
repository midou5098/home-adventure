#include "ui_shared.h"
#include <ctype.h>
#include <math.h>
#include <string.h>

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
        font = TTF_OpenFont(candidates[i], point_size);
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
        "../menu/option in game/assets/fonts/options.ttf",
        "menu/option in game/assets/fonts/options.ttf",
        "assets/fonts/options.ttf",
        "./assets/fonts/options.ttf",
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
        "../menu/option in game/assets/fonts/options.ttf",
        "menu/option in game/assets/fonts/options.ttf",
        "assets/fonts/options.ttf",
        "./assets/fonts/options.ttf",
        "../menu/option in game/assets/main_menu/fonts/christmas_comeback_2.otf",
        "menu/option in game/assets/main_menu/fonts/christmas_comeback_2.otf",
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

static Uint8 ui_lerp_u8(Uint8 a, Uint8 b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Uint8)(a + (b - a) * t);
}

static SDL_Color ui_lerp_color(SDL_Color a, SDL_Color b, float t)
{
    SDL_Color out;
    out.r = ui_lerp_u8(a.r, b.r, t);
    out.g = ui_lerp_u8(a.g, b.g, t);
    out.b = ui_lerp_u8(a.b, b.b, t);
    out.a = ui_lerp_u8(a.a, b.a, t);
    return out;
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

static void ui_fill_vertical_gradient(SDL_Renderer* renderer,
                                      const SDL_Rect* rect,
                                      SDL_Color top,
                                      SDL_Color bottom)
{
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;

    if (rect->h == 1) {
        ui_fill_rect(renderer, rect, top);
        return;
    }

    for (int row = 0; row < rect->h; ++row) {
        float t = (float)row / (float)(rect->h - 1);
        SDL_Color color = ui_lerp_color(top, bottom, t);
        SDL_Rect line = {rect->x, rect->y + row, rect->w, 1};
        ui_fill_rect(renderer, &line, color);
    }
}

static void ui_render_hover_glow_main_menu(SDL_Renderer* renderer,
                                           const SDL_Rect* rect,
                                           float hover_amount,
                                           int pressed)
{
    SDL_Color layers[3] = {
        {0xFF, 0xDC, 0x92, 0x26},
        {0xFF, 0xB3, 0x47, 0x32},
        {0xFF, 0x89, 0x3D, 0x1F}
    };
    const int pad[] = {9, 6, 4};
    float press_dampen = 0.0f;
    float alpha_scale = 0.0f;

    if (!renderer || !rect || hover_amount <= 0.0f) return;

    press_dampen = pressed ? 0.55f : 1.0f;
    alpha_scale = hover_amount * press_dampen;

    for (int i = 0; i < 3; ++i) {
        int alpha = (int)(layers[i].a * alpha_scale);
        SDL_Rect glow = {
            rect->x - pad[i],
            rect->y - pad[i],
            rect->w + pad[i] * 2,
            rect->h + pad[i] * 2
        };

        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        layers[i].a = (Uint8)alpha;
        ui_fill_rect(renderer, &glow, layers[i]);
    }
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

void ui_render_main_menu_style_button(SDL_Renderer* renderer,
                                      TTF_Font* font,
                                      const SDL_Rect* rect,
                                      const char* label,
                                      int hovered,
                                      int pressed)
{
    const SDL_Color border_dark = {0x16, 0x1E, 0x30, 0xFF};
    const SDL_Color border_warm = {0xF8, 0xD5, 0x78, 0xFF};
    const SDL_Color shell_color = {0x20, 0x32, 0x4F, 0xFF};
    const SDL_Color top_normal = {0x4E, 0x83, 0xB8, 0xFF};
    const SDL_Color bot_normal = {0x2C, 0x45, 0x6A, 0xFF};
    const SDL_Color top_hover = {0x64, 0xA2, 0xDA, 0xFF};
    const SDL_Color bot_hover = {0x3A, 0x66, 0x99, 0xFF};
    const SDL_Color top_pressed = {0x2D, 0x4A, 0x71, 0xFF};
    const SDL_Color bot_pressed = {0x1E, 0x33, 0x50, 0xFF};
    const SDL_Color text_normal = {0xF7, 0xF2, 0xE8, 0xFF};
    const SDL_Color text_hover = {0xFF, 0xFA, 0xE2, 0xFF};
    const SDL_Color text_pressed = {0xF0, 0xE6, 0xD0, 0xFF};
    SDL_Rect shell = {0, 0, 0, 0};
    SDL_Rect face = {0, 0, 0, 0};
    SDL_Color top_color = top_normal;
    SDL_Color bottom_color = bot_normal;
    SDL_Color text_color = text_normal;
    SDL_Color border_color = border_dark;
    float hover_pulse = 0.0f;

    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;

    shell = (SDL_Rect){rect->x + 1, rect->y + 1, rect->w - 2, rect->h - 2};
    face = (SDL_Rect){rect->x + 2, rect->y + 2, rect->w - 4, rect->h - 4};
    if (shell.w <= 0 || shell.h <= 0 || face.w <= 0 || face.h <= 0) return;

    if (hovered) {
        hover_pulse = 0.70f + 0.30f * (sinf(SDL_GetTicks() * 0.010f) * 0.5f + 0.5f);
        top_color = ui_lerp_color(top_normal, top_hover, hover_pulse);
        bottom_color = ui_lerp_color(bot_normal, bot_hover, hover_pulse);
        text_color = text_hover;
        border_color = border_warm;
    }

    if (pressed) {
        top_color = top_pressed;
        bottom_color = bot_pressed;
        text_color = text_pressed;
        border_color = ui_lerp_color(border_dark, border_warm, 0.45f);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    ui_render_hover_glow_main_menu(renderer, rect, hover_pulse, pressed);

    ui_fill_rect(renderer, rect, border_dark);
    ui_fill_rect(renderer, &shell, shell_color);
    ui_fill_vertical_gradient(renderer, &face, top_color, bottom_color);
    ui_draw_rect(renderer, &shell, border_color);

    if (hovered) {
        SDL_Rect ring = {rect->x - 1, rect->y - 1, rect->w + 2, rect->h + 2};
        SDL_Color ring_color = border_warm;
        ring_color.a = (Uint8)(130 + (int)(70.0f * hover_pulse));
        ui_draw_rect(renderer, &ring, ring_color);
    }

    if (shell.h > 1) {
        SDL_Rect top_line = {shell.x, shell.y, shell.w, 1};
        SDL_Rect bottom_line = {shell.x, shell.y + shell.h - 1, shell.w, 1};
        SDL_Color top_highlight = {0xE5, 0xF2, 0xFF, pressed ? 0x45 : 0x7A};
        SDL_Color bottom_shadow = {0x10, 0x1C, 0x2E, 0xB8};

        ui_fill_rect(renderer, &top_line, top_highlight);
        ui_fill_rect(renderer, &bottom_line, bottom_shadow);
    }

    if (font && label && label[0] != '\0') {
        ui_render_button_label(renderer, font, rect, label, text_color, pressed);
    }
}
