#include "enigma_scene.h"
#include "mainmenu_headers.h"
#include "asset_paths.h"
#include "ui_shared.h"

#define ENIGMA_BUTTON_COUNT 4
#define ENIGMA_CHARACTER_COUNT 4
#define ENIGMA_RANDOM_CHARACTER_COUNT 3
#define ENIGMA_FRAME_COLS 6
#define ENIGMA_FRAME_ROWS 6
#define ENIGMA_TOTAL_FRAMES 36

typedef enum {
    ENIGMA_STATE_FADE_IN = 0,
    ENIGMA_STATE_DIALOGUE,
    ENIGMA_STATE_CHOICES,
    ENIGMA_STATE_RESULT_ANIM,
    ENIGMA_STATE_FADE_OUT,
    ENIGMA_STATE_EXIT
} EnigmaState;

typedef struct {
    SDL_Texture* texture;
    SDL_Rect src_rect;
    SDL_Rect dst_rect;
    int frame_w;
    int frame_h;
    int current_frame;
    int frame_delay_ms;
    Uint32 last_update;
    int loop;
} EnigmaSprite;

typedef struct {
    SDL_Rect rect;
    char text[64];
    int visible;
    Button visual;
} EnigmaButton;

typedef struct {
    const char* background_path;
    const char* idle_path;
    const char* happy_path;
    const char* sad_path;
} EnigmaCharacterAssets;

typedef struct {
    const char* intro_text;
    const char* question;
    const char* answers[ENIGMA_BUTTON_COUNT];
    int correct_answer_index;
} EnigmaPrompt;

static SDL_Renderer* g_renderer = NULL;
static TTF_Font* g_font = NULL;
static Mix_Music* g_music = NULL;
static Mix_Music* g_happy4_music = NULL;

static SDL_Texture* g_background = NULL;
static EnigmaSprite g_character = {0};
static EnigmaButton g_buttons[ENIGMA_BUTTON_COUNT];
static char g_happy_path[256] = {0};
static char g_sad_path[256] = {0};
static char g_question[256] = {0};
static char g_dialogue_text[256] = {0};

static const EnigmaCharacterAssets g_character_assets[ENIGMA_CHARACTER_COUNT] = {
    {ASSET_ENIGMA_BG_1, ASSET_ENIGMA_IDLE_1, ASSET_ENIGMA_HAPPY_1, ASSET_ENIGMA_SAD_1},
    {ASSET_ENIGMA_BG_2, ASSET_ENIGMA_IDLE_2, ASSET_ENIGMA_HAPPY_2, ASSET_ENIGMA_SAD_2},
    {ASSET_ENIGMA_BG_3, ASSET_ENIGMA_IDLE_3, ASSET_ENIGMA_HAPPY_3, ASSET_ENIGMA_SAD_3},
    {ASSET_ENIGMA_BG_4, ASSET_ENIGMA_IDLE_4, ASSET_ENIGMA_HAPPY_4, ASSET_ENIGMA_SAD_4}
};

static const EnigmaPrompt g_enigma_prompts[ENIGMA_CHARACTER_COUNT] = {
    {
        "Snow or no snow, you only pass if you think clearly.",
        "Which season comes right after autumn?",
        {"Winter", "Spring", "Summer", "Monsoon"},
        0
    },
    {
        "The road is slippery today. Give me one clean answer.",
        "Which number is even?",
        {"9", "11", "14", "17"},
        2
    },
    {
        "Almost there. Prove you can solve something simple under pressure.",
        "What is 7 + 5?",
        {"10", "11", "12", "13"},
        2
    },
    {
        "Last checkpoint. Stay calm and answer without guessing.",
        "Which animal is known for barking?",
        {"Cat", "Dog", "Fish", "Crow"},
        1
    }
};

static EnigmaState g_state = ENIGMA_STATE_FADE_IN;
static int g_fade_alpha = 255;
static int g_correct_answer = 1;
static int g_result_finished = 0;
static int g_return_to_main = 0;
static int g_initialized = 0;
static int g_active = 0;
static int g_music_volume_sdl = MIX_MAX_VOLUME;
static int g_hovered_button = -1;
static int g_current_character_id = 0;

