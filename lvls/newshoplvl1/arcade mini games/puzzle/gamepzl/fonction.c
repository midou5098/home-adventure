#include "header.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define ROUND_TIME_MS 12000
#define OVERLAY_ANIM_MS 650
#define PI_F 3.14159265358979323846f
#define DEFAULT_MUSIC_VOLUME (MIX_MAX_VOLUME / 3)
#define DEFAULT_CHUNK_VOLUME (MIX_MAX_VOLUME / 2)

static const SDL_Color COLOR_BG = {6, 10, 16, 255};
static const SDL_Color COLOR_PANEL = {14, 23, 34, 214};
static const SDL_Color COLOR_PANEL_ALT = {18, 29, 42, 226};
static const SDL_Color COLOR_PANEL_SOFT = {41, 63, 87, 120};
static const SDL_Color COLOR_TEXT = {246, 243, 236, 255};
static const SDL_Color COLOR_MUTED = {172, 185, 198, 255};
static const SDL_Color COLOR_ACCENT = {239, 191, 102, 255};
static const SDL_Color COLOR_SUCCESS = {125, 202, 165, 255};
static const SDL_Color COLOR_DANGER = {220, 112, 112, 255};
static const SDL_Color COLOR_SHADOW = {4, 7, 12, 110};

static const SDL_FRect HEADER_BAR = {26.0f, 22.0f, 1314.0f, 132.0f};
static const SDL_FRect STAGE_PANEL = {26.0f, 164.0f, 882.0f, 620.0f};
static const SDL_FRect STAGE_IMAGE_RECT = {58.0f, 246.0f, 818.0f, 474.0f};
static const SDL_FRect OPTIONS_PANEL = {934.0f, 164.0f, 406.0f, 620.0f};
static const SDL_FRect TIMER_BOUNDS = {792.0f, 30.0f, 114.0f, 110.0f};

static float random_float(float min_value, float max_value) {
    return min_value + ((float) rand() / (float) RAND_MAX) * (max_value - min_value);
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool point_in_frect(float x, float y, SDL_FRect rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

static void shuffle_ints(int *values, int count) {
    for (int i = count - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = values[i];
        values[i] = values[j];
        values[j] = tmp;
    }
}

static bool has_image_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) {
        return false;
    }
    return strcasecmp(ext, ".png") == 0 ||
           strcasecmp(ext, ".jpg") == 0 ||
           strcasecmp(ext, ".jpeg") == 0 ||
           strcasecmp(ext, ".webp") == 0;
}

static void get_filename_without_extension(const char *input, char *output, size_t output_size) {
    size_t length = strlen(input);
    const char *dot = strrchr(input, '.');
    if (dot) {
        length = (size_t) (dot - input);
    }
    if (length >= output_size) {
        length = output_size - 1;
    }
    memcpy(output, input, length);
    output[length] = '\0';
}

static SDL_FRect fit_contain_rect(int source_w, int source_h, SDL_FRect box) {
    float scale_x = box.w / (float) source_w;
    float scale_y = box.h / (float) source_h;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    SDL_FRect result;
    result.w = source_w * scale;
    result.h = source_h * scale;
    result.x = box.x + (box.w - result.w) * 0.5f;
    result.y = box.y + (box.h - result.h) * 0.5f;
    return result;
}

static SDL_FRect fit_cover_rect(int source_w, int source_h, SDL_FRect box) {
    float scale_x = box.w / (float) source_w;
    float scale_y = box.h / (float) source_h;
    float scale = scale_x > scale_y ? scale_x : scale_y;

    SDL_FRect result;
    result.w = source_w * scale;
    result.h = source_h * scale;
    result.x = box.x + (box.w - result.w) * 0.5f;
    result.y = box.y + (box.h - result.h) * 0.5f;
    return result;
}

static void draw_filled_rect(SDL_Renderer *renderer, SDL_FRect rect, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRectF(renderer, &rect);
}

static void draw_outline_rect(SDL_Renderer *renderer, SDL_FRect rect, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRectF(renderer, &rect);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, float x, float y);
static void draw_wrapped_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                              float x, float y, Uint32 wrap_length);
static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, SDL_FRect rect, float y);

static void draw_shadow_rect(SDL_Renderer *renderer, SDL_FRect rect, float spread, SDL_Color color) {
    SDL_FRect shadow = {rect.x + spread * 0.35f, rect.y + spread, rect.w, rect.h};
    draw_filled_rect(renderer, shadow, color);
}

static void draw_panel_shell(SDL_Renderer *renderer, SDL_FRect rect, SDL_Color fill, SDL_Color border) {
    draw_shadow_rect(renderer, rect, 12.0f, COLOR_SHADOW);
    draw_filled_rect(renderer, rect, fill);
    draw_outline_rect(renderer, rect, border);

    SDL_FRect glow = {rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 22.0f};
    draw_filled_rect(renderer, glow, (SDL_Color){255, 255, 255, 18});
}

static void draw_stat_chip_score(Game *game, SDL_FRect rect, const char *label, const char *value,
                                 SDL_Color chip_color, SDL_Color value_color, float label_shift_x) {
    draw_panel_shell(game->renderer, rect, chip_color, (SDL_Color){255, 255, 255, 26});
    draw_text(game->renderer, game->font_small, label, COLOR_MUTED, rect.x + 16.0f + label_shift_x, rect.y + 10.0f);
    draw_text_centered(game->renderer, game->font_small, value, value_color, rect, rect.y + 38.0f);
}

