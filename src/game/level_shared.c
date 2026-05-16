#include "level_shared.h"
#include "mainmenu_headers.h"
#include "asset_paths.h"
#include "debug_log.h"
#include "game_progress.h"
#include "skin_registry.h"
#include "ui_shared.h"

#include <string.h>

#define LEVEL_FRAME_COLS 6
#define LEVEL_FRAME_ROWS 6
#define LEVEL_RUN_ANIM_STEP 0.09f
#define LEVEL_IDLE_ANIM_STEP 0.14f
#define LEVEL_GRAVITY 1600.0f
#define LEVEL_JUMP_VELOCITY -620.0f
#define LEVEL_COMPLETE_DELAY_SECONDS 2.5f

static int level_total_frames(int frame_cols, int frame_rows)
{
    int cols = frame_cols > 0 ? frame_cols : 1;
    int rows = frame_rows > 0 ? frame_rows : 1;
    return cols * rows;
}

static const char* level_control_name(int scheme)
{
    if (scheme == 2) return "ARROWS + UP";
    if (scheme == 3) return "J/L + I";
    return "A/D + W";
}

static int level_control_left(const Uint8* keys, int scheme)
{
    if (!keys) return 0;
    if (scheme == 2) return keys[SDL_SCANCODE_LEFT] != 0;
    if (scheme == 3) return keys[SDL_SCANCODE_J] != 0;
    return keys[SDL_SCANCODE_A] != 0;
}

static int level_control_right(const Uint8* keys, int scheme)
{
    if (!keys) return 0;
    if (scheme == 2) return keys[SDL_SCANCODE_RIGHT] != 0;
    if (scheme == 3) return keys[SDL_SCANCODE_L] != 0;
    return keys[SDL_SCANCODE_D] != 0;
}

static int level_control_jump(const Uint8* keys, int scheme)
{
    if (!keys) return 0;
    if (scheme == 2) return keys[SDL_SCANCODE_UP] != 0;
    if (scheme == 3) return keys[SDL_SCANCODE_I] != 0;
    return keys[SDL_SCANCODE_W] != 0;
}

static void level_draw_text(SDL_Renderer* renderer, TTF_Font* font, const char* text, int x, int y, SDL_Color c)
{
    TTF_Font* render_font = NULL;

    if (!renderer || !font || !text || !text[0]) return;

    render_font = ui_font_for_text(font, text);
    if (!render_font) render_font = font;

    SDL_Surface* surf = TTF_RenderUTF8_Blended(render_font, text, c);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }

    SDL_FreeSurface(surf);
}

static void level_destroy_texture(SDL_Texture** texture)
{
    if (texture && *texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }
}

static void level_clear_player_skin(LevelPlayerSkin* skin)
{
    if (!skin) return;

    level_destroy_texture(&skin->idle_sheet);
    level_destroy_texture(&skin->run_sheet);
    skin->skin_id = 0;
    skin->frame_cols = 0;
    skin->idle_rows = 0;
    skin->run_rows = 0;
    skin->idle_frame_w = 0;
    skin->idle_frame_h = 0;
    skin->run_frame_w = 0;
    skin->run_frame_h = 0;
}

static const SkinDefinition* level_resolve_skin_definition(const SkinDefinition* skins, int skin_count, int requested_id)
{
    const SkinDefinition* def = NULL;
    if (!skins || skin_count <= 0) return NULL;

    def = skin_registry_find_by_id(skins, skin_count, requested_id);
    if (!def) def = skin_registry_first(skins, skin_count);
    return def;
}

