#include "mainmenu_headers.h"
#include "asset_paths.h"
#include "ui_shared.h"

static TTF_Font* g_button_font = NULL;

static void set_draw_color(SDL_Renderer* renderer, SDL_Color c)
{
    if (!renderer) return;
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color)
{
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;
    set_draw_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color)
{
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;
    set_draw_color(renderer, color);
    SDL_RenderDrawRect(renderer, rect);
}

static int point_in_rect(const SDL_Rect* rect, int x, int y)
{
    if (!rect) return 0;
    return (x >= rect->x && x < rect->x + rect->w &&
            y >= rect->y && y < rect->y + rect->h);
}

static Uint8 lerp_u8(Uint8 a, Uint8 b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Uint8)(a + (b - a) * t);
}

static SDL_Color lerp_color(SDL_Color a, SDL_Color b, float t)
{
    SDL_Color out;
    out.r = lerp_u8(a.r, b.r, t);
    out.g = lerp_u8(a.g, b.g, t);
    out.b = lerp_u8(a.b, b.b, t);
    out.a = lerp_u8(a.a, b.a, t);
    return out;
}

static void fill_vertical_gradient(SDL_Renderer* renderer, const SDL_Rect* rect,
                                   SDL_Color top, SDL_Color bottom)
{
    if (!renderer || !rect || rect->w <= 0 || rect->h <= 0) return;

    if (rect->h == 1) {
        fill_rect(renderer, rect, top);
        return;
    }

    for (int row = 0; row < rect->h; ++row) {
        float t = (float)row / (float)(rect->h - 1);
        SDL_Color c = lerp_color(top, bottom, t);
        SDL_Rect line = {rect->x, rect->y + row, rect->w, 1};
        fill_rect(renderer, &line, c);
    }
}

static void render_hover_glow(SDL_Renderer* renderer, const SDL_Rect* rect,
                              float hover_amount, int pressed)
{
    if (!renderer || !rect || hover_amount <= 0.0f) return;

    SDL_Color glow_a = {0xFF, 0xDC, 0x92, 0x26};
    SDL_Color glow_b = {0xFF, 0xB3, 0x47, 0x32};
    SDL_Color glow_c = {0xFF, 0x89, 0x3D, 0x1F};
    const int pad[] = {9, 6, 4};

    float press_dampen = pressed ? 0.55f : 1.0f;
    float alpha_scale = hover_amount * press_dampen;

    SDL_Color layers[3] = {glow_a, glow_b, glow_c};
    for (int i = 0; i < 3; ++i) {
        int alpha = (int)(layers[i].a * alpha_scale);
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        layers[i].a = (Uint8)alpha;

        SDL_Rect g = {
            rect->x - pad[i],
            rect->y - pad[i],
            rect->w + pad[i] * 2,
            rect->h + pad[i] * 2
        };
        fill_rect(renderer, &g, layers[i]);
    }
}

/* ---------------- SDL init & texture ---------------- */
int init_SDL(SDL_Window** window, SDL_Renderer** renderer)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return 0;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("IMG_Init: %s", IMG_GetError());
        return 0;
    }
    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init: %s", TTF_GetError());
        return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        SDL_Log("Mix_OpenAudio: %s", Mix_GetError());
        return 0;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    *window = SDL_CreateWindow("Home Adventure",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_W, WINDOW_H,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!*window) { SDL_Log("CreateWindow: %s", SDL_GetError()); return 0; }

    *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) {
        SDL_Log("CreateRenderer (vsync) failed: %s", SDL_GetError());
        *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!*renderer) {
        SDL_Log("CreateRenderer (accelerated) failed: %s", SDL_GetError());
        *renderer = SDL_CreateRenderer(*window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!*renderer) { SDL_Log("CreateRenderer: %s", SDL_GetError()); return 0; }

    SDL_RenderSetLogicalSize(*renderer, LOGICAL_W, LOGICAL_H);
    SDL_RenderSetIntegerScale(*renderer, SDL_FALSE);

    return 1;
}
SDL_Rect get_button_dest(const Button* b)
{
    if (!b) return (SDL_Rect){0, 0, 0, 0};

    int scaled_w = (int)(b->rect.w * b->scale);
    int scaled_h = (int)(b->rect.h * b->scale);
    int scaled_x = b->rect.x - (scaled_w - b->rect.w) / 2;
    int scaled_y = b->rect.y - (scaled_h - b->rect.h) / 2;

    SDL_Rect dest = {scaled_x, scaled_y, scaled_w, scaled_h};
    dest.y += (int)b->bounceOffset;
    if (b->pressedTimer > 0.0f) dest.y += 1;

    return dest;
}

