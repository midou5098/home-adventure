#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../shared/session.h"
#include "../menu/option in game/src/options_internal.h"

#define SCREEN_W 1280
#define SCREEN_H 720

typedef struct {
    int w;
    int h;
} ResolutionOption;

static const ResolutionOption kResolutionOptions[] = {
    {1280, 720},
    {1152, 648},
    {1024, 576},
    {960, 540},
    {854, 480},
    {800, 450},
    {640, 360},
};

#define RESOLUTION_OPTION_COUNT ((int)(sizeof(kResolutionOptions) / sizeof(kResolutionOptions[0])))

int runLevel1(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel2(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel3(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel4(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);

static SDL_Renderer *launcher_renderer = NULL;
static const char *launcher_project_root = NULL;

static void launcher_log(const char *fmt, ...)
{
    FILE *log_file;
    va_list args;

    log_file = fopen(".level_transition.log", "a");
    if (!log_file)
        return;

    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fputc('\n', log_file);
    fclose(log_file);
}

static SDL_Texture *createText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                               SDL_Color color, int *w, int *h)
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    if (w) *w = 0;
    if (h) *h = 0;
    if (!font || !text || !renderer) return NULL;

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        if (w) *w = surface->w;
        if (h) *h = surface->h;
    }
    SDL_FreeSurface(surface);
    return texture;
}

