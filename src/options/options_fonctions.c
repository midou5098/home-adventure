#include "options_internal.h"
#include "asset_paths.h"
#include "game_progress.h"
#include "ui_shared.h"
#include <limits.h>

void render_main_menu_style_button(SDL_Renderer* renderer,
                                   TTF_Font* font,
                                   const SDL_Rect* dest,
                                   const char* label,
                                   int selected,
                                   int pressed);

#define BRICK_HEIGHT 100
#define BRICK_Y 280
#define SLIDER_COUNT 4
#define SLIDER_BAR_COUNT 10
#define SLIDER_BAR_STEP 22
#define SLIDER_BAR_W 35
#define SLIDER_BAR_H 50
#define SLIDER_ICON_W 60
#define SLIDER_ICON_H 60
#define SLIDER_ICON_OFFSET_X (10 + (SLIDER_BAR_COUNT * SLIDER_BAR_STEP) + 16)
#define SLIDER_HIT_EXTRA_W 90
#define SLIDER_VISUAL_W 306

Snowflake snow[MAX_SNOW];
SDL_Texture* snowTex = NULL;
SDL_Texture* upTex = NULL;
static TTF_Font* g_options_button_font = NULL;

typedef struct {
    SDL_Rect rect;
    int active;
    int selected;
    float scale;
    float target_scale;
    float bounce_offset;
    float pressed_timer;
} OptionsButtonVisual;

static OptionsButtonVisual g_action_button_visuals[MENU_ITEMS];

static void destroy_texture(SDL_Texture** texture)
{
    if (texture && *texture) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }
}

static void destroy_texture_array(SDL_Texture** textures, int count)
{
    if (!textures || count <= 0) return;
    for (int i = 0; i < count; ++i) {
        destroy_texture(&textures[i]);
    }
}

static void destroy_chunk(Mix_Chunk** chunk)
{
    if (chunk && *chunk) {
        Mix_FreeChunk(*chunk);
        *chunk = NULL;
    }
}

static void GetRenderSize(int* outW, int* outH) {
    int w = SCREEN_WIDTH;
    int h = SCREEN_HEIGHT;

    if (renderer) {
        int logicalW = 0;
        int logicalH = 0;
        SDL_RenderGetLogicalSize(renderer, &logicalW, &logicalH);

        if (logicalW > 0 && logicalH > 0) {
            w = logicalW;
            h = logicalH;
        } else if (SDL_GetRendererOutputSize(renderer, &w, &h) != 0 && window) {
            SDL_GetWindowSize(window, &w, &h);
        }
    } else if (window) {
        SDL_GetWindowSize(window, &w, &h);
    }

    if (outW) *outW = w;
    if (outH) *outH = h;
}

static int options_resolve_asset_path(const char* relative_path,
                                      char* out_path,
                                      size_t out_path_size)
{
    char settings_path[PATH_MAX];
    char* slash = NULL;

    if (!relative_path || !out_path || out_path_size == 0) return 0;

    if (!options_settings_resolve_global_path(settings_path, sizeof(settings_path))) {
        snprintf(out_path, out_path_size, "%s", relative_path);
        return 1;
    }

    slash = strrchr(settings_path, '/');
    if (!slash) {
        snprintf(out_path, out_path_size, "%s", relative_path);
        return 1;
    }

    *slash = '\0';
    if (snprintf(out_path, out_path_size, "%s/%s", settings_path, relative_path) >= (int)out_path_size) {
        snprintf(out_path, out_path_size, "%s", relative_path);
    }
    return 1;
}

static SDL_Texture* options_load_texture(const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!renderer || !relative_path) return NULL;
    options_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return IMG_LoadTexture(renderer, asset_path);
}

static Mix_Chunk* options_load_wav(const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!relative_path) return NULL;
    options_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return Mix_LoadWAV(asset_path);
}

static Mix_Music* options_load_music(const char* relative_path)
{
    char asset_path[PATH_MAX];

    if (!relative_path) return NULL;
    options_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return Mix_LoadMUS(asset_path);
}

static FILE* options_open_asset_file(const char* relative_path, const char* mode)
{
    char asset_path[PATH_MAX];

    if (!relative_path || !mode) return NULL;
    options_resolve_asset_path(relative_path, asset_path, sizeof(asset_path));
    return fopen(asset_path, mode);
}

static int ScaleFromBaseHeight(int value, int renderH) {
    if (renderH <= 0) return value;
    return (value * renderH) / SCREEN_HEIGHT;
}

static SDL_Rect GetOptionsContainerRect(void) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);

    int panelW = (renderW * 74) / 100;
    int panelH = (renderH * 78) / 100;

    if (panelW < 620) panelW = 620;
    if (panelH < 430) panelH = 430;
    if (panelW > renderW - 40) panelW = renderW - 40;
    if (panelH > renderH - 40) panelH = renderH - 40;

    if (panelW < 120) panelW = renderW;
    if (panelH < 120) panelH = renderH;

    SDL_Rect panel = {
        (renderW - panelW) / 2,
        (renderH - panelH) / 2,
        panelW,
        panelH
    };
    return panel;
}

static int GetOptionRowY(int index) {
    SDL_Rect panel = GetOptionsContainerRect();
    int topPadding = (panel.h * 16) / 100;
    int bottomPadding = (panel.h * 18) / 100;
    int usableHeight = panel.h - topPadding - bottomPadding;
    int stepY = (MENU_ITEMS > 1) ? usableHeight / (MENU_ITEMS - 1) : 0;
    if (stepY < 42) stepY = 42;

    int y = panel.y + topPadding + index * stepY;
    int maxY = panel.y + panel.h - bottomPadding;
    if (y > maxY) y = maxY;
    return y;
}

static int WindowIsFullscreen(void) {
    if (!window) return 0;
    Uint32 flags = SDL_GetWindowFlags(window);
    return ((flags & SDL_WINDOW_FULLSCREEN) || (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)) ? 1 : 0;
}