static const int g_random_character_ids[ENIGMA_RANDOM_CHARACTER_COUNT] = {1, 3, 4};

static const EnigmaCharacterAssets* enigma_assets_for_character(int character_id)
{
    if (character_id < 1 || character_id > ENIGMA_CHARACTER_COUNT) return NULL;
    return &g_character_assets[character_id - 1];
}

static const EnigmaPrompt* enigma_prompt_for_character(int character_id)
{
    if (character_id < 1 || character_id > ENIGMA_CHARACTER_COUNT) return NULL;
    return &g_enigma_prompts[character_id - 1];
}

static void destroy_texture(SDL_Texture** tex)
{
    if (tex && *tex) {
        SDL_DestroyTexture(*tex);
        *tex = NULL;
    }
}

static void sprite_destroy(EnigmaSprite* s)
{
    if (!s) return;
    destroy_texture(&s->texture);
    s->src_rect = (SDL_Rect){0, 0, 0, 0};
    s->dst_rect = (SDL_Rect){0, 0, 0, 0};
    s->frame_w = 0;
    s->frame_h = 0;
    s->current_frame = 0;
    s->frame_delay_ms = 0;
    s->last_update = 0;
    s->loop = 0;
}

static int sprite_init(EnigmaSprite* s,
                       const char* absolute_path,
                       int x, int y, int w, int h,
                       int frame_delay_ms, int loop)
{
    if (!s || !g_renderer || !absolute_path) return 0;

    sprite_destroy(s);

    SDL_Texture* tex = IMG_LoadTexture(g_renderer, absolute_path);
    if (!tex) {
        SDL_Log("Enigma sprite load failed (%s): %s", absolute_path, IMG_GetError());
        return 0;
    }

    int tw = 0;
    int th = 0;
    if (SDL_QueryTexture(tex, NULL, NULL, &tw, &th) != 0 || tw <= 0 || th <= 0) {
        SDL_Log("Enigma sprite query failed (%s): %s", absolute_path, SDL_GetError());
        SDL_DestroyTexture(tex);
        return 0;
    }

    s->texture = tex;
    s->frame_w = tw / ENIGMA_FRAME_COLS;
    s->frame_h = th / ENIGMA_FRAME_ROWS;
    if (s->frame_w <= 0 || s->frame_h <= 0) {
        SDL_Log("Enigma sprite sheet invalid (%s)", absolute_path);
        sprite_destroy(s);
        return 0;
    }

    s->src_rect = (SDL_Rect){0, 0, s->frame_w, s->frame_h};
    s->dst_rect = (SDL_Rect){x, y, w, h};
    s->current_frame = 0;
    s->frame_delay_ms = frame_delay_ms;
    s->last_update = SDL_GetTicks();
    s->loop = loop ? 1 : 0;
    return 1;
}

static void sprite_update(EnigmaSprite* s)
{
    if (!s || !s->texture || s->frame_delay_ms <= 0) return;

    Uint32 now = SDL_GetTicks();
    if (now - s->last_update < (Uint32)s->frame_delay_ms) return;

    s->current_frame++;
    if (s->current_frame >= ENIGMA_TOTAL_FRAMES) {
        s->current_frame = s->loop ? 0 : ENIGMA_TOTAL_FRAMES - 1;
    }

    int row = s->current_frame / ENIGMA_FRAME_COLS;
    int col = s->current_frame % ENIGMA_FRAME_COLS;
    s->src_rect.x = col * s->frame_w;
    s->src_rect.y = row * s->frame_h;
    s->last_update = now;
}

static void sprite_render(const EnigmaSprite* s)
{
    if (!s || !s->texture || !g_renderer) return;
    SDL_RenderCopy(g_renderer, s->texture, &s->src_rect, &s->dst_rect);
}

static void buttons_destroy(void)
{
    for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
        g_buttons[i].rect = (SDL_Rect){0, 0, 0, 0};
        g_buttons[i].text[0] = '\0';
        g_buttons[i].visible = 0;
        g_buttons[i].visual = (Button){0};
    }
}