static int level_load_player_skin(LevelRuntime* runtime,
                                  int player_index,
                                  int requested_id,
                                  const SkinDefinition* skins,
                                  int skin_count)
{
    const SkinDefinition* def = NULL;
    LevelPlayerSkin* target = NULL;
    int idle_w = 0;
    int idle_h = 0;
    int run_w = 0;
    int run_h = 0;

    if (!runtime || !runtime->renderer) return 0;
    if (player_index < 0 || player_index > 1) return 0;

    def = level_resolve_skin_definition(skins, skin_count, requested_id);
    if (!def) {
        debug_logf("level: no skin definitions available");
        return 0;
    }

    target = &runtime->player_skins[player_index];

    if (target->idle_sheet && target->run_sheet && target->skin_id == def->id) {
        return 1;
    }

    level_clear_player_skin(target);

    target->idle_sheet = IMG_LoadTexture(runtime->renderer, def->idle_path);
    if (!target->idle_sheet) {
        SDL_Log("Gameplay idle skin load failed (%s): %s", def->idle_path, IMG_GetError());
        debug_logf("level: idle skin load failed id=%d path=%s err=%s", def->id, def->idle_path, IMG_GetError());
        return 0;
    }

    target->run_sheet = IMG_LoadTexture(runtime->renderer, def->run_path);
    if (!target->run_sheet) {
        SDL_Log("Gameplay running skin load failed (%s): %s", def->run_path, IMG_GetError());
        debug_logf("level: run skin load failed id=%d path=%s err=%s", def->id, def->run_path, IMG_GetError());
        level_clear_player_skin(target);
        return 0;
    }

    if (SDL_QueryTexture(target->idle_sheet, NULL, NULL, &idle_w, &idle_h) != 0 || idle_w <= 0 || idle_h <= 0) {
        SDL_Log("Gameplay idle skin query failed: %s", SDL_GetError());
        debug_logf("level: idle skin query failed id=%d err=%s", def->id, SDL_GetError());
        level_clear_player_skin(target);
        return 0;
    }

    if (SDL_QueryTexture(target->run_sheet, NULL, NULL, &run_w, &run_h) != 0 || run_w <= 0 || run_h <= 0) {
        SDL_Log("Gameplay running skin query failed: %s", SDL_GetError());
        debug_logf("level: run skin query failed id=%d err=%s", def->id, SDL_GetError());
        level_clear_player_skin(target);
        return 0;
    }

    target->skin_id = def->id;
    target->frame_cols = LEVEL_FRAME_COLS;
    target->idle_rows = LEVEL_FRAME_ROWS;
    target->run_rows = LEVEL_FRAME_ROWS;
    target->idle_frame_w = idle_w / target->frame_cols;
    target->idle_frame_h = idle_h / target->idle_rows;
    target->run_frame_w = run_w / target->frame_cols;
    target->run_frame_h = run_h / target->run_rows;

    if (target->idle_frame_w <= 0 || target->idle_frame_h <= 0 ||
        target->run_frame_w <= 0 || target->run_frame_h <= 0) {
        SDL_Log("Gameplay skin sheets have invalid frame geometry.");
        debug_logf("level: invalid frame geometry id=%d idle=%dx%d run=%dx%d",
                   def->id,
                   target->idle_frame_w,
                   target->idle_frame_h,
                   target->run_frame_w,
                   target->run_frame_h);
        level_clear_player_skin(target);
        return 0;
    }

    debug_logf("level: loaded player=%d skin_id=%d idle=%s run=%s",
               player_index + 1,
               def->id,
               def->idle_path,
               def->run_path);
    return 1;
}

static SDL_Texture* level_create_text_texture(SDL_Renderer* renderer,
                                              TTF_Font* font,
                                              const char* text,
                                              SDL_Color color,
                                              SDL_Rect* out_rect)
{
    SDL_Surface* surf = NULL;
    SDL_Texture* tex = NULL;
    TTF_Font* render_font = NULL;

    if (out_rect) {
        *out_rect = (SDL_Rect){0, 0, 0, 0};
    }

    if (!renderer || !font || !text || !text[0]) return NULL;

    render_font = ui_font_for_text(font, text);
    if (!render_font) render_font = font;

    surf = TTF_RenderUTF8_Blended(render_font, text, color);
    if (!surf) return NULL;

    tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex && out_rect) {
        out_rect->w = surf->w;
        out_rect->h = surf->h;
    }

    SDL_FreeSurface(surf);
    return tex;
}

static void level_destroy_hud_cache(LevelRuntime* runtime)
{
    if (!runtime) return;

    level_destroy_texture(&runtime->hud_level_tex);
    level_destroy_texture(&runtime->hud_objective_tex);
    level_destroy_texture(&runtime->hud_controls_p1_tex);
    level_destroy_texture(&runtime->hud_controls_p2_tex);
    level_destroy_texture(&runtime->hud_complete_tex);

    runtime->hud_level_rect = (SDL_Rect){0, 0, 0, 0};
    runtime->hud_objective_rect = (SDL_Rect){0, 0, 0, 0};
    runtime->hud_controls_p1_rect = (SDL_Rect){0, 0, 0, 0};
    runtime->hud_controls_p2_rect = (SDL_Rect){0, 0, 0, 0};
    runtime->hud_complete_rect = (SDL_Rect){0, 0, 0, 0};
}