static int CanAnimateWindowOpacity(void) {
    static int support = -1;
    if (!window) return 0;
    if (support != -1) return support;

    if (SDL_SetWindowOpacity(window, 1.0f) == 0) {
        support = 1;
    } else {
        support = 0;
        SDL_ClearError();
    }
    return support;
}

static void AnimateWindowOpacity(float from, float to, Uint32 durationMs) {
    if (!CanAnimateWindowOpacity()) return;
    if (durationMs == 0) return;

    Uint32 start = SDL_GetTicks();
    for (;;) {
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed > durationMs) elapsed = durationMs;

        float t = (float)elapsed / (float)durationMs;
        float value = from + (to - from) * t;
        SDL_SetWindowOpacity(window, value);

        if (elapsed >= durationMs) break;
        SDL_Delay(12);
    }
}

void InitUpSprite(void) {
    char up_path[PATH_MAX];

    options_resolve_asset_path(ASSET_OPTIONS_UP, up_path, sizeof(up_path));
    upTex = IMG_LoadTexture(renderer, up_path);
    if (!upTex) {
        SDL_Log("Failed to load up.png from %s: %s", up_path, IMG_GetError());
        exit(1);
    }
}

void RenderUpSprite(void) {
    if (!upTex) return;

    int origW, origH;
    SDL_QueryTexture(upTex, NULL, NULL, &origW, &origH);
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);

    const float scale = 0.13f;

    SDL_Rect dst = {
        (renderW - (int)(origW * scale)) / 2,
        renderH - (int)(origH * scale) - 16,
        (int)(origW * scale),
        (int)(origH * scale)
    };

    SDL_RenderCopy(renderer, upTex, NULL, &dst);
}

void CleanupUpSprite(void) {
    destroy_texture(&upTex);
}

void InitSnow(void) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    snowTex = options_load_texture(ASSET_OPTIONS_SNOW);
    if (!snowTex) {
        SDL_Log("Failed to load snow texture: %s", IMG_GetError());
    }

    for (int i = 0; i < MAX_SNOW; i++) {
        snow[i].x = rand() % renderW;
        snow[i].y = rand() % renderH;
        snow[i].speed = 50 + rand() % 100;
    }
}

void UpdateSnow(float delta) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    for (int i = 0; i < MAX_SNOW; i++) {
        snow[i].y += snow[i].speed * delta;
        if (snow[i].y > renderH) {
            snow[i].y = -16;
            snow[i].x = rand() % renderW;
        }
    }
}

void RenderSnow(void) {
    if (!snowTex) return;
    SDL_Rect dst = {0,0,16,16};
    for (int i = 0; i < MAX_SNOW; i++) {
        dst.x = (int)snow[i].x;
        dst.y = (int)snow[i].y;
        SDL_RenderCopy(renderer, snowTex, NULL, &dst);
    }
}

void CleanupSnow(void)
{
    destroy_texture(&snowTex);
}

void snow_quit(void)
{
    CleanupSnow();
}

/* ---------- Font ---------- */
void InitFont(void) {
    char options_font_path[PATH_MAX];
    char button_font_path[PATH_MAX];
    char menu_font_path[PATH_MAX];
    const char* button_candidates[3];

    options_resolve_asset_path(ASSET_OPTIONS_FONT, options_font_path, sizeof(options_font_path));
    options_resolve_asset_path(ASSET_BUTTON_FONT, button_font_path, sizeof(button_font_path));
    options_resolve_asset_path(ASSET_MAIN_MENU_FONT_TEXT, menu_font_path, sizeof(menu_font_path));

    font = ui_open_arial_font(28, 0);
    if (!font) {
        font = TTF_OpenFont(options_font_path, 28);
    }
    if (!font) {
        SDL_Log("TTF_OpenFont error: %s", TTF_GetError());
        exit(1);
    }
    ui_apply_font_quality(font);

    g_options_button_font = ui_open_arial_font(24, 0);

    button_candidates[0] = button_font_path;
    button_candidates[1] = options_font_path;
    button_candidates[2] = menu_font_path;
    const int button_candidate_count = (int)(sizeof(button_candidates) / sizeof(button_candidates[0]));
    if (!g_options_button_font) {
        g_options_button_font = ui_open_font_from_candidates(button_candidates, button_candidate_count, 24, 0);
    }
    if (g_options_button_font) {
        ui_apply_font_quality(g_options_button_font);
    }
    if (!g_options_button_font) {
        SDL_Log("Options button font load failed: %s", TTF_GetError());
    }
}

void CleanupFont(void) {
    if (g_options_button_font) {
        TTF_CloseFont(g_options_button_font);
        g_options_button_font = NULL;
    }
    if (font) { TTF_CloseFont(font); font = NULL; }
}