void prototype_button_init(Button* button, int x, int y, int w, int h, const char* label)
{
    if (!button) return;

    button->rect.x = x;
    button->rect.y = y;
    button->rect.w = w;
    button->rect.h = h;

    if (label) {
        snprintf(button->label, sizeof(button->label), "%s", label);
    } else {
        button->label[0] = '\0';
    }

    button->selected = 0;
    button->bounceOffset = 0.0f;
    button->scale = 1.0f;
    button->targetScale = 1.0f;
    button->pressedTimer = 0.0f;
}

void prototype_button_update(Button* button, float dt)
{
    if (!button) return;

    button->targetScale = button->selected ? 1.07f : 1.0f;
    button->bounceOffset = button->selected ? -1.0f : 0.0f;

    float diff = button->targetScale - button->scale;
    button->scale += diff * 11.0f * dt;
    if (fabsf(diff) < 0.001f) {
        button->scale = button->targetScale;
    }

    if (button->pressedTimer > 0.0f) {
        button->pressedTimer -= dt;
        if (button->pressedTimer < 0.0f) {
            button->pressedTimer = 0.0f;
        }
    }
}

int prototype_button_handle_event(Button* button, const SDL_Event* ev)
{
    if (!button || !ev) return 0;

    SDL_Rect dest = get_button_dest(button);

    if (ev->type == SDL_MOUSEMOTION) {
        button->selected = point_in_rect(&dest, ev->motion.x, ev->motion.y);
        return 0;
    }

    if (ev->type == SDL_MOUSEBUTTONDOWN && ev->button.button == SDL_BUTTON_LEFT) {
        if (point_in_rect(&dest, ev->button.x, ev->button.y)) {
            button->pressedTimer = 0.13f;
            return 1;
        }
    }

    return 0;
}

void prototype_button_render(const Button* button, SDL_Renderer* renderer, TTF_Font* font)
{
    if (!button || !renderer) return;

    SDL_Rect dest = get_button_dest(button);
    SDL_Rect shell = {dest.x + 1, dest.y + 1, dest.w - 2, dest.h - 2};
    SDL_Rect face = {dest.x + 2, dest.y + 2, dest.w - 4, dest.h - 4};

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

    int pressed = button->pressedTimer > 0.0f;
    float hover_pulse = 0.0f;
    if (button->selected) {
        hover_pulse = 0.70f + 0.30f * (sinf(SDL_GetTicks() * 0.010f) * 0.5f + 0.5f);
    }

    SDL_Color top_color = top_normal;
    SDL_Color bottom_color = bot_normal;
    SDL_Color text_color = text_normal;
    SDL_Color border_color = border_dark;

    if (button->selected) {
        top_color = lerp_color(top_normal, top_hover, hover_pulse);
        bottom_color = lerp_color(bot_normal, bot_hover, hover_pulse);
        text_color = text_hover;
        border_color = border_warm;
    }

    if (pressed) {
        top_color = top_pressed;
        bottom_color = bot_pressed;
        text_color = text_pressed;
        border_color = lerp_color(border_dark, border_warm, 0.45f);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    render_hover_glow(renderer, &dest, hover_pulse, pressed);

    fill_rect(renderer, &dest, border_dark);
    fill_rect(renderer, &shell, shell_color);
    fill_vertical_gradient(renderer, &face, top_color, bottom_color);
    draw_rect(renderer, &shell, border_color);

    if (button->selected) {
        SDL_Rect ring = {dest.x - 1, dest.y - 1, dest.w + 2, dest.h + 2};
        SDL_Color ring_color = border_warm;
        ring_color.a = (Uint8)(130 + (int)(70.0f * hover_pulse));
        draw_rect(renderer, &ring, ring_color);
    }

    if (shell.w > 0 && shell.h > 1) {
        SDL_Rect top_line = {shell.x, shell.y, shell.w, 1};
        SDL_Rect bottom_line = {shell.x, shell.y + shell.h - 1, shell.w, 1};

        SDL_Color top_highlight = {0xE5, 0xF2, 0xFF, pressed ? 0x45 : 0x7A};
        SDL_Color bottom_shadow = {0x10, 0x1C, 0x2E, 0xB8};

        fill_rect(renderer, &top_line, top_highlight);
        fill_rect(renderer, &bottom_line, bottom_shadow);
    }

    if (font && button->label[0] != '\0') {
        TTF_Font* label_font = ui_font_for_text(font, button->label);
        if (!label_font) label_font = font;

        SDL_Surface* text_surf = TTF_RenderUTF8_Blended(label_font, button->label, text_color);
        if (!text_surf) return;

        SDL_Texture* text_tex = SDL_CreateTextureFromSurface(renderer, text_surf);
        if (text_tex) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
            SDL_SetTextureScaleMode(text_tex, SDL_ScaleModeLinear);
#endif
            int text_x = dest.x + (dest.w - text_surf->w) / 2;
            int text_y = dest.y + (dest.h - text_surf->h) / 2 + (pressed ? 1 : 0);
            SDL_Rect text_rect = {text_x, text_y, text_surf->w, text_surf->h};
            SDL_RenderCopy(renderer, text_tex, NULL, &text_rect);
            SDL_DestroyTexture(text_tex);
        }

        SDL_FreeSurface(text_surf);
    }
}
SDL_Texture* load_texture(const char* path, SDL_Renderer* renderer)
{
    SDL_Texture* tex = ui_load_texture(renderer, path);
    if (!tex) SDL_Log("CreateTextureFromSurface(%s): %s", path, SDL_GetError());
    return tex;
}

