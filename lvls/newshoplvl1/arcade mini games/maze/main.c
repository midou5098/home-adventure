#include <stdio.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include "headers.h"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define RENDER_WIDTH 640
#define RENDER_HEIGHT 360
#define PLANE_SCALE 0.57735026919f
#define MOVE_SPEED 3.0f
#define TURN_SPEED 2.2f
#define MAX_DT 0.05f
#define PLAYER_RADIUS 0.18f
#define BASE_ENGINE_VOLUME 34
#define BOOST_ENGINE_VOLUME 92
#define RADIO_VOLUME_IDLE 38
#define RADIO_VOLUME_WALKING 20
#define ENGINE_VOLUME_BLEND_SPEED 7.0f
#define ENGINE_CHANNEL 0
#define RADIO_CHANNEL 1
#define TILE_FINISH 2
#define TILE_SPAWN 3
#define TILE_FAN 4
#define TILE_SWALL 5
#define FINISH_TRIGGER_DISTANCE 0.30f
#define FAN_FRAME_TIME_MS 110
#define FAN_LAYOUT_MAX_DIVISOR 12
#define FAN_OVERLAY_WIDTH_FRACTION 0.55f
#define FAN_OVERLAY_HEIGHT_FRACTION 0.62f

typedef struct Player {
    float x;
    float y;
    float angle;
} Player;

static bool is_tile_solid(int tile_value) {
    return tile_value != 0 && tile_value != TILE_SPAWN;
}

static bool is_position_walkable(float x, float y, float radius) {
    const int min_x = (int)floorf(x - radius);
    const int max_x = (int)floorf(x + radius);
    const int min_y = (int)floorf(y - radius);
    const int max_y = (int)floorf(y + radius);

    for (int cell_y = min_y; cell_y <= max_y; ++cell_y) {
        for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
            if (cell_x < 0 || cell_x >= MAP_WIDTH || cell_y < 0 || cell_y >= MAP_HEIGHT) {
                return false;
            }
            if (!is_tile_solid(damap[cell_y][cell_x])) {
                continue;
            }

            const float nearest_x = fmaxf((float)cell_x, fminf(x, (float)cell_x + 1.0f));
            const float nearest_y = fmaxf((float)cell_y, fminf(y, (float)cell_y + 1.0f));
            const float dx = x - nearest_x;
            const float dy = y - nearest_y;
            if ((dx * dx + dy * dy) < (radius * radius)) {
                return false;
            }
        }
    }
    return true;
}

static bool find_spawn_point(float *spawn_x, float *spawn_y) {
    if (spawn_x == NULL || spawn_y == NULL) {
        return false;
    }

    for (int cell_y = 0; cell_y < MAP_HEIGHT; ++cell_y) {
        for (int cell_x = 0; cell_x < MAP_WIDTH; ++cell_x) {
            if (damap[cell_y][cell_x] == TILE_SPAWN) {
                *spawn_x = (float)cell_x + 0.5f;
                *spawn_y = (float)cell_y + 0.5f;
                return true;
            }
        }
    }

    return false;
}

static bool is_near_tile_type(float x, float y, int tile_type, float distance) {
    const float distance_sq = distance * distance;
    for (int cell_y = 0; cell_y < MAP_HEIGHT; ++cell_y) {
        for (int cell_x = 0; cell_x < MAP_WIDTH; ++cell_x) {
            if (damap[cell_y][cell_x] != tile_type) {
                continue;
            }

            const float nearest_x = fmaxf((float)cell_x, fminf(x, (float)cell_x + 1.0f));
            const float nearest_y = fmaxf((float)cell_y, fminf(y, (float)cell_y + 1.0f));
            const float dx = x - nearest_x;
            const float dy = y - nearest_y;
            if ((dx * dx + dy * dy) <= distance_sq) {
                return true;
            }
        }
    }
    return false;
}