/* ---------- Background ---------- */
void InitBackground(void) {
    char bg_path[PATH_MAX];
    char bar_full_path[PATH_MAX];
    char bar_empty_path[PATH_MAX];
    char vol1_path[PATH_MAX];
    char vol2_path[PATH_MAX];
    char vol3_path[PATH_MAX];
    char vol4_path[PATH_MAX];
    char sun1_path[PATH_MAX];
    char sun2_path[PATH_MAX];
    char sun3_path[PATH_MAX];

    options_resolve_asset_path(ASSET_OPTIONS_BACKGROUND, bg_path, sizeof(bg_path));
    options_resolve_asset_path(ASSET_OPTIONS_BAR_FULL, bar_full_path, sizeof(bar_full_path));
    options_resolve_asset_path(ASSET_OPTIONS_BAR_EMPTY, bar_empty_path, sizeof(bar_empty_path));
    options_resolve_asset_path(ASSET_OPTIONS_VOLUME_1, vol1_path, sizeof(vol1_path));
    options_resolve_asset_path(ASSET_OPTIONS_VOLUME_2, vol2_path, sizeof(vol2_path));
    options_resolve_asset_path(ASSET_OPTIONS_VOLUME_3, vol3_path, sizeof(vol3_path));
    options_resolve_asset_path(ASSET_OPTIONS_VOLUME_4, vol4_path, sizeof(vol4_path));
    options_resolve_asset_path(ASSET_OPTIONS_SUN_1, sun1_path, sizeof(sun1_path));
    options_resolve_asset_path(ASSET_OPTIONS_SUN_2, sun2_path, sizeof(sun2_path));
    options_resolve_asset_path(ASSET_OPTIONS_SUN_3, sun3_path, sizeof(sun3_path));

    bgTexture = IMG_LoadTexture(renderer, bg_path);
    if (!bgTexture) {
        SDL_Log("Failed to load background.png from %s: %s", bg_path, IMG_GetError());
        exit(1);
    }

    barFullTex = IMG_LoadTexture(renderer, bar_full_path);
    barEmptyTex = IMG_LoadTexture(renderer, bar_empty_path);

    volIcons[0] = IMG_LoadTexture(renderer, vol1_path);
    volIcons[1] = IMG_LoadTexture(renderer, vol2_path);
    volIcons[2] = IMG_LoadTexture(renderer, vol3_path);
    volIcons[3] = IMG_LoadTexture(renderer, vol4_path);

    sunIcons[0] = IMG_LoadTexture(renderer, sun1_path);
    sunIcons[1] = IMG_LoadTexture(renderer, sun2_path);
    sunIcons[2] = IMG_LoadTexture(renderer, sun3_path);

    if (!barFullTex || !barEmptyTex) {
        SDL_Log("Failed to load bar textures: %s", IMG_GetError());
        exit(1);
    }
}

void CleanupBackground(void) {
    destroy_texture(&bgTexture);
    destroy_texture(&barFullTex);
    destroy_texture(&barEmptyTex);
    destroy_texture_array(volIcons, 4);
    destroy_texture_array(sunIcons, 3);
}

/* ---------- Settings ---------- */
void LoadSettings(void) {
    if (!options_settings_load_global(&settings)) {
        SDL_Log("global settings read failed, using defaults.");
        options_settings_set_defaults(&settings);
    }
    settings.master = clamp_int(settings.master, 0, SLIDER_BAR_COUNT);
    settings.music = clamp_int(settings.music, 0, SLIDER_BAR_COUNT);
    settings.vfx = clamp_int(settings.vfx, 0, SLIDER_BAR_COUNT);
    settings.brightness = clamp_int(settings.brightness, 0, SLIDER_BAR_COUNT);
    settings.fullscreen = settings.fullscreen ? 1 : 0;
    game_progress_set_options(settings.master, settings.music, settings.vfx, settings.brightness, settings.fullscreen);
}

void SaveSettings(void) {
    game_progress_set_options(settings.master, settings.music, settings.vfx, settings.brightness, settings.fullscreen);
    if (!options_settings_save_global(&settings)) {
        SDL_Log("global settings write failed.");
    }
}

void ApplySettings(void) {
    options_settings_normalize(&settings);
    settings.master = clamp_int(settings.master, 0, SLIDER_BAR_COUNT);
    settings.music = clamp_int(settings.music, 0, SLIDER_BAR_COUNT);
    settings.vfx = clamp_int(settings.vfx, 0, SLIDER_BAR_COUNT);
    settings.brightness = clamp_int(settings.brightness, 0, SLIDER_BAR_COUNT);

    int finalMusic = (settings.music * settings.master) / 10;
    int finalVfx   = (settings.vfx * settings.master) / 10;
    Mix_VolumeMusic(finalMusic * MIX_MAX_VOLUME / 10);
    Mix_Volume(-1, finalVfx * MIX_MAX_VOLUME / 10);

    int wantFullscreen = settings.fullscreen ? 1 : 0;
    int isFullscreen = WindowIsFullscreen();
    if (wantFullscreen == isFullscreen) return;

    AnimateWindowOpacity(1.0f, 0.0f, 140);

    Uint32 fullscreenMode = wantFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(window, fullscreenMode) != 0) {
        SDL_Log("SDL_SetWindowFullscreen failed: %s", SDL_GetError());
        settings.fullscreen = isFullscreen;
    } else if (!wantFullscreen) {
        SDL_SetWindowSize(window, SCREEN_WIDTH, SCREEN_HEIGHT);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }

    AnimateWindowOpacity(0.0f, 1.0f, 170);
}

/* ---------- Audio ---------- */
static Mix_Chunk* click_sfx = NULL;
static Mix_Chunk* shoot1 = NULL;
static Mix_Chunk* shoot2 = NULL;
static Mix_Music* music = NULL;

void InitAudio(void) {
    CleanupAudio();

    click_sfx = options_load_wav(ASSET_OPTIONS_CLICK);
    shoot1 = options_load_wav(ASSET_OPTIONS_SHOOT);
    shoot2 = options_load_wav(ASSET_OPTIONS_SHOOT);
    music = options_load_music(ASSET_OPTIONS_MUSIC);
    if (music) Mix_PlayMusic(music, -1);
}

void CleanupAudio(void) {
    destroy_chunk(&click_sfx);
    destroy_chunk(&shoot1);
    destroy_chunk(&shoot2);
    if (music) { Mix_FreeMusic(music); music=NULL; }
}

void PlayClick(void) {
    if (click_sfx) Mix_PlayChannel(-1, click_sfx, 0);
}

void PlayShootLeft(void)  { if (shoot1) Mix_PlayChannel(-1, shoot1, 0); }
void PlayShootRight(void) { if (shoot2) Mix_PlayChannel(-1, shoot2, 0); }