static void draw_stat_chip_centered_value(Game *game, SDL_FRect rect, const char *label, const char *value,
                                          SDL_Color chip_color, SDL_Color value_color, float label_shift_x) {
    draw_panel_shell(game->renderer, rect, chip_color, (SDL_Color){255, 255, 255, 26});
    draw_text(game->renderer, game->font_small, label, COLOR_MUTED, rect.x + 16.0f + label_shift_x, rect.y + 10.0f);
    draw_text_centered(game->renderer, game->font_small, value, value_color, rect, rect.y + 38.0f);
}

static void draw_stat_chip_mode(Game *game, SDL_FRect rect, const char *label, const char *value,
                                SDL_Color chip_color, SDL_Color value_color, float label_shift_x) {
    draw_panel_shell(game->renderer, rect, chip_color, (SDL_Color){255, 255, 255, 26});
    draw_text(game->renderer, game->font_small, label, COLOR_MUTED, rect.x + 16.0f + label_shift_x, rect.y + 10.0f);
    draw_text_centered(game->renderer, game->font_tiny, value, value_color, rect, rect.y + 42.0f);
}

static void draw_snowflake(SDL_Renderer *renderer, float x, float y, float radius, SDL_Color color) {
    SDL_FRect body = {x - radius, y - radius, radius * 2.0f, radius * 2.0f};
    draw_filled_rect(renderer, body, color);
}