static void detect_fan_sheet_layout(int width, int height, int *out_cols, int *out_rows) {
    if (out_cols == NULL || out_rows == NULL) {
        return;
    }

    *out_cols = 1;
    *out_rows = 1;

    if (width <= 0 || height <= 0) {
        return;
    }

    if (width >= height * 2 && (width % height) == 0) {
        *out_cols = width / height;
        *out_rows = 1;
        return;
    }

    if (height >= width * 2 && (height % width) == 0) {
        *out_cols = 1;
        *out_rows = height / width;
        return;
    }

    const float aspect = (width > height) ? ((float)width / (float)height) : ((float)height / (float)width);
    if (aspect <= 1.35f) {
        int best_divisor = 1;
        for (int divisor = 2; divisor <= FAN_LAYOUT_MAX_DIVISOR; ++divisor) {
            if ((width % divisor) != 0 || (height % divisor) != 0) {
                continue;
            }

            const int frame_w = width / divisor;
            const int frame_h = height / divisor;
            if (frame_w < 48 || frame_h < 48) {
                continue;
            }
            best_divisor = divisor;
        }

        if (best_divisor > 1) {
            *out_cols = best_divisor;
            *out_rows = best_divisor;
            return;
        }
    }

    if ((width % height) == 0) {
        *out_cols = width / height;
        *out_rows = 1;
        return;
    }

    if ((height % width) == 0) {
        *out_cols = 1;
        *out_rows = height / width;
    }
}

static void draw_dark_background(SDL_Renderer *renderer) {
    const SDL_Rect ceiling_dst = {0, 0, RENDER_WIDTH, RENDER_HEIGHT / 2};
    const SDL_Rect floor_dst = {0, RENDER_HEIGHT / 2, RENDER_WIDTH, RENDER_HEIGHT / 2};
    SDL_SetRenderDrawColor(renderer, 54, 54, 54, 255);
    SDL_RenderFillRect(renderer, &ceiling_dst);
    SDL_SetRenderDrawColor(renderer, 34, 34, 34, 255);
    SDL_RenderFillRect(renderer, &floor_dst);
}

static SDL_Texture *build_static_background(SDL_Renderer *renderer) {
    SDL_Texture *background = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        RENDER_WIDTH,
        RENDER_HEIGHT
    );
    if (background == NULL) {
        return NULL;
    }

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (SDL_SetRenderTarget(renderer, background) != 0) {
        SDL_DestroyTexture(background);
        return NULL;
    }

    draw_dark_background(renderer);

    SDL_SetRenderTarget(renderer, previous_target);
    return background;
}