/* ---------- Shooters ---------- */
void InitShooters(void) {
    leftShooter.texture  = options_load_texture(ASSET_OPTIONS_THIEF_LEFT);
    rightShooter.texture = options_load_texture(ASSET_OPTIONS_THIEF_RIGHT);
    bulletTex = options_load_texture(ASSET_OPTIONS_BULLET);
    brickTex  = options_load_texture(ASSET_OPTIONS_BRICK);

    if (!leftShooter.texture || !rightShooter.texture || !bulletTex || !brickTex) {
        SDL_Log("Missing shooter assets: %s", IMG_GetError());
        exit(1);
    }

    int texW, texH;
    SDL_QueryTexture(leftShooter.texture, NULL, NULL, &texW, &texH);
    leftShooter.frameW = texW / 6;
    leftShooter.frameH = texH / 6;

    SDL_QueryTexture(rightShooter.texture, NULL, NULL, &texW, &texH);
    rightShooter.frameW = texW / 6;
    rightShooter.frameH = texH / 6;

    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    int shooterBaseY = ScaleFromBaseHeight(BRICK_HEIGHT, renderH);

    leftShooter.x = 8;
    leftShooter.y = (float)(shooterBaseY - leftShooter.frameH);
    rightShooter.x = (float)(renderW - rightShooter.frameW - 8);
    rightShooter.y = (float)(shooterBaseY - rightShooter.frameH);

    leftShooter.direction = 1;
    rightShooter.direction = -1;

    leftShooter.shootInterval  = 0.9f;
    rightShooter.shootInterval = 1.1f;

    leftShooter.frame = rightShooter.frame = 0;
    leftShooter.animTimer = rightShooter.animTimer = 0.0f;
    leftShooter.shootTimer = rightShooter.shootTimer = 0.0f;
    leftShooter.firedThisCycle = rightShooter.firedThisCycle = 0;

    for (int i=0;i<MAX_BULLETS;i++) bullets[i].active = 0;
}

void CleanupShooters(void) {
    destroy_texture(&leftShooter.texture);
    destroy_texture(&rightShooter.texture);
    destroy_texture(&bulletTex);
    destroy_texture(&brickTex);
}

void SpawnBullet(Shooter* s) {
    for (int i=0;i<MAX_BULLETS;i++) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].x = s->x + s->frameW * 0.5f;
            bullets[i].y = s->y + s->frameH * 0.5f;
            bullets[i].speed = 600.0f * (s->direction > 0 ? 1.0f : -1.0f);
            if (s == &leftShooter) PlayShootLeft();
            else PlayShootRight();
            break;
        }
    }
}

void UpdateShooter(Shooter* s, float delta) {
    s->animTimer += delta;
    s->shootTimer += delta;

    if (s->animTimer >= 0.05f) {
        s->frame++;
        if (s->frame >= 36) {
            s->frame = 0;
            s->firedThisCycle = 0;
        }
        s->animTimer = 0.0f;
    }

    if (s->frame == 17 && !s->firedThisCycle && s->shootTimer >= s->shootInterval) {
        SpawnBullet(s);
        s->firedThisCycle = 1;
        s->shootTimer = 0.0f;
    }
}

void UpdateShooters(float delta) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    int shooterBaseY = ScaleFromBaseHeight(BRICK_HEIGHT, renderH);

    leftShooter.y = (float)(shooterBaseY - leftShooter.frameH);
    rightShooter.x = (float)(renderW - rightShooter.frameW - 8);
    rightShooter.y = (float)(shooterBaseY - rightShooter.frameH);

    UpdateShooter(&leftShooter, delta);
    UpdateShooter(&rightShooter, delta);

    for (int i=0;i<MAX_BULLETS;i++){
        if (bullets[i].active) {
            bullets[i].x += bullets[i].speed * delta;
            if (bullets[i].x < -50.0f || bullets[i].x > renderW + 50.0f)
                bullets[i].active = 0;
        }
    }
}

void RenderShooter(Shooter* s) {
    int row = s->frame / 6;
    int col = s->frame % 6;
    SDL_Rect src = { col * s->frameW, row * s->frameH, s->frameW, s->frameH };
    SDL_Rect dst = { (int)s->x, (int)s->y, s->frameW, s->frameH };
    SDL_RenderCopy(renderer, s->texture, &src, &dst);
}

void RenderShooters(void) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    int brickBandH = ScaleFromBaseHeight(BRICK_Y, renderH);
    if (brickBandH < leftShooter.frameH + 20) brickBandH = leftShooter.frameH + 20;

    SDL_Rect leftBrick  = { 0, 0, leftShooter.frameW + 24, brickBandH };
    SDL_Rect rightBrick = { renderW - (rightShooter.frameW + 24), 0, rightShooter.frameW + 24, brickBandH };

    SDL_RenderCopy(renderer, brickTex, NULL, &leftBrick);
    SDL_RenderCopy(renderer, brickTex, NULL, &rightBrick);

    RenderShooter(&leftShooter);
    RenderShooter(&rightShooter);

    for (int i=0;i<MAX_BULLETS;i++){
        if (bullets[i].active) {
            SDL_Rect dst = { (int)bullets[i].x - 10, (int)bullets[i].y - 5, 20, 10 };
            SDL_RenderCopy(renderer, bulletTex, NULL, &dst);
        }
    }
}

/* ---------- Credits ---------- */
static char* creditsText = NULL;
static int creditsLength = 0;
static int visibleChars = 0;
static float typeTimer = 0.0f;
static float scrollY = 0.0f;
static int finishedTyping = 0;
static SDL_Texture* creditsCacheTex = NULL;
static int creditsCacheLen = -1;
static int creditsCacheWrapWidth = 0;
static int creditsCacheW = 0;
static int creditsCacheH = 0;

static void ResetCreditsCache(void)
{
    destroy_texture(&creditsCacheTex);
    creditsCacheLen = -1;
    creditsCacheWrapWidth = 0;
    creditsCacheW = 0;
    creditsCacheH = 0;
}

