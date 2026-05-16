#include "merged_levels.h"

#include "final_cutscene.h"
#include "game_progress.h"
#include "options_internal.h"
#include "online_client.h"
#include "ui_shared.h"

#include "../../lvls/shared/session.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int runLevel1(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel2(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel3(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);
int runLevel4(GameSession *session, SDL_Window *window, SDL_Renderer *renderer);

static void merged_levels_log(const char* fmt, ...)
{
    FILE* f = NULL;
    va_list args;

    f = fopen(".level_transition.log", "a");
    if (!f) return;

    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}

static int merged_levels_map_skin(int skin_id)
{
    return (skin_id == 2) ? 2 : 1;
}

static ControlScheme merged_levels_map_control(int control_scheme)
{
    if (control_scheme == 1) return CONTROL_SCHEME_WASD;
    if (control_scheme == 3) return CONTROL_SCHEME_CONTROLLER;
    return CONTROL_SCHEME_ARROWS;
}

static int merged_levels_saved_current_level(const GameProgress* progress)
{
    if (!progress) return 1;
    if (progress->current_level == 5) {
        return 4;
    }
    if (progress->current_level == 1 || progress->current_level == 2 ||
        progress->current_level == 3 || progress->current_level == 4) {
        return progress->current_level;
    }
    return 1;
}

static void merged_levels_compute_level_entry_lives(const GameSession* session,
                                                    int current_level,
                                                    int* out_p1_lives,
                                                    int* out_p2_lives)
{
    int p1_lives = 9;
    int p2_lives = 0;

    if (!session) {
        if (out_p1_lives) *out_p1_lives = p1_lives;
        if (out_p2_lives) *out_p2_lives = p2_lives;
        return;
    }

    if (current_level == 1) {
        p1_lives = (session->level1.lives_remaining > 0) ? session->level1.lives_remaining : 9;
        p2_lives = (session->mode == GAME_MODE_DUO) ? p1_lives : 0;
    } else if (current_level == 2) {
        if (session->level2.starting_lives > 0) {
            p1_lives = session->level2.starting_lives - session->level2.player_lives_lost;
            p2_lives = (session->mode == GAME_MODE_DUO && session->level2.multiplayer)
                ? (session->level2.starting_lives - session->level2.marv_lives_lost)
                : ((session->mode == GAME_MODE_DUO) ? p1_lives : 0);
        } else {
            p1_lives = session->level1.lives_remaining + session->bonus_lives_for_level2;
            p2_lives = (session->mode == GAME_MODE_DUO) ? p1_lives : 0;
        }
        if (p1_lives < 1) p1_lives = 1;
        if (p2_lives < 0) p2_lives = 0;
    } else {
        p1_lives = session->level2.starting_lives - session->level2.player_lives_lost;
        p2_lives = (session->mode == GAME_MODE_DUO && session->level2.multiplayer)
            ? (session->level2.starting_lives - session->level2.marv_lives_lost)
            : 0;
        if (p1_lives < 0) p1_lives = 0;
        if (p2_lives < 0) p2_lives = 0;
    }

    if (out_p1_lives) *out_p1_lives = p1_lives;
    if (out_p2_lives) *out_p2_lives = p2_lives;
}

static int merged_levels_change_to_dir(const char* root, const char* subdir)
{
    char path[PATH_MAX];

    if (!root || !subdir) return 0;
    if (snprintf(path, sizeof(path), "%s/%s", root, subdir) >= (int)sizeof(path)) return 0;
    return chdir(path) == 0;
}

static void merged_levels_apply_selection(GameSession* session, const GameSelection* selection)
{
    int duo_mode = 0;
    int p1_skin = 1;
    int p2_skin = 2;

    if (!session) return;

    duo_mode = (selection && selection->duo_mode) ? 1 : 0;
    if (selection) {
        p1_skin = merged_levels_map_skin(selection->selected_skin[0]);
        p2_skin = merged_levels_map_skin(selection->selected_skin[1]);
    }

    session_set_start_config(session,
                             duo_mode ? GAME_MODE_DUO : GAME_MODE_SOLO,
                             p1_skin,
                             p2_skin);

    if (selection) {
        session->player_control_scheme[0] = merged_levels_map_control(selection->control_scheme[0]);
        session->player_control_scheme[1] = merged_levels_map_control(selection->control_scheme[1]);
        session->control_scheme = session->player_control_scheme[0];
        session->save_enabled = selection->save_enabled ? 1 : 0;
        session->player_interact_bind[0] =
            (session->player_control_scheme[0] == CONTROL_SCHEME_WASD) ? INTERACT_BIND_F : INTERACT_BIND_0;
        session->player_interact_bind[1] =
            (session->player_control_scheme[1] == CONTROL_SCHEME_WASD) ? INTERACT_BIND_F : INTERACT_BIND_0;
    }
}