static void render_scene(
    SDL_Renderer *renderer,
    const Player *player,
    SDL_Texture *wall_texture,
    int wall_w,
    int wall_h,
    SDL_Texture *finish_texture,
    int finish_w,
    int finish_h,
    SDL_Texture *swall_texture,
    int swall_w,
    int swall_h,
    SDL_Texture *fan_texture,
    int fan_w,
    int fan_h,
    int fan_cols,
    int fan_rows,
    int fan_frame_count,
    SDL_Texture *background_texture
) {
    if (background_texture != NULL) {
        SDL_RenderCopy(renderer, background_texture, NULL, NULL);
    } else {
        draw_dark_background(renderer);
    }

    const float dir_x = cosf(player->angle);
    const float dir_y = sinf(player->angle);
    const float plane_x = -dir_y * PLANE_SCALE;
    const float plane_y = dir_x * PLANE_SCALE;

    for (int x = 0; x < RENDER_WIDTH; ++x) {
        const float camera_x = (2.0f * x / (float)RENDER_WIDTH) - 1.0f;
        const float ray_dir_x = dir_x + plane_x * camera_x;
        const float ray_dir_y = dir_y + plane_y * camera_x;

        int map_x = (int)player->x;
        int map_y = (int)player->y;

        const float delta_dist_x = (fabsf(ray_dir_x) < 0.00001f) ? 1e30f : fabsf(1.0f / ray_dir_x);
        const float delta_dist_y = (fabsf(ray_dir_y) < 0.00001f) ? 1e30f : fabsf(1.0f / ray_dir_y);

        int step_x;
        int step_y;
        float side_dist_x;
        float side_dist_y;

        if (ray_dir_x < 0.0f) {
            step_x = -1;
            side_dist_x = (player->x - (float)map_x) * delta_dist_x;
        } else {
            step_x = 1;
            side_dist_x = ((float)map_x + 1.0f - player->x) * delta_dist_x;
        }

        if (ray_dir_y < 0.0f) {
            step_y = -1;
            side_dist_y = (player->y - (float)map_y) * delta_dist_y;
        } else {
            step_y = 1;
            side_dist_y = ((float)map_y + 1.0f - player->y) * delta_dist_y;
        }

        int side = 0;
        while (true) {
            if (side_dist_x < side_dist_y) {
                side_dist_x += delta_dist_x;
                map_x += step_x;
                side = 0;
            } else {
                side_dist_y += delta_dist_y;
                map_y += step_y;
                side = 1;
            }

            if (map_x < 0 || map_x >= MAP_WIDTH || map_y < 0 || map_y >= MAP_HEIGHT) {
                break;
            }

            if (is_tile_solid(damap[map_y][map_x])) {
                break;
            }
        }

        int wall_type = 1;
        if (map_x >= 0 && map_x < MAP_WIDTH && map_y >= 0 && map_y < MAP_HEIGHT) {
            wall_type = damap[map_y][map_x];
        }

        SDL_Texture *column_texture = wall_texture;
        int tex_w = wall_w;
        int tex_h_total = wall_h;
        if (wall_type == TILE_FINISH && finish_texture != NULL && finish_w > 0 && finish_h > 0) {
            column_texture = finish_texture;
            tex_w = finish_w;
            tex_h_total = finish_h;
        } else if (wall_type == TILE_SWALL && swall_texture != NULL && swall_w > 0 && swall_h > 0) {
            column_texture = swall_texture;
            tex_w = swall_w;
            tex_h_total = swall_h;
        }
        const bool draw_fan_overlay = (
            wall_type == TILE_FAN &&
            fan_texture != NULL &&
            fan_w > 0 &&
            fan_h > 0 &&
            fan_frame_count > 0
        );

        float perp_wall_dist;
        if (side == 0) {
            perp_wall_dist = side_dist_x - delta_dist_x;
        } else {
            perp_wall_dist = side_dist_y - delta_dist_y;
        }
        if (perp_wall_dist < 0.03f) {
            perp_wall_dist = 0.03f;
        }

        int line_height = (int)(RENDER_HEIGHT / perp_wall_dist);
        int draw_start_raw = (-line_height / 2) + (RENDER_HEIGHT / 2);
        int draw_end_raw = (line_height / 2) + (RENDER_HEIGHT / 2);
        int draw_start = draw_start_raw;
        int draw_end = draw_end_raw;

        if (draw_start < 0) {
            draw_start = 0;
        }
        if (draw_end >= RENDER_HEIGHT) {
            draw_end = RENDER_HEIGHT - 1;
        }
        if (draw_end < draw_start) {
            continue;
        }

        float wall_x;
        if (side == 0) {
            wall_x = player->y + perp_wall_dist * ray_dir_y;
        } else {
            wall_x = player->x + perp_wall_dist * ray_dir_x;
        }
        wall_x -= floorf(wall_x);

        int tex_x = (int)(wall_x * (float)tex_w);
        if (tex_x < 0) {
            tex_x = 0;
        } else if (tex_x >= tex_w) {
            tex_x = tex_w - 1;
        }

        if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f)) {
            tex_x = tex_w - tex_x - 1;
        }

        const int draw_height = draw_end - draw_start + 1;
        const float tex_step = (line_height > 0) ? ((float)tex_h_total / (float)line_height) : 0.0f;
        int tex_y = (int)((draw_start - draw_start_raw) * tex_step);
        if (tex_y < 0) {
            tex_y = 0;
        }
        int tex_h = (int)(draw_height * tex_step);
        if (tex_h < 1) {
            tex_h = 1;
        }
        if (tex_y + tex_h > tex_h_total) {
            tex_h = tex_h_total - tex_y;
        }
        if (tex_h < 1) {
            tex_h = 1;
        }

        const SDL_Rect src = {tex_x, tex_y, 1, tex_h};
        const SDL_Rect dst = {x, draw_start, 1, draw_height};

        float shade = 1.0f / (1.0f + 0.12f * perp_wall_dist * perp_wall_dist);
        if (side == 1) {
            shade *= 0.78f;
        }
        const Uint8 mod = (Uint8)(55.0f + 200.0f * shade);
        SDL_SetTextureColorMod(column_texture, mod, mod, mod);
        SDL_RenderCopy(renderer, column_texture, &src, &dst);

        if (draw_fan_overlay) {
            const int frame_w = (fan_cols > 0) ? (fan_w / fan_cols) : 0;
            const int frame_h = (fan_rows > 0) ? (fan_h / fan_rows) : 0;
            if (frame_w > 0 && frame_h > 0) {
                const float fan_half_width = FAN_OVERLAY_WIDTH_FRACTION * 0.5f;
                const float fan_left = 0.5f - fan_half_width;
                const float fan_right = 0.5f + fan_half_width;
                if (wall_x >= fan_left && wall_x <= fan_right) {
                    const int frame_index = (int)((SDL_GetTicks() / FAN_FRAME_TIME_MS) % (Uint32)fan_frame_count);
                    const int frame_col = frame_index % fan_cols;
                    const int frame_row = frame_index / fan_cols;
                    float fan_u = (wall_x - fan_left) / (fan_right - fan_left);
                    if ((side == 0 && ray_dir_x > 0.0f) || (side == 1 && ray_dir_y < 0.0f)) {
                        fan_u = 1.0f - fan_u;
                    }

                    int fan_tex_x = (int)(fan_u * (float)frame_w);
                    if (fan_tex_x < 0) {
                        fan_tex_x = 0;
                    } else if (fan_tex_x >= frame_w) {
                        fan_tex_x = frame_w - 1;
                    }

                    const int fan_draw_start_raw = draw_start_raw + (int)((line_height * (1.0f - FAN_OVERLAY_HEIGHT_FRACTION)) * 0.5f);
                    const int fan_draw_end_raw = fan_draw_start_raw + (int)(line_height * FAN_OVERLAY_HEIGHT_FRACTION);
                    int fan_draw_start = fan_draw_start_raw;
                    int fan_draw_end = fan_draw_end_raw;

                    if (fan_draw_start < draw_start) {
                        fan_draw_start = draw_start;
                    }
                    if (fan_draw_end > draw_end) {
                        fan_draw_end = draw_end;
                    }

                    if (fan_draw_end >= fan_draw_start) {
                        int fan_draw_h = fan_draw_end - fan_draw_start + 1;
                        int fan_raw_h = fan_draw_end_raw - fan_draw_start_raw + 1;
                        if (fan_raw_h < 1) {
                            fan_raw_h = 1;
                        }

                        int fan_tex_y = (int)(((fan_draw_start - fan_draw_start_raw) / (float)fan_raw_h) * frame_h);
                        if (fan_tex_y < 0) {
                            fan_tex_y = 0;
                        }

                        int fan_tex_h = (int)((fan_draw_h / (float)fan_raw_h) * frame_h);
                        if (fan_tex_h < 1) {
                            fan_tex_h = 1;
                        }
                        if (fan_tex_y + fan_tex_h > frame_h) {
                            fan_tex_h = frame_h - fan_tex_y;
                        }
                        if (fan_tex_h < 1) {
                            fan_tex_h = 1;
                        }

                        const SDL_Rect fan_src = {
                            frame_col * frame_w + fan_tex_x,
                            frame_row * frame_h + fan_tex_y,
                            1,
                            fan_tex_h
                        };
                        const SDL_Rect fan_dst = {x, fan_draw_start, 1, fan_draw_h};
                        SDL_SetTextureColorMod(fan_texture, mod, mod, mod);
                        SDL_RenderCopy(renderer, fan_texture, &fan_src, &fan_dst);
                    }
                }
            }
        }
    }

    SDL_SetTextureColorMod(wall_texture, 255, 255, 255);
    if (finish_texture != NULL) {
        SDL_SetTextureColorMod(finish_texture, 255, 255, 255);
    }
    if (swall_texture != NULL) {
        SDL_SetTextureColorMod(swall_texture, 255, 255, 255);
    }
    if (fan_texture != NULL) {
        SDL_SetTextureColorMod(fan_texture, 255, 255, 255);
    }
}