static int RebuildCreditsCache(int len, int wrapWidth)
{
    SDL_Color white = {255, 255, 255, 255};
    char* tmp = NULL;
    SDL_Surface* surf = NULL;

    if (!creditsText || !font || len < 1) return 0;

    tmp = malloc((size_t)len + 1);
    if (!tmp) return 0;

    memcpy(tmp, creditsText, (size_t)len);
    tmp[len] = '\0';

    surf = TTF_RenderText_Blended_Wrapped(font, tmp, white, wrapWidth);
    free(tmp);
    if (!surf) return 0;

    ResetCreditsCache();
    creditsCacheTex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!creditsCacheTex) {
        SDL_FreeSurface(surf);
        return 0;
    }

    creditsCacheLen = len;
    creditsCacheWrapWidth = wrapWidth;
    creditsCacheW = surf->w;
    creditsCacheH = surf->h;
    SDL_FreeSurface(surf);
    return 1;
}

void LoadCreditsFromFile(void) {
    CleanupCredits();

    FILE* f = options_open_asset_file(ASSET_OPTIONS_CREDITS, "rb");
    if (!f) {
        SDL_Log("Unable to open credits file: %s", ASSET_OPTIONS_CREDITS);
        return;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }

    long file_size = ftell(f);
    if (file_size < 0 || file_size > INT_MAX) {
        fclose(f);
        return;
    }
    rewind(f);

    char* loaded_text = malloc((size_t)file_size + 1);
    if (!loaded_text) {
        fclose(f);
        return;
    }

    size_t expected = (size_t)file_size;
    size_t read_total = fread(loaded_text, 1, expected, f);
    fclose(f);

    if (read_total != expected) {
        free(loaded_text);
        return;
    }

    loaded_text[expected] = '\0';
    creditsText = loaded_text;
    creditsLength = (int)expected;
}

void InitCredits(void) {
    int renderH;
    GetRenderSize(NULL, &renderH);

    ResetCreditsCache();
    visibleChars = 0;
    typeTimer = 0.0f;
    scrollY = (float)renderH;
    finishedTyping = 0;
}

void UpdateCredits(float delta) {
    if (!creditsText) return;
    if (!finishedTyping) {
        typeTimer += delta;
        if (typeTimer >= 0.03f) {
            if (visibleChars < creditsLength) visibleChars++;
            typeTimer = 0.0f;
        }
        if (visibleChars >= creditsLength) finishedTyping = 1;
    } else {
        scrollY -= 30.0f * delta;
    }
}

void RenderCredits(void) {
    if (!creditsText || !font) return;
    int len = visibleChars;
    if (len < 1) return;
    int renderW;
    GetRenderSize(&renderW, NULL);

    int wrapWidth = (renderW > 260) ? (renderW - 200) : 60;
    if ((creditsCacheLen != len || creditsCacheWrapWidth != wrapWidth) &&
        !RebuildCreditsCache(len, wrapWidth)) {
        return;
    }
    if (!creditsCacheTex) return;

    SDL_Rect dst = {(renderW - creditsCacheW) / 2, (int)scrollY, creditsCacheW, creditsCacheH};
    SDL_RenderCopy(renderer, creditsCacheTex, NULL, &dst);
}

void CleanupCredits(void)
{
    ResetCreditsCache();
    if (creditsText) {
        free(creditsText);
        creditsText = NULL;
    }
    creditsLength = 0;
    visibleChars = 0;
    typeTimer = 0.0f;
    scrollY = 0.0f;
    finishedTyping = 0;
}

/* ---------- Options menu ---------- */
static const char* optionItems[MENU_ITEMS] = {
    "Master Volume",
    "Music Volume",
    "VFX Volume",
    "Brightness",
    "Full Screen",
    "Credits",
    "Return"
};

static int options_is_pause_overlay(void)
{
    return options_scene_is_pause_overlay_mode();
}

static int options_is_action_item(int index)
{
    return index >= 4 && index < MENU_ITEMS;
}

static int sliderMouseCapture = -1;

static void options_reset_button_visual(OptionsButtonVisual* visual)
{
    if (!visual) return;

    visual->rect = (SDL_Rect){0, 0, 0, 0};
    visual->active = 0;
    visual->selected = 0;
    visual->scale = 1.0f;
    visual->target_scale = 1.0f;
    visual->bounce_offset = 0.0f;
    visual->pressed_timer = 0.0f;
}

void OptionsResetButtonVisuals(void)
{
    for (int i = 0; i < MENU_ITEMS; ++i) {
        options_reset_button_visual(&g_action_button_visuals[i]);
    }
    sliderMouseCapture = -1;
}

static void set_draw_color(SDL_Renderer* r, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer* r, const SDL_Rect* rect, SDL_Color c) {
    set_draw_color(r, c);
    SDL_RenderFillRect(r, rect);
}

static void draw_rect(SDL_Renderer* r, const SDL_Rect* rect, SDL_Color c) {
    set_draw_color(r, c);
    SDL_RenderDrawRect(r, rect);
}