static void merged_levels_prepare_shared_runtime(SDL_Window* window,
                                                 SDL_Renderer* renderer,
                                                 const char* title)
{
    if (!window || !renderer) return;

    SDL_StopTextInput();
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_CaptureMouse(SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_SetWindowTitle(window, title ? title : "Home Alone - Merged");
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderSetViewport(renderer, NULL);
    SDL_RenderSetClipRect(renderer, NULL);
    SDL_RenderSetScale(renderer, 1.0f, 1.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer, 1280, 720);
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

static void merged_levels_restore_session_from_progress(GameSession* session, const GameProgress* progress)
{
    int p1_remaining = 0;
    int p2_remaining = 0;

    if (!session || !progress) return;

    session->level1.completed = progress->level1_completed;
    session->level1.lives_remaining = progress->level1_lives_remaining;
    session->level1.points = progress->level1_points;
    session->bonus_lives_for_level2 = progress->bonus_lives_for_level2;

    session->level2.completed = progress->level2_completed;
    session->level2.starting_lives = progress->level2_starting_lives;
    session->level2.player_lives_lost = progress->level2_player_lives_lost;
    session->level2.marv_lives_lost = progress->level2_marv_lives_lost;
    session->level2.multiplayer = progress->level2_multiplayer;
    session->level2.mini_key_found = progress->level2_mini_key_found;
    session->level2.points = progress->level2_points;

    session->level3.completed = progress->level3_completed;
    session->level3.points = progress->level3_points;
    session->level5.completed = progress->level5_completed;
    session->level5.points = progress->level5_points;
    session->total_points = progress->score;

    p1_remaining = progress->current_lives[0];
    p2_remaining = progress->current_lives[1];
    if (p1_remaining > 0 && !session->level2.completed) {
        session->level1.lives_remaining = p1_remaining;
    }
    if (session->level2.starting_lives > 0) {
        if (p1_remaining >= 0) {
            session->level2.player_lives_lost = session->level2.starting_lives - p1_remaining;
            if (session->level2.player_lives_lost < 0) session->level2.player_lives_lost = 0;
        }
        if (session->mode == GAME_MODE_DUO && session->level2.multiplayer && p2_remaining >= 0) {
            session->level2.marv_lives_lost = session->level2.starting_lives - p2_remaining;
            if (session->level2.marv_lives_lost < 0) session->level2.marv_lives_lost = 0;
        }
    }
}

static int merged_levels_current_mode_from_selection_or_session(const GameSelection* selection,
                                                                const GameSession* session)
{
    if (selection) return selection->duo_mode ? 2 : 1;
    if (session) return (session->mode == GAME_MODE_DUO) ? 2 : 1;
    return 1;
}

static void merged_levels_save_checkpoint(const GameSession* session,
                                          const GameSelection* selection,
                                          int current_level)
{
    GameProgress progress = {0};
    char save_path[256];
    int p1_lives = 0;
    int p2_lives = 0;

    if (!session) return;
    merged_levels_compute_level_entry_lives(session, current_level, &p1_lives, &p2_lives);

    game_progress_get(&progress);
    if (selection) {
        progress.selection = *selection;
    }
    progress.selection.resume_from_save = 1;
    progress.save_enabled = 1;
    progress.current_level = current_level;
    progress.current_lives[0] = p1_lives;
    progress.current_lives[1] = p2_lives;
    progress.score = session->total_points;
    progress.has_started_game = 1;
    progress.level1_completed = session->level1.completed;
    progress.level1_lives_remaining = session->level1.lives_remaining;
    progress.level1_points = session->level1.points;
    progress.bonus_lives_for_level2 = session->bonus_lives_for_level2;
    progress.level2_completed = session->level2.completed;
    progress.level2_starting_lives = session->level2.starting_lives;
    progress.level2_player_lives_lost = session->level2.player_lives_lost;
    progress.level2_marv_lives_lost = session->level2.marv_lives_lost;
    progress.level2_multiplayer = session->level2.multiplayer;
    progress.level2_mini_key_found = session->level2.mini_key_found;
    progress.level2_points = session->level2.points;
    progress.level3_completed = session->level3.completed;
    progress.level3_points = session->level3.points;
    progress.level5_completed = session->level5.completed;
    progress.level5_points = session->level5.points;
    game_progress_set(&progress);
    game_progress_copy_last_save_path_for_mode(merged_levels_current_mode_from_selection_or_session(selection, session),
                                               save_path,
                                               sizeof(save_path));
    if (game_progress_save_global_to_path(save_path)) {
        online_client_notify_save(current_level,
                                  progress.score,
                                  p1_lives,
                                  p2_lives,
                                  "CHECKPOINT");
    }
}

static void merged_levels_clear_resume_progress(int mode)
{
    char save_path[256];

    game_progress_copy_last_save_path_for_mode(mode, save_path, sizeof(save_path));
    game_progress_delete_global_path(save_path);
}

static void merged_levels_sync_progress(const GameSession* session, int save_enabled)
{
    GameProgress progress = {0};
    char save_path[256];
    int highest_level = 1;
    int current_level = 1;
    int total_score = 0;

    if (!session) return;

    if (session->level1.completed) {
        highest_level = 2;
        current_level = 2;
    }
    if (session->level2.completed) {
        highest_level = 3;
        current_level = 3;
    }
    if (session->level3.completed) {
        highest_level = 4;
        current_level = 4;
    }
    if (session->level5.completed) {
        highest_level = 4;
        current_level = 4;
    }

    total_score = session->total_points;
    if (total_score < 0) total_score = 0;

    game_progress_get(&progress);
    if (progress.highest_level_unlocked > highest_level) highest_level = progress.highest_level_unlocked;
    game_progress_set_level_progress(current_level, highest_level, total_score);
    game_progress_mark_started_game(1);
    if (save_enabled) {
        game_progress_copy_last_save_path_for_mode((session->mode == GAME_MODE_DUO) ? 2 : 1,
                                                   save_path,
                                                   sizeof(save_path));
        if (game_progress_save_global_to_path(save_path)) {
            int p1_lives = 0;
            int p2_lives = 0;
            merged_levels_compute_level_entry_lives(session, current_level, &p1_lives, &p2_lives);
            online_client_notify_save(current_level,
                                      total_score,
                                      p1_lives,
                                      p2_lives,
                                      "PROGRESS_SYNC");
        }
    }
}

static int merged_levels_current_active_level(int start_level, const GameSession* session)
{
    if (!session) return start_level;
    if (start_level <= 1 && !session->level1.completed) return 1;
    if (start_level <= 2 && !session->level2.completed) return 2;
    if (start_level <= 3 && !session->level3.completed) return 3;
    if (start_level <= 4 && !session->level5.completed) return 4;
    return 4;
}

static void merged_levels_seed_final_prerequisites(GameSession* session)
{
    if (!session) return;

    session->level1.completed = 1;
    if (session->level1.lives_remaining <= 0) session->level1.lives_remaining = 9;
    if (session->level1.points <= 0) session->level1.points = 1000;

    session->level2.completed = 1;
    if (session->level2.starting_lives <= 0) session->level2.starting_lives = 9;
    session->level2.player_lives_lost = 0;
    session->level2.marv_lives_lost = -1;
    session->level2.mini_key_found = 1;
    if (session->level2.points <= 0) session->level2.points = 1000;

    session->level3.completed = 1;
    session->level3.points = 0;
    session->dev_jump_to_final = 0;
    session_calculate_total_points(session);
}

int merged_levels_run_from_level(SDL_Window* window,
                                 SDL_Renderer* renderer,
                                 const GameSelection* selection,
                                 int forced_start_level)
{
    char lvls_root[PATH_MAX];
    char original_cwd[PATH_MAX];
    char save_path[256];
    GameSession session;
    GameProgress progress = {0};
    int start_level = 1;
    int save_mode = 1;
    int rc = 0;
    Uint32 run_start_ticks = SDL_GetTicks();

    if (!window || !renderer) return 0;
    if (!getcwd(original_cwd, sizeof(original_cwd))) return 0;
    if (!ui_resolve_asset_path("lvls", lvls_root, sizeof(lvls_root))) return 0;

    session_reset(&session);
    session_clear_level_life_carry();
    merged_levels_apply_selection(&session, selection);
    save_mode = merged_levels_current_mode_from_selection_or_session(selection, &session);
    game_progress_copy_last_save_path_for_mode(save_mode, save_path, sizeof(save_path));
    if (!selection || !selection->save_enabled) {
        merged_levels_clear_resume_progress(save_mode);
    }
    if (selection && selection->resume_from_save &&
        game_progress_load_global_from_path(save_path)) {
        game_progress_get(&progress);
        if (game_progress_is_resumable(&progress)) {
            merged_levels_restore_session_from_progress(&session, &progress);
            start_level = merged_levels_saved_current_level(&progress);
        }
    }
    if (forced_start_level >= 1 && forced_start_level <= 4) {
        start_level = forced_start_level;
        if (start_level > 1) {
            session.level1.completed = 1;
            session.level1.lives_remaining = 9;
            session.level1.points = 1000;
        }
        if (start_level > 2) {
            session.level2.completed = 1;
            session.level2.starting_lives = 9;
            session.level2.player_lives_lost = 0;
            session.level2.marv_lives_lost = -1;
            session.level2.mini_key_found = 1;
            session.level2.points = 1000;
        }
        if (start_level > 3) {
            session.level3.completed = 1;
            session.level3.points = 0;
        }
        session_calculate_total_points(&session);
    }

    merged_levels_prepare_shared_runtime(window, renderer, "Home Alone - Level 1");
    merged_levels_log("[merged] start_level=%d mode=%d ctrl1=%d ctrl2=%d skin1=%d skin2=%d",
                      start_level,
                      session.mode,
                      session.player_control_scheme[0],
                      session.player_control_scheme[1],
                      session.player_skin_number[0],
                      session.player_skin_number[1]);

    if (start_level <= 1 && !merged_levels_change_to_dir(lvls_root, "level1-climb")) {
        if (chdir(original_cwd) != 0) {
            return 0;
        }
        return 0;
    }
    if (start_level <= 1) {
        if (selection && selection->save_enabled) {
            merged_levels_save_checkpoint(&session, selection, 1);
        }
        merged_levels_prepare_shared_runtime(window, renderer, "Home Alone - Level 1");
        rc = runLevel1(&session, window, renderer);
        merged_levels_log("[merged] level1 rc=%d completed=%d quit=%d lives=%d score=%d",
                          rc,
                          session.level1.completed,
                          session.quit_requested,
                          session.level1.lives_remaining,
                          session.level1.points);
        if (rc != 0 || session.quit_requested || !session.level1.completed) goto done;
        session_calculate_total_points(&session);
        if (session.dev_jump_to_level2) {
            session.dev_jump_to_level2 = 0;
            goto run_level2;
        }
        if (session.dev_jump_to_final_cutscene) {
            final_cutscene_run_from_chase(window, renderer);
            goto done;
        }
        if (session.dev_jump_to_final) {
            merged_levels_seed_final_prerequisites(&session);
            goto run_level4;
        }
    }

run_level2:
    if (start_level <= 2) {
        if (!merged_levels_change_to_dir(lvls_root, "level2-chase")) goto done;
        if (selection && selection->save_enabled) {
            merged_levels_save_checkpoint(&session, selection, 2);
        }
        merged_levels_prepare_shared_runtime(window, renderer, "Home Alone - Level 2");
        rc = runLevel2(&session, window, renderer);
        merged_levels_log("[merged] level2 rc=%d completed=%d quit=%d start_lives=%d lost1=%d lost2=%d score=%d",
                          rc,
                          session.level2.completed,
                          session.quit_requested,
                          session.level2.starting_lives,
                          session.level2.player_lives_lost,
                          session.level2.marv_lives_lost,
                          session.level2.points);
        if (rc != 0 || session.quit_requested || !session.level2.completed) goto done;
        session_calculate_total_points(&session);
        if (session.dev_jump_to_final_cutscene) {
            final_cutscene_run_from_chase(window, renderer);
            goto done;
        }
        if (session.dev_jump_to_final) {
            merged_levels_seed_final_prerequisites(&session);
            goto run_level4;
        }
    }

    if (start_level <= 3) {
        if (!merged_levels_change_to_dir(lvls_root, "level3-hell")) goto done;
        if (selection && selection->save_enabled) {
            merged_levels_save_checkpoint(&session, selection, 3);
        }
        merged_levels_prepare_shared_runtime(window, renderer, "Home Alone - Level 3");
        rc = runLevel3(&session, window, renderer);
        merged_levels_log("[merged] level3 rc=%d completed=%d quit=%d score=%d",
                          rc,
                          session.level3.completed,
                          session.quit_requested,
                          session.level3.points);
        if (rc != 0 || session.quit_requested || !session.level3.completed) goto done;
        session_calculate_total_points(&session);
        if (session.dev_jump_to_final_cutscene) {
            final_cutscene_run_from_chase(window, renderer);
            goto done;
        }
        if (session.dev_jump_to_final) {
            merged_levels_seed_final_prerequisites(&session);
            goto run_level4;
        }
    }

run_level4:
    if (start_level <= 4) {
        if (!merged_levels_change_to_dir(lvls_root, "level4-pool")) goto done;
        if (selection && selection->save_enabled) {
            merged_levels_save_checkpoint(&session, selection, 4);
        }
        merged_levels_prepare_shared_runtime(window, renderer, "Home Alone - Level 4");
        rc = runLevel4(&session, window, renderer);
        merged_levels_log("[merged] level4 rc=%d completed=%d quit=%d score=%d",
                          rc,
                          session.level5.completed,
                          session.quit_requested,
                          session.level5.points);
        if (rc != 0 || session.quit_requested || !session.level5.completed) goto done;
        session_calculate_total_points(&session);
        if (session.dev_jump_to_final_cutscene) {
            final_cutscene_run_from_chase(window, renderer);
        } else {
            final_cutscene_run(window, renderer);
        }
    }

done:
    {
        int should_clear_resume = 0;
        int should_sync_progress = 0;
        int allow_save_sync = 0;

    session_calculate_total_points(&session);
        should_clear_resume =
            (((start_level <= 1 && !session.level1.completed) && !session.quit_requested) ||
             ((start_level <= 2 && session.level1.completed && !session.level2.completed) && !session.quit_requested) ||
             ((start_level <= 3 && session.level2.completed && !session.level3.completed) && !session.quit_requested) ||
             ((start_level <= 4 && session.level3.completed && !session.level5.completed) && !session.quit_requested) ||
             session.level5.completed ||
             rc != 0);
        should_sync_progress =
            (session.level1.completed ||
             session.level2.completed ||
             session.level3.completed ||
             session.level5.completed);
        allow_save_sync = (selection && selection->save_enabled && !should_clear_resume) ? 1 : 0;

        if (selection && selection->save_enabled && session.quit_requested) {
            merged_levels_save_checkpoint(&session,
                                          selection,
                                          merged_levels_current_active_level(start_level, &session));
        }

        {
            GameProgress best_progress = {0};
            GameLeaderboardEntry leaderboard_entry;
            int health_left = 0;
            int unused_p2_lives = 0;
            int elapsed_sec = (int)((SDL_GetTicks() - run_start_ticks) / 1000u);

            game_progress_get(&best_progress);
            game_progress_update_best_score(GAME_PROGRESS_BEST_SCORE_PATH,
                                            best_progress.player_name,
                                            session.total_points);
            merged_levels_compute_level_entry_lives(&session, 4, &health_left, &unused_p2_lives);
            memset(&leaderboard_entry, 0, sizeof(leaderboard_entry));
            snprintf(leaderboard_entry.player_name,
                     sizeof(leaderboard_entry.player_name),
                     "%s",
                     best_progress.player_name[0] ? best_progress.player_name : "PLAYER");
            leaderboard_entry.score = session.total_points;
            leaderboard_entry.time_sec = elapsed_sec;
            leaderboard_entry.keys_left = session.level1.keys_left;
            leaderboard_entry.keys_spent = session.level1.keys_spent;
            leaderboard_entry.health_left = health_left;
            if (session.level5.completed || session.total_points > 0) {
                game_progress_record_leaderboard_entry(GAME_PROGRESS_LEADERBOARD_PATH, &leaderboard_entry);
            }
        }

        if (should_clear_resume) {
            merged_levels_clear_resume_progress(save_mode);
        }
        if (should_sync_progress) {
            merged_levels_sync_progress(&session, allow_save_sync);
        }
    }
    session_clear_level_life_carry();
    if (chdir(original_cwd) != 0) {
        return 0;
    }
    SDL_SetWindowTitle(window, "menu");
    return (rc == 0);
}

int merged_levels_run(SDL_Window* window, SDL_Renderer* renderer, const GameSelection* selection)
{
    return merged_levels_run_from_level(window, renderer, selection, 0);
}