static int button_init(EnigmaButton* b, const char* text, int x, int y, int w, int h)
{
    if (!b || !text) return 0;

    b->rect = (SDL_Rect){x, y, w, h};
    snprintf(b->text, sizeof(b->text), "%s", text);
    b->visible = 1;
    prototype_button_init(&b->visual, x, y, w, h, text);
    return 1;
}

static int setup_buttons(const EnigmaPrompt* prompt)
{
    if (!prompt) return 0;

    buttons_destroy();
    g_hovered_button = -1;

    if (!button_init(&g_buttons[0], prompt->answers[0], 84, 220, 300, 72)) return 0;
    if (!button_init(&g_buttons[1], prompt->answers[1], 84, 314, 300, 72)) return 0;
    if (!button_init(&g_buttons[2], prompt->answers[2], 896, 220, 300, 72)) return 0;
    if (!button_init(&g_buttons[3], prompt->answers[3], 896, 314, 300, 72)) return 0;

    return 1;
}

static void clear_runtime_assets(void)
{
    destroy_texture(&g_background);
    sprite_destroy(&g_character);
    buttons_destroy();
    g_happy_path[0] = '\0';
    g_sad_path[0] = '\0';
    g_question[0] = '\0';
    g_dialogue_text[0] = '\0';
    g_current_character_id = 0;
}

static int load_character_pack(int character_id)
{
    const EnigmaCharacterAssets* assets = enigma_assets_for_character(character_id);
    const EnigmaPrompt* prompt = enigma_prompt_for_character(character_id);

    if (!assets || !prompt) return 0;

    g_background = IMG_LoadTexture(g_renderer, assets->background_path);
    if (!g_background) {
        SDL_Log("Enigma background load failed (%s): %s", assets->background_path, IMG_GetError());
        return 0;
    }

    if (!sprite_init(&g_character, assets->idle_path, 530, 570, 150, 150, 80, 1)) {
        clear_runtime_assets();
        return 0;
    }

    snprintf(g_happy_path, sizeof(g_happy_path), "%s", assets->happy_path);
    snprintf(g_sad_path, sizeof(g_sad_path), "%s", assets->sad_path);
    snprintf(g_question, sizeof(g_question), "%s", prompt->question);
    snprintf(g_dialogue_text, sizeof(g_dialogue_text), "%s", prompt->intro_text);
    g_correct_answer = prompt->correct_answer_index;

    if (!setup_buttons(prompt)) {
        clear_runtime_assets();
        return 0;
    }

    return 1;
}

static void render_text_centered(const char* text, int y)
{
    if (!g_renderer || !g_font || !text) return;

    SDL_Color white = {255, 255, 255, 255};
    ui_draw_text_center(g_renderer, g_font, text, WINDOW_W / 2, y, white);
}