static void level_draw_background(LevelRuntime* runtime)
{
    int w = 0;
    int h = 0;

    if (!runtime || !runtime->renderer) return;

    w = runtime->config.world_w;
    h = runtime->config.world_h;

    for (int y = 0; y < h; ++y) {
        float t = (h > 1) ? ((float)y / (float)(h - 1)) : 0.0f;
        Uint8 r = (Uint8)((1.0f - t) * runtime->config.bg_top.r + t * runtime->config.bg_bottom.r);
        Uint8 g = (Uint8)((1.0f - t) * runtime->config.bg_top.g + t * runtime->config.bg_bottom.g);
        Uint8 b = (Uint8)((1.0f - t) * runtime->config.bg_top.b + t * runtime->config.bg_bottom.b);
        SDL_SetRenderDrawColor(runtime->renderer, r, g, b, 255);
        SDL_RenderDrawLine(runtime->renderer, 0, y, w, y);
    }

    SDL_SetRenderDrawColor(runtime->renderer,
                           runtime->config.ground_color.r,
                           runtime->config.ground_color.g,
                           runtime->config.ground_color.b,
                           255);
    SDL_Rect ground = {0, runtime->config.floor_y, w, h - runtime->config.floor_y};
    SDL_RenderFillRect(runtime->renderer, &ground);
}

static const char* level_objective_text(const LevelRuntime* runtime)
{
    if (!runtime) return "Reach the finish point at the right side";
    if (runtime->config.objective_text[0]) return runtime->config.objective_text;
    return "Reach the finish point at the right side";
}

static void level_rebuild_hud_cache(LevelRuntime* runtime)
{
    char level_text[64];
    char objective_text[128];
    char controls_p1[64];
    char controls_p2[64];
    SDL_Color text_color = {245, 245, 245, 255};
    SDL_Color complete_color = {255, 235, 150, 255};

    if (!runtime) return;

    level_destroy_hud_cache(runtime);

    if (!runtime->renderer || !runtime->hud_font) return;

    snprintf(level_text, sizeof(level_text), "LEVEL %d", runtime->config.level_number);
    snprintf(objective_text, sizeof(objective_text), "%s", level_objective_text(runtime));
    snprintf(controls_p1, sizeof(controls_p1), "P1 controls: %s", level_control_name(runtime->players[0].control_scheme));

    runtime->hud_level_tex = level_create_text_texture(runtime->renderer,
                                                       runtime->hud_font,
                                                       level_text,
                                                       text_color,
                                                       &runtime->hud_level_rect);
    runtime->hud_level_rect.x = 26;
    runtime->hud_level_rect.y = 22;

    runtime->hud_objective_tex = level_create_text_texture(runtime->renderer,
                                                           runtime->hud_font,
                                                           objective_text,
                                                           text_color,
                                                           &runtime->hud_objective_rect);
    runtime->hud_objective_rect.x = 26;
    runtime->hud_objective_rect.y = 54;

    runtime->hud_controls_p1_tex = level_create_text_texture(runtime->renderer,
                                                             runtime->hud_font,
                                                             controls_p1,
                                                             text_color,
                                                             &runtime->hud_controls_p1_rect);
    runtime->hud_controls_p1_rect.x = 26;
    runtime->hud_controls_p1_rect.y = 86;

    if (runtime->player_count == 2) {
        snprintf(controls_p2, sizeof(controls_p2), "P2 controls: %s", level_control_name(runtime->players[1].control_scheme));
        runtime->hud_controls_p2_tex = level_create_text_texture(runtime->renderer,
                                                                 runtime->hud_font,
                                                                 controls_p2,
                                                                 text_color,
                                                                 &runtime->hud_controls_p2_rect);
        runtime->hud_controls_p2_rect.x = 26;
        runtime->hud_controls_p2_rect.y = 118;
    }

    runtime->hud_complete_tex = level_create_text_texture(runtime->renderer,
                                                          runtime->hud_font,
                                                          "LEVEL CLEAR",
                                                          complete_color,
                                                          &runtime->hud_complete_rect);
    runtime->hud_complete_rect.x = (runtime->config.world_w - runtime->hud_complete_rect.w) / 2;
    runtime->hud_complete_rect.y = 30;
}