static void drawText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                     int x, int y, SDL_Color color)
{
    SDL_Texture *texture;
    SDL_Rect dst;
    int w;
    int h;

    texture = createText(renderer, font, text, color, &w, &h);
    if (!texture) return;

    dst.x = x;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static void drawCenteredText(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                             int y, SDL_Color color)
{
    SDL_Texture *texture;
    SDL_Rect dst;
    int w;
    int h;

    texture = createText(renderer, font, text, color, &w, &h);
    if (!texture) return;

    dst.x = SCREEN_W / 2 - w / 2;
    dst.y = y;
    dst.w = w;
    dst.h = h;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

static TTF_Font *openLauncherFont(const char *project_root, int ptsize)
{
    char local_font[PATH_MAX];
    const char *fallbacks[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        NULL
    };
    TTF_Font *font = NULL;

    snprintf(local_font, sizeof(local_font), "%s/level1-climb/font.ttf", project_root);
    font = TTF_OpenFont(local_font, ptsize);
    if (font) return font;

    for (int i = 0; fallbacks[i] && !font; i++)
        font = TTF_OpenFont(fallbacks[i], ptsize);

    return font;
}

static int change_to_dir(const char *project_root, const char *subdir)
{
    char path[PATH_MAX];
    size_t root_len;
    size_t subdir_len;

    root_len = strlen(project_root);
    subdir_len = strlen(subdir);
    if (root_len + 1 + subdir_len + 1 > sizeof(path))
        return -1;

    memcpy(path, project_root, root_len);
    path[root_len] = '/';
    memcpy(path + root_len + 1, subdir, subdir_len);
    path[root_len + 1 + subdir_len] = '\0';
    return chdir(path);
}

static void apply_window_mode(SDL_Window *window, SDL_Renderer *renderer,
                              int resolution_idx, int fullscreen_enabled)
{
    Uint32 fullscreen_flag;

    if (!window || !renderer)
        return;
    if (resolution_idx < 0 || resolution_idx >= RESOLUTION_OPTION_COUNT)
        resolution_idx = 0;

    fullscreen_flag = fullscreen_enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    SDL_SetWindowFullscreen(window, fullscreen_flag);
    if (!fullscreen_enabled) {
        SDL_SetWindowSize(window,
                          kResolutionOptions[resolution_idx].w,
                          kResolutionOptions[resolution_idx].h);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
}

static void renderStartConfig(SDL_Renderer *renderer, TTF_Font *title_font, TTF_Font *body_font,
                              const GameSession *session, int selected_row, int resolution_idx,
                              int fullscreen_enabled)
{
    SDL_Color title = {245, 231, 164, 255};
    SDL_Color body = {225, 225, 225, 255};
    SDL_Color muted = {150, 160, 170, 255};
    SDL_Color accent = {90, 170, 255, 255};
    SDL_Rect panel = {200, 96, 880, 556};
    SDL_Rect rows[5] = {
        {260, 216, 760, 56},
        {260, 282, 760, 56},
        {260, 348, 760, 56},
        {260, 414, 760, 56},
        {260, 480, 760, 56}
    };
    GameMode mode = GAME_MODE_SOLO;
    int p1_skin = 1;
    int p2_skin = 2;
    char mode_line[64];
    char p1_skin_line[64];
    char p2_skin_line[96];
    char resolution_line[96];
    char fullscreen_line[96];
    char p1_controls_line[96];
    char p2_controls_line[96];

    SDL_SetRenderDrawColor(renderer, 18, 19, 30, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 36, 41, 58, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 90, 170, 255, 255);
    SDL_RenderDrawRect(renderer, &panel);

    for (int i = 0; i < 5; i++) {
        if (selected_row == i) {
            SDL_SetRenderDrawColor(renderer, 52, 70, 112, 255);
            SDL_RenderFillRect(renderer, &rows[i]);
        }
    }

    if (session) {
        mode = (session->mode == GAME_MODE_DUO) ? GAME_MODE_DUO : GAME_MODE_SOLO;
        p1_skin = (session->player_skin_number[0] == 2) ? 2 : 1;
        p2_skin = (session->player_skin_number[1] == 2) ? 2 : 1;
    }

    if (resolution_idx < 0 || resolution_idx >= RESOLUTION_OPTION_COUNT)
        resolution_idx = 0;

    drawCenteredText(renderer, title_font, "HOME ALONE - MERGED LEVELS", 152, title);
    drawCenteredText(renderer, body_font, "Select mode and skins, then press Enter.", 196, body);

    snprintf(mode_line, sizeof(mode_line), "Mode: %s", session_game_mode_label(mode));
    snprintf(p1_skin_line, sizeof(p1_skin_line), "Player 1 Skin: %d", p1_skin);
    if (mode == GAME_MODE_DUO)
        snprintf(p2_skin_line, sizeof(p2_skin_line), "Player 2 Skin: %d", p2_skin);
    else
        snprintf(p2_skin_line, sizeof(p2_skin_line), "Player 2 Skin: (duo only)");
    snprintf(resolution_line, sizeof(resolution_line), "Resolution: %dx%d",
             kResolutionOptions[resolution_idx].w,
             kResolutionOptions[resolution_idx].h);
    snprintf(fullscreen_line, sizeof(fullscreen_line), "Fullscreen: %s",
             fullscreen_enabled ? "On" : "Off");
    snprintf(p1_controls_line, sizeof(p1_controls_line), "P1 Controls: Arrow Keys + [%s]",
             session_interact_bind_label(INTERACT_BIND_0));
    snprintf(p2_controls_line, sizeof(p2_controls_line), "P2 Controls: WASD + [%s]",
             session_interact_bind_label(INTERACT_BIND_F));

    drawText(renderer, body_font, mode_line, 288, 232, selected_row == 0 ? accent : body);
    drawText(renderer, body_font, p1_skin_line, 288, 298, selected_row == 1 ? accent : body);
    drawText(renderer, body_font, p2_skin_line, 288, 364,
             selected_row == 2 ? accent : (mode == GAME_MODE_DUO ? body : muted));
    drawText(renderer, body_font, resolution_line, 288, 430, selected_row == 3 ? accent : body);
    drawText(renderer, body_font, fullscreen_line, 288, 496, selected_row == 4 ? accent : body);

    drawCenteredText(renderer, body_font, p1_controls_line, 560, body);
    drawCenteredText(renderer, body_font, p2_controls_line, 588,
                     mode == GAME_MODE_DUO ? body : muted);
    drawCenteredText(renderer, body_font, "If both duo players are eliminated, run returns to menu.", 620, muted);
    drawCenteredText(renderer, body_font, "Left/Right changes value - Up/Down changes row", 648, body);
    drawCenteredText(renderer, body_font, "Resolution is used in windowed mode. Fullscreen applies to gameplay too.", 676, muted);

    SDL_RenderPresent(renderer);
}

static int configureSession(GameSession *session, SDL_Window *window, SDL_Renderer *renderer,
                            const char *project_root, int *resolution_idx,
                            int *fullscreen_enabled)
{
    TTF_Font *title_font;
    TTF_Font *body_font;
    SDL_Event event;
    int running = 1;
    int selected_row = 0;
    int local_resolution_idx;
    int local_fullscreen_enabled;

    title_font = openLauncherFont(project_root, 44);
    body_font = openLauncherFont(project_root, 24);
    if (!body_font) body_font = title_font;

    local_resolution_idx = (resolution_idx ? *resolution_idx : 0);
    if (local_resolution_idx < 0 || local_resolution_idx >= RESOLUTION_OPTION_COUNT)
        local_resolution_idx = 0;
    local_fullscreen_enabled = (fullscreen_enabled && *fullscreen_enabled) ? 1 : 0;

    session->mode = (session->mode == GAME_MODE_DUO) ? GAME_MODE_DUO : GAME_MODE_SOLO;
    session->player_skin_number[0] = (session->player_skin_number[0] == 2) ? 2 : 1;
    session->player_skin_number[1] = (session->player_skin_number[1] == 2) ? 2 : 1;

    apply_window_mode(window, renderer, local_resolution_idx, local_fullscreen_enabled);

    while (running) {
        renderStartConfig(renderer, title_font, body_font, session, selected_row,
                          local_resolution_idx, local_fullscreen_enabled);

        while (SDL_WaitEvent(&event)) {
            if (event.type == SDL_QUIT) {
                session->quit_requested = 1;
                running = 0;
                break;
            }

            if (event.type != SDL_KEYDOWN)
                continue;

            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    session->quit_requested = 1;
                    running = 0;
                    break;
                case SDLK_UP:
                case SDLK_w:
                    selected_row = (selected_row - 1 + 5) % 5;
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    selected_row = (selected_row + 1) % 5;
                    break;
                case SDLK_LEFT:
                case SDLK_a:
                    if (selected_row == 0)
                        session->mode = (session->mode == GAME_MODE_DUO) ? GAME_MODE_SOLO : GAME_MODE_DUO;
                    else if (selected_row == 1)
                        session->player_skin_number[0] = (session->player_skin_number[0] == 1) ? 2 : 1;
                    else if (selected_row == 2) {
                        if (session->mode == GAME_MODE_DUO)
                            session->player_skin_number[1] = (session->player_skin_number[1] == 1) ? 2 : 1;
                    } else if (selected_row == 3) {
                        local_resolution_idx = (local_resolution_idx - 1 + RESOLUTION_OPTION_COUNT)
                            % RESOLUTION_OPTION_COUNT;
                        apply_window_mode(window, renderer, local_resolution_idx, local_fullscreen_enabled);
                    } else {
                        local_fullscreen_enabled = !local_fullscreen_enabled;
                        apply_window_mode(window, renderer, local_resolution_idx, local_fullscreen_enabled);
                    }
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    if (selected_row == 0)
                        session->mode = (session->mode == GAME_MODE_DUO) ? GAME_MODE_SOLO : GAME_MODE_DUO;
                    else if (selected_row == 1)
                        session->player_skin_number[0] = (session->player_skin_number[0] == 1) ? 2 : 1;
                    else if (selected_row == 2) {
                        if (session->mode == GAME_MODE_DUO)
                            session->player_skin_number[1] = (session->player_skin_number[1] == 1) ? 2 : 1;
                    } else if (selected_row == 3) {
                        local_resolution_idx = (local_resolution_idx + 1) % RESOLUTION_OPTION_COUNT;
                        apply_window_mode(window, renderer, local_resolution_idx, local_fullscreen_enabled);
                    } else {
                        local_fullscreen_enabled = !local_fullscreen_enabled;
                        apply_window_mode(window, renderer, local_resolution_idx, local_fullscreen_enabled);
                    }
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    running = 0;
                    break;
                default:
                    break;
            }

            session->skin_number = session->player_skin_number[0];
            session->control_scheme = CONTROL_SCHEME_ARROWS;

            break;
        }
    }

    if (title_font) TTF_CloseFont(title_font);
    if (body_font && body_font != title_font) TTF_CloseFont(body_font);
    if (resolution_idx)
        *resolution_idx = local_resolution_idx;
    if (fullscreen_enabled)
        *fullscreen_enabled = local_fullscreen_enabled;

    return session->quit_requested ? 0 : 1;
}

static int wait_for_key_or_timeout(GameSession *session, Uint32 timeout_ms)
{
    Uint32 start = SDL_GetTicks();
    SDL_Event event;

    while (!session->quit_requested) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                session->quit_requested = 1;
                return 0;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    session->quit_requested = 1;
                return session->quit_requested ? 0 : 1;
            }
        }

        if (timeout_ms > 0 && SDL_GetTicks() - start >= timeout_ms)
            return 1;
        SDL_Delay(16);
    }

    return 0;
}