/* ---------------- Background ---------------- */
void init_background(Background* bg, SDL_Renderer* renderer)
{
    bg->background = load_texture(ASSET_MAIN_MENU_BACKGROUND, renderer);
    bg->snow_tex   = load_texture(ASSET_MAIN_MENU_SNOW, renderer);

    for (int i = 0; i < MAX_SNOW; ++i) {
        bg->snow[i].x = (float)(rand() % LOGICAL_W);
        bg->snow[i].y = (float)(rand() % LOGICAL_H);
        bg->snow[i].speed = 6.0f + (rand() % 12);
        bg->snow[i].drift = -6.0f + (rand() % 12);
        bg->snow[i].angle = (float)(rand() % 360);
        bg->snow[i].rotationSpeed = -20.0f + (rand() % 40);
        bg->snow[i].size = 2 + (rand() % 3);
    }
}

void update_background(Background* bg, float dt)
{
    for (int i = 0; i < MAX_SNOW; ++i) {
        bg->snow[i].y += bg->snow[i].speed * dt;
        bg->snow[i].x += bg->snow[i].drift * dt;
        bg->snow[i].angle += bg->snow[i].rotationSpeed * dt;

        if (bg->snow[i].y > LOGICAL_H) {
            bg->snow[i].y = -4.0f;
            bg->snow[i].x = (float)(rand() % LOGICAL_W);
        }
        if (bg->snow[i].x < -8.0f) bg->snow[i].x = (float)LOGICAL_W;
        if (bg->snow[i].x > LOGICAL_W + 8.0f) bg->snow[i].x = 0.0f;
    }
}

void render_background(Background* bg, SDL_Renderer* renderer)
{
    if (bg->background) {
        SDL_Rect dst = {0, 0, LOGICAL_W, LOGICAL_H};
        SDL_RenderCopy(renderer, bg->background, NULL, &dst);
    }

    if (bg->snow_tex) {
        for (int i = 0; i < MAX_SNOW; ++i) {
            SDL_Rect r = {(int)bg->snow[i].x, (int)bg->snow[i].y, bg->snow[i].size, bg->snow[i].size};
            SDL_RenderCopyEx(renderer, bg->snow_tex, NULL, &r, bg->snow[i].angle, NULL, SDL_FLIP_NONE);
        }
    }
}

void destroy_background(Background* bg)
{
    if (bg->background) SDL_DestroyTexture(bg->background);
    if (bg->snow_tex) SDL_DestroyTexture(bg->snow_tex);
}

/* ---------------- Pixel-text buttons ---------------- */
void init_buttons(Button buttons[], int count, SDL_Renderer* renderer)
{
    (void)renderer;

    const char* labels[MAX_BUTTONS] = {
        "PLAY",
        "OPTIONS",
        "TOP SCORES",
        "STORY",
        "EXIT"
    };

    const int y_start = 38;
    const int spacing = 22;
    const int button_w = 110;
    const int button_h = 18;
    const int button_x = (LOGICAL_W - button_w) / 2;

    if (!g_button_font) {
        g_button_font = ui_open_arial_font(12, 0);

        const char* font_candidates[] = {
            ASSET_BUTTON_FONT,
            ASSET_MAIN_MENU_FONT_OPTIONS,
            ASSET_MAIN_MENU_FONT_TEXT,
            ASSET_MAIN_MENU_FONT_TITLE
        };
        const int candidate_count = (int)(sizeof(font_candidates) / sizeof(font_candidates[0]));

        g_button_font = ui_open_font_from_candidates(font_candidates, candidate_count, 12, 0);

        if (!g_button_font) {
            SDL_Log("Button font load failed: %s", TTF_GetError());
        } else {
            ui_apply_font_quality(g_button_font);
        }
    }

    for (int i = 0; i < count; ++i) {
        const char* label = (i < MAX_BUTTONS) ? labels[i] : "";
        prototype_button_init(&buttons[i], button_x, y_start + i * spacing, button_w, button_h, label);
    }
}