static void render_question_text(const char* question_text)
{
    TTF_Font* render_font = NULL;

    if (!g_renderer || !g_font || !question_text) return;

    render_font = ui_font_for_text(g_font, question_text);
    if (!render_font) render_font = g_font;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(render_font, question_text, white, WINDOW_W - 100);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(g_renderer, surf);
    if (tex) {
        SDL_Rect dst = {(WINDOW_W - surf->w) / 2, 20, surf->w, surf->h};
        SDL_RenderCopy(g_renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

static void render_button(const EnigmaButton* b)
{
    Button temp = {0};

    if (!b || !b->visible || !g_renderer) return;
    if (!g_font) return;

    temp = b->visual;
    snprintf(temp.label, sizeof(temp.label), "%s", b->text);
    prototype_button_render(&temp, g_renderer, g_font);
}

static int button_contains_point(const EnigmaButton* b, int x, int y)
{
    SDL_Rect dest = {0, 0, 0, 0};

    if (!b || !b->visible) return 0;

    dest = get_button_dest(&b->visual);
    return (x >= dest.x && x <= dest.x + dest.w &&
            y >= dest.y && y <= dest.y + dest.h);
}

static void render_fade_overlay(int alpha)
{
    if (!g_renderer || alpha <= 0) return;

    if (alpha > 255) alpha = 255;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, (Uint8)alpha);
    SDL_Rect r = {0, 0, WINDOW_W, WINDOW_H};
    SDL_RenderFillRect(g_renderer, &r);
}

static void show_result_for_answer(int selected_answer)
{
    int is_correct = (selected_answer == g_correct_answer);

    for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
        g_buttons[i].visible = 0;
        g_buttons[i].visual.selected = 0;
    }
    g_hovered_button = -1;

    if (is_correct && g_current_character_id == 4 && g_happy4_music) {
        Mix_HaltMusic();
        Mix_VolumeMusic(g_music_volume_sdl);
        Mix_PlayMusic(g_happy4_music, 0);
    }

    const char* result_path = is_correct ? g_happy_path : g_sad_path;
    if (!sprite_init(&g_character, result_path, 530, 570, 150, 150, 80, 0)) {
        g_result_finished = 1;
    } else {
        g_result_finished = 0;
    }

    g_state = ENIGMA_STATE_RESULT_ANIM;
}

int enigma_scene_init(SDL_Window* shared_window, SDL_Renderer* shared_renderer)
{
    if (g_initialized) return 1;
    if (!shared_renderer) return 0;

    (void)shared_window;
    g_renderer = shared_renderer;

    g_font = ui_open_arial_font(24, 0);
    if (!g_font) {
        char button_font_path[512];
        ui_resolve_asset_path(ASSET_BUTTON_FONT, button_font_path, sizeof(button_font_path));
        g_font = TTF_OpenFont(button_font_path, 24);
    }
    if (!g_font) {
        char enigma_font_path[512];
        ui_resolve_asset_path(ASSET_ENIGMA_FONT, enigma_font_path, sizeof(enigma_font_path));
        g_font = TTF_OpenFont(enigma_font_path, 24);
    }
    if (!g_font) {
        SDL_Log("Enigma font load failed: %s", TTF_GetError());
        return 0;
    }
    ui_apply_font_quality(g_font);

    g_music = ui_load_music(ASSET_ENIGMA_MUSIC);
    if (!g_music) {
        SDL_Log("Enigma music load warning (%s): %s", ASSET_ENIGMA_MUSIC, Mix_GetError());
    }
    g_happy4_music = ui_load_music(ASSET_ENIGMA_HAPPY4_MUSIC);
    if (!g_happy4_music) {
        SDL_Log("Enigma happy4 music load warning (%s): %s", ASSET_ENIGMA_HAPPY4_MUSIC, Mix_GetError());
    }

    g_initialized = 1;
    return 1;
}

void enigma_scene_enter_random(void)
{
    if (!g_initialized) return;

    clear_runtime_assets();
    g_return_to_main = 0;
    g_active = 0;

    int character_id = g_random_character_ids[rand() % ENIGMA_RANDOM_CHARACTER_COUNT];
    if (!load_character_pack(character_id)) {
        SDL_Log("Enigma scene failed to load character pack %d.", character_id);
        g_return_to_main = 1;
        return;
    }

    g_current_character_id = character_id;
    g_result_finished = 0;
    g_fade_alpha = 255;
    g_state = ENIGMA_STATE_FADE_IN;
    g_hovered_button = -1;
    g_active = 1;

    if (g_music) {
        Mix_HaltMusic();
        Mix_VolumeMusic(g_music_volume_sdl);
        Mix_PlayMusic(g_music, -1);
    }
}

void enigma_scene_leave(void)
{
    if (!g_initialized) return;

    g_active = 0;
    g_return_to_main = 0;
    g_state = ENIGMA_STATE_FADE_IN;
    g_fade_alpha = 255;
    g_result_finished = 0;
    g_hovered_button = -1;
    Mix_HaltMusic();
    clear_runtime_assets();
}

void enigma_scene_set_music_volume(int sdl_volume)
{
    if (sdl_volume < 0) sdl_volume = 0;
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
    g_music_volume_sdl = sdl_volume;
    Mix_VolumeMusic(g_music_volume_sdl);
}

void enigma_scene_handle_event(const SDL_Event* e)
{
    if (!g_initialized || !g_active || !e) return;

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode key = e->key.keysym.sym;

        if (key == SDLK_ESCAPE) {
            if (g_state != ENIGMA_STATE_FADE_OUT && g_state != ENIGMA_STATE_EXIT) {
                g_state = ENIGMA_STATE_FADE_OUT;
            }
            return;
        }

        if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) {
            if (g_state == ENIGMA_STATE_DIALOGUE) {
                g_state = ENIGMA_STATE_CHOICES;
            } else if (g_state == ENIGMA_STATE_RESULT_ANIM && g_result_finished) {
                g_state = ENIGMA_STATE_FADE_OUT;
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN &&
        e->button.button == SDL_BUTTON_LEFT &&
        g_state == ENIGMA_STATE_CHOICES) {
        int mx = e->button.x;
        int my = e->button.y;
        for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
            if (button_contains_point(&g_buttons[i], mx, my)) {
                g_buttons[i].visual.pressedTimer = 0.13f;
                show_result_for_answer(i);
                break;
            }
        }
    }

    if (e->type == SDL_MOUSEMOTION && g_state == ENIGMA_STATE_CHOICES) {
        g_hovered_button = -1;
        for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
            g_buttons[i].visual.selected = 0;
            if (button_contains_point(&g_buttons[i], e->motion.x, e->motion.y)) {
                g_hovered_button = i;
                g_buttons[i].visual.selected = 1;
                break;
            }
        }
    }
}