void level_config_set_defaults(LevelConfig* cfg, int level_number)
{
    if (!cfg) return;

    cfg->level_number = level_number;
    cfg->world_w = WINDOW_W;
    cfg->world_h = WINDOW_H;
    cfg->start_x = 30;
    cfg->finish_x = WINDOW_W - 120;
    cfg->floor_y = WINDOW_H - 170;
    cfg->player_w = 96;
    cfg->player_h = 96;
    cfg->player_speed = 240.0f;
    cfg->bg_top = (SDL_Color){0x22, 0x34, 0x56, 0xFF};
    cfg->bg_bottom = (SDL_Color){0x4C, 0x6C, 0x91, 0xFF};
    cfg->ground_color = (SDL_Color){0x27, 0x58, 0x46, 0xFF};
    cfg->finish_color = (SDL_Color){0xFF, 0xDD, 0x77, 0xFF};
    snprintf(cfg->objective_text, sizeof(cfg->objective_text), "Reach the finish point at the right side");
}

int level_runtime_init(LevelRuntime* runtime, SDL_Renderer* renderer, const LevelConfig* cfg)
{
    if (!runtime || !renderer || !cfg) return 0;

    if (runtime->renderer ||
        runtime->hud_font ||
        runtime->hud_level_tex ||
        runtime->hud_objective_tex ||
        runtime->hud_controls_p1_tex ||
        runtime->hud_controls_p2_tex ||
        runtime->hud_complete_tex ||
        runtime->player_skins[0].idle_sheet ||
        runtime->player_skins[0].run_sheet ||
        runtime->player_skins[1].idle_sheet ||
        runtime->player_skins[1].run_sheet) {
        level_runtime_cleanup(runtime);
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->renderer = renderer;
    runtime->config = *cfg;

    const char* hud_font_candidates[] = {
        ASSET_MAIN_MENU_FONT_TEXT,
        ASSET_MAIN_MENU_FONT_OPTIONS
    };

    runtime->hud_font = ui_open_arial_font(24, 0);
    if (!runtime->hud_font) {
        runtime->hud_font = ui_open_font_from_candidates(hud_font_candidates,
                                                          (int)(sizeof(hud_font_candidates) / sizeof(hud_font_candidates[0])),
                                                          24,
                                                          0);
    }
    if (!runtime->hud_font) {
        SDL_Log("Gameplay HUD font load warning: %s", TTF_GetError());
        debug_logf("level: hud font warning err=%s", TTF_GetError());
    }

    debug_logf("level: runtime init success level=%d", runtime->config.level_number);
    return 1;
}

void level_runtime_enter(LevelRuntime* runtime, const GameSelection* selection)
{
    SkinDefinition skins[SKIN_REGISTRY_MAX];
    int discovered = 0;
    int start_skin_1 = 1;
    int start_skin_2 = 1;
    int control_1 = 1;
    int control_2 = 2;
    int duo_mode = 0;

    if (!runtime) return;

    if (selection) {
        runtime->selection = *selection;
    } else {
        runtime->selection = (GameSelection){0};
        runtime->selection.player_count = 1;
        runtime->selection.selected_skin[0] = 1;
        runtime->selection.control_scheme[0] = 1;
    }

    duo_mode = runtime->selection.duo_mode ? 1 : 0;
    runtime->player_count = duo_mode ? 2 : 1;
    if (runtime->selection.player_count == 2) runtime->player_count = 2;
    if (runtime->player_count < 1) runtime->player_count = 1;
    if (runtime->player_count > 2) runtime->player_count = 2;

    start_skin_1 = runtime->selection.selected_skin[0];
    start_skin_2 = runtime->selection.selected_skin[1];
    control_1 = runtime->selection.control_scheme[0];
    control_2 = runtime->selection.control_scheme[1];

    if (start_skin_1 < 1) start_skin_1 = 1;
    if (start_skin_2 < 1) start_skin_2 = start_skin_1;
    if (control_1 < 1 || control_1 > 3) control_1 = 1;
    if (control_2 < 1 || control_2 > 3) control_2 = 2;

    discovered = skin_registry_load(skins, SKIN_REGISTRY_MAX);
    if (discovered <= 0) {
        debug_logf("level: no skins available for level=%d", runtime->config.level_number);
        runtime->active = 0;
        runtime->return_requested = 1;
        return;
    }

    if (!level_load_player_skin(runtime, 0, start_skin_1, skins, discovered)) {
        debug_logf("level: failed to load player 1 skin requested=%d", start_skin_1);
        runtime->active = 0;
        runtime->return_requested = 1;
        return;
    }

    if (runtime->player_count == 2) {
        if (!level_load_player_skin(runtime, 1, start_skin_2, skins, discovered)) {
            debug_logf("level: failed to load player 2 skin requested=%d", start_skin_2);
            runtime->active = 0;
            runtime->return_requested = 1;
            return;
        }
    }

    memset(runtime->players, 0, sizeof(runtime->players));

    runtime->players[0].x = (float)runtime->config.start_x;
    runtime->players[0].y = (float)(runtime->config.floor_y - runtime->config.player_h);
    runtime->players[0].vy = 0.0f;
    runtime->players[0].skin_id = runtime->player_skins[0].skin_id > 0 ? runtime->player_skins[0].skin_id : start_skin_1;
    runtime->players[0].control_scheme = control_1;
    runtime->players[0].tint = (SDL_Color){255, 255, 255, 255};
    runtime->players[0].on_ground = 1;
    runtime->players[0].jump_was_down = 0;
    runtime->players[0].facing = 1;

    runtime->players[1].x = (float)(runtime->config.start_x + runtime->config.player_w + 14);
    runtime->players[1].y = (float)(runtime->config.floor_y - runtime->config.player_h);
    runtime->players[1].vy = 0.0f;
    runtime->players[1].skin_id = runtime->player_skins[1].skin_id > 0 ? runtime->player_skins[1].skin_id : start_skin_2;
    runtime->players[1].control_scheme = control_2;
    runtime->players[1].tint = (SDL_Color){255, 255, 255, 255};
    runtime->players[1].on_ground = 1;
    runtime->players[1].jump_was_down = 0;
    runtime->players[1].facing = 1;

    level_rebuild_hud_cache(runtime);

    debug_logf("level: enter level=%d players=%d skin1=%d skin2=%d ctrl1=%d ctrl2=%d",
               runtime->config.level_number,
               runtime->player_count,
               runtime->players[0].skin_id,
               runtime->players[1].skin_id,
               runtime->players[0].control_scheme,
               runtime->players[1].control_scheme);

    game_progress_set_selection(&runtime->selection);
    game_progress_set_level_progress(runtime->config.level_number,
                                     runtime->config.level_number,
                                     0);
    game_progress_mark_started_game(1);
    game_progress_save_global_to_path(GAME_PROGRESS_LAST_SAVE_PATH);

    runtime->active = 1;
    runtime->return_requested = 0;
    runtime->next_level_requested = 0;
    runtime->level_complete = 0;
    runtime->level_complete_timer = 0.0f;
}

void level_runtime_leave(LevelRuntime* runtime)
{
    if (!runtime) return;
    runtime->active = 0;
    runtime->return_requested = 0;
    runtime->next_level_requested = 0;
    runtime->level_complete = 0;
    runtime->level_complete_timer = 0.0f;
}

void level_runtime_handle_event(LevelRuntime* runtime, const SDL_Event* e)
{
    if (!runtime || !runtime->active || !e) return;

    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        runtime->return_requested = 1;
    }
}

