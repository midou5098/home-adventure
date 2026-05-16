#ifndef MAIN_SCENE_HELPERS_H
#define MAIN_SCENE_HELPERS_H

#include "mainmenu_headers.h"

SDL_Texture* main_scene_create_title_texture(SDL_Renderer* renderer,
                                             TTF_Font* title_font,
                                             SDL_Rect* out_rect);

int main_scene_touch_to_logical(float nx, float ny, int* lx, int* ly);
int main_scene_button_from_logical_point(Button buttons[], int count, int lx, int ly);

void main_scene_set_menu_render_mode(SDL_Renderer* renderer);
void main_scene_set_window_render_mode(SDL_Renderer* renderer);

void main_scene_render_brightness_overlay(SDL_Renderer* renderer, int brightness);
void main_scene_render_fade_overlay(SDL_Renderer* renderer, float fade_alpha, int use_window_space);

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
                       int brightness);

#endif