static void render_frosted_panel(SDL_Renderer* r, const SDL_Rect* panel) {
    if (!panel || panel->w <= 0 || panel->h <= 0) return;

    const SDL_Color glowLayers[3] = {
        {0x78, 0xA6, 0xD8, 0x16},
        {0x8C, 0xB8, 0xE2, 0x22},
        {0x9E, 0xC5, 0xEC, 0x2E}
    };
    const int pads[3] = {16, 10, 5};

    for (int i = 0; i < 3; ++i) {
        SDL_Rect glow = {
            panel->x - pads[i],
            panel->y - pads[i],
            panel->w + pads[i] * 2,
            panel->h + pads[i] * 2
        };
        fill_rect(r, &glow, glowLayers[i]);
    }

    SDL_Rect shell = *panel;
    SDL_Rect inner = {panel->x + 2, panel->y + 2, panel->w - 4, panel->h - 4};
    fill_rect(r, &shell, (SDL_Color){0x11, 0x1C, 0x2D, 0x7A});
    fill_rect(r, &inner, (SDL_Color){0x2B, 0x47, 0x67, 0x58});

    /* Faux blur: soft horizontal streaks that mimic frosted glass diffusion. */
    if (inner.w > 12 && inner.h > 10) {
        for (int y = inner.y + 3; y < inner.y + inner.h - 3; y += 12) {
            Uint8 alpha = (Uint8)((((y - inner.y) / 12) % 2) ? 0x16 : 0x0E);
            SDL_Rect streak = {inner.x + 6, y, inner.w - 12, 5};
            fill_rect(r, &streak, (SDL_Color){0xCB, 0xDF, 0xF4, alpha});
        }

        SDL_Rect sheen = {inner.x + 8, inner.y + 8, inner.w / 3, inner.h - 16};
        fill_rect(r, &sheen, (SDL_Color){0xE6, 0xF0, 0xFA, 0x14});
    }

    draw_rect(r, &shell, (SDL_Color){0xC6, 0xDE, 0xF5, 0x78});
    draw_rect(r, &inner, (SDL_Color){0x70, 0x9B, 0xC4, 0xAA});
}

static SDL_Rect GetReturnButtonRect(void) {
    int renderH;
    GetRenderSize(NULL, &renderH);
    SDL_Rect panel = GetOptionsContainerRect();

    int buttonW = (panel.w * 46) / 100;
    int buttonH = (54 * renderH) / SCREEN_HEIGHT;
    if (buttonW > 380) buttonW = 380;
    if (buttonW < 180) buttonW = 180;
    if (buttonH < 42) buttonH = 42;

    int x = panel.x + (panel.w - buttonW) / 2;
    int y = GetOptionRowY(6) - buttonH / 2 + ScaleFromBaseHeight(12, renderH);
    int minY = panel.y + 16;
    int maxY = panel.y + panel.h - buttonH - 16;
    if (y < minY) y = minY;
    if (y > maxY) y = maxY;

    SDL_Rect rect = {x, y, buttonW, buttonH};
    return rect;
}

static SDL_Rect GetActionButtonRect(int index) {
    int renderH;
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect rect = {0, 0, 0, 0};
    int buttonW = 0;
    int buttonH = 0;
    int x = 0;
    int y = 0;
    int minY = 0;
    int maxY = 0;

    GetRenderSize(NULL, &renderH);
    panel = GetOptionsContainerRect();

    buttonW = (panel.w * 46) / 100;
    buttonH = (54 * renderH) / SCREEN_HEIGHT;
    if (buttonW > 380) buttonW = 380;
    if (buttonW < 180) buttonW = 180;
    if (buttonH < 42) buttonH = 42;

    x = panel.x + (panel.w - buttonW) / 2;
    y = GetOptionRowY(index) - buttonH / 2 + ScaleFromBaseHeight(12, renderH);
    minY = panel.y + 16;
    maxY = panel.y + panel.h - buttonH - 16;
    if (y < minY) y = minY;
    if (y > maxY) y = maxY;

    rect = (SDL_Rect){x, y, buttonW, buttonH};
    return rect;
}

static SDL_Rect options_get_button_dest(const OptionsButtonVisual* visual)
{
    SDL_Rect dest = {0, 0, 0, 0};
    int scaled_w = 0;
    int scaled_h = 0;

    if (!visual) return dest;

    scaled_w = (int)(visual->rect.w * visual->scale);
    scaled_h = (int)(visual->rect.h * visual->scale);
    dest.w = scaled_w;
    dest.h = scaled_h;
    dest.x = visual->rect.x - (scaled_w - visual->rect.w) / 2;
    dest.y = visual->rect.y - (scaled_h - visual->rect.h) / 2;
    dest.y += (int)visual->bounce_offset;
    if (visual->pressed_timer > 0.0f) dest.y += 1;
    return dest;
}

static const char* GetOptionLabel(int index) {
    if (index < 0 || index >= MENU_ITEMS) return "";
    if (index == 4) return settings.fullscreen ? "Window" : "Full Screen";
    if (index == 5 && options_is_pause_overlay()) return "Quit To Menu";
    return optionItems[index];
}

static int GetOptionLayout(int index, SDL_Rect* labelRect, SDL_Rect* sliderRect, SDL_Rect* rowRect) {
    if (!font || index < 0 || index >= MENU_ITEMS) return 0;

    SDL_Rect panel = GetOptionsContainerRect();

    int textW = 0;
    int textH = 0;
    if (TTF_SizeText(font, GetOptionLabel(index), &textW, &textH) != 0) return 0;

    int y = GetOptionRowY(index);
    int x;

    if (options_is_action_item(index)) {
        SDL_Rect buttonRect = (index == 6) ? GetReturnButtonRect() : GetActionButtonRect(index);
        if (labelRect) {
            labelRect->x = buttonRect.x + (buttonRect.w - textW) / 2;
            labelRect->y = buttonRect.y + (buttonRect.h - textH) / 2;
            labelRect->w = textW;
            labelRect->h = textH;
        }
        if (sliderRect) *sliderRect = (SDL_Rect){0, 0, 0, 0};
        if (rowRect) *rowRect = buttonRect;
        return 1;
    }

    if (index < SLIDER_COUNT) {
        const int labelGap = 24;
        int rowContentW = textW + labelGap + SLIDER_VISUAL_W;
        x = panel.x + (panel.w - rowContentW) / 2;
    } else {
        x = panel.x + (panel.w - textW) / 2;
    }

    SDL_Rect label = { x, y, textW, textH };
    if (labelRect) *labelRect = label;

    int rowLeft = x - 20;
    int rowTop = y - 12;
    int rowRight = x + textW + 20;
    int rowBottom = y + textH + 12;

    if (index < SLIDER_COUNT) {
        int bx = x + textW + 24;
        int by = y + (textH - 20) / 2 - 13;

        int sliderTrackW = ((SLIDER_BAR_COUNT - 1) * SLIDER_BAR_STEP) + SLIDER_BAR_W;
        SDL_Rect slider = { bx, by, sliderTrackW, SLIDER_BAR_H };
        if (sliderRect) *sliderRect = slider;

        int sliderRight = bx + SLIDER_ICON_OFFSET_X + SLIDER_ICON_W + 20;
        int sliderTop = by - 10;
        int sliderBottom = by + SLIDER_BAR_H + 10;

        if (sliderTop < rowTop) rowTop = sliderTop;
        if (sliderRight > rowRight) rowRight = sliderRight;
        if (sliderBottom > rowBottom) rowBottom = sliderBottom;
    } else {
        if (sliderRect) *sliderRect = (SDL_Rect){0, 0, 0, 0};
        rowLeft -= 36;
        rowRight += 36;
    }

    if (rowRect) *rowRect = (SDL_Rect){ rowLeft, rowTop, rowRight - rowLeft, rowBottom - rowTop };
    return 1;
}

