#include "final_cutscene.h"

#include "ui_shared.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define FC_W 1280
#define FC_H 720
#define FC_ANIM_COLS 6
#define FC_ANIM_ROWS 6
#define FC_ANIM_FRAMES 36
#define FC_HARRY_CUTSCENE_MAX_FRAMES 512
#define FC_MERV_CUTSCENE_MAX_FRAMES 512
#define FC_HARRY_VAN_MAX_FRAMES 512
#define FC_DECISION_SLIDE_MS 700
#define FC_HARRY_VAN_DIALOGUE_LINES 5
#define FC_DECISION_PATH "saves/final_decision.txt"
#define FC_HARRY_CUTSCENE_DIR "assets/final_cutscenes/harry"
#define FC_MERV_CUTSCENE_DIR "assets/final_cutscenes/merv"
#define FC_GUNSHOT_PATH "assets/final_cutscenes/gunshot.wav"

static int chase_tile_x = 0;
static int chase_tile_y = -340;
static int chase_tile_w = 0;
static int chase_tile_h = 1440;

typedef enum {
    FC_STAGE_FADE_OUT_LEVEL,
    FC_STAGE_CHASE_FADE_IN,
    FC_STAGE_CHASE_DIALOGUE,
    FC_STAGE_CHASE,
    FC_STAGE_WHITE_FADE,
    FC_STAGE_STORY_1,
    FC_STAGE_STORY_2,
    FC_STAGE_BASE_FADE_IN,
    FC_STAGE_DIALOGUE,
    FC_STAGE_DECISION_SLIDE,
    FC_STAGE_DECISION,
    FC_STAGE_BLACK_FADE,
    FC_STAGE_HARRY_CUTSCENE_FADE_IN,
    FC_STAGE_HARRY_CUTSCENE,
    FC_STAGE_MERV_CUTSCENE_FADE_IN,
    FC_STAGE_MERV_CUTSCENE,
    FC_STAGE_BRANCH_STORY,
    FC_STAGE_HARRY_VAN_FADE_IN,
    FC_STAGE_HARRY_VAN_DIALOGUE,
    FC_STAGE_HARRY_VAN,
    FC_STAGE_HARRY_VAN_FADE_OUT,
    FC_STAGE_HARRY_VAN_TRUCK,
    FC_STAGE_CRASH_BLACK,
    FC_STAGE_ESCAPE_FADE_IN,
    FC_STAGE_ESCAPE_RUN,
    FC_STAGE_ESCAPE_FADE_OUT,
    FC_STAGE_GUNSHOT_HOLD,
    FC_STAGE_CREDITS_FADE_OUT,
    FC_STAGE_CREDITS,
    FC_STAGE_DONE
} FinalCutsceneStage;

typedef enum {
    FC_SPEAKER_MARV,
    FC_SPEAKER_HARRY
} FinalSpeaker;

typedef struct {
    FinalSpeaker speaker;
    const char* text;
} FinalDialogueLine;

typedef struct {
    SDL_Texture* chase_bg;
    SDL_Texture* base_bg;
    SDL_Texture* kid_trapped;
    SDL_Texture* harry_forest_bg;
    SDL_Texture* harry_van;
    SDL_Texture* harry_truck;
    SDL_Texture* harry_wreck;
    SDL_Texture* kid_escape_run;
    SDL_Texture* kid_run;
    SDL_Texture* marv_run;
    SDL_Texture* harry_run;
    SDL_Texture* marv_idle;
    SDL_Texture* harry_idle;
    Mix_Music* chase_music;
    Mix_Music* harry_cutscene_music;
    Mix_Music* merv_cutscene_music;
    Mix_Chunk* gunshot_sfx;
    SDL_Texture* harry_cutscene_frames[FC_HARRY_CUTSCENE_MAX_FRAMES];
    int harry_cutscene_frame_count;
    int harry_cutscene_frame_index;
    Uint32 harry_cutscene_frame_tick;
    SDL_Texture* merv_cutscene_frames[FC_MERV_CUTSCENE_MAX_FRAMES];
    int merv_cutscene_frame_count;
    int merv_cutscene_frame_index;
    Uint32 merv_cutscene_frame_tick;
    int music_muted;
    TTF_Font* font;
    TTF_Font* big_font;
    FinalCutsceneStage stage;
    Uint32 stage_started;
    int running;
    int frame;
    Uint32 frame_tick;
    float catch_meter;
    float chase_scroll;
    Uint32 last_chase_press_tick;
    int chase_dialogue_index;
    int chase_dialogue_chars;
    Uint32 chase_dialogue_tick;
    int chase_prompt_visible;
    int harry_cutscene_requested;
    int merv_cutscene_requested;
    int dialogue_index;
    int dialogue_chars;
    Uint32 dialogue_tick;
    int hovered_choice;
    int selected_choice;
    int harry_cutscene_music_started;
    int merv_cutscene_music_started;
    int branch_story_index;
    int gunshot_played;
    int harry_van_dialogue_index;
    int harry_van_dialogue_chars;
    Uint32 harry_van_dialogue_tick;
    Uint32 harry_van_dialogue_line_started;
    int harry_van_wobble_started;
    float harry_van_wobble_phase;
    float harry_van_truck_x;
    float escape_kid_x;
} FinalCutscene;

static const FinalDialogueLine kHarryVanDialogue[] = {
    { FC_SPEAKER_HARRY, "I'm gonna take you now to your parents and I'm gonna threaten them on the way so that I can get my money on the go" },
    { FC_SPEAKER_MARV, "Can I tell you something?" },
    { FC_SPEAKER_HARRY, "What, you little fucker?" },
    { FC_SPEAKER_MARV, "Your breath stinks" },
    { FC_SPEAKER_HARRY, "Okay, I'm gonna ignore that because I'm gonna kill you just like Merv said if I don't" },
    { FC_SPEAKER_MARV, "No wonder your parents didn't love you if you were always like this" },
    { FC_SPEAKER_HARRY, "Come here, you little fucker" }
};

static const char* const kHarryBranchStory[] = {
    "harry knocked merv unconcious",
    "he took kevin to the car",
    "now he wants achive his bail plan for money"
};

static const char* const kMervBranchStory[] = {
    "merv shot harry to death",
    "and next he aimed the gun at kevin and ..."
};

static const char* const kTempCredits[] = {
    "HOME ADVENTURE",
    "",
    "Final cutscene branch",
    "Temporary credits",
    "",
    "Code and integration",
    "OpenAI Codex",
    "",
    "Assets and direction",
    "bro",
    "",
    "Thanks for playing"
};

static const FinalDialogueLine kFinalDialogue[] = {
    { FC_SPEAKER_MARV, "we finally caught the cunt." },
    { FC_SPEAKER_HARRY, "yeah we finally get to ask for money" },
    { FC_SPEAKER_MARV, "what money?" },
    { FC_SPEAKER_HARRY, "we gonna bail him right?" },
    { FC_SPEAKER_MARV, "no we gonna kill him, dont u remember he almost killed us by a fucking refrigerator!" },
    { FC_SPEAKER_HARRY, "no no no i need money were gonna fucking bail him aint nobody killing a 10 years old here" },
    { FC_SPEAKER_MARV, "its either u go with me or ill just finish u as well, im tired of this crap" },
    { FC_SPEAKER_HARRY, "u can try thou..." }
};

static const FinalDialogueLine kChaseDialogue[] = {
    { FC_SPEAKER_MARV, "cmon harry hes out of wits" },
    { FC_SPEAKER_HARRY, "we almost got him runnnnnn" }
};

static const char* fc_speaker_name(FinalSpeaker speaker);
static SDL_Texture* fc_speaker_texture(FinalCutscene* scene, FinalSpeaker speaker);
static void fc_wrap_and_draw(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int visible_chars,
                             int x,
                             int y,
                             int max_w,
                             SDL_Color color);