static void draw_player_car_overlay(
    SDL_Renderer *renderer,
    SDL_Texture *car_texture,
    int car_w,
    int car_h
) {
    if (car_texture == NULL || car_w <= 0 || car_h <= 0) {
        return;
    }

    int dst_w = (RENDER_WIDTH * 70) / 100;
    int dst_h = (int)((float)dst_w * ((float)car_h / (float)car_w));
    const int max_h = (RENDER_HEIGHT * 75) / 100;
    if (dst_h > max_h) {
        dst_h = max_h;
        dst_w = (int)((float)dst_h * ((float)car_w / (float)car_h));
    }

    dst_w /= 2;
    dst_h /= 2;
    dst_w = (dst_w * 3) / 2;
    if (dst_w < 1) {
        dst_w = 1;
    }
    if (dst_h < 1) {
        dst_h = 1;
    }

    const SDL_Rect dst = {
        (RENDER_WIDTH - dst_w) / 2,
        RENDER_HEIGHT - dst_h,
        dst_w,
        dst_h
    };
    SDL_RenderCopy(renderer, car_texture, NULL, &dst);
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    const int image_flags = IMG_INIT_PNG;
    if ((IMG_Init(image_flags) & image_flags) != image_flags) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    bool mixer_initialized = false;
    bool audio_opened = false;
    bool engine_active = false;
    bool radio_playing = false;
    Mix_Chunk *engine_sound = NULL;
    Mix_Chunk *radio_sound = NULL;
    float current_engine_volume = (float)BASE_ENGINE_VOLUME;

    const int mixer_flags = MIX_INIT_MP3;
    if ((Mix_Init(mixer_flags) & mixer_flags) != mixer_flags) {
        fprintf(stderr, "Warning: Mix_Init MP3 failed: %s\n", Mix_GetError());
    } else {
        mixer_initialized = true;
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
            fprintf(stderr, "Warning: Mix_OpenAudio failed: %s\n", Mix_GetError());
        } else {
            audio_opened = true;
            Mix_AllocateChannels(8);

            engine_sound = Mix_LoadWAV("carsound.mp3");
            if (engine_sound == NULL) {
                fprintf(stderr, "Warning: Mix_LoadWAV carsound.mp3 failed: %s\n", Mix_GetError());
            } else {
                Mix_Volume(ENGINE_CHANNEL, BASE_ENGINE_VOLUME);
                if (Mix_PlayChannel(ENGINE_CHANNEL, engine_sound, -1) < 0) {
                    fprintf(stderr, "Warning: Mix_PlayChannel engine failed: %s\n", Mix_GetError());
                } else {
                    engine_active = true;
                }
            }

            radio_sound = Mix_LoadWAV("radio.mp3");
            if (radio_sound == NULL) {
                fprintf(stderr, "Warning: Mix_LoadWAV radio.mp3 failed: %s\n", Mix_GetError());
            }
        }
    }

    SDL_Window *window = SDL_CreateWindow(
        "Raycaster Prototype",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    if (SDL_RenderSetLogicalSize(renderer, RENDER_WIDTH, RENDER_HEIGHT) != 0) {
        fprintf(stderr, "SDL_RenderSetLogicalSize warning: %s\n", SDL_GetError());
    }

    SDL_Surface *wall_surface = IMG_Load("walls.png");
    if (wall_surface == NULL) {
        fprintf(stderr, "IMG_Load walls.png failed: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture *wall_texture = SDL_CreateTextureFromSurface(renderer, wall_surface);
    SDL_FreeSurface(wall_surface);
    if (wall_texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int wall_w = 0;
    int wall_h = 0;
    SDL_QueryTexture(wall_texture, NULL, NULL, &wall_w, &wall_h);
    if (wall_w <= 0 || wall_h <= 0) {
        fprintf(stderr, "walls.png texture has invalid size.\n");
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface *finish_surface = IMG_Load("finall.png");
    if (finish_surface == NULL) {
        fprintf(stderr, "IMG_Load finall.png failed: %s\n", IMG_GetError());
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture *finish_texture = SDL_CreateTextureFromSurface(renderer, finish_surface);
    SDL_FreeSurface(finish_surface);
    if (finish_texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface (finall.png) failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int finish_w = 0;
    int finish_h = 0;
    SDL_QueryTexture(finish_texture, NULL, NULL, &finish_w, &finish_h);
    if (finish_w <= 0 || finish_h <= 0) {
        fprintf(stderr, "finall.png texture has invalid size.\n");
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface *swall_surface = IMG_Load("swall.png");
    if (swall_surface == NULL) {
        fprintf(stderr, "IMG_Load swall.png failed: %s\n", IMG_GetError());
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture *swall_texture = SDL_CreateTextureFromSurface(renderer, swall_surface);
    SDL_FreeSurface(swall_surface);
    if (swall_texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface (swall.png) failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    int swall_w = 0;
    int swall_h = 0;
    SDL_QueryTexture(swall_texture, NULL, NULL, &swall_w, &swall_h);
    if (swall_w <= 0 || swall_h <= 0) {
        fprintf(stderr, "swall.png texture has invalid size.\n");
        SDL_DestroyTexture(swall_texture);
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture *fan_texture = NULL;
    int fan_w = 0;
    int fan_h = 0;
    int fan_cols = 1;
    int fan_rows = 1;
    int fan_frame_count = 1;
    SDL_Surface *fan_surface = IMG_Load("fan.png");
    if (fan_surface == NULL) {
        fprintf(stderr, "Warning: IMG_Load fan.png failed: %s\n", IMG_GetError());
    } else {
        fan_texture = SDL_CreateTextureFromSurface(renderer, fan_surface);
        SDL_FreeSurface(fan_surface);
        if (fan_texture == NULL) {
            fprintf(stderr, "Warning: SDL_CreateTextureFromSurface (fan.png) failed: %s\n", SDL_GetError());
        } else {
            SDL_SetTextureBlendMode(fan_texture, SDL_BLENDMODE_BLEND);
            SDL_QueryTexture(fan_texture, NULL, NULL, &fan_w, &fan_h);
            if (fan_w <= 0 || fan_h <= 0) {
                SDL_DestroyTexture(fan_texture);
                fan_texture = NULL;
                fan_w = 0;
                fan_h = 0;
                fprintf(stderr, "Warning: fan.png texture has invalid size.\n");
            } else {
                fan_frame_count = fan_w / fan_h;
                if (fan_frame_count < 1) {
                    fan_frame_count = 1;
                }
                detect_fan_sheet_layout(fan_w, fan_h, &fan_cols, &fan_rows);
                fan_frame_count = fan_cols * fan_rows;
                if (fan_frame_count < 1) {
                    fan_frame_count = 1;
                    fan_cols = 1;
                    fan_rows = 1;
                }
            }
        }
    }

    SDL_Texture *background_texture = build_static_background(renderer);
    if (background_texture == NULL) {
        fprintf(stderr, "Warning: could not build static background, drawing directly each frame.\n");
    }

    SDL_Surface *car_surface = IMG_Load("car.png");
    if (car_surface == NULL) {
        fprintf(stderr, "IMG_Load car.png failed: %s\n", IMG_GetError());
        SDL_DestroyTexture(background_texture);
        SDL_DestroyTexture(fan_texture);
        SDL_DestroyTexture(swall_texture);
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture *car_texture = SDL_CreateTextureFromSurface(renderer, car_surface);
    SDL_FreeSurface(car_surface);
    if (car_texture == NULL) {
        fprintf(stderr, "SDL_CreateTextureFromSurface (car.png) failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(background_texture);
        SDL_DestroyTexture(fan_texture);
        SDL_DestroyTexture(swall_texture);
        SDL_DestroyTexture(finish_texture);
        SDL_DestroyTexture(wall_texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetTextureBlendMode(car_texture, SDL_BLENDMODE_BLEND);

    int car_w = 0;
    int car_h = 0;
    SDL_QueryTexture(car_texture, NULL, NULL, &car_w, &car_h);

    Player player = {1.5f, 1.5f, 0.0f};
    (void)find_spawn_point(&player.x, &player.y);
    bool running = true;
    Uint32 prev_ticks = SDL_GetTicks();

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (
                event.type == SDL_KEYDOWN &&
                event.key.repeat == 0 &&
                event.key.keysym.sym == SDLK_r &&
                audio_opened &&
                radio_sound != NULL
            ) {
                if (radio_playing) {
                    Mix_HaltChannel(RADIO_CHANNEL);
                    radio_playing = false;
                } else {
                    Mix_Volume(RADIO_CHANNEL, RADIO_VOLUME_IDLE);
                    if (Mix_PlayChannel(RADIO_CHANNEL, radio_sound, -1) < 0) {
                        fprintf(stderr, "Warning: Mix_PlayChannel radio failed: %s\n", Mix_GetError());
                    } else {
                        radio_playing = true;
                    }
                }
            }
        }

        const Uint32 now = SDL_GetTicks();
        float dt = (now - prev_ticks) / 1000.0f;
        prev_ticks = now;
        if (dt > MAX_DT) {
            dt = MAX_DT;
        }

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        float move_step = 0.0f;

        if (keys[SDL_SCANCODE_W]) {
            move_step += MOVE_SPEED * dt;
        }
        if (keys[SDL_SCANCODE_S]) {
            move_step -= MOVE_SPEED * dt;
        }
        if (keys[SDL_SCANCODE_A]) {
            player.angle -= TURN_SPEED * dt;
        }
        if (keys[SDL_SCANCODE_D]) {
            player.angle += TURN_SPEED * dt;
        }

        const float next_x = player.x + cosf(player.angle) * move_step;
        const float next_y = player.y + sinf(player.angle) * move_step;

        if (is_position_walkable(next_x, player.y, PLAYER_RADIUS)) {
            player.x = next_x;
        }
        if (is_position_walkable(player.x, next_y, PLAYER_RADIUS)) {
            player.y = next_y;
        }

        if (is_near_tile_type(player.x, player.y, TILE_FINISH, FINISH_TRIGGER_DISTANCE)) {
            printf("complet\n");
            fflush(stdout);
            running = false;
            continue;
        }

        if (engine_active) {
            const float target_engine_volume = keys[SDL_SCANCODE_W] ? (float)BOOST_ENGINE_VOLUME : (float)BASE_ENGINE_VOLUME;
            const float blend = fminf(1.0f, dt * ENGINE_VOLUME_BLEND_SPEED);
            current_engine_volume += (target_engine_volume - current_engine_volume) * blend;
            Mix_Volume(ENGINE_CHANNEL, (int)(current_engine_volume + 0.5f));
        }
        if (radio_playing) {
            const int target_radio_volume = keys[SDL_SCANCODE_W] ? RADIO_VOLUME_WALKING : RADIO_VOLUME_IDLE;
            Mix_Volume(RADIO_CHANNEL, target_radio_volume);
        }

        render_scene(
            renderer,
            &player,
            wall_texture,
            wall_w,
            wall_h,
            finish_texture,
            finish_w,
            finish_h,
            swall_texture,
            swall_w,
            swall_h,
            fan_texture,
            fan_w,
            fan_h,
            fan_cols,
            fan_rows,
            fan_frame_count,
            background_texture
        );
        draw_player_car_overlay(renderer, car_texture, car_w, car_h);
        SDL_RenderPresent(renderer);
    }

    if (engine_active) {
        Mix_HaltChannel(ENGINE_CHANNEL);
    }
    if (radio_playing) {
        Mix_HaltChannel(RADIO_CHANNEL);
    }
    if (engine_sound != NULL) {
        Mix_FreeChunk(engine_sound);
    }
    if (radio_sound != NULL) {
        Mix_FreeChunk(radio_sound);
    }
    if (audio_opened) {
        Mix_CloseAudio();
    }
    if (mixer_initialized) {
        Mix_Quit();
    }

    SDL_DestroyTexture(car_texture);
    SDL_DestroyTexture(background_texture);
    SDL_DestroyTexture(fan_texture);
    SDL_DestroyTexture(swall_texture);
    SDL_DestroyTexture(finish_texture);
    SDL_DestroyTexture(wall_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