static void options_sync_action_button_visual(int index, int selected)
{
    OptionsButtonVisual* visual = NULL;
    SDL_Rect rect = {0, 0, 0, 0};

    if (!options_is_action_item(index)) return;

    visual = &g_action_button_visuals[index];
    rect = (index == 6) ? GetReturnButtonRect() : GetActionButtonRect(index);

    if (!visual->active) {
        options_reset_button_visual(visual);
        visual->active = 1;
    }

    visual->rect = rect;
    visual->selected = selected ? 1 : 0;
}

void UpdateOptionsMenuButtons(int selected, float delta)
{
    if (delta < 0.0f) delta = 0.0f;
    if (delta > 0.1f) delta = 0.1f;

    for (int i = 0; i < MENU_ITEMS; ++i) {
        OptionsButtonVisual* visual = &g_action_button_visuals[i];
        float diff = 0.0f;

        if (!options_is_action_item(i)) {
            options_reset_button_visual(visual);
            continue;
        }

        options_sync_action_button_visual(i, i == selected);
        visual->target_scale = visual->selected ? 1.07f : 1.0f;
        visual->bounce_offset = visual->selected ? -1.0f : 0.0f;
        diff = visual->target_scale - visual->scale;
        visual->scale += diff * 11.0f * delta;
        if (diff > -0.001f && diff < 0.001f) {
            visual->scale = visual->target_scale;
        }

        if (visual->pressed_timer > 0.0f) {
            visual->pressed_timer -= delta;
            if (visual->pressed_timer < 0.0f) {
                visual->pressed_timer = 0.0f;
            }
        }
    }
}

void OptionsPressActionButton(int index)
{
    if (!options_is_action_item(index)) return;

    options_sync_action_button_visual(index, 1);
    g_action_button_visuals[index].pressed_timer = 0.13f;
}

static int GetOptionFromMousePosition(int mx, int my) {
    SDL_Point p = { mx, my };
    for (int i = 0; i < MENU_ITEMS; i++) {
        SDL_Rect rowRect;
        if (!GetOptionLayout(i, NULL, NULL, &rowRect)) continue;
        if (SDL_PointInRect(&p, &rowRect)) return i;
    }
    return -1;
}

static int SliderValueFromMouseX(int mouseX, const SDL_Rect* sliderRect) {
    if (!sliderRect) return 0;
    if (mouseX < sliderRect->x) return 0;
    if (mouseX >= sliderRect->x + sliderRect->w) return SLIDER_BAR_COUNT;
    return clamp_int((mouseX - sliderRect->x) / SLIDER_BAR_STEP + 1, 0, SLIDER_BAR_COUNT);
}

static int* GetSettingForSliderIndex(int index) {
    if (index == 0) return &settings.master;
    if (index == 1) return &settings.music;
    if (index == 2) return &settings.vfx;
    if (index == 3) return &settings.brightness;
    return NULL;
}

static int SetSliderValue(int sliderIndex, int newValue, int playClickSfx)
{
    int* setting = GetSettingForSliderIndex(sliderIndex);
    if (!setting) return 0;

    int clampedValue = clamp_int(newValue, 0, SLIDER_BAR_COUNT);
    if (*setting == clampedValue) return 0;

    *setting = clampedValue;
    ApplySettings();
    SaveSettings();
    if (playClickSfx) {
        PlayClick();
    }
    return 1;
}

int OptionsAdjustSliderByDelta(int sliderIndex, int delta, int playClickSfx)
{
    int* setting = GetSettingForSliderIndex(sliderIndex);
    if (!setting) return 0;
    if (delta == 0) return 0;

    return SetSliderValue(sliderIndex, *setting + delta, playClickSfx);
}

void OptionsToggleFullscreen(int playClickSfx)
{
    settings.fullscreen = !settings.fullscreen;
    ApplySettings();
    SaveSettings();
    if (playClickSfx) {
        PlayClick();
    }
}

void OptionsShowCredits(int playClickSfx)
{
    if (playClickSfx) {
        PlayClick();
    }
    state = STATE_CREDITS;
    InitCredits();
}

static void UpdateSliderFromMouse(int sliderIndex, int mouseX, int playClickSfx) {
    SDL_Rect sliderRect;
    if (!GetOptionLayout(sliderIndex, NULL, &sliderRect, NULL)) return;

    int newValue = SliderValueFromMouseX(mouseX, &sliderRect);
    SetSliderValue(sliderIndex, newValue, playClickSfx);
}

