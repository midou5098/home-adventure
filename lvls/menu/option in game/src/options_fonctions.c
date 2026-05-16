#include "options_internal.h"
#include "asset_paths.h"
#include "ui_shared.h"
#include <limits.h>

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

static void prepend_parent_path(char* out_path, size_t out_size, const char* path)
{
    if (!out_path || out_size == 0) return;
    if (!path || !path[0]) {
        out_path[0] = '\0';
        return;
    }
    SDL_snprintf(out_path, out_size, "../%s", path);
}

static SDL_Texture* load_texture_candidate(SDL_Renderer* target_renderer,
                                           const char* merged_path,
                                           const char* local_path)
{
    SDL_Texture* texture = NULL;
    char parent_merged_path[PATH_MAX];

    if (!target_renderer) return NULL;
    if (merged_path && merged_path[0]) {
        texture = IMG_LoadTexture(target_renderer, merged_path);
    }
    if (!texture) {
        prepend_parent_path(parent_merged_path, sizeof(parent_merged_path), merged_path);
        if (parent_merged_path[0]) {
            texture = IMG_LoadTexture(target_renderer, parent_merged_path);
        }
    }
    if (!texture && local_path && local_path[0]) {
        texture = IMG_LoadTexture(target_renderer, local_path);
    }
    return texture;
}

static Mix_Chunk* load_wav_candidate(const char* merged_path, const char* local_path)
{
    Mix_Chunk* chunk = NULL;
    char parent_merged_path[PATH_MAX];

    if (merged_path && merged_path[0]) {
        chunk = Mix_LoadWAV(merged_path);
    }
    if (!chunk) {
        prepend_parent_path(parent_merged_path, sizeof(parent_merged_path), merged_path);
        if (parent_merged_path[0]) {
            chunk = Mix_LoadWAV(parent_merged_path);
        }
    }
    if (!chunk && local_path && local_path[0]) {
        chunk = Mix_LoadWAV(local_path);
    }
    return chunk;
}

static Mix_Music* load_music_candidate(const char* merged_path, const char* local_path)
{
    Mix_Music* loaded_music = NULL;
    char parent_merged_path[PATH_MAX];

    if (merged_path && merged_path[0]) {
        loaded_music = Mix_LoadMUS(merged_path);
    }
    if (!loaded_music) {
        prepend_parent_path(parent_merged_path, sizeof(parent_merged_path), merged_path);
        if (parent_merged_path[0]) {
            loaded_music = Mix_LoadMUS(parent_merged_path);
        }
    }
    if (!loaded_music && local_path && local_path[0]) {
        loaded_music = Mix_LoadMUS(local_path);
    }
    return loaded_music;
}

static FILE* open_file_candidate(const char* merged_path, const char* local_path, const char* mode)
{
    FILE* file = NULL;
    char parent_merged_path[PATH_MAX];

    if (merged_path && merged_path[0]) {
        file = fopen(merged_path, mode);
    }
    if (!file) {
        prepend_parent_path(parent_merged_path, sizeof(parent_merged_path), merged_path);
        if (parent_merged_path[0]) {
            file = fopen(parent_merged_path, mode);
        }
    }
    if (!file && local_path && local_path[0]) {
        file = fopen(local_path, mode);
    }
    return file;
}

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
    upTex = NULL;
}

void RenderUpSprite(void) {
    (void)upTex;
}

void CleanupUpSprite(void) {
    destroy_texture(&upTex);
}