static void show_results_screen(GameSession *session)
{
    TTF_Font *title_font;
    TTF_Font *body_font;
    SDL_Color title = {245, 231, 164, 255};
    SDL_Color body = {235, 235, 235, 255};
    char line[8][96];

    session_calculate_total_points(session);
    SDL_RenderSetLogicalSize(launcher_renderer, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawColor(launcher_renderer, 0, 0, 0, 255);
    SDL_RenderClear(launcher_renderer);

    title_font = openLauncherFont(launcher_project_root, 42);
    body_font = openLauncherFont(launcher_project_root, 24);
    if (!body_font) body_font = title_font;

    snprintf(line[0], sizeof(line[0]), "Level 1 points: %d", session->level1.points);
    snprintf(line[1], sizeof(line[1]), "Level 2 points: %d", session->level2.points);
    snprintf(line[2], sizeof(line[2]), "Level 3 points: %d", session->level3.points);
    snprintf(line[3], sizeof(line[3]), "Level 4 points: %d", session->level5.points);
    line[4][0] = '\0';
    snprintf(line[5], sizeof(line[5]), "Total points: %d", session->total_points);
    snprintf(line[6], sizeof(line[6]), "Grade: %c", session->grade);
    line[7][0] = '\0';

    drawCenteredText(launcher_renderer, title_font, "FINAL RESULTS", 120, title);
    for (int i = 0; i < 7; i++)
        drawCenteredText(launcher_renderer, body_font, line[i], 216 + (i * 44), body);
    drawCenteredText(launcher_renderer, body_font, session_grade_flavor(session->grade), 620, title);
    drawCenteredText(launcher_renderer, body_font, "Press any key to exit.", 666, body);
    SDL_RenderPresent(launcher_renderer);

    if (title_font) TTF_CloseFont(title_font);
    if (body_font && body_font != title_font) TTF_CloseFont(body_font);
    wait_for_key_or_timeout(session, 0);
}

int main(int argc, char *argv[])
{
    GameSession session;
    Settings global_settings;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    char project_root[PATH_MAX];
    int level_rc = 0;
    int exit_code = 0;
    int resolution_idx = 0;
    int fullscreen_enabled = 0;

    (void)argc;
    (void)argv;

    if (!getcwd(project_root, sizeof(project_root))) {
        fprintf(stderr, "Failed to resolve project root.\n");
        return 1;
    }

    launcher_log("[launcher] start");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    IMG_Init(IMG_INIT_PNG);

    window = SDL_CreateWindow("Home Alone - Merged",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer, SCREEN_W, SCREEN_H);
    launcher_renderer = renderer;
    launcher_project_root = project_root;

    options_settings_set_defaults(&global_settings);
    if (options_settings_load_global(&global_settings))
        fullscreen_enabled = global_settings.fullscreen ? 1 : 0;
    apply_window_mode(window, renderer, resolution_idx, fullscreen_enabled);

    session_reset(&session);

    while (!session.quit_requested) {
        if (chdir(project_root) != 0) {
            fprintf(stderr, "Failed to restore project root before menu.\n");
            exit_code = 1;
            goto cleanup;
        }

        session_reset(&session);
        session_clear_level_life_carry();
        if (options_settings_load_global(&global_settings))
            fullscreen_enabled = global_settings.fullscreen ? 1 : 0;
        apply_window_mode(window, renderer, resolution_idx, fullscreen_enabled);

        if (!configureSession(&session, window, renderer, project_root,
                              &resolution_idx, &fullscreen_enabled)) {
            if (!session.quit_requested) {
                fprintf(stderr, "Session configuration failed.\n");
                exit_code = 1;
            }
            goto cleanup;
        }

        session_set_start_config(&session, session.mode,
                                 session.player_skin_number[0],
                                 session.player_skin_number[1]);
        launcher_log("[launcher] start_run mode=%s p1_skin=%d p2_skin=%d",
                     session_game_mode_label(session.mode),
                     session.player_skin_number[0],
                     session.player_skin_number[1]);

        if (change_to_dir(project_root, "level1-climb") != 0) {
            fprintf(stderr, "Failed to change to level1-climb.\n");
            goto cleanup;
        }
        level_rc = runLevel1(&session, window, renderer);
        launcher_log("[launcher] level1 rc=%d completed=%d lives_remaining=%d bonus=%d quit=%d",
                     level_rc,
                     session.level1.completed,
                     session.level1.lives_remaining,
                     session.bonus_lives_for_level2,
                     session.quit_requested);
        if (level_rc != 0) {
            fprintf(stderr, "Level 1 failed with code %d.\n", level_rc);
            exit_code = 1;
            goto cleanup;
        }
        if (session.quit_requested)
            goto cleanup;
        if (!session.level1.completed) {
            launcher_log("[launcher] return_to_menu level=1 reason=level_failed");
            continue;
        }

        if (change_to_dir(project_root, "level2-chase") != 0) {
            fprintf(stderr, "Failed to change to level2-chase.\n");
            goto cleanup;
        }
        level_rc = runLevel2(&session, window, renderer);
        launcher_log("[launcher] level2 rc=%d completed=%d start_lives=%d lost=%d quit=%d",
                     level_rc,
                     session.level2.completed,
                     session.level2.starting_lives,
                     session.level2.player_lives_lost,
                     session.quit_requested);
        if (level_rc != 0) {
            fprintf(stderr, "Level 2 failed with code %d.\n", level_rc);
            exit_code = 1;
            goto cleanup;
        }
        if (session.quit_requested)
            goto cleanup;
        if (!session.level2.completed) {
            launcher_log("[launcher] return_to_menu level=2 reason=level_failed");
            continue;
        }

        if (change_to_dir(project_root, "level3-hell") != 0) {
            fprintf(stderr, "Failed to change to level3-hell.\n");
            goto cleanup;
        }
        level_rc = runLevel3(&session, window, renderer);
        launcher_log("[launcher] level3 rc=%d completed=%d quit=%d",
                     level_rc, session.level3.completed, session.quit_requested);
        if (level_rc != 0) {
            fprintf(stderr, "Level 3 failed with code %d.\n", level_rc);
            exit_code = 1;
            goto cleanup;
        }
        if (session.quit_requested)
            goto cleanup;
        if (!session.level3.completed) {
            launcher_log("[launcher] return_to_menu level=3 reason=level_failed");
            continue;
        }

        if (change_to_dir(project_root, "level4-pool") != 0) {
            fprintf(stderr, "Failed to change to level4-pool.\n");
            goto cleanup;
        }
        level_rc = runLevel4(&session, window, renderer);
        launcher_log("[launcher] level4 rc=%d completed=%d quit=%d",
                     level_rc, session.level5.completed, session.quit_requested);
        if (level_rc != 0) {
            fprintf(stderr, "Level 4 failed with code %d.\n", level_rc);
            exit_code = 1;
            goto cleanup;
        }
        if (session.quit_requested)
            goto cleanup;
        if (!session.level5.completed) {
            launcher_log("[launcher] return_to_menu level=4 reason=level_failed");
            continue;
        }

        show_results_screen(&session);
        session_clear_level_life_carry();
        launcher_log("[launcher] full_run_complete total=%d grade=%c", session.total_points, session.grade);
        break;
    }

cleanup:
    launcher_log("[launcher] cleanup exit_code=%d quit=%d", exit_code, session.quit_requested);
    if (chdir(project_root) != 0)
        fprintf(stderr, "Warning: failed to restore project root.\n");
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return exit_code;
}