void level_runtime_update(LevelRuntime* runtime, float dt)
{
    const Uint8* keys = NULL;

    if (!runtime || !runtime->active) return;
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.05f) dt = 0.05f;

    keys = SDL_GetKeyboardState(NULL);

    if (!runtime->level_complete) {
        for (int i = 0; i < runtime->player_count; ++i) {
            LevelPlayer* p = &runtime->players[i];
            int left = level_control_left(keys, p->control_scheme);
            int right = level_control_right(keys, p->control_scheme);
            int jump = level_control_jump(keys, p->control_scheme);
            int dir = 0;
            float floor_y = (float)(runtime->config.floor_y - runtime->config.player_h);

            if (left && !right) dir = -1;
            if (right && !left) dir = 1;

            if (jump && !p->jump_was_down && p->on_ground) {
                p->vy = LEVEL_JUMP_VELOCITY;
                p->on_ground = 0;
            }
            p->jump_was_down = jump;

            if (dir < 0) p->facing = -1;
            if (dir > 0) p->facing = 1;

            p->moving = (dir != 0) || !p->on_ground;

            if (p->moving) {
                p->x += (float)dir * runtime->config.player_speed * dt;
                if (p->x < 0.0f) p->x = 0.0f;
                if (p->x > (float)(runtime->config.world_w - runtime->config.player_w)) {
                    p->x = (float)(runtime->config.world_w - runtime->config.player_w);
                }
            }

            p->vy += LEVEL_GRAVITY * dt;
            p->y += p->vy * dt;
            if (p->y >= floor_y) {
                p->y = floor_y;
                p->vy = 0.0f;
                p->on_ground = 1;
            } else {
                p->on_ground = 0;
            }

            p->frame_timer += dt;
            if (p->moving) {
                LevelPlayerSkin* skin = &runtime->player_skins[i];
                int run_total = level_total_frames(skin->frame_cols, skin->run_rows);
                if (p->frame_timer >= LEVEL_RUN_ANIM_STEP) {
                    p->frame_timer -= LEVEL_RUN_ANIM_STEP;
                    p->frame = (p->frame + 1) % run_total;
                }
            } else {
                LevelPlayerSkin* skin = &runtime->player_skins[i];
                int idle_total = level_total_frames(skin->frame_cols, skin->idle_rows);
                if (p->frame_timer >= LEVEL_IDLE_ANIM_STEP) {
                    p->frame_timer -= LEVEL_IDLE_ANIM_STEP;
                    p->frame = (p->frame + 1) % idle_total;
                }
            }

            p->reached_finish = ((int)(p->x + runtime->config.player_w) >= runtime->config.finish_x);
        }

        if (runtime->player_count == 1) {
            runtime->level_complete = runtime->players[0].reached_finish;
        } else {
            runtime->level_complete = runtime->players[0].reached_finish && runtime->players[1].reached_finish;
        }

        if (runtime->level_complete) {
            runtime->level_complete_timer = 0.0f;
        }
    } else {
        runtime->level_complete_timer += dt;
        if (runtime->level_complete_timer >= LEVEL_COMPLETE_DELAY_SECONDS) {
            runtime->next_level_requested = 1;
        }
    }
}

