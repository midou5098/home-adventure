#include "main_scene_helpers.h"
#include "ui_shared.h"

static int main_scene_logical_to_window_x(int logical_x)
{
    return (logical_x * WINDOW_W) / LOGICAL_W;
}

static int main_scene_logical_to_window_y(int logical_y)
{
    return (logical_y * WINDOW_H) / LOGICAL_H;
}

SDL_Texture* main_scene_create_title_texture(SDL_Renderer* renderer,
                                             TTF_Font* title_font,
                                             SDL_Rect* out_rect)
{
    SDL_Texture* tex = NULL;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf = NULL;

    if (out_rect) {
        *out_rect = (SDL_Rect){0, 0, 0, 0};
    }

    if (!renderer || !title_font || !out_rect) return NULL;

    surf = TTF_RenderUTF8_Blended(title_font, "Home Adventure", white);
    if (!surf) return NULL;

    tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
        SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
#endif
        out_rect->x = (LOGICAL_W - surf->w) / 2;
        out_rect->y = 8;
        out_rect->w = surf->w;
        out_rect->h = surf->h;
    }

    SDL_FreeSurface(surf);
    return tex;
}

int main_scene_touch_to_logical(float nx, float ny, int* lx, int* ly)
{
    int x = 0;
    int y = 0;

    if (!lx || !ly) return 0;
    if (nx < 0.0f || ny < 0.0f || nx > 1.0f || ny > 1.0f) return 0;

    x = (int)floorf(nx * (float)LOGICAL_W);
    y = (int)floorf(ny * (float)LOGICAL_H);

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= LOGICAL_W) x = LOGICAL_W - 1;
    if (y >= LOGICAL_H) y = LOGICAL_H - 1;

    *lx = x;
    *ly = y;
    return 1;
}

int main_scene_button_from_logical_point(Button buttons[], int count, int lx, int ly)
{
    if (lx < 0 || ly < 0 || lx >= LOGICAL_W || ly >= LOGICAL_H) return -1;
    return button_at_point(buttons, count, lx, ly);
}

void main_scene_set_menu_render_mode(SDL_Renderer* renderer)
{
    if (!renderer) return;
    SDL_RenderSetLogicalSize(renderer, LOGICAL_W, LOGICAL_H);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
}

void main_scene_set_window_render_mode(SDL_Renderer* renderer)
{
    if (!renderer) return;
    SDL_RenderSetLogicalSize(renderer, WINDOW_W, WINDOW_H);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
}

void main_scene_render_brightness_overlay(SDL_Renderer* renderer, int brightness)
{
    int alpha = 0;
    SDL_Rect r = {0, 0, LOGICAL_W, LOGICAL_H};

    if (!renderer) return;
    if (brightness < 0) brightness = 0;
    if (brightness > 10) brightness = 10;

    alpha = (10 - brightness) * 18;
    if (alpha <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)alpha);
    SDL_RenderFillRect(renderer, &r);
}

void main_scene_render_fade_overlay(SDL_Renderer* renderer, float fade_alpha, int use_window_space)
{
    SDL_Rect fade_rect = {0, 0, LOGICAL_W, LOGICAL_H};

    if (!renderer || fade_alpha <= 0.0f) return;

    if (use_window_space) {
        fade_rect = (SDL_Rect){0, 0, WINDOW_W, WINDOW_H};
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x0D, 0x18, 0x28, (Uint8)fade_alpha);
    SDL_RenderFillRect(renderer, &fade_rect);
}

static void main_scene_render_text_overlay_hi_res(SDL_Renderer* renderer,
                                                  TTF_Font* title_font,
                                                  TTF_Font* button_font,
                                                  Button buttons[],
                                                  int button_count)
{
    const SDL_Color title_color = {0xFF, 0xFF, 0xFF, 0xFF};
    const SDL_Color text_normal = {0xF7, 0xF2, 0xE8, 0xFF};
    const SDL_Color text_hover = {0xFF, 0xFA, 0xE2, 0xFF};
    const SDL_Color text_pressed = {0xF0, 0xE6, 0xD0, 0xFF};

    if (!renderer || !title_font || !button_font || !buttons || button_count <= 0) return;

    main_scene_set_window_render_mode(renderer);

    ui_draw_text_center(renderer,
                        title_font,
                        "Home Adventure",
                        WINDOW_W / 2,
                        main_scene_logical_to_window_y(8),
                        title_color);

    for (int i = 0; i < button_count; ++i) {
        SDL_Rect logical_dest = get_button_dest(&buttons[i]);
        SDL_Rect win_dest = {
            main_scene_logical_to_window_x(logical_dest.x),
            main_scene_logical_to_window_y(logical_dest.y),
            main_scene_logical_to_window_x(logical_dest.w),
            main_scene_logical_to_window_y(logical_dest.h)
        };
        SDL_Color color = text_normal;
        int pressed = (buttons[i].pressedTimer > 0.0f);
        int pressed_offset = pressed ? main_scene_logical_to_window_y(1) : 0;
        int text_h = TTF_FontHeight(button_font);
        int text_y = win_dest.y + (win_dest.h - text_h) / 2 + pressed_offset;
        int center_x = win_dest.x + win_dest.w / 2;

        if (buttons[i].selected) color = text_hover;
        if (pressed) color = text_pressed;

        ui_draw_text_center(renderer, button_font, buttons[i].label, center_x, text_y, color);
    }

    main_scene_set_menu_render_mode(renderer);
}

void main_scene_render(SDL_Renderer* renderer,
                       Background* bg,
                       AnimatedSprite* kid,
                       AnimatedSprite* dog,
                       AnimatedSprite* thief1,
                       AnimatedSprite* thief2,
                       int thieves_started,
                       TTF_Font* title_font_hi,
                       TTF_Font* button_font_hi,
                       SDL_Texture* title_texture,
                       const SDL_Rect* title_rect,
                       Button buttons[],
                       int button_count,
                       int brightness)
{
    int use_hi_res_text = 0;

    if (!renderer || !bg || !kid || !dog || !thief1 || !thief2 || !buttons) return;
    if (button_count <= 0) return;

    use_hi_res_text = (title_font_hi != NULL && button_font_hi != NULL);

    render_background(bg, renderer);
    render_sprite(kid, renderer);
    render_sprite(dog, renderer);
    if (thieves_started) {
        render_sprite(thief1, renderer);
        render_sprite(thief2, renderer);
    }

    if (!use_hi_res_text && title_texture && title_rect) {
        SDL_RenderCopy(renderer, title_texture, NULL, title_rect);
    }

    if (use_hi_res_text) {
        render_buttons_no_text(buttons, button_count, renderer);
        main_scene_render_text_overlay_hi_res(renderer, title_font_hi, button_font_hi, buttons, button_count);
    } else {
        render_buttons(buttons, button_count, renderer);
    }

    main_scene_render_brightness_overlay(renderer, brightness);
}