void HandleOptionsMouseEvent(const SDL_Event* e, int* selected, int* keep_open, int* quit_to_menu) {
    if (!e || !selected) return;

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        sliderMouseCapture = -1;
        return;
    }

    if (e->type == SDL_MOUSEMOTION) {
        int hovered = GetOptionFromMousePosition(e->motion.x, e->motion.y);
        if (hovered >= 0) *selected = hovered;

        if ((e->motion.state & SDL_BUTTON_LMASK) && sliderMouseCapture >= 0) {
            UpdateSliderFromMouse(sliderMouseCapture, e->motion.x, 0);
        }
        return;
    }

    if (e->type != SDL_MOUSEBUTTONDOWN || e->button.button != SDL_BUTTON_LEFT) return;

    int hovered = GetOptionFromMousePosition(e->button.x, e->button.y);
    if (hovered < 0) {
        sliderMouseCapture = -1;
        return;
    }

    *selected = hovered;
    sliderMouseCapture = -1;

    if (hovered >= 0 && hovered < SLIDER_COUNT) {
        SDL_Rect sliderRect;
        SDL_Rect sliderHitRect;
        SDL_Point p = { e->button.x, e->button.y };

        if (!GetOptionLayout(hovered, NULL, &sliderRect, NULL)) return;
        sliderHitRect = sliderRect;
        sliderHitRect.w += SLIDER_HIT_EXTRA_W;

        if (SDL_PointInRect(&p, &sliderHitRect)) {
            sliderMouseCapture = hovered;
            UpdateSliderFromMouse(hovered, e->button.x, 1);
        }
        return;
    }

    if (hovered == 4) {
        OptionsPressActionButton(hovered);
        OptionsToggleFullscreen(1);
        return;
    }

    if (hovered == 5) {
        OptionsPressActionButton(hovered);
        if (options_is_pause_overlay()) {
            PlayClick();
            if (keep_open) *keep_open = 0;
            if (quit_to_menu) *quit_to_menu = 1;
        } else {
            OptionsShowCredits(1);
        }
        return;
    }

    if (hovered == 6) {
        OptionsPressActionButton(hovered);
        PlayClick();
        if (keep_open) *keep_open = 0;
        if (quit_to_menu) *quit_to_menu = 0;
    }
}

static int GetSliderValue(int index)
{
    int* setting = GetSettingForSliderIndex(index);
    if (!setting) return 0;
    return clamp_int(*setting, 0, SLIDER_BAR_COUNT);
}

static SDL_Texture* GetSliderIconTexture(int optionIndex, int value)
{
    if (optionIndex == 3) {
        int sunIndex = (value >= 6) ? 2 : (value >= 3) ? 1 : 0;
        return sunIcons[sunIndex];
    }

    int volIndex = (value >= 8) ? 3 : (value >= 6) ? 2 : (value >= 3) ? 1 : 0;
    return volIcons[volIndex];
}

static void RenderSliderControl(const SDL_Rect* sliderRect, int optionIndex, int value)
{
    if (!sliderRect) return;

    int bx = sliderRect->x;
    int by = sliderRect->y;
    for (int t = 0; t < SLIDER_BAR_COUNT; ++t) {
        SDL_Rect barDst = {bx + t * SLIDER_BAR_STEP, by, SLIDER_BAR_W, SLIDER_BAR_H};
        SDL_Texture* barTex = (t < value) ? barFullTex : barEmptyTex;
        SDL_RenderCopy(renderer, barTex, NULL, &barDst);
    }

    SDL_Texture* iconTex = GetSliderIconTexture(optionIndex, value);
    if (!iconTex) return;

    SDL_Rect iconDst = {
        bx + SLIDER_ICON_OFFSET_X,
        by - 6,
        SLIDER_ICON_W,
        SLIDER_ICON_H
    };
    SDL_RenderCopy(renderer, iconTex, NULL, &iconDst);
}

void RenderOptionsMenu(int selected) {
    SDL_Color white = {255,255,255,255};
    SDL_Color warm  = {255,200,100,255};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect panel = GetOptionsContainerRect();
    render_frosted_panel(renderer, &panel);

    for (int i=0;i<MENU_ITEMS;i++) {
        SDL_Rect labelRect;
        SDL_Rect sliderRect;
        SDL_Rect rowRect;
        if (!GetOptionLayout(i, &labelRect, &sliderRect, &rowRect)) continue;

        if (options_is_action_item(i)) {
            OptionsButtonVisual* visual = &g_action_button_visuals[i];
            SDL_Rect dest = visual->active ? options_get_button_dest(visual) : rowRect;
            int hovered = (i == selected);
            TTF_Font* button_font = g_options_button_font ? g_options_button_font : font;
            int pressed = visual->pressed_timer > 0.0f;

            render_main_menu_style_button(renderer, button_font, &dest, GetOptionLabel(i), hovered, pressed);
            continue;
        }

        if (i == selected) {
            SDL_Rect hoverGlow = {rowRect.x - 4, rowRect.y - 2, rowRect.w + 8, rowRect.h + 4};
            fill_rect(renderer, &hoverGlow, (SDL_Color){0xD4, 0xE9, 0xFB, 0x24});
            draw_rect(renderer, &hoverGlow, (SDL_Color){0xC6, 0xDE, 0xF5, 0x60});
        }

        SDL_Color c = (i==selected) ? warm : white;
        ui_draw_text_left(renderer, font, GetOptionLabel(i), labelRect.x, labelRect.y, c);

        if (i < SLIDER_COUNT) {
            int value = GetSliderValue(i);
            RenderSliderControl(&sliderRect, i, value);
        }
    }
}

/* ---------- Brightness overlay ---------- */
void RenderBrightnessOverlay(void) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);

    int alpha = (SLIDER_BAR_COUNT - clamp_int(settings.brightness, 0, SLIDER_BAR_COUNT)) * 18;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0,0,0, alpha);
    SDL_Rect r = {0,0,renderW,renderH};
    SDL_RenderFillRect(renderer, &r);
}
