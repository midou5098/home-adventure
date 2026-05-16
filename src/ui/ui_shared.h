#ifndef UI_SHARED_H
#define UI_SHARED_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

int ui_resolve_asset_path(const char* relative_path, char* out_path, size_t out_path_size);
SDL_Texture* ui_load_texture(SDL_Renderer* renderer, const char* relative_path);
Mix_Chunk* ui_load_wav(const char* relative_path);
Mix_Music* ui_load_music(const char* relative_path);
FILE* ui_open_asset_file(const char* relative_path, const char* mode);

TTF_Font* ui_open_font_from_candidates(const char* const* candidates,
                                       int count,
                                       int point_size,
                                       int disable_kerning);
TTF_Font* ui_open_arial_font(int point_size, int disable_kerning);
TTF_Font* ui_font_for_text(TTF_Font* preferred_font, const char* text);
void ui_apply_font_quality(TTF_Font* font);

void ui_draw_text_center(SDL_Renderer* renderer,
                         TTF_Font* font,
                         const char* text,
                         int center_x,
                         int y,
                         SDL_Color color);

void ui_draw_text_left(SDL_Renderer* renderer,
                       TTF_Font* font,
                       const char* text,
                       int x,
                       int y,
                       SDL_Color color);

void ui_fill_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color);
void ui_draw_rect(SDL_Renderer* renderer, const SDL_Rect* rect, SDL_Color color);

void ui_render_styled_button(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const SDL_Rect* rect,
                             const char* label,
                             int hovered,
                             int pressed,
                             int enable_hover_glow);

#endif