void level_runtime_render(LevelRuntime* runtime)
{
    char level_text[64];
    char objective_text[128];
    char controls_p1[64];
    char controls_p2[64];
    SDL_Color text_color = {245, 245, 245, 255};
    SDL_Color complete_color = {255, 235, 150, 255};

    if (!runtime || !runtime->active || !runtime->renderer) return;

    level_draw_background(runtime);

    SDL_SetRenderDrawColor(runtime->renderer,
                           runtime->config.finish_color.r,
                           runtime->config.finish_color.g,
                           runtime->config.finish_color.b,
                           255);
    SDL_Rect finish_zone = {
        runtime->config.finish_x,
        runtime->config.floor_y - 150,
        16,
        150
    };
    SDL_RenderFillRect(runtime->renderer, &finish_zone);

    for (int i = 0; i < runtime->player_count; ++i) {
        LevelPlayer* p = &runtime->players[i];
        LevelPlayerSkin* skin = &runtime->player_skins[i];
        int use_running_state = p->moving || !p->on_ground;
        SDL_Texture* active_sheet = use_running_state ? skin->run_sheet : skin->idle_sheet;
        int frame_w = use_running_state ? skin->run_frame_w : skin->idle_frame_w;
        int frame_h = use_running_state ? skin->run_frame_h : skin->idle_frame_h;
        int frame_rows = use_running_state ? skin->run_rows : skin->idle_rows;
        int frame_cols = (skin->frame_cols > 0) ? skin->frame_cols : LEVEL_FRAME_COLS;
        int total_frames = level_total_frames(frame_cols, frame_rows);
        int frame_index = (total_frames > 0) ? (p->frame % total_frames) : 0;
        int row = frame_index / frame_cols;
        int col = frame_index % frame_cols;
        SDL_Rect src = {
            col * frame_w,
            row * frame_h,
            frame_w,
            frame_h
        };
        SDL_Rect dst = {
            (int)p->x,
            (int)p->y,
            runtime->config.player_w,
            runtime->config.player_h
        };

        if (active_sheet) {
            SDL_SetTextureColorMod(active_sheet, p->tint.r, p->tint.g, p->tint.b);
            SDL_RendererFlip flip = (p->facing < 0) ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
            SDL_RenderCopyEx(runtime->renderer, active_sheet, &src, &dst, 0.0, NULL, flip);
            SDL_SetTextureColorMod(active_sheet, 255, 255, 255);
        }
    }

    if (runtime->hud_level_tex) {
        SDL_RenderCopy(runtime->renderer, runtime->hud_level_tex, NULL, &runtime->hud_level_rect);
    } else if (runtime->hud_font) {
        snprintf(level_text, sizeof(level_text), "LEVEL %d", runtime->config.level_number);
        level_draw_text(runtime->renderer, runtime->hud_font, level_text, 26, 22, text_color);
    }

    if (runtime->hud_objective_tex) {
        SDL_RenderCopy(runtime->renderer, runtime->hud_objective_tex, NULL, &runtime->hud_objective_rect);
    } else if (runtime->hud_font) {
        snprintf(objective_text, sizeof(objective_text), "%s", level_objective_text(runtime));
        level_draw_text(runtime->renderer, runtime->hud_font, objective_text, 26, 54, text_color);
    }

    if (runtime->hud_controls_p1_tex) {
        SDL_RenderCopy(runtime->renderer, runtime->hud_controls_p1_tex, NULL, &runtime->hud_controls_p1_rect);
    } else if (runtime->hud_font) {
        snprintf(controls_p1, sizeof(controls_p1), "P1 controls: %s", level_control_name(runtime->players[0].control_scheme));
        level_draw_text(runtime->renderer, runtime->hud_font, controls_p1, 26, 86, text_color);
    }

    if (runtime->player_count == 2) {
        if (runtime->hud_controls_p2_tex) {
            SDL_RenderCopy(runtime->renderer, runtime->hud_controls_p2_tex, NULL, &runtime->hud_controls_p2_rect);
        } else if (runtime->hud_font) {
            snprintf(controls_p2, sizeof(controls_p2), "P2 controls: %s", level_control_name(runtime->players[1].control_scheme));
            level_draw_text(runtime->renderer, runtime->hud_font, controls_p2, 26, 118, text_color);
        }
    }

    if (runtime->level_complete) {
        if (runtime->hud_complete_tex) {
            SDL_RenderCopy(runtime->renderer, runtime->hud_complete_tex, NULL, &runtime->hud_complete_rect);
        } else if (runtime->hud_font) {
            level_draw_text(runtime->renderer, runtime->hud_font, "LEVEL CLEAR", runtime->config.world_w / 2 - 90, 30, complete_color);
        }
    }
}

int level_runtime_consume_return_request(LevelRuntime* runtime)
{
    int out = 0;
    if (!runtime) return 0;
    out = runtime->return_requested;
    runtime->return_requested = 0;
    return out;
}

int level_runtime_consume_next_level_request(LevelRuntime* runtime)
{
    int out = 0;
    if (!runtime) return 0;
    out = runtime->next_level_requested;
    runtime->next_level_requested = 0;
    if (out) {
        game_progress_set_level_progress(runtime->config.level_number + 1,
                                         runtime->config.level_number + 1,
                                         0);
        game_progress_save_global_to_path(GAME_PROGRESS_LAST_SAVE_PATH);
    }
    return out;
}

void level_runtime_cleanup(LevelRuntime* runtime)
{
    if (!runtime) return;

    for (int i = 0; i < 2; ++i) {
        level_clear_player_skin(&runtime->player_skins[i]);
    }
    if (runtime->hud_font) {
        TTF_CloseFont(runtime->hud_font);
        runtime->hud_font = NULL;
    }
    level_destroy_hud_cache(runtime);

    runtime->renderer = NULL;
}