void enigma_scene_update(float dt)
{
    if (!g_initialized || !g_active) return;
    if (dt < 0.0f) dt = 0.0f;

    sprite_update(&g_character);
    for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
        if (!g_buttons[i].visible) continue;
        prototype_button_update(&g_buttons[i].visual, dt);
    }

    int fade_step = (int)(420.0f * dt);
    if (fade_step < 1) fade_step = 1;

    if (g_state == ENIGMA_STATE_FADE_IN) {
        g_fade_alpha -= fade_step;
        if (g_fade_alpha <= 0) {
            g_fade_alpha = 0;
            g_state = ENIGMA_STATE_DIALOGUE;
        }
    } else if (g_state == ENIGMA_STATE_RESULT_ANIM) {
        if (g_character.current_frame >= ENIGMA_TOTAL_FRAMES - 1) {
            g_result_finished = 1;
        }
    } else if (g_state == ENIGMA_STATE_FADE_OUT) {
        g_fade_alpha += fade_step;
        if (g_fade_alpha >= 255) {
            g_fade_alpha = 255;
            g_state = ENIGMA_STATE_EXIT;
            g_return_to_main = 1;
        }
    }
}

void enigma_scene_render(void)
{
    if (!g_initialized || !g_active || !g_renderer) return;

    if (g_background) {
        SDL_Rect full = {0, 0, WINDOW_W, WINDOW_H};
        SDL_RenderCopy(g_renderer, g_background, NULL, &full);
    }

    sprite_render(&g_character);

    if (g_state == ENIGMA_STATE_DIALOGUE) {
        render_question_text(g_dialogue_text);
        render_text_centered("Press ENTER", WINDOW_H / 2);
    } else if (g_state == ENIGMA_STATE_CHOICES) {
        render_question_text(g_question);
        for (int i = 0; i < ENIGMA_BUTTON_COUNT; ++i) {
            render_button(&g_buttons[i]);
        }
    } else if (g_state == ENIGMA_STATE_RESULT_ANIM && g_result_finished) {
        render_text_centered("Press ENTER", WINDOW_H / 2);
    }

    if (g_state == ENIGMA_STATE_FADE_IN || g_state == ENIGMA_STATE_FADE_OUT) {
        render_fade_overlay(g_fade_alpha);
    }
}

int enigma_scene_consume_return_request(void)
{
    if (!g_initialized) return 0;
    int request = g_return_to_main;
    g_return_to_main = 0;
    return request;
}

void enigma_scene_cleanup(void)
{
    if (!g_initialized) return;

    enigma_scene_leave();
    if (g_happy4_music) {
        Mix_FreeMusic(g_happy4_music);
        g_happy4_music = NULL;
    }
    if (g_music) {
        Mix_FreeMusic(g_music);
        g_music = NULL;
    }
    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }

    g_renderer = NULL;
    g_initialized = 0;
}