static float fc_clampf(float v, float min_v, float max_v)
{
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static void fc_set_stage(FinalCutscene* scene, FinalCutsceneStage stage)
{
    if (!scene) return;
    scene->stage = stage;
    scene->stage_started = SDL_GetTicks();
}

static int fc_is_skip_to_basement_key(const SDL_KeyboardEvent* key)
{
    SDL_Keymod mod;
    SDL_Keycode sym;
    SDL_Scancode scan;

    if (!key || key->repeat) return 0;
    mod = key->keysym.mod;
    if (!(mod & KMOD_SHIFT) || !(mod & KMOD_ALT)) return 0;

    sym = key->keysym.sym;
    scan = key->keysym.scancode;
    return sym == 0x00F9 ||       /* ù */
           sym == SDLK_PERCENT || /* shifted ù on some layouts */
           scan == SDL_SCANCODE_APOSTROPHE;
}

static void fc_skip_to_basement(FinalCutscene* scene)
{
    if (!scene) return;
    Mix_HaltMusic();
    scene->catch_meter = 0.0f;
    scene->chase_prompt_visible = 0;
    scene->dialogue_index = 0;
    scene->dialogue_chars = 0;
    scene->dialogue_tick = SDL_GetTicks();
    fc_set_stage(scene, FC_STAGE_BASE_FADE_IN);
}

static void fc_start_branch_story(FinalCutscene* scene)
{
    if (!scene) return;
    scene->branch_story_index = 0;
    fc_set_stage(scene, FC_STAGE_BRANCH_STORY);
}

static SDL_Texture* fc_load_texture(SDL_Renderer* renderer, const char* path)
{
    return ui_load_texture(renderer, path);
}

static void fc_draw_text(SDL_Renderer* renderer,
                         TTF_Font* font,
                         const char* text,
                         int x,
                         int y,
                         SDL_Color color)
{
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;
    SDL_Rect dst;

    if (!renderer || !font || !text || !text[0]) return;
    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }
    dst.x = x;
    dst.y = y;
    dst.w = surface->w;
    dst.h = surface->h;
    SDL_FreeSurface(surface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, color.a);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void fc_draw_text_center(SDL_Renderer* renderer,
                                TTF_Font* font,
                                const char* text,
                                int center_x,
                                int y,
                                SDL_Color color)
{
    int w = 0;
    int h = 0;

    if (!font || !text) return;
    if (TTF_SizeUTF8(font, text, &w, &h) != 0) return;
    fc_draw_text(renderer, font, text, center_x - w / 2, y, color);
}

static void fc_draw_filled(SDL_Renderer* renderer, SDL_Rect rect, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static void fc_draw_full_texture(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Color fallback)
{
    SDL_Rect dst = {0, 0, FC_W, FC_H};

    if (texture) {
        SDL_RenderCopy(renderer, texture, NULL, &dst);
        return;
    }
    fc_draw_filled(renderer, dst, fallback);
}

static SDL_Rect fc_lerp_rect(SDL_Rect from, SDL_Rect to, float t)
{
    SDL_Rect out;

    t = fc_clampf(t, 0.0f, 1.0f);
    out.x = from.x + (int)((float)(to.x - from.x) * t);
    out.y = from.y + (int)((float)(to.y - from.y) * t);
    out.w = from.w + (int)((float)(to.w - from.w) * t);
    out.h = from.h + (int)((float)(to.h - from.h) * t);
    return out;
}

static void fc_draw_tiled_texture(SDL_Renderer* renderer,
                                  SDL_Texture* texture,
                                  float scroll_x,
                                  int tile_x,
                                  int tile_y,
                                  int tile_w,
                                  int tile_h,
                                  SDL_Color fallback)
{
    int tex_w = 0;
    int tex_h = 0;
    int start_x = 0;

    if (!texture || SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0 ||
        tex_w <= 0 || tex_h <= 0) {
        fc_draw_full_texture(renderer, NULL, fallback);
        return;
    }

    if (tile_w <= 0) tile_w = tex_w;
    if (tile_h <= 0) tile_h = tex_h;
    start_x = tile_x - ((int)scroll_x % tile_w);
    if (start_x > tile_x) start_x -= tile_w;
    for (int y = tile_y; y < FC_H; y += tile_h) {
        for (int x = start_x; x < FC_W; x += tile_w) {
            SDL_Rect dst = {x, y, tile_w, tile_h};
            SDL_RenderCopy(renderer, texture, NULL, &dst);
        }
    }
}

static void fc_draw_sprite_sheet(SDL_Renderer* renderer,
                                 SDL_Texture* texture,
                                 int frame,
                                 SDL_Rect dst,
                                 SDL_RendererFlip flip,
                                 Uint8 alpha)
{
    int tex_w = 0;
    int tex_h = 0;
    SDL_Rect src;

    if (!texture) {
        fc_draw_filled(renderer, dst, (SDL_Color){180, 180, 190, alpha});
        return;
    }
    if (SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0 || tex_w <= 0 || tex_h <= 0) return;
    src.w = tex_w / FC_ANIM_COLS;
    src.h = tex_h / FC_ANIM_ROWS;
    src.x = (frame % FC_ANIM_COLS) * src.w;
    src.y = (frame / FC_ANIM_COLS) * src.h;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, alpha);
    SDL_RenderCopyEx(renderer, texture, &src, &dst, 0.0, NULL, flip);
    SDL_SetTextureAlphaMod(texture, 255);
}

static void fc_draw_overlay(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_Rect full = {0, 0, FC_W, FC_H};
    fc_draw_filled(renderer, full, (SDL_Color){r, g, b, a});
}

static SDL_Texture* fc_load_abs_texture(SDL_Renderer* renderer, const char* path)
{
    if (!renderer || !path) return NULL;
    return IMG_LoadTexture(renderer, path);
}

static void fc_free_harry_cutscene_frames(FinalCutscene* scene)
{
    if (!scene) return;
    for (int i = 0; i < scene->harry_cutscene_frame_count; ++i) {
        if (scene->harry_cutscene_frames[i]) {
            SDL_DestroyTexture(scene->harry_cutscene_frames[i]);
            scene->harry_cutscene_frames[i] = NULL;
        }
    }
    scene->harry_cutscene_frame_count = 0;
}

static void fc_free_merv_cutscene_frames(FinalCutscene* scene)
{
    if (!scene) return;
    for (int i = 0; i < scene->merv_cutscene_frame_count; ++i) {
        if (scene->merv_cutscene_frames[i]) {
            SDL_DestroyTexture(scene->merv_cutscene_frames[i]);
            scene->merv_cutscene_frames[i] = NULL;
        }
    }
    scene->merv_cutscene_frame_count = 0;
}

static void fc_load_harry_cutscene_frames(FinalCutscene* scene, SDL_Renderer* renderer)
{
    char path[PATH_MAX];

    if (!scene || !renderer) return;
    fc_free_harry_cutscene_frames(scene);
    for (int i = 1; i <= FC_HARRY_CUTSCENE_MAX_FRAMES; ++i) {
        SDL_Texture* tex = NULL;

        snprintf(path, sizeof(path), "%s/ezgif-frame-%03d.jpg", FC_HARRY_CUTSCENE_DIR, i);
        tex = fc_load_abs_texture(renderer, path);
        if (!tex) {
            if (i <= 4) {
                SDL_Log("Could not load Harry cutscene frame: %s", path);
            }
            break;
        }
        scene->harry_cutscene_frames[scene->harry_cutscene_frame_count++] = tex;
    }
}

static void fc_load_merv_cutscene_frames(FinalCutscene* scene, SDL_Renderer* renderer)
{
    char path[PATH_MAX];

    if (!scene || !renderer) return;
    fc_free_merv_cutscene_frames(scene);
    for (int i = 1; i <= FC_MERV_CUTSCENE_MAX_FRAMES; ++i) {
        SDL_Texture* tex = NULL;

        snprintf(path, sizeof(path), "%s/ezgif-frame-%03d.jpg", FC_MERV_CUTSCENE_DIR, i);
        tex = fc_load_abs_texture(renderer, path);
        if (!tex) {
            if (i <= 4) {
                SDL_Log("Could not load Merv cutscene frame: %s", path);
            }
            break;
        }
        scene->merv_cutscene_frames[scene->merv_cutscene_frame_count++] = tex;
    }
}

static void fc_advance_frame(FinalCutscene* scene)
{
    Uint32 now = SDL_GetTicks();

    if (!scene) return;
    if (now - scene->frame_tick >= 42) {
        scene->frame = (scene->frame + 1) % FC_ANIM_FRAMES;
        scene->frame_tick = now;
    }
}

static void fc_render_chase(FinalCutscene* scene, SDL_Renderer* renderer)
{
    float t = fc_clampf(scene->catch_meter, 0.0f, 1.0f);
    int kid_x = 1050;
    int thief_start_x = 110;
    int thief_final_x = kid_x - 92;
    int thief_base = thief_start_x + (int)((float)(thief_final_x - thief_start_x) * t);
    SDL_Rect kid = {kid_x, 472+120, 98, 124};
    SDL_Rect marv = {thief_base, 474+120, 102, 124};
    SDL_Rect harry = {thief_base - 66, 472+120, 102, 124};
    SDL_Rect bar_outer = {320, 92, 640, 30};
    SDL_Rect bar_inner = {326, 98, (int)(628.0f * t), 18};

    fc_draw_tiled_texture(renderer, scene->chase_bg, scene->chase_scroll,
                          chase_tile_x, chase_tile_y, chase_tile_w, chase_tile_h,
                          (SDL_Color){38, 45, 54, 255});
    fc_draw_overlay(renderer, 0, 0, 0, 55);
    if (scene->chase_prompt_visible) {
        fc_draw_text_center(renderer, scene->big_font, "Spam ENTER TO CATCH THE KID", FC_W / 2, 34,
                            (SDL_Color){255, 238, 184, 255});
        fc_draw_filled(renderer, bar_outer, (SDL_Color){20, 22, 28, 220});
        fc_draw_filled(renderer, bar_inner, (SDL_Color){224, 64, 47, 255});
        SDL_SetRenderDrawColor(renderer, 255, 238, 184, 255);
        SDL_RenderDrawRect(renderer, &bar_outer);
    }
    fc_draw_sprite_sheet(renderer, scene->harry_run, scene->frame, harry, SDL_FLIP_NONE, 255);
    fc_draw_sprite_sheet(renderer, scene->marv_run, scene->frame, marv, SDL_FLIP_NONE, 255);
    fc_draw_sprite_sheet(renderer, scene->kid_run, scene->frame, kid, SDL_FLIP_NONE, 255);
}

static void fc_render_harry_cutscene(FinalCutscene* scene, SDL_Renderer* renderer)
{
    SDL_Rect dst = {0, 0, FC_W, FC_H};
    SDL_Texture* frame = NULL;

    if (!scene || scene->harry_cutscene_frame_count <= 0) {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
        return;
    }

    if (scene->harry_cutscene_frame_index < 0) {
        scene->harry_cutscene_frame_index = 0;
    } else if (scene->harry_cutscene_frame_index >= scene->harry_cutscene_frame_count) {
        scene->harry_cutscene_frame_index = scene->harry_cutscene_frame_count - 1;
    }

    frame = scene->harry_cutscene_frames[scene->harry_cutscene_frame_index];
    if (frame) {
        SDL_RenderCopy(renderer, frame, NULL, &dst);
    } else {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
    }
}

static void fc_render_merv_cutscene(FinalCutscene* scene, SDL_Renderer* renderer)
{
    SDL_Rect dst = {0, 0, FC_W, FC_H};
    SDL_Texture* frame = NULL;

    if (!scene || scene->merv_cutscene_frame_count <= 0) {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
        return;
    }

    if (scene->merv_cutscene_frame_index < 0) {
        scene->merv_cutscene_frame_index = 0;
    } else if (scene->merv_cutscene_frame_index >= scene->merv_cutscene_frame_count) {
        scene->merv_cutscene_frame_index = scene->merv_cutscene_frame_count - 1;
    }

    frame = scene->merv_cutscene_frames[scene->merv_cutscene_frame_index];
    if (frame) {
        SDL_RenderCopy(renderer, frame, NULL, &dst);
    } else {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
    }
}

static void fc_render_harry_van_scene(FinalCutscene* scene, SDL_Renderer* renderer)
{
    int van_y = 446;
    SDL_Rect van = {438, van_y, 420, 182};
    SDL_Rect truck = {0, van_y + 32, 318, 124};

    fc_draw_tiled_texture(renderer, scene->harry_forest_bg, scene->chase_scroll * 0.65f,
                          0, 0, 0, 0, (SDL_Color){18, 25, 22, 255});
    fc_draw_overlay(renderer, 0, 0, 0, 42);
    if (scene->harry_van_wobble_started) {
        van_y = 446 + (int)(sinf(scene->harry_van_wobble_phase) * 12.0f);
        van.y = van_y;
        truck.y = van_y + 32;
    }
    if (scene->harry_van) {
        fc_draw_sprite_sheet(renderer, scene->harry_van, scene->frame, van, SDL_FLIP_NONE, 255);
    } else {
        fc_draw_filled(renderer, van, (SDL_Color){190, 190, 200, 255});
    }
    if (scene->harry_van_truck_x >= 0.0f) {
        truck.x = (int)scene->harry_van_truck_x;
        if (scene->harry_truck) {
            SDL_RenderCopy(renderer, scene->harry_truck, NULL, &truck);
        } else {
            fc_draw_filled(renderer, truck, (SDL_Color){90, 90, 98, 255});
        }
    }
}

static void fc_render_escape_scene(FinalCutscene* scene, SDL_Renderer* renderer)
{
    SDL_Rect wreck = {24, 420, 520, 260};
    SDL_Rect kid = {(int)scene->escape_kid_x, 468, 82, 116};

    fc_draw_full_texture(renderer, scene->harry_forest_bg, (SDL_Color){18, 25, 22, 255});
    fc_draw_overlay(renderer, 0, 0, 0, 48);
    if (scene->harry_wreck) {
        SDL_RenderCopy(renderer, scene->harry_wreck, NULL, &wreck);
    } else {
        fc_draw_filled(renderer, wreck, (SDL_Color){70, 70, 76, 255});
    }
    fc_draw_sprite_sheet(renderer,
                         scene->kid_escape_run ? scene->kid_escape_run : scene->kid_run,
                         scene->frame,
                         kid,
                         SDL_FLIP_NONE,
                         255);
}

static void fc_render_story_text(FinalCutscene* scene, SDL_Renderer* renderer, const char* text)
{
    Uint32 elapsed = SDL_GetTicks() - scene->stage_started;
    float overlay_alpha = 255.0f;
    float text_alpha = 0.0f;
    const Uint32 fade_in_ms = 650;
    const Uint32 hold_ms = 1500;
    const Uint32 fade_out_ms = 650;
    const Uint32 start_text_ms = 250;
    const Uint32 end_fade_out_ms = start_text_ms + fade_in_ms + hold_ms + fade_out_ms;

    if (elapsed < start_text_ms) {
        text_alpha = 0.0f;
    } else if (elapsed < start_text_ms + fade_in_ms) {
        text_alpha = fc_clampf((float)(elapsed - start_text_ms) / (float)fade_in_ms, 0.0f, 1.0f) * 255.0f;
    } else if (elapsed < start_text_ms + fade_in_ms + hold_ms) {
        text_alpha = 255.0f;
    } else if (elapsed < end_fade_out_ms) {
        text_alpha = (1.0f - fc_clampf((float)(elapsed - (start_text_ms + fade_in_ms + hold_ms)) / (float)fade_out_ms,
                                       0.0f, 1.0f)) * 255.0f;
    }

    if (elapsed < start_text_ms) {
        overlay_alpha = 255.0f;
    } else {
        overlay_alpha = 255.0f;
    }

    fc_draw_overlay(renderer, 255, 255, 255, (Uint8)overlay_alpha);
    fc_draw_text_center(renderer, scene->big_font, text, FC_W / 2, FC_H / 2 - 28,
                        (SDL_Color){24, 24, 28, (Uint8)text_alpha});
}

static void fc_render_branch_story(FinalCutscene* scene, SDL_Renderer* renderer)
{
    const char* text = "";

    if (scene->selected_choice == 2) {
        if (scene->branch_story_index >= 0 &&
            scene->branch_story_index < (int)(sizeof(kHarryBranchStory) / sizeof(kHarryBranchStory[0]))) {
            text = kHarryBranchStory[scene->branch_story_index];
        }
    } else {
        if (scene->branch_story_index >= 0 &&
            scene->branch_story_index < (int)(sizeof(kMervBranchStory) / sizeof(kMervBranchStory[0]))) {
            text = kMervBranchStory[scene->branch_story_index];
        }
    }
    fc_render_story_text(scene, renderer, text);
}

static void fc_advance_harry_van_dialogue(FinalCutscene* scene)
{
    int total = (int)(sizeof(kHarryVanDialogue) / sizeof(kHarryVanDialogue[0]));
    const FinalDialogueLine* line = NULL;

    if (!scene || scene->harry_van_dialogue_index < 0 || scene->harry_van_dialogue_index >= total) {
        return;
    }

    line = &kHarryVanDialogue[scene->harry_van_dialogue_index];
    if (scene->harry_van_dialogue_chars < (int)strlen(line->text)) {
        scene->harry_van_dialogue_chars = (int)strlen(line->text);
        return;
    }

    scene->harry_van_dialogue_index++;
    scene->harry_van_dialogue_chars = 0;
    scene->harry_van_dialogue_tick = SDL_GetTicks();
    scene->harry_van_dialogue_line_started = SDL_GetTicks();
    if (scene->harry_van_dialogue_index >= total) {
        scene->harry_van_wobble_started = 1;
        scene->harry_van_wobble_phase = 0.0f;
        scene->harry_van_truck_x = -1.0f;
        fc_set_stage(scene, FC_STAGE_HARRY_VAN);
    }
}

static void fc_render_harry_van_dialogue(FinalCutscene* scene, SDL_Renderer* renderer)
{
    const FinalDialogueLine* line = NULL;
    SDL_Rect box = {60, FC_H - 170, FC_W - 120, 140};
    SDL_Rect portrait = {82, FC_H - 146, 76, 104};
    int total = (int)(sizeof(kHarryVanDialogue) / sizeof(kHarryVanDialogue[0]));

    if (!scene || scene->harry_van_dialogue_index < 0 || scene->harry_van_dialogue_index >= total) {
        fc_render_harry_van_scene(scene, renderer);
        return;
    }

    line = &kHarryVanDialogue[scene->harry_van_dialogue_index];
    fc_render_harry_van_scene(scene, renderer);
    fc_draw_filled(renderer, box, (SDL_Color){8, 10, 16, 225});
    SDL_SetRenderDrawColor(renderer, 244, 224, 150, 255);
    SDL_RenderDrawRect(renderer, &box);
    fc_draw_sprite_sheet(renderer, fc_speaker_texture(scene, line->speaker), 0, portrait, SDL_FLIP_NONE, 255);
    fc_draw_text(renderer, scene->font, fc_speaker_name(line->speaker), 82, FC_H - 36,
                 (SDL_Color){244, 224, 150, 255});
    fc_wrap_and_draw(renderer, scene->font, line->text, scene->harry_van_dialogue_chars,
                     185, FC_H - 142, FC_W - 290, (SDL_Color){255, 255, 255, 255});
    fc_draw_text(renderer, scene->font, "[ Space / Enter ]", FC_W - 270, FC_H - 58,
                 (SDL_Color){190, 190, 200, 220});
}

static void fc_render_credits(SDL_Renderer* renderer, TTF_Font* font, int y_offset)
{
    int y = y_offset;
    SDL_Color color = {242, 236, 220, 255};

    fc_draw_text_center(renderer, font, "CREDITS", FC_W / 2, y, color);
    y += 64;
    for (int i = 0; i < (int)(sizeof(kTempCredits) / sizeof(kTempCredits[0])); ++i) {
        if (kTempCredits[i][0] == '\0') {
            y += 26;
            continue;
        }
        fc_draw_text_center(renderer, font, kTempCredits[i], FC_W / 2, y, color);
        y += 38;
    }
}

static const char* fc_speaker_name(FinalSpeaker speaker)
{
    return speaker == FC_SPEAKER_HARRY ? "Harry" : "Marv";
}

static SDL_Texture* fc_speaker_texture(FinalCutscene* scene, FinalSpeaker speaker)
{
    return speaker == FC_SPEAKER_HARRY ? scene->harry_idle : scene->marv_idle;
}

static void fc_wrap_and_draw(SDL_Renderer* renderer,
                             TTF_Font* font,
                             const char* text,
                             int visible_chars,
                             int x,
                             int y,
                             int max_w,
                             SDL_Color color)
{
    char visible[512];
    char line[512] = "";
    char test[512];
    const char* p = NULL;
    int drawn = 0;
    int line_y = y;

    if (!text || !font) return;
    snprintf(visible, sizeof(visible), "%.*s", visible_chars, text);
    p = visible;
    while (*p) {
        char word[96];
        int wi = 0;
        int w = 0;
        int h = 0;

        while (*p == ' ') p++;
        while (*p && *p != ' ' && wi + 1 < (int)sizeof(word)) {
            word[wi++] = *p++;
        }
        word[wi] = '\0';
        if (!word[0]) break;
        snprintf(test, sizeof(test), "%s%s%s", line, line[0] ? " " : "", word);
        if (TTF_SizeUTF8(font, test, &w, &h) == 0 && w > max_w && line[0]) {
            fc_draw_text(renderer, font, line, x, line_y, color);
            line_y += 28;
            drawn++;
            snprintf(line, sizeof(line), "%s", word);
            if (drawn >= 4) break;
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
    }
    if (line[0] && drawn < 5) {
        fc_draw_text(renderer, font, line, x, line_y, color);
    }
}

static void fc_basement_rects(SDL_Rect* out_kid,
                              SDL_Rect* out_marv_dialogue,
                              SDL_Rect* out_marv_decision,
                              SDL_Rect* out_harry)
{
    if (out_kid) *out_kid = (SDL_Rect){105, 300, 175, 250};
    if (out_marv_dialogue) *out_marv_dialogue = (SDL_Rect){760, 355, 135, 194};
    if (out_marv_decision) *out_marv_decision = (SDL_Rect){560, 355, 135, 194};
    if (out_harry) *out_harry = (SDL_Rect){1000, 362, 135, 194};
}

static void fc_render_base_scene_with_marv(FinalCutscene* scene,
                                           SDL_Renderer* renderer,
                                           int decision_mode,
                                           SDL_Rect marv)
{
    SDL_Rect kid;
    SDL_Rect harry;

    fc_basement_rects(&kid, NULL, NULL, &harry);

    fc_draw_full_texture(renderer, scene->base_bg, (SDL_Color){42, 35, 38, 255});
    fc_draw_overlay(renderer, 0, 0, 0, decision_mode ? 125 : 35);
    if (scene->kid_trapped) {
        SDL_SetTextureBlendMode(scene->kid_trapped, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(scene->kid_trapped, decision_mode ? 42 : 255);
        fc_draw_sprite_sheet(renderer, scene->kid_trapped, scene->frame, kid, SDL_FLIP_NONE,
                             decision_mode ? 42 : 255);
        SDL_SetTextureAlphaMod(scene->kid_trapped, 255);
    } else {
        fc_draw_filled(renderer, kid, (SDL_Color){110, 120, 150, decision_mode ? 42 : 220});
    }
    fc_draw_sprite_sheet(renderer, scene->marv_idle, scene->frame, marv, SDL_FLIP_NONE, 255);
    fc_draw_sprite_sheet(renderer, scene->harry_idle, scene->frame, harry, SDL_FLIP_NONE, 255);
}

static void fc_render_base_scene(FinalCutscene* scene, SDL_Renderer* renderer, int decision_mode)
{
    SDL_Rect marv_dialogue;
    SDL_Rect marv_decision;

    fc_basement_rects(NULL, &marv_dialogue, &marv_decision, NULL);
    fc_render_base_scene_with_marv(scene,
                                   renderer,
                                   decision_mode,
                                   decision_mode ? marv_decision : marv_dialogue);
}

static void fc_render_dialogue(FinalCutscene* scene, SDL_Renderer* renderer)
{
    const FinalDialogueLine* line = &kFinalDialogue[scene->dialogue_index];
    SDL_Rect box = {60, FC_H - 170, FC_W - 120, 140};
    SDL_Rect portrait = {82, FC_H - 146, 76, 104};

    fc_render_base_scene(scene, renderer, 0);
    fc_draw_filled(renderer, box, (SDL_Color){8, 10, 16, 225});
    SDL_SetRenderDrawColor(renderer, 244, 224, 150, 255);
    SDL_RenderDrawRect(renderer, &box);
    fc_draw_sprite_sheet(renderer, fc_speaker_texture(scene, line->speaker), 0, portrait, SDL_FLIP_NONE, 255);
    fc_draw_text(renderer, scene->font, fc_speaker_name(line->speaker), 82, FC_H - 36,
                 (SDL_Color){244, 224, 150, 255});
    fc_wrap_and_draw(renderer, scene->font, line->text, scene->dialogue_chars,
                     185, FC_H - 142, FC_W - 290, (SDL_Color){255, 255, 255, 255});
    fc_draw_text(renderer, scene->font, "[ Space / Enter ]", FC_W - 270, FC_H - 58,
                 (SDL_Color){190, 190, 200, 220});
}

static void fc_render_chase_dialogue(FinalCutscene* scene, SDL_Renderer* renderer)
{
    const FinalDialogueLine* line = &kChaseDialogue[scene->chase_dialogue_index];
    SDL_Rect box = {60, FC_H - 300, FC_W - 120, 128};
    SDL_Rect portrait = {82, FC_H - 295, 70, 92};

    fc_render_chase(scene, renderer);
    fc_draw_filled(renderer, box, (SDL_Color){8, 10, 16, 225});
    SDL_SetRenderDrawColor(renderer, 244, 224, 150, 255);
    SDL_RenderDrawRect(renderer, &box);
    fc_draw_sprite_sheet(renderer, fc_speaker_texture(scene, line->speaker), 0, portrait, SDL_FLIP_NONE, 255);
    fc_draw_text(renderer, scene->font, fc_speaker_name(line->speaker), 82, FC_H - 195,
                 (SDL_Color){244, 224, 150, 255});
    fc_wrap_and_draw(renderer, scene->font, line->text, scene->chase_dialogue_chars,
                     180, FC_H - 272, FC_W - 285, (SDL_Color){255, 255, 255, 255});
    fc_draw_text(renderer, scene->font, "[ Space / Enter ]", FC_W - 410, FC_H - 198,
                 (SDL_Color){190, 190, 200, 220});
}

static void fc_choice_rects(SDL_Rect* out_marv, SDL_Rect* out_harry)
{
    fc_basement_rects(NULL, NULL, out_marv, out_harry);
}

static int fc_point_in_rect(int x, int y, SDL_Rect r)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void fc_render_decision(FinalCutscene* scene, SDL_Renderer* renderer)
{
    SDL_Rect marv;
    SDL_Rect harry;
    SDL_Rect focus;
    const char* info = NULL;

    fc_choice_rects(&marv, &harry);
    fc_render_base_scene(scene, renderer, 1);
    fc_draw_text_center(renderer, scene->big_font, "choose your decision", FC_W / 2, 64,
                        (SDL_Color){255, 238, 184, 255});

    if (scene->hovered_choice == 1) {
        focus = marv;
        info = "kill the kid";
    } else if (scene->hovered_choice == 2) {
        focus = harry;
        info = "bail the kid";
    } else {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 255, 238, 184, 255);
    SDL_RenderDrawRect(renderer, &focus);
    if (info) {
        SDL_Rect info_box = {focus.x - 20, focus.y - 58, 190, 40};
        fc_draw_filled(renderer, info_box, (SDL_Color){8, 10, 16, 230});
        SDL_RenderDrawRect(renderer, &info_box);
        fc_draw_text_center(renderer, scene->font, info, info_box.x + info_box.w / 2, info_box.y + 9,
                            (SDL_Color){255, 255, 255, 255});
    }
}

static void fc_render_decision_slide(FinalCutscene* scene, SDL_Renderer* renderer)
{
    SDL_Rect marv_dialogue;
    SDL_Rect marv_decision;
    SDL_Rect marv;
    Uint32 elapsed = SDL_GetTicks() - scene->stage_started;
    float t = fc_clampf((float)elapsed / (float)FC_DECISION_SLIDE_MS, 0.0f, 1.0f);

    fc_basement_rects(NULL, &marv_dialogue, &marv_decision, NULL);
    marv = fc_lerp_rect(marv_dialogue, marv_decision, t);
    fc_render_base_scene_with_marv(scene, renderer, 1, marv);
}

static void fc_save_decision(int choice)
{
    char path[PATH_MAX];
    char* slash = NULL;
    FILE* f = NULL;

    if (!ui_resolve_asset_path(FC_DECISION_PATH, path, sizeof(path))) return;
    slash = strrchr(path, '/');
    if (slash) {
        *slash = '\0';
        mkdir(path, 0777);
        *slash = '/';
    }
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%d\n", choice);
    fclose(f);
}

static void fc_handle_dialogue_advance(FinalCutscene* scene)
{
    const FinalDialogueLine* line = &kFinalDialogue[scene->dialogue_index];
    int total = (int)strlen(line->text);

    if (scene->dialogue_chars < total) {
        scene->dialogue_chars = total;
        return;
    }
    scene->dialogue_index++;
    scene->dialogue_chars = 0;
    scene->dialogue_tick = SDL_GetTicks();
    if (scene->dialogue_index >= (int)(sizeof(kFinalDialogue) / sizeof(kFinalDialogue[0]))) {
        fc_set_stage(scene, FC_STAGE_DECISION_SLIDE);
    }
}

static void fc_handle_chase_dialogue_advance(FinalCutscene* scene)
{
    const FinalDialogueLine* line = &kChaseDialogue[scene->chase_dialogue_index];
    int total = (int)strlen(line->text);

    if (scene->chase_dialogue_chars < total) {
        scene->chase_dialogue_chars = total;
        return;
    }
    scene->chase_dialogue_index++;
    scene->chase_dialogue_chars = 0;
    scene->chase_dialogue_tick = SDL_GetTicks();
    if (scene->chase_dialogue_index >= (int)(sizeof(kChaseDialogue) / sizeof(kChaseDialogue[0]))) {
        scene->chase_prompt_visible = 1;
        fc_set_stage(scene, FC_STAGE_CHASE);
    }
}

static void fc_handle_event(FinalCutscene* scene, SDL_Event* event)
{
    SDL_Rect marv;
    SDL_Rect harry;

    if (!scene || !event) return;
    if (event->type == SDL_QUIT) {
        scene->running = 0;
        return;
    }
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            scene->running = 0;
            return;
        }
        if (fc_is_skip_to_basement_key(&event->key)) {
            fc_skip_to_basement(scene);
            return;
        }
        if (event->key.keysym.sym == SDLK_m &&
            (event->key.keysym.mod & KMOD_SHIFT) &&
            !event->key.repeat) {
            scene->music_muted = !scene->music_muted;
            if (scene->music_muted) {
                Mix_PauseMusic();
            } else {
                Mix_ResumeMusic();
            }
            return;
        }
        if (scene->stage == FC_STAGE_CHASE_DIALOGUE &&
            (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_SPACE)) {
            fc_handle_chase_dialogue_advance(scene);
        } else if (scene->stage == FC_STAGE_CHASE && event->key.keysym.sym == SDLK_RETURN && !event->key.repeat) {
            scene->catch_meter = fc_clampf(scene->catch_meter + 0.018f, 0.0f, 1.0f);
            scene->last_chase_press_tick = SDL_GetTicks();
            if (scene->catch_meter >= 1.0f) fc_set_stage(scene, FC_STAGE_WHITE_FADE);
        } else if (scene->stage == FC_STAGE_HARRY_VAN_DIALOGUE &&
                   (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_SPACE)) {
            fc_advance_harry_van_dialogue(scene);
        } else if (scene->stage == FC_STAGE_DIALOGUE &&
                   (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_SPACE)) {
            fc_handle_dialogue_advance(scene);
        }
    } else if (event->type == SDL_MOUSEMOTION && scene->stage == FC_STAGE_DECISION) {
        fc_choice_rects(&marv, &harry);
        if (fc_point_in_rect(event->motion.x, event->motion.y, marv)) scene->hovered_choice = 1;
        else if (fc_point_in_rect(event->motion.x, event->motion.y, harry)) scene->hovered_choice = 2;
        else scene->hovered_choice = 0;
    } else if (event->type == SDL_MOUSEBUTTONDOWN && scene->stage == FC_STAGE_DECISION &&
               event->button.button == SDL_BUTTON_LEFT) {
        fc_choice_rects(&marv, &harry);
        if (fc_point_in_rect(event->button.x, event->button.y, marv)) scene->selected_choice = 1;
        else if (fc_point_in_rect(event->button.x, event->button.y, harry)) scene->selected_choice = 2;
        if (scene->selected_choice) {
            fc_save_decision(scene->selected_choice);
            if (scene->selected_choice == 2) {
                scene->harry_cutscene_requested = 1;
            } else if (scene->selected_choice == 1) {
                scene->merv_cutscene_requested = 1;
            }
            fc_set_stage(scene, FC_STAGE_BLACK_FADE);
        }
    }
}

static void fc_update(FinalCutscene* scene)
{
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - scene->stage_started;

    fc_advance_frame(scene);
    if (scene->stage == FC_STAGE_CHASE) {
        scene->chase_scroll += 7.5f;
        if (scene->chase_scroll > 100000.0f) scene->chase_scroll = 0.0f;
        if (scene->last_chase_press_tick != 0 &&
            now - scene->last_chase_press_tick > 320 &&
            scene->catch_meter > 0.0f) {
            scene->catch_meter = fc_clampf(scene->catch_meter - 0.006f, 0.0f, 1.0f);
        }
    } else if (scene->stage == FC_STAGE_CHASE_DIALOGUE) {
        const FinalDialogueLine* line = &kChaseDialogue[scene->chase_dialogue_index];
        int total = (int)strlen(line->text);
        scene->chase_scroll += 7.5f;
        if (scene->chase_scroll > 100000.0f) scene->chase_scroll = 0.0f;
        if (scene->chase_dialogue_chars < total && now - scene->chase_dialogue_tick >= 24) {
            scene->chase_dialogue_chars++;
            scene->chase_dialogue_tick = now;
        }
    } else if (scene->stage == FC_STAGE_DIALOGUE) {
        const FinalDialogueLine* line = &kFinalDialogue[scene->dialogue_index];
        int total = (int)strlen(line->text);
        if (scene->dialogue_chars < total && now - scene->dialogue_tick >= 24) {
            scene->dialogue_chars++;
            scene->dialogue_tick = now;
        }
    }

    if (scene->stage == FC_STAGE_FADE_OUT_LEVEL && elapsed >= 850) {
        fc_set_stage(scene, FC_STAGE_CHASE_FADE_IN);
    } else if (scene->stage == FC_STAGE_CHASE_FADE_IN && elapsed >= 850) {
        scene->chase_dialogue_index = 0;
        scene->chase_dialogue_chars = 0;
        scene->chase_dialogue_tick = now;
        scene->chase_prompt_visible = 0;
        scene->last_chase_press_tick = 0;
        fc_set_stage(scene, FC_STAGE_CHASE_DIALOGUE);
    } else if (scene->stage == FC_STAGE_WHITE_FADE && elapsed >= 1200) {
        fc_set_stage(scene, FC_STAGE_STORY_1);
    } else if (scene->stage == FC_STAGE_STORY_1 && elapsed >= 3600) {
        fc_set_stage(scene, FC_STAGE_STORY_2);
    } else if (scene->stage == FC_STAGE_STORY_2 && elapsed >= 3900) {
        fc_set_stage(scene, FC_STAGE_BASE_FADE_IN);
    } else if (scene->stage == FC_STAGE_BASE_FADE_IN && elapsed >= 1000) {
        scene->dialogue_index = 0;
        scene->dialogue_chars = 0;
        scene->dialogue_tick = now;
        fc_set_stage(scene, FC_STAGE_DIALOGUE);
    } else if (scene->stage == FC_STAGE_DECISION_SLIDE && elapsed >= FC_DECISION_SLIDE_MS) {
        fc_set_stage(scene, FC_STAGE_DECISION);
    } else if (scene->stage == FC_STAGE_BLACK_FADE && elapsed >= 900) {
        if (scene->harry_cutscene_requested) {
            scene->harry_cutscene_frame_index = 0;
            scene->harry_cutscene_frame_tick = now;
            scene->harry_cutscene_music_started = 0;
            fc_set_stage(scene, FC_STAGE_HARRY_CUTSCENE_FADE_IN);
        } else if (scene->merv_cutscene_requested) {
            scene->merv_cutscene_frame_index = 0;
            scene->merv_cutscene_frame_tick = now;
            scene->merv_cutscene_music_started = 0;
            fc_set_stage(scene, FC_STAGE_MERV_CUTSCENE_FADE_IN);
        } else {
            fc_set_stage(scene, FC_STAGE_DONE);
            scene->running = 0;
        }
    } else if (scene->stage == FC_STAGE_HARRY_CUTSCENE_FADE_IN && elapsed >= 850) {
        if (!scene->harry_cutscene_music_started && scene->harry_cutscene_music) {
            if (Mix_PlayMusic(scene->harry_cutscene_music, -1) == 0) {
                scene->harry_cutscene_music_started = 1;
            } else {
                SDL_Log("Could not play Harry cutscene music: %s", Mix_GetError());
            }
        }
        fc_set_stage(scene, FC_STAGE_HARRY_CUTSCENE);
    } else if (scene->stage == FC_STAGE_HARRY_CUTSCENE) {
        if (scene->harry_cutscene_frame_count > 0 &&
            now - scene->harry_cutscene_frame_tick >= 42) {
            if (scene->harry_cutscene_frame_index < scene->harry_cutscene_frame_count - 1) {
                scene->harry_cutscene_frame_index++;
                scene->harry_cutscene_frame_tick = now;
            } else if (elapsed >= 2500) {
                Mix_HaltMusic();
                fc_start_branch_story(scene);
            }
        }
    } else if (scene->stage == FC_STAGE_MERV_CUTSCENE_FADE_IN && elapsed >= 850) {
        if (!scene->merv_cutscene_music_started && scene->merv_cutscene_music) {
            if (Mix_PlayMusic(scene->merv_cutscene_music, -1) == 0) {
                scene->merv_cutscene_music_started = 1;
            } else {
                SDL_Log("Could not play Merv cutscene music: %s", Mix_GetError());
            }
        }
        fc_set_stage(scene, FC_STAGE_MERV_CUTSCENE);
    } else if (scene->stage == FC_STAGE_MERV_CUTSCENE) {
        if (scene->merv_cutscene_frame_count > 0 &&
            now - scene->merv_cutscene_frame_tick >= 42) {
            if (scene->merv_cutscene_frame_index < scene->merv_cutscene_frame_count - 1) {
                scene->merv_cutscene_frame_index++;
                scene->merv_cutscene_frame_tick = now;
            } else if (elapsed >= 2500) {
                Mix_HaltMusic();
                fc_start_branch_story(scene);
            }
        }
    } else if (scene->stage == FC_STAGE_BRANCH_STORY) {
        int limit = (scene->selected_choice == 2)
                        ? (int)(sizeof(kHarryBranchStory) / sizeof(kHarryBranchStory[0]))
                        : (int)(sizeof(kMervBranchStory) / sizeof(kMervBranchStory[0]));
        if (elapsed >= 3400) {
            if (scene->branch_story_index + 1 < limit) {
                scene->branch_story_index++;
                fc_set_stage(scene, FC_STAGE_BRANCH_STORY);
            } else {
                if (scene->selected_choice == 2) {
                    scene->chase_scroll = 0.0f;
                    fc_set_stage(scene, FC_STAGE_HARRY_VAN_FADE_IN);
                } else {
                    scene->gunshot_played = 0;
                    fc_set_stage(scene, FC_STAGE_GUNSHOT_HOLD);
                }
            }
        }
    } else if (scene->stage == FC_STAGE_HARRY_VAN_FADE_IN && elapsed >= 850) {
        scene->harry_van_dialogue_index = 0;
        scene->harry_van_dialogue_chars = 0;
        scene->harry_van_dialogue_tick = now;
        scene->harry_van_dialogue_line_started = now;
        scene->harry_van_wobble_started = 0;
        scene->harry_van_wobble_phase = 0.0f;
        scene->harry_van_truck_x = -1.0f;
        scene->chase_scroll = 0.0f;
        fc_set_stage(scene, FC_STAGE_HARRY_VAN_DIALOGUE);
    } else if (scene->stage == FC_STAGE_HARRY_VAN_DIALOGUE) {
        const FinalDialogueLine* line = &kHarryVanDialogue[scene->harry_van_dialogue_index];
        int total = (int)strlen(line->text);
        scene->chase_scroll += 8.5f;
        if (scene->chase_scroll > 100000.0f) scene->chase_scroll = 0.0f;
        if (scene->harry_van_dialogue_chars < total && now - scene->harry_van_dialogue_tick >= 24) {
            scene->harry_van_dialogue_chars++;
            scene->harry_van_dialogue_tick = now;
        }
    } else if (scene->stage == FC_STAGE_HARRY_VAN) {
        scene->harry_van_wobble_started = 1;
        scene->harry_van_wobble_phase += 0.22f;
        scene->chase_scroll += 8.5f;
        if (scene->chase_scroll > 100000.0f) scene->chase_scroll = 0.0f;
        if (elapsed >= 3000) {
            scene->harry_van_truck_x = (float)(FC_W + 120);
            fc_set_stage(scene, FC_STAGE_HARRY_VAN_TRUCK);
        }
    } else if (scene->stage == FC_STAGE_HARRY_VAN_TRUCK) {
        int van_y = 438 + (int)(sinf(scene->harry_van_wobble_phase) * 14.0f);
        SDL_Rect van = {398, van_y, 524, 228};
        SDL_Rect truck = {(int)scene->harry_van_truck_x, van_y + 30, 360, 140};

        scene->harry_van_wobble_phase += 0.22f;
        scene->harry_van_truck_x -= 26.0f;
        if (scene->harry_van_truck_x < -500.0f) {
            scene->escape_kid_x = 490.0f;
            fc_set_stage(scene, FC_STAGE_CRASH_BLACK);
        } else if (SDL_HasIntersection(&van, &truck)) {
            scene->escape_kid_x = 490.0f;
            fc_set_stage(scene, FC_STAGE_CRASH_BLACK);
        }
    } else if (scene->stage == FC_STAGE_HARRY_VAN_FADE_OUT && elapsed >= 900) {
        fc_set_stage(scene, FC_STAGE_BLACK_FADE);
    } else if (scene->stage == FC_STAGE_CRASH_BLACK && elapsed >= 900) {
        scene->escape_kid_x = 490.0f;
        fc_set_stage(scene, FC_STAGE_ESCAPE_FADE_IN);
    } else if (scene->stage == FC_STAGE_ESCAPE_FADE_IN && elapsed >= 900) {
        fc_set_stage(scene, FC_STAGE_ESCAPE_RUN);
    } else if (scene->stage == FC_STAGE_ESCAPE_RUN) {
        scene->escape_kid_x += 6.8f;
        if (scene->escape_kid_x > FC_W + 100.0f) {
            fc_set_stage(scene, FC_STAGE_ESCAPE_FADE_OUT);
        }
    } else if (scene->stage == FC_STAGE_ESCAPE_FADE_OUT && elapsed >= 900) {
        fc_set_stage(scene, FC_STAGE_CREDITS);
    } else if (scene->stage == FC_STAGE_GUNSHOT_HOLD) {
        if (!scene->gunshot_played) {
            if (scene->gunshot_sfx) {
                Mix_PlayChannel(-1, scene->gunshot_sfx, 0);
            }
            scene->gunshot_played = 1;
        }
        if (elapsed >= 2300) {
            fc_set_stage(scene, FC_STAGE_CREDITS_FADE_OUT);
        }
    } else if (scene->stage == FC_STAGE_CREDITS_FADE_OUT && elapsed >= 900) {
        fc_set_stage(scene, FC_STAGE_CREDITS);
    } else if (scene->stage == FC_STAGE_CREDITS && elapsed >= 12000) {
        fc_set_stage(scene, FC_STAGE_DONE);
        scene->running = 0;
    }
}

static void fc_render(FinalCutscene* scene, SDL_Renderer* renderer)
{
    Uint32 elapsed = SDL_GetTicks() - scene->stage_started;
    float t = 0.0f;

    if (scene->stage == FC_STAGE_FADE_OUT_LEVEL) {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
        t = fc_clampf((float)elapsed / 850.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_CHASE_FADE_IN) {
        fc_render_chase(scene, renderer);
        t = 1.0f - fc_clampf((float)elapsed / 850.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_CHASE_DIALOGUE) {
        fc_render_chase_dialogue(scene, renderer);
    } else if (scene->stage == FC_STAGE_CHASE) {
        fc_render_chase(scene, renderer);
    } else if (scene->stage == FC_STAGE_WHITE_FADE) {
        fc_render_chase(scene, renderer);
        t = fc_clampf((float)elapsed / 1200.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 255, 255, 255, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_STORY_1) {
        fc_render_story_text(scene, renderer, "keving finally got caught u");
    } else if (scene->stage == FC_STAGE_STORY_2) {
        fc_render_story_text(scene, renderer, "now the thieves took him to theitr basement to finish him");
    } else if (scene->stage == FC_STAGE_BASE_FADE_IN) {
        fc_render_base_scene(scene, renderer, 0);
        t = 1.0f - fc_clampf((float)elapsed / 1000.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 255, 255, 255, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_DIALOGUE) {
        fc_render_dialogue(scene, renderer);
    } else if (scene->stage == FC_STAGE_DECISION_SLIDE) {
        fc_render_decision_slide(scene, renderer);
    } else if (scene->stage == FC_STAGE_DECISION) {
        fc_render_decision(scene, renderer);
    } else if (scene->stage == FC_STAGE_BLACK_FADE) {
        fc_render_decision(scene, renderer);
        t = fc_clampf((float)elapsed / 900.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_HARRY_CUTSCENE_FADE_IN) {
        fc_render_harry_cutscene(scene, renderer);
        t = 1.0f - fc_clampf((float)elapsed / 850.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_HARRY_CUTSCENE) {
        fc_render_harry_cutscene(scene, renderer);
    } else if (scene->stage == FC_STAGE_MERV_CUTSCENE_FADE_IN) {
        fc_render_merv_cutscene(scene, renderer);
        t = 1.0f - fc_clampf((float)elapsed / 850.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_MERV_CUTSCENE) {
        fc_render_merv_cutscene(scene, renderer);
    } else if (scene->stage == FC_STAGE_BRANCH_STORY) {
        fc_render_branch_story(scene, renderer);
    } else if (scene->stage == FC_STAGE_HARRY_VAN_FADE_IN) {
        fc_render_harry_van_scene(scene, renderer);
        t = 1.0f - fc_clampf((float)elapsed / 850.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_HARRY_VAN_DIALOGUE) {
        fc_render_harry_van_dialogue(scene, renderer);
    } else if (scene->stage == FC_STAGE_HARRY_VAN) {
        fc_render_harry_van_scene(scene, renderer);
    } else if (scene->stage == FC_STAGE_HARRY_VAN_TRUCK) {
        fc_render_harry_van_scene(scene, renderer);
        if (scene->harry_van_truck_x >= 0.0f) {
            float t_truck = fc_clampf(1.0f - (scene->harry_van_truck_x / (float)(FC_W + 120)), 0.0f, 1.0f);
            fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(60.0f * t_truck));
        }
    } else if (scene->stage == FC_STAGE_CRASH_BLACK) {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
    } else if (scene->stage == FC_STAGE_ESCAPE_FADE_IN) {
        fc_render_escape_scene(scene, renderer);
        t = 1.0f - fc_clampf((float)elapsed / 900.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_ESCAPE_RUN) {
        fc_render_escape_scene(scene, renderer);
    } else if (scene->stage == FC_STAGE_ESCAPE_FADE_OUT) {
        fc_render_escape_scene(scene, renderer);
        t = fc_clampf((float)elapsed / 900.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_GUNSHOT_HOLD) {
        fc_draw_overlay(renderer, 255, 255, 255, 255);
    } else if (scene->stage == FC_STAGE_CREDITS_FADE_OUT) {
        fc_draw_overlay(renderer, 255, 255, 255, 255);
        t = fc_clampf((float)elapsed / 900.0f, 0.0f, 1.0f);
        fc_draw_overlay(renderer, 0, 0, 0, (Uint8)(255.0f * t));
    } else if (scene->stage == FC_STAGE_CREDITS) {
        fc_draw_overlay(renderer, 0, 0, 0, 255);
        fc_render_credits(renderer, scene->big_font, FC_H - (int)((float)elapsed * 0.08f));
    }
}

static void fc_cleanup(FinalCutscene* scene)
{
    if (!scene) return;
    if (scene->chase_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(scene->chase_music);
    }
    if (scene->chase_bg) SDL_DestroyTexture(scene->chase_bg);
    if (scene->base_bg) SDL_DestroyTexture(scene->base_bg);
    if (scene->kid_trapped) SDL_DestroyTexture(scene->kid_trapped);
    if (scene->harry_forest_bg) SDL_DestroyTexture(scene->harry_forest_bg);
    if (scene->harry_van) SDL_DestroyTexture(scene->harry_van);
    if (scene->harry_truck) SDL_DestroyTexture(scene->harry_truck);
    if (scene->harry_wreck) SDL_DestroyTexture(scene->harry_wreck);
    if (scene->kid_escape_run) SDL_DestroyTexture(scene->kid_escape_run);
    if (scene->kid_run) SDL_DestroyTexture(scene->kid_run);
    if (scene->marv_run) SDL_DestroyTexture(scene->marv_run);
    if (scene->harry_run) SDL_DestroyTexture(scene->harry_run);
    if (scene->marv_idle) SDL_DestroyTexture(scene->marv_idle);
    if (scene->harry_idle) SDL_DestroyTexture(scene->harry_idle);
    fc_free_harry_cutscene_frames(scene);
    fc_free_merv_cutscene_frames(scene);
    if (scene->harry_cutscene_music) {
        Mix_FreeMusic(scene->harry_cutscene_music);
    }
    if (scene->merv_cutscene_music) {
        Mix_FreeMusic(scene->merv_cutscene_music);
    }
    if (scene->gunshot_sfx) {
        Mix_FreeChunk(scene->gunshot_sfx);
    }
    if (scene->font) TTF_CloseFont(scene->font);
    if (scene->big_font) TTF_CloseFont(scene->big_font);
}

static int final_cutscene_run_internal(SDL_Window* window, SDL_Renderer* renderer, int start_at_chase)
{
    FinalCutscene scene;
    SDL_Event event;
    Uint32 frame_start = 0;
    const char* const font_candidates[] = {
        "assets/main_menu/fonts/text.ttf",
        "assets/main_menu/fonts/pixel_operator.ttf",
        "assets/enigme/assets/font.ttf",
        "lvls/level2-chase/font.ttf"
    };

    if (!window || !renderer) return 0;
    memset(&scene, 0, sizeof(scene));
    scene.running = 1;
    scene.stage = start_at_chase ? FC_STAGE_CHASE_DIALOGUE : FC_STAGE_FADE_OUT_LEVEL;
    scene.stage_started = SDL_GetTicks();
    scene.frame_tick = scene.stage_started;
    scene.chase_dialogue_tick = scene.stage_started;

    SDL_SetWindowTitle(window, "Home Alone - Finale");
    SDL_RenderSetLogicalSize(renderer, FC_W, FC_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_ENABLE);

    scene.font = ui_open_font_from_candidates(font_candidates, 4, 22, 1);
    scene.big_font = ui_open_font_from_candidates(font_candidates, 4, 42, 1);
    if (!scene.font) scene.font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 22);
    if (!scene.big_font) scene.big_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 42);

    scene.chase_bg = fc_load_texture(renderer, "assets/chase.png");
    scene.base_bg = fc_load_texture(renderer, "assets/base.png");
    scene.kid_trapped = fc_load_texture(renderer, "assets/kid_trapped.png");
    scene.harry_forest_bg = fc_load_texture(renderer, "assets/final_cutscenes/harry/forest.png");
    scene.chase_music = ui_load_music("assets/chase.wav");
    if (scene.chase_music) {
        if (Mix_PlayMusic(scene.chase_music, -1) == 0) {
            if (Mix_SetMusicPosition(12.0) != 0) {
                SDL_Log("Could not seek assets/chase.wav to 12s: %s", Mix_GetError());
            }
        } else {
            SDL_Log("Could not play assets/chase.wav: %s", Mix_GetError());
        }
    }
    scene.kid_run = fc_load_texture(renderer, "lvls/level2-chase/assets/animations/kid_idle.png");
    scene.marv_run = fc_load_texture(renderer, "lvls/level2-chase/assets/animations/marv_run.png");
    scene.harry_run = fc_load_texture(renderer, "lvls/level2-chase/assets/animations/harry_run.png");
    scene.marv_idle = fc_load_texture(renderer, "lvls/level1-climb/assets/animations/skin1_idle.png");
    scene.harry_idle = fc_load_texture(renderer, "lvls/level1-climb/assets/animations/harry_idle.png");
    scene.harry_cutscene_music = ui_load_music("assets/final_cutscenes/harry/Can-can-song.mp3");
    fc_load_harry_cutscene_frames(&scene, renderer);
    scene.merv_cutscene_music = ui_load_music("assets/final_cutscenes/merv/Can-can-song.mp3");
    fc_load_merv_cutscene_frames(&scene, renderer);
    scene.gunshot_sfx = ui_load_wav(FC_GUNSHOT_PATH);
    scene.harry_van = fc_load_texture(renderer, "assets/final_cutscenes/harry/van.png");
    scene.harry_truck = fc_load_texture(renderer, "assets/final_cutscenes/harry/truck.png");
    scene.harry_wreck = fc_load_texture(renderer, "assets/final_cutscenes/harry/wreck.png");
    scene.kid_escape_run = fc_load_texture(renderer, "assets/main_menu/sprites/kid.png");

    while (scene.running) {
        frame_start = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            fc_handle_event(&scene, &event);
        }
        fc_update(&scene);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        fc_render(&scene, renderer);
        SDL_RenderPresent(renderer);
        {
            Uint32 spent = SDL_GetTicks() - frame_start;
            if (spent < 16) SDL_Delay(16 - spent);
        }
    }

    fc_cleanup(&scene);
    return scene.stage == FC_STAGE_DONE || scene.selected_choice != 0;
}

int final_cutscene_run(SDL_Window* window, SDL_Renderer* renderer)
{
    return final_cutscene_run_internal(window, renderer, 0);
}

int final_cutscene_run_from_chase(SDL_Window* window, SDL_Renderer* renderer)
{
    return final_cutscene_run_internal(window, renderer, 1);
}