static void draw_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, float x, float y) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_FRect dst = {x, y, (float) surface->w, (float) surface->h};
        SDL_RenderCopyF(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void draw_text_centered(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color, SDL_FRect rect, float y) {
    int text_width = 0;
    int text_height = 0;
    if (TTF_SizeUTF8(font, text, &text_width, &text_height) != 0) {
        return;
    }

    float x = rect.x + (rect.w - (float) text_width) * 0.5f;
    draw_text(renderer, font, text, color, x, y);
}

static void draw_wrapped_text(SDL_Renderer *renderer, TTF_Font *font, const char *text, SDL_Color color,
                              float x, float y, Uint32 wrap_length) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended_Wrapped(font, text, color, wrap_length);
    if (!surface) {
        return;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_FRect dst = {x, y, (float) surface->w, (float) surface->h};
        SDL_RenderCopyF(renderer, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

static void draw_circle_segments(SDL_Renderer *renderer, float cx, float cy, float radius,
                                 float ratio, SDL_Color color, int segments) {
    if (ratio <= 0.0f) {
        return;
    }

    float end_angle = -90.0f + 360.0f * ratio;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    float previous_x = cx + radius * cosf(-0.5f * PI_F);
    float previous_y = cy + radius * sinf(-0.5f * PI_F);

    for (int i = 1; i <= segments; ++i) {
        float angle = -90.0f + (end_angle + 90.0f) * ((float) i / (float) segments);
        float radians = angle * (PI_F / 180.0f);
        float x = cx + radius * cosf(radians);
        float y = cy + radius * sinf(radians);
        SDL_RenderDrawLineF(renderer, previous_x, previous_y, x, y);
        previous_x = x;
        previous_y = y;
    }
}

static void respawn_snowflake(Snowflake *flake, bool initial_spawn) {
    flake->x = random_float(-80.0f, WINDOW_WIDTH + 80.0f);
    flake->y = initial_spawn ? random_float(-40.0f, WINDOW_HEIGHT + 40.0f) : random_float(-220.0f, -20.0f);
    flake->front_layer = (rand() % 100) < 36;

    if (flake->front_layer) {
        flake->radius = random_float(2.8f, 5.2f);
        flake->speed_y = random_float(150.0f, 245.0f);
        flake->drift = random_float(32.0f, 72.0f);
        flake->alpha = (Uint8) random_float(150.0f, 230.0f);
    } else {
        flake->radius = random_float(1.4f, 3.2f);
        flake->speed_y = random_float(72.0f, 155.0f);
        flake->drift = random_float(18.0f, 52.0f);
        flake->alpha = (Uint8) random_float(70.0f, 170.0f);
    }

    flake->phase = random_float(0.0f, PI_F * 2.0f);
}

static void init_snow(Game *game) {
    for (int i = 0; i < SNOWFLAKE_COUNT; ++i) {
        respawn_snowflake(&game->snowflakes[i], true);
    }
}

static void update_snow(Game *game, float delta_seconds) {
    Uint32 ticks = SDL_GetTicks();
    float wind = 36.0f + 18.0f * sinf((float) ticks * 0.0012f);

    for (int i = 0; i < SNOWFLAKE_COUNT; ++i) {
        Snowflake *flake = &game->snowflakes[i];
        flake->phase += delta_seconds * (flake->front_layer ? 2.1f : 1.2f);
        flake->x += (wind + sinf(flake->phase) * flake->drift) * delta_seconds;
        flake->y += flake->speed_y * delta_seconds;

        if (flake->y > WINDOW_HEIGHT + 30.0f || flake->x > WINDOW_WIDTH + 120.0f) {
            respawn_snowflake(flake, false);
            flake->x = random_float(-160.0f, WINDOW_WIDTH * 0.8f);
        }
    }
}

static void draw_timer(SDL_Renderer *renderer, float progress) {
    const float cx = TIMER_BOUNDS.x + TIMER_BOUNDS.w * 0.5f;
    const float cy = TIMER_BOUNDS.y + 64.0f;
    const float radius = 31.0f;

    SDL_Color track = {255, 255, 255, 42};
    SDL_Color fill = COLOR_ACCENT;

    if (progress < 0.5f) {
        fill = (SDL_Color){255, 177, 74, 255};
    }
    if (progress < 0.2f) {
        fill = COLOR_DANGER;
    }

    draw_filled_rect(renderer, (SDL_FRect){cx - 42.0f, cy - 42.0f, 84.0f, 84.0f}, (SDL_Color){10, 18, 28, 236});
    draw_outline_rect(renderer, (SDL_FRect){cx - 42.0f, cy - 42.0f, 84.0f, 84.0f}, (SDL_Color){255, 255, 255, 38});
    draw_circle_segments(renderer, cx, cy, radius, 1.0f, track, 64);
    for (int offset = -3; offset <= 3; ++offset) {
        draw_circle_segments(renderer, cx, cy, radius + (float) offset, progress, fill, 64);
    }

    SDL_FRect core = {cx - 17.0f, cy - 17.0f, 34.0f, 34.0f};
    draw_filled_rect(renderer, core, (SDL_Color){7, 12, 20, 255});
    draw_outline_rect(renderer, core, (SDL_Color){255, 255, 255, 24});
}

static bool create_panel_texture(Game *game) {
    if (game->overlay.panel_texture) {
        SDL_DestroyTexture(game->overlay.panel_texture);
        game->overlay.panel_texture = NULL;
    }

    const int width = 640;
    const int height = 420;
    SDL_Texture *target = SDL_CreateTexture(game->renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET, width, height);
    if (!target) {
        return false;
    }

    SDL_SetTextureBlendMode(target, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(game->renderer, target);
    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 11, 18, 28, 236);
    SDL_RenderClear(game->renderer);

    SDL_SetRenderDrawColor(game->renderer, 255, 255, 255, 34);
    SDL_Rect border = {0, 0, width, height};
    SDL_RenderDrawRect(game->renderer, &border);

    SDL_Color top_color = game->overlay.type == MESSAGE_SUCCESS ? COLOR_SUCCESS : COLOR_DANGER;
    bool success_message = game->overlay.type == MESSAGE_SUCCESS || game->overlay.type == MESSAGE_FINAL;
    SDL_SetRenderDrawColor(game->renderer, top_color.r, top_color.g, top_color.b, 255);
    SDL_Rect stripe = {0, 0, width, 10};
    SDL_RenderFillRect(game->renderer, &stripe);

    if (success_message) {
        draw_filled_rect(game->renderer, (SDL_FRect){26.0f, 26.0f, width - 52.0f, height - 52.0f}, (SDL_Color){125, 202, 165, 10});
        draw_filled_rect(game->renderer, (SDL_FRect){width - 126.0f, 28.0f, 72.0f, 72.0f}, (SDL_Color){125, 202, 165, 34});
        draw_text(game->renderer, game->font_medium, "+", COLOR_TEXT, width - 96.0f, 46.0f);
    }

    draw_filled_rect(game->renderer, (SDL_FRect){24.0f, 24.0f, width - 48.0f, 54.0f}, (SDL_Color){255, 255, 255, 10});
    draw_text(game->renderer, game->font_small, game->overlay.eyebrow, top_color, 34.0f, 34.0f);
    draw_text(game->renderer, game->font_medium, game->overlay.title, COLOR_TEXT, 34.0f, 92.0f);
    draw_filled_rect(game->renderer, (SDL_FRect){34.0f, 150.0f, width - 68.0f, 170.0f}, (SDL_Color){255, 255, 255, 6});
    draw_wrapped_text(game->renderer, game->font_small, game->overlay.text, COLOR_MUTED, 42.0f, 166.0f, 548);

    SDL_FRect button_rect = {34.0f, 350.0f, 218.0f, 42.0f};
    draw_filled_rect(game->renderer, button_rect, COLOR_ACCENT);
    draw_outline_rect(game->renderer, button_rect, (SDL_Color){255, 255, 255, 20});
    draw_text(game->renderer, game->font_small, game->overlay.button, COLOR_BG, 64.0f, 360.0f);

    SDL_SetRenderTarget(game->renderer, NULL);
    game->overlay.panel_texture = target;
    game->overlay.panel_width = width;
    game->overlay.panel_height = height;
    return true;
}

static void set_message(Game *game, MessageType type, const char *eyebrow,
                        const char *title, const char *text, const char *button, bool finished) {
    game->overlay.visible = true;
    game->overlay.type = type;
    game->overlay.start_ticks = SDL_GetTicks();
    game->overlay.game_finished = finished;

    SDL_strlcpy(game->overlay.eyebrow, eyebrow, sizeof(game->overlay.eyebrow));
    SDL_strlcpy(game->overlay.title, title, sizeof(game->overlay.title));
    SDL_strlcpy(game->overlay.text, text, sizeof(game->overlay.text));
    SDL_strlcpy(game->overlay.button, button, sizeof(game->overlay.button));
    create_panel_texture(game);
}

static void clear_message(Game *game) {
    game->overlay.visible = false;
    game->overlay.type = MESSAGE_NONE;
}

static void ensure_music_playing(Game *game) {
    if (!game->music) {
        return;
    }

    if (!Mix_PlayingMusic() && !Mix_PausedMusic()) {
        Mix_PlayMusic(game->music, -1);
    }
}

static void apply_audio_state(Game *game) {
    Mix_VolumeMusic(game->muted ? 0 : DEFAULT_MUSIC_VOLUME);

    if (game->success_sound) {
        Mix_VolumeChunk(game->success_sound, game->muted ? 0 : DEFAULT_CHUNK_VOLUME);
    }
    if (game->failure_sound) {
        Mix_VolumeChunk(game->failure_sound, game->muted ? 0 : DEFAULT_CHUNK_VOLUME);
    }
}

static void toggle_mute(Game *game) {
    game->muted = !game->muted;
    apply_audio_state(game);
}

static bool set_fullscreen(Game *game, bool enabled) {
    Uint32 flags = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(game->window, flags) != 0) {
        fprintf(stderr, "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
        return false;
    }

    game->fullscreen = enabled;
    return true;
}

static void toggle_fullscreen(Game *game) {
    set_fullscreen(game, !game->fullscreen);
}

static bool load_photos(Game *game) {
    DIR *directory = opendir(PHOTOS_DIR);
    if (!directory) {
        fprintf(stderr, "Cannot open %s\n", PHOTOS_DIR);
        return false;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.' || !has_image_extension(entry->d_name)) {
            continue;
        }
        if (game->photo_count >= MAX_PHOTOS) {
            break;
        }

        PhotoAsset *asset = &game->photos[game->photo_count];
        snprintf(asset->path, sizeof(asset->path), "%s/%s", PHOTOS_DIR, entry->d_name);
        get_filename_without_extension(entry->d_name, asset->name, sizeof(asset->name));

        asset->texture = IMG_LoadTexture(game->renderer, asset->path);
        if (!asset->texture) {
            fprintf(stderr, "Cannot load image %s: %s\n", asset->path, IMG_GetError());
            continue;
        }

        SDL_QueryTexture(asset->texture, NULL, NULL, &asset->width, &asset->height);
        game->photo_count += 1;
    }

    closedir(directory);
    return game->photo_count >= MIN_ROUNDS;
}

static bool load_background(Game *game) {
    game->background_texture = IMG_LoadTexture(game->renderer, BACKGROUND_PATH);
    if (!game->background_texture) {
        fprintf(stderr, "Cannot load background %s: %s\n", BACKGROUND_PATH, IMG_GetError());
        return false;
    }

    SDL_QueryTexture(game->background_texture, NULL, NULL, &game->background_width, &game->background_height);
    return true;
}

static bool load_music(Game *game) {
    DIR *directory = opendir(SONG_DIR);
    if (!directory) {
        return false;
    }

    struct dirent *entry = NULL;
    char path[768];
    bool loaded = false;

    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", SONG_DIR, entry->d_name);
        game->music = Mix_LoadMUS(path);
        if (game->music) {
            loaded = true;
            break;
        }
    }

    closedir(directory);
    return loaded;
}

static bool load_success_sound(Game *game) {
    game->success_sound = Mix_LoadWAV(SUCCESS_SOUND_PATH);
    if (!game->success_sound) {
        fprintf(stderr, "Cannot load success sound %s: %s\n", SUCCESS_SOUND_PATH, Mix_GetError());
        return false;
    }

    Mix_VolumeChunk(game->success_sound, DEFAULT_CHUNK_VOLUME);
    return true;
}

static bool load_failure_sound(Game *game) {
    game->failure_sound = Mix_LoadWAV(FAILURE_SOUND_PATH);
    if (!game->failure_sound) {
        fprintf(stderr, "Cannot load failure sound %s: %s\n", FAILURE_SOUND_PATH, Mix_GetError());
        return false;
    }

    Mix_VolumeChunk(game->failure_sound, DEFAULT_CHUNK_VOLUME);
    return true;
}

static void play_success_sound(Game *game) {
    if (!game->success_sound) {
        return;
    }

    Mix_PlayChannel(-1, game->success_sound, 0);
}

static void play_failure_sound(Game *game) {
    if (!game->failure_sound) {
        return;
    }

    Mix_PlayChannel(-1, game->failure_sound, 0);
}

static SDL_Rect source_from_norm(const PhotoAsset *asset, float norm_x, float norm_y, float norm_w, float norm_h) {
    SDL_Rect src;
    src.x = (int) (norm_x * asset->width);
    src.y = (int) (norm_y * asset->height);
    src.w = (int) (norm_w * asset->width);
    src.h = (int) (norm_h * asset->height);

    if (src.w < 1) {
        src.w = 1;
    }
    if (src.h < 1) {
        src.h = 1;
    }
    if (src.x + src.w > asset->width) {
        src.x = asset->width - src.w;
    }
    if (src.y + src.h > asset->height) {
        src.y = asset->height - src.h;
    }
    if (src.x < 0) {
        src.x = 0;
    }
    if (src.y < 0) {
        src.y = 0;
    }
    return src;
}

static void assign_option_positions(RoundData *round) {
    for (int i = 0; i < OPTION_COUNT; ++i) {
        round->options[i].card_rect.x = OPTIONS_PANEL.x + 28.0f;
        round->options[i].card_rect.y = 282.0f + i * 156.0f;
        round->options[i].card_rect.w = OPTIONS_PANEL.w - 56.0f;
        round->options[i].card_rect.h = 132.0f;

        round->options[i].preview_rect.x = round->options[i].card_rect.x + 12.0f;
        round->options[i].preview_rect.y = round->options[i].card_rect.y + 12.0f;
        round->options[i].preview_rect.w = round->options[i].card_rect.w - 24.0f;
        round->options[i].preview_rect.h = 84.0f;
    }
}

static void build_rounds(Game *game) {
    int photo_indices[MAX_PHOTOS];
    for (int i = 0; i < game->photo_count; ++i) {
        photo_indices[i] = i;
    }
    shuffle_ints(photo_indices, game->photo_count);

    game->round_count = game->photo_count < MAX_ROUNDS ? game->photo_count : MAX_ROUNDS;
    for (int round_index = 0; round_index < game->round_count; ++round_index) {
        RoundData *round = &game->rounds[round_index];
        PhotoAsset *photo = &game->photos[photo_indices[round_index]];
        round->puzzle_photo_index = photo_indices[round_index];
        round->image_rect = fit_contain_rect(photo->width, photo->height, STAGE_IMAGE_RECT);

        round->piece_rect.w = clamp_float(round->image_rect.w * random_float(0.18f, 0.24f), 140.0f, 220.0f);
        round->piece_rect.h = clamp_float(round->image_rect.h * random_float(0.20f, 0.28f), 100.0f, 180.0f);
        round->piece_rect.x = round->image_rect.x + random_float(34.0f, round->image_rect.w - round->piece_rect.w - 34.0f);
        round->piece_rect.y = round->image_rect.y + random_float(28.0f, round->image_rect.h - round->piece_rect.h - 28.0f);

        round->norm_x = (round->piece_rect.x - round->image_rect.x) / round->image_rect.w;
        round->norm_y = (round->piece_rect.y - round->image_rect.y) / round->image_rect.h;
        round->norm_w = round->piece_rect.w / round->image_rect.w;
        round->norm_h = round->piece_rect.h / round->image_rect.h;

        int candidates[MAX_PHOTOS];
        int candidate_count = 0;
        for (int i = 0; i < game->photo_count; ++i) {
            if (i != round->puzzle_photo_index) {
                candidates[candidate_count++] = i;
            }
        }
        shuffle_ints(candidates, candidate_count);

        round->options[0].photo_index = round->puzzle_photo_index;
        round->options[0].correct = true;
        round->options[1].photo_index = candidates[0];
        round->options[1].correct = false;
        round->options[2].photo_index = candidates[1];
        round->options[2].correct = false;

        for (int i = 0; i < OPTION_COUNT; ++i) {
            PhotoAsset *asset = &game->photos[round->options[i].photo_index];
            round->options[i].src_rect = source_from_norm(asset, round->norm_x, round->norm_y, round->norm_w, round->norm_h);
        }

        int order[OPTION_COUNT] = {0, 1, 2};
        shuffle_ints(order, OPTION_COUNT);
        PieceOption shuffled[OPTION_COUNT];
        for (int i = 0; i < OPTION_COUNT; ++i) {
            shuffled[i] = round->options[order[i]];
        }
        for (int i = 0; i < OPTION_COUNT; ++i) {
            round->options[i] = shuffled[i];
        }
        assign_option_positions(round);
    }
}

static void start_round(Game *game, int round_index) {
    game->current_round = round_index;
    game->round_start_ticks = SDL_GetTicks();
    game->round_locked = false;
    game->show_completed_piece = false;
    game->hover_target = false;
    game->drag.active = false;
    clear_message(game);
}

static void restart_game(Game *game) {
    game->score = 0;
    build_rounds(game);
    start_round(game, 0);
}

static void finish_round(Game *game, bool success, bool timeout) {
    game->round_locked = true;
    game->drag.active = false;
    game->hover_target = false;
    game->show_completed_piece = success;

    if (success) {
        game->score += 1;
        play_success_sound(game);
    } else {
        play_failure_sound(game);
    }

    bool last_round = game->current_round == game->round_count - 1;
    if (last_round) {
        char summary[160];
        snprintf(summary, sizeof(summary), "You finished %d rounds with %d correct placements.", game->round_count, game->score);
        set_message(game, success ? MESSAGE_FINAL : MESSAGE_FAILURE,
                    success ? "SUCCESS" : "FAILURE",
                    success ? "Dossier solved" : "Dossier closed",
                    success ? summary : "The last round ended in failure. Click to restart the full dossier.",
                    "Play again", true);
        return;
    }

    if (success) {
        set_message(game, MESSAGE_SUCCESS, "SUCCESS", "Piece accepted",
                    "The fragment fits the missing slot. Click to move to the next randomized puzzle.",
                    "Next round", false);
    } else if (timeout) {
        set_message(game, MESSAGE_FAILURE, "FAILURE", "Time expired",
                    "The animated timer emptied before the correct piece reached the target.", "Next round", false);
    } else {
        set_message(game, MESSAGE_FAILURE, "FAILURE", "Wrong piece",
                    "That proposal does not belong to the highlighted area.", "Next round", false);
    }
}

static void advance_after_message(Game *game) {
    if (game->overlay.game_finished) {
        restart_game(game);
        return;
    }
    start_round(game, game->current_round + 1);
}

static void draw_background(Game *game) {
    SDL_SetRenderDrawColor(game->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(game->renderer);

    SDL_FRect viewport = {0.0f, 0.0f, (float) WINDOW_WIDTH, (float) WINDOW_HEIGHT};
    if (game->background_texture && game->background_width > 0 && game->background_height > 0) {
        SDL_FRect dst = fit_cover_rect(game->background_width, game->background_height, viewport);
        SDL_RenderCopyF(game->renderer, game->background_texture, NULL, &dst);
        draw_filled_rect(game->renderer, viewport, (SDL_Color){4, 7, 12, 158});
    }

    draw_filled_rect(game->renderer, (SDL_FRect){0.0f, 0.0f, (float) WINDOW_WIDTH, 260.0f}, (SDL_Color){31, 57, 84, 68});
    draw_filled_rect(game->renderer, (SDL_FRect){0.0f, 560.0f, (float) WINDOW_WIDTH, 260.0f}, (SDL_Color){7, 11, 18, 118});
    draw_filled_rect(game->renderer, (SDL_FRect){-120.0f, -80.0f, 620.0f, 260.0f}, (SDL_Color){239, 191, 102, 20});
    draw_filled_rect(game->renderer, (SDL_FRect){920.0f, 40.0f, 420.0f, 220.0f}, (SDL_Color){124, 166, 218, 18});
}

static void draw_snow_layer(Game *game, bool front_layer) {
    for (int i = 0; i < SNOWFLAKE_COUNT; ++i) {
        Snowflake *flake = &game->snowflakes[i];
        if (flake->front_layer != front_layer) {
            continue;
        }

        Uint8 alpha = front_layer ? (Uint8) (flake->alpha * 0.45f) : (Uint8) (flake->alpha * 0.25f);
        SDL_Color color = {232, 240, 248, alpha};
        draw_snowflake(game->renderer, flake->x, flake->y, flake->radius, color);
    }
}

static void draw_chrome(Game *game) {
    draw_panel_shell(game->renderer, HEADER_BAR, (SDL_Color){11, 18, 28, 198}, (SDL_Color){255, 255, 255, 26});
    draw_panel_shell(game->renderer, STAGE_PANEL, COLOR_PANEL, (SDL_Color){255, 255, 255, 24});
    draw_panel_shell(game->renderer, OPTIONS_PANEL, COLOR_PANEL_ALT, (SDL_Color){255, 255, 255, 24});

    draw_text(game->renderer, game->font_medium, "PUZZLE ROOM", COLOR_ACCENT, 52.0f, 34.0f);
    draw_text(game->renderer, game->font_medium, "Find the missing fragment.", COLOR_TEXT, 52.0f, 78.0f);
    draw_text(game->renderer, game->font_small, "Pick one piece and drop it into the empty spot.", COLOR_MUTED, 54.0f, 110.0f);

    char round_label[32];
    snprintf(round_label, sizeof(round_label), "%d / %d", game->current_round + 1, game->round_count);
    char score_label[32];
    snprintf(score_label, sizeof(score_label), "%d", game->score);

    draw_stat_chip_centered_value(game, (SDL_FRect){938.0f, 40.0f, 112.0f, 74.0f}, "ROUND", round_label,
                                  (SDL_Color){17, 30, 47, 206}, COLOR_TEXT, -6.0f);
    draw_stat_chip_score(game, (SDL_FRect){1062.0f, 40.0f, 104.0f, 74.0f}, "SCORE", score_label,
                         (SDL_Color){15, 34, 31, 206}, COLOR_SUCCESS, -10.0f);
    draw_stat_chip_mode(game, (SDL_FRect){1174.0f, 40.0f, 166.0f, 74.0f}, "MODE", game->fullscreen ? "Full screen" : "Windowed",
                        (SDL_Color){37, 28, 22, 206}, COLOR_ACCENT, 0.0f);

    draw_text(game->renderer, game->font_medium, "MAIN IMAGE", COLOR_ACCENT, 56.0f, 166.0f);
    draw_text(game->renderer, game->font_medium, "Place the right fragment in the empty spot.", COLOR_TEXT, 58.0f, 200.0f);

    draw_text(game->renderer, game->font_medium, "OPTIONS", COLOR_ACCENT, 944.0f, 180.0f);
    draw_text(game->renderer, game->font_small, "Pick one piece.", COLOR_TEXT, 944.0f, 216.0f);
    draw_text(game->renderer, game->font_small, "Drag it to the slot.", COLOR_MUTED, 944.0f, 242.0f);
}

static void draw_stage(Game *game) {
    RoundData *round = &game->rounds[game->current_round];
    PhotoAsset *photo = &game->photos[round->puzzle_photo_index];

    draw_panel_shell(game->renderer,
                     (SDL_FRect){STAGE_IMAGE_RECT.x - 12.0f, STAGE_IMAGE_RECT.y - 12.0f, STAGE_IMAGE_RECT.w + 24.0f, STAGE_IMAGE_RECT.h + 24.0f},
                     (SDL_Color){8, 14, 22, 214}, (SDL_Color){255, 255, 255, 16});
    draw_filled_rect(game->renderer, STAGE_IMAGE_RECT, (SDL_Color){7, 10, 15, 255});
    SDL_RenderCopyF(game->renderer, photo->texture, NULL, &round->image_rect);

    if (game->show_completed_piece) {
        SDL_Rect source = source_from_norm(photo, round->norm_x, round->norm_y, round->norm_w, round->norm_h);
        SDL_RenderCopyF(game->renderer, photo->texture, &source, &round->piece_rect);
        draw_outline_rect(game->renderer, round->piece_rect, (SDL_Color){125, 202, 165, 220});
    } else {
        draw_filled_rect(game->renderer, round->piece_rect, (SDL_Color){6, 10, 16, 212});
        draw_filled_rect(game->renderer,
                         (SDL_FRect){round->piece_rect.x + 6.0f, round->piece_rect.y + 6.0f, round->piece_rect.w - 12.0f, 18.0f},
                         (SDL_Color){255, 255, 255, 12});

        SDL_Color border = game->hover_target ? COLOR_ACCENT : (SDL_Color){255, 255, 255, 160};
        draw_outline_rect(game->renderer, round->piece_rect, border);
        draw_text_centered(game->renderer, game->font_tiny, "DROP HERE",
                           game->hover_target ? COLOR_ACCENT : COLOR_MUTED,
                           round->piece_rect, round->piece_rect.y + round->piece_rect.h * 0.5f - 8.0f);
    }
}

static void draw_options(Game *game) {
    RoundData *round = &game->rounds[game->current_round];
    for (int i = 0; i < OPTION_COUNT; ++i) {
        PieceOption *option = &round->options[i];
        SDL_Color card_color = COLOR_PANEL_SOFT;
        if (game->drag.active && game->drag.option_index == i) {
            card_color = (SDL_Color){71, 108, 149, 96};
        }
        draw_panel_shell(game->renderer, option->card_rect, card_color, (SDL_Color){255, 255, 255, 22});

        SDL_FRect preview = option->preview_rect;
        draw_filled_rect(game->renderer, preview, (SDL_Color){7, 10, 16, 255});
        SDL_RenderCopyF(game->renderer, game->photos[option->photo_index].texture, &option->src_rect, &preview);
        draw_outline_rect(game->renderer, preview, (SDL_Color){255, 255, 255, 14});

        char label[48];
        snprintf(label, sizeof(label), "Fragment %d  |  Drag piece", i + 1);
        draw_text_centered(game->renderer, game->font_tiny, label, COLOR_ACCENT, option->card_rect, option->card_rect.y + 106.0f);
    }
}

static void draw_drag_piece(Game *game) {
    if (!game->drag.active) {
        return;
    }

    RoundData *round = &game->rounds[game->current_round];
    PieceOption *option = &round->options[game->drag.option_index];
    SDL_FRect dst = {
        game->drag.mouse_x - game->drag.offset_x,
        game->drag.mouse_y - game->drag.offset_y,
        option->preview_rect.w,
        option->preview_rect.h
    };
    draw_shadow_rect(game->renderer, dst, 10.0f, (SDL_Color){4, 6, 10, 120});
    SDL_RenderCopyF(game->renderer, game->photos[option->photo_index].texture, &option->src_rect, &dst);
    draw_outline_rect(game->renderer, dst, (SDL_Color){255, 255, 255, 110});
}

static void draw_message_overlay(Game *game) {
    if (!game->overlay.visible || !game->overlay.panel_texture) {
        return;
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(game->renderer, 8, 8, 9, 170);
    SDL_Rect full = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(game->renderer, &full);

    Uint32 elapsed = SDL_GetTicks() - game->overlay.start_ticks;
    float t = clamp_float((float) elapsed / (float) OVERLAY_ANIM_MS, 0.0f, 1.0f);
    bool success_message = game->overlay.type == MESSAGE_SUCCESS || game->overlay.type == MESSAGE_FINAL;
    float scale = success_message ? (0.64f + 0.42f * t) : (0.78f + 0.26f * t);
    if (t > 0.65f) {
        scale = success_message ? (1.12f - (t - 0.65f) * 0.22f) : (1.04f - (t - 0.65f) * 0.08f);
    }
    float float_offset = 0.0f;
    if (success_message) {
        float pulse = sinf((float) elapsed * 0.012f);
        float_offset = -8.0f * sinf((float) elapsed * 0.0065f);

        SDL_FRect glow_outer = {
            WINDOW_WIDTH * 0.5f - game->overlay.panel_width * 0.62f,
            WINDOW_HEIGHT * 0.5f - game->overlay.panel_height * 0.62f + float_offset,
            game->overlay.panel_width * 1.24f,
            game->overlay.panel_height * 1.24f
        };
        SDL_FRect glow_inner = {
            WINDOW_WIDTH * 0.5f - game->overlay.panel_width * 0.55f,
            WINDOW_HEIGHT * 0.5f - game->overlay.panel_height * 0.55f + float_offset,
            game->overlay.panel_width * 1.10f,
            game->overlay.panel_height * 1.10f
        };
        draw_filled_rect(game->renderer, glow_outer, (SDL_Color){125, 202, 165, (Uint8) (22 + 10 * (pulse + 1.0f))});
        draw_filled_rect(game->renderer, glow_inner, (SDL_Color){239, 191, 102, (Uint8) (16 + 8 * (pulse + 1.0f))});
    }

    SDL_FRect dst = {
        WINDOW_WIDTH * 0.5f - game->overlay.panel_width * scale * 0.5f,
        WINDOW_HEIGHT * 0.5f - game->overlay.panel_height * scale * 0.5f + float_offset,
        game->overlay.panel_width * scale,
        game->overlay.panel_height * scale
    };
    double angle = success_message ? (3.0 * sin((double) elapsed * 0.008)) : 0.0;
    SDL_RenderCopyExF(game->renderer, game->overlay.panel_texture, NULL, &dst, angle, NULL, SDL_FLIP_NONE);
}

static void render_game(Game *game) {
    draw_background(game);
    draw_snow_layer(game, false);
    draw_chrome(game);
    draw_stage(game);
    draw_options(game);

    Uint32 elapsed = SDL_GetTicks() - game->round_start_ticks;
    float progress = 1.0f - (float) elapsed / (float) ROUND_TIME_MS;
    progress = clamp_float(progress, 0.0f, 1.0f);
    draw_panel_shell(game->renderer, (SDL_FRect){TIMER_BOUNDS.x + 8.0f, TIMER_BOUNDS.y - 2.0f, TIMER_BOUNDS.w - 16.0f, 28.0f},
                     (SDL_Color){24, 38, 56, 224}, (SDL_Color){255, 255, 255, 24});
    draw_timer(game->renderer, progress);
    draw_text_centered(game->renderer, game->font_small, "TIME", COLOR_ACCENT, TIMER_BOUNDS, TIMER_BOUNDS.y + 2.0f);

    draw_drag_piece(game);
    draw_snow_layer(game, true);
    draw_message_overlay(game);
    SDL_RenderPresent(game->renderer);
}

static void update_game(Game *game) {
    ensure_music_playing(game);

    Uint32 now = SDL_GetTicks();
    float delta_seconds = (float) (now - game->last_frame_ticks) / 1000.0f;
    if (delta_seconds < 0.0f) {
        delta_seconds = 0.0f;
    }
    if (delta_seconds > 0.05f) {
        delta_seconds = 0.05f;
    }
    game->last_frame_ticks = now;
    update_snow(game, delta_seconds);

    if (game->round_locked || game->overlay.visible) {
        return;
    }

    Uint32 elapsed = SDL_GetTicks() - game->round_start_ticks;
    if (elapsed >= ROUND_TIME_MS) {
        finish_round(game, false, true);
    }
}

static void handle_mouse_down(Game *game, float mouse_x, float mouse_y) {
    if (game->overlay.visible) {
        advance_after_message(game);
        return;
    }
    if (game->round_locked) {
        return;
    }

    RoundData *round = &game->rounds[game->current_round];
    for (int i = 0; i < OPTION_COUNT; ++i) {
        PieceOption *option = &round->options[i];
        if (point_in_frect(mouse_x, mouse_y, option->preview_rect)) {
            game->drag.active = true;
            game->drag.option_index = i;
            game->drag.offset_x = mouse_x - option->preview_rect.x;
            game->drag.offset_y = mouse_y - option->preview_rect.y;
            game->drag.mouse_x = mouse_x;
            game->drag.mouse_y = mouse_y;
            break;
        }
    }
}

static void handle_mouse_up(Game *game, float mouse_x, float mouse_y) {
    if (!game->drag.active) {
        return;
    }

    RoundData *round = &game->rounds[game->current_round];
    PieceOption *option = &round->options[game->drag.option_index];
    bool inside_target = point_in_frect(mouse_x, mouse_y, round->piece_rect);

    game->drag.active = false;
    game->hover_target = false;

    if (inside_target) {
        finish_round(game, option->correct, false);
    }
}

static void handle_mouse_motion(Game *game, float mouse_x, float mouse_y) {
    game->drag.mouse_x = mouse_x;
    game->drag.mouse_y = mouse_y;

    if (!game->drag.active) {
        game->hover_target = false;
        return;
    }

    RoundData *round = &game->rounds[game->current_round];
    game->hover_target = point_in_frect(mouse_x, mouse_y, round->piece_rect);
}

static void handle_key(Game *game, SDL_Keycode key) {
    if (key == SDLK_F11 || key == SDLK_f) {
        toggle_fullscreen(game);
        return;
    }

    if (key == SDLK_n) {
        toggle_mute(game);
        return;
    }

    if (key == SDLK_ESCAPE) {
        if (game->fullscreen) {
            set_fullscreen(game, false);
        } else {
            game->running = false;
        }
        return;
    }

    if (game->overlay.visible && (key == SDLK_SPACE || key == SDLK_RETURN || key == SDLK_KP_ENTER)) {
        advance_after_message(game);
    }
}

static bool init_fonts(Game *game) {
    game->font_tiny = TTF_OpenFont(FONT_PATH, 16);
    game->font_small = TTF_OpenFont(FONT_PATH, 20);
    game->font_medium = TTF_OpenFont(FONT_PATH, 26);
    game->font_large = TTF_OpenFont(FONT_PATH, 44);
    return game->font_tiny && game->font_small && game->font_medium && game->font_large;
}

int game_init(Game *game) {
    memset(game, 0, sizeof(*game));
    srand((unsigned int) time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    if ((IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP) & (IMG_INIT_PNG | IMG_INIT_WEBP)) == 0) {
        fprintf(stderr, "IMG_Init failed: %s\n", IMG_GetError());
        return 0;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 0;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        return 0;
    }

    game->window = SDL_CreateWindow("Puzzle Dossier SDL2",
                                    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!game->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!game->renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_SetRenderDrawBlendMode(game->renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(game->renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_RenderSetIntegerScale(game->renderer, SDL_FALSE);

    if (!load_background(game)) {
        return 0;
    }

    if (!init_fonts(game)) {
        fprintf(stderr, "Unable to load font: %s\n", FONT_PATH);
        return 0;
    }

    if (!load_photos(game)) {
        fprintf(stderr, "At least %d images are required inside %s\n", MIN_ROUNDS, PHOTOS_DIR);
        return 0;
    }

    if (load_music(game)) {
        Mix_VolumeMusic(DEFAULT_MUSIC_VOLUME);
        Mix_PlayMusic(game->music, -1);
    }

    load_success_sound(game);
    load_failure_sound(game);
    apply_audio_state(game);

    init_snow(game);
    restart_game(game);
    game->last_frame_ticks = SDL_GetTicks();
    game->running = true;
    return 1;
}

void game_run(Game *game) {
    SDL_Event event;
    while (game->running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    game->running = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_mouse_down(game, (float) event.button.x, (float) event.button.y);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_mouse_up(game, (float) event.button.x, (float) event.button.y);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    handle_mouse_motion(game, (float) event.motion.x, (float) event.motion.y);
                    break;
                case SDL_KEYDOWN:
                    handle_key(game, event.key.keysym.sym);
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
                        event.window.event == SDL_WINDOWEVENT_RESTORED) {
                        ensure_music_playing(game);
                    }
                    break;
                default:
                    break;
            }
        }

        update_game(game);
        render_game(game);
    }
}

void game_cleanup(Game *game) {
    if (game->music) {
        Mix_HaltMusic();
        Mix_FreeMusic(game->music);
        game->music = NULL;
    }

    if (game->success_sound) {
        Mix_FreeChunk(game->success_sound);
        game->success_sound = NULL;
    }

    if (game->failure_sound) {
        Mix_FreeChunk(game->failure_sound);
        game->failure_sound = NULL;
    }

    if (game->overlay.panel_texture) {
        SDL_DestroyTexture(game->overlay.panel_texture);
        game->overlay.panel_texture = NULL;
    }

    if (game->background_texture) {
        SDL_DestroyTexture(game->background_texture);
        game->background_texture = NULL;
    }

    for (int i = 0; i < game->photo_count; ++i) {
        if (game->photos[i].texture) {
            SDL_DestroyTexture(game->photos[i].texture);
            game->photos[i].texture = NULL;
        }
    }

    if (game->font_tiny) {
        TTF_CloseFont(game->font_tiny);
    }
    if (game->font_small) {
        TTF_CloseFont(game->font_small);
    }
    if (game->font_medium) {
        TTF_CloseFont(game->font_medium);
    }
    if (game->font_large) {
        TTF_CloseFont(game->font_large);
    }
    if (game->renderer) {
        SDL_DestroyRenderer(game->renderer);
    }
    if (game->window) {
        SDL_DestroyWindow(game->window);
    }

    TTF_Quit();
    Mix_CloseAudio();
    IMG_Quit();
    SDL_Quit();
}