void update_buttons(Button buttons[], int count, int* selected, float dt)
{
    int selected_index = selected ? *selected : -1;

    for (int i = 0; i < count; ++i) {
        buttons[i].selected = (i == selected_index);
        prototype_button_update(&buttons[i], dt);
    }
}

int button_at_point(Button buttons[], int count, int x, int y)
{
    for (int i = 0; i < count; ++i) {
        SDL_Rect rect = get_button_dest(&buttons[i]);
        if (point_in_rect(&rect, x, y)) {
            return i;
        }
    }
    return -1;
}

void render_main_menu_style_button(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const SDL_Rect* dest,
                                   const char* label,
                                   int selected,
                                   int pressed)
{
    if (!renderer || !dest || dest->w <= 0 || dest->h <= 0) return;

    Button temp = {0};
    temp.rect = *dest;
    temp.selected = selected ? 1 : 0;
    temp.scale = 1.0f;
    temp.targetScale = 1.0f;
    temp.bounceOffset = 0.0f;
    temp.pressedTimer = pressed ? 0.13f : 0.0f;
    if (label) {
        snprintf(temp.label, sizeof(temp.label), "%s", label);
    }

    prototype_button_render(&temp, renderer, font);
}

void render_buttons(Button buttons[], int count, SDL_Renderer* renderer)
{
    for (int i = 0; i < count; ++i) {
        prototype_button_render(&buttons[i], renderer, g_button_font);
    }
}

void render_buttons_no_text(Button buttons[], int count, SDL_Renderer* renderer)
{
    for (int i = 0; i < count; ++i) {
        prototype_button_render(&buttons[i], renderer, NULL);
    }
}

void destroy_buttons(Button buttons[], int count)
{
    (void)buttons;
    (void)count;

    if (g_button_font) {
        TTF_CloseFont(g_button_font);
        g_button_font = NULL;
    }
}

/* ---------------- Sprites ---------------- */
int init_sprite(AnimatedSprite* s, SDL_Renderer* renderer,
                const char* path, float y, float speed,
                int displayW, int displayH, int frames, int columns, float frameDelay)
{
    if (!s || !renderer || !path) return 0;

    s->texture = load_texture(path, renderer);
    if (!s->texture) {
        SDL_Log("init_sprite: failed to load %s", path);
        return 0;
    }

    int texW = 0, texH = 0;
    SDL_QueryTexture(s->texture, NULL, NULL, &texW, &texH);

    s->frames = frames;
    s->columns = columns > 0 ? columns : 1;
    int rows = (frames + s->columns - 1) / s->columns;

    s->frameW = texW / s->columns;
    s->frameH = texH / (rows > 0 ? rows : 1);
    s->displayW = displayW;
    s->displayH = displayH;
    s->currentFrame = 0;
    s->frameTimer = 0.0f;
    s->frameDelay = (frameDelay > 0.0f) ? frameDelay : 0.05f;
    s->speed = speed;
    s->x = - (float)s->displayW;
    s->y = y;

    return 1;
}

void update_sprite(AnimatedSprite* s, float dt)
{
    if (!s || !s->texture) return;

    s->x += s->speed * dt;
    if (s->x > LOGICAL_W) s->x = - (float)s->displayW;

    s->frameTimer += dt;
    if (s->frameTimer >= s->frameDelay) {
        s->frameTimer -= s->frameDelay;
        s->currentFrame = (s->currentFrame + 1) % s->frames;
    }
}

void render_sprite(AnimatedSprite* s, SDL_Renderer* renderer)
{
    if (!s || !s->texture) return;

    int col = s->currentFrame % s->columns;
    int row = s->currentFrame / s->columns;

    SDL_Rect src = { col * s->frameW, row * s->frameH, s->frameW, s->frameH };
    SDL_Rect dst = { (int)s->x, (int)s->y, s->displayW, s->displayH };
    SDL_RenderCopy(renderer, s->texture, &src, &dst);
}

void destroy_sprite(AnimatedSprite* s)
{
    if (s && s->texture) {
        SDL_DestroyTexture(s->texture);
        s->texture = NULL;
    }
}