void InitSnow(void) {
    int renderW, renderH;
    GetRenderSize(&renderW, &renderH);
    if (renderW < 1) renderW = 1;
    if (renderH < 1) renderH = 1;

    snowTex = load_texture_candidate(renderer, ASSET_OPTIONS_SNOW_MERGED, ASSET_OPTIONS_SNOW);
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
    font = ui_open_arial_font(28, 0);
    if (!font) {
        font = TTF_OpenFont(ASSET_OPTIONS_FONT_MERGED, 28);
    }
    if (!font) {
        char parent_font_path[PATH_MAX];
        prepend_parent_path(parent_font_path, sizeof(parent_font_path), ASSET_OPTIONS_FONT_MERGED);
        font = TTF_OpenFont(parent_font_path, 28);
    }
    if (!font) {
        font = TTF_OpenFont(ASSET_OPTIONS_FONT, 28);
    }
    if (!font) {
        SDL_Log("TTF_OpenFont error: %s", TTF_GetError());
        exit(1);
    }
    ui_apply_font_quality(font);

    g_options_button_font = ui_open_arial_font(24, 0);

    const char* button_candidates[] = {
        ASSET_BUTTON_FONT,
        ASSET_OPTIONS_FONT,
        ASSET_MAIN_MENU_FONT_TEXT
    };
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
    bgTexture = NULL;

    barFullTex = load_texture_candidate(renderer, ASSET_OPTIONS_BAR_FULL_MERGED, ASSET_OPTIONS_BAR_FULL);
    barEmptyTex = load_texture_candidate(renderer, ASSET_OPTIONS_BAR_EMPTY_MERGED, ASSET_OPTIONS_BAR_EMPTY);

    volIcons[0] = load_texture_candidate(renderer, ASSET_OPTIONS_VOLUME_1_MERGED, ASSET_OPTIONS_VOLUME_1);
    volIcons[1] = load_texture_candidate(renderer, ASSET_OPTIONS_VOLUME_2_MERGED, ASSET_OPTIONS_VOLUME_2);
    volIcons[2] = load_texture_candidate(renderer, ASSET_OPTIONS_VOLUME_3_MERGED, ASSET_OPTIONS_VOLUME_3);
    volIcons[3] = load_texture_candidate(renderer, ASSET_OPTIONS_VOLUME_4_MERGED, ASSET_OPTIONS_VOLUME_4);

    sunIcons[0] = load_texture_candidate(renderer, ASSET_OPTIONS_SUN_1_MERGED, ASSET_OPTIONS_SUN_1);
    sunIcons[1] = load_texture_candidate(renderer, ASSET_OPTIONS_SUN_2_MERGED, ASSET_OPTIONS_SUN_2);
    sunIcons[2] = load_texture_candidate(renderer, ASSET_OPTIONS_SUN_3_MERGED, ASSET_OPTIONS_SUN_3);

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
}

void SaveSettings(void) {
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

    click_sfx = load_wav_candidate(ASSET_OPTIONS_CLICK_MERGED, ASSET_OPTIONS_CLICK);
    shoot1 = load_wav_candidate(ASSET_OPTIONS_SHOOT_MERGED, ASSET_OPTIONS_SHOOT);
    shoot2 = load_wav_candidate(ASSET_OPTIONS_SHOOT_MERGED, ASSET_OPTIONS_SHOOT);
    music = load_music_candidate(ASSET_OPTIONS_MUSIC_MERGED, ASSET_OPTIONS_MUSIC);
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
    leftShooter.texture  = load_texture_candidate(renderer, ASSET_OPTIONS_THIEF_LEFT_MERGED, ASSET_OPTIONS_THIEF_LEFT);
    rightShooter.texture = load_texture_candidate(renderer, ASSET_OPTIONS_THIEF_RIGHT_MERGED, ASSET_OPTIONS_THIEF_RIGHT);
    bulletTex = load_texture_candidate(renderer, ASSET_OPTIONS_BULLET_MERGED, ASSET_OPTIONS_BULLET);
    brickTex  = load_texture_candidate(renderer, ASSET_OPTIONS_BRICK_MERGED, ASSET_OPTIONS_BRICK);

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

    FILE* f = open_file_candidate(ASSET_OPTIONS_CREDITS_MERGED, ASSET_OPTIONS_CREDITS, "rb");
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
    "Quit",
    "Return"
};

static int sliderMouseCapture = -1;

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

static SDL_Rect GetMenuActionButtonRect(int index) {
    SDL_Rect rect = GetReturnButtonRect();
    int renderH = SCREEN_HEIGHT;

    GetRenderSize(NULL, &renderH);
    if (index == 5)
        rect.y = GetOptionRowY(5) - rect.h / 2 + ScaleFromBaseHeight(12, renderH);
    else
        rect.y = GetOptionRowY(6) - rect.h / 2 + ScaleFromBaseHeight(12, renderH);

    return rect;
}

static const char* GetOptionLabel(int index) {
    if (index < 0 || index >= MENU_ITEMS) return "";
    if (index == 4) return settings.fullscreen ? "Window" : "Full Screen";
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

    if (index == 5 || index == 6) {
        SDL_Rect buttonRect = GetMenuActionButtonRect(index);
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
        OptionsToggleFullscreen(1);
        return;
    }

    if (hovered == 5) {
        PlayClick();
        if (quit_to_menu) *quit_to_menu = 1;
        if (keep_open) *keep_open = 0;
        return;
    }

    if (hovered == 6) {
        PlayClick();
        if (keep_open) *keep_open = 0;
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

        if (i == 5 || i == 6) {
            SDL_Rect dest = rowRect;
            int hovered = (i == selected);
            TTF_Font* button_font = g_options_button_font ? g_options_button_font : font;

            ui_render_main_menu_style_button(renderer, button_font, &dest, GetOptionLabel(i), hovered, 0);
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
