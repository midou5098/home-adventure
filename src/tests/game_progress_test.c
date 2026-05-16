#include "game_progress.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int expect_int(const char* label, int got, int want)
{
    if (got == want) return 1;
    fprintf(stderr, "FAIL %s: got=%d want=%d\n", label, got, want);
    return 0;
}

static int expect_string(const char* label, const char* got, const char* want)
{
    if (strcmp(got, want) == 0) return 1;
    fprintf(stderr, "FAIL %s: got='%s' want='%s'\n", label, got, want);
    return 0;
}

static int write_legacy_save_file(const char* path)
{
    FILE* f = NULL;

    if (!path) return 0;

    f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "player_name,duo_mode,player_count,selected_skin1,selected_skin2,control_scheme1,control_scheme2,resume_from_save,save_enabled,master,music,vfx,brightness,fullscreen,current_lives1,current_lives2,current_level,highest_level_unlocked,score,has_started_game,level1_completed,level1_lives_remaining,level1_points,bonus_lives_for_level2,level2_completed,level2_starting_lives,level2_player_lives_lost,level2_marv_lives_lost,level2_multiplayer,level2_mini_key_found,level2_points,level3_completed,level3_points,level5_completed,level5_points\n");
    fprintf(f, "\"legacy\",1,2,2,1,1,2,1,1,10,9,8,7,1,5,4,5,5,1500,1,1,7,320,2,1,11,2,1,1,1,910,1,590,1,590\n");
    fclose(f);
    return 1;
}

int main(void)
{
    char original_cwd[1024];
    const char* save_path = "saves/test_game_progress.csv";
    const char* legacy_path = "saves/test_game_progress_legacy.csv";
    const char* best_score_path = "saves/test_best_score.csv";
    GameProgress progress = {0};
    GameProgress loaded = {0};
    FILE* f = NULL;
    char best_name[32];
    int best_score = 0;
    int ok = 1;

    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        fprintf(stderr, "FAIL unable to resolve cwd\n");
        return 1;
    }

    remove(save_path);
    remove(legacy_path);
    remove(best_score_path);

    game_progress_set_defaults(&progress);
    snprintf(progress.player_name, sizeof(progress.player_name), "tester");
    progress.selection.duo_mode = 0;
    progress.selection.player_count = 1;
    progress.selection.selected_skin[0] = 2;
    progress.selection.selected_skin[1] = 0;
    progress.selection.control_scheme[0] = 1;
    progress.selection.control_scheme[1] = 0;
    progress.selection.resume_from_save = 1;
    progress.save_enabled = 1;
    progress.current_lives[0] = 6;
    progress.current_lives[1] = 0;
    progress.current_level = 2;
    progress.highest_level_unlocked = 3;
    progress.score = 1234;
    progress.has_started_game = 1;
    progress.level1_completed = 1;
    progress.level1_lives_remaining = 6;
    progress.level1_points = 250;
    progress.bonus_lives_for_level2 = 2;
    progress.level2_completed = 1;
    progress.level2_starting_lives = 8;
    progress.level2_player_lives_lost = 2;
    progress.level2_marv_lives_lost = -1;
    progress.level2_multiplayer = 0;
    progress.level2_mini_key_found = 1;
    progress.level2_points = 900;
    progress.level3_completed = 0;
    progress.level3_points = 0;

    if (chdir("lvls/level1-climb") != 0) {
        fprintf(stderr, "FAIL unable to enter level dir for path resolution case\n");
        return 1;
    }

    if (!game_progress_save_to_path(save_path, &progress)) {
        fprintf(stderr, "FAIL save_to_path returned 0 from nested cwd\n");
        ok = 0;
    }

    if (chdir(original_cwd) != 0) {
        fprintf(stderr, "FAIL unable to restore cwd after nested save case\n");
        return 1;
    }

    f = fopen(save_path, "r");
    if (!f) {
        fprintf(stderr, "FAIL save file was not created at repo-root path\n");
        ok = 0;
    } else {
        fclose(f);
    }

    if (!game_progress_load_from_path(save_path, &loaded)) {
        fprintf(stderr, "FAIL load_from_path returned 0 for round-trip save\n");
        ok = 0;
    } else {
        ok &= expect_string("roundtrip.player_name", loaded.player_name, "tester");
        ok &= expect_int("roundtrip.resume_from_save", loaded.selection.resume_from_save, 1);
        ok &= expect_int("roundtrip.current_level", loaded.current_level, 2);
        ok &= expect_int("roundtrip.highest_level_unlocked", loaded.highest_level_unlocked, 3);
        ok &= expect_int("roundtrip.score", loaded.score, 1234);
        ok &= expect_int("roundtrip.level2_points", loaded.level2_points, 900);
        ok &= expect_int("roundtrip.has_resumable_save",
                         game_progress_has_resumable_save(save_path),
                         1);
    }

    if (!write_legacy_save_file(legacy_path)) {
        fprintf(stderr, "FAIL unable to write legacy save fixture\n");
        ok = 0;
    } else if (!game_progress_load_from_path(legacy_path, &loaded)) {
        fprintf(stderr, "FAIL unable to load legacy save fixture\n");
        ok = 0;
    } else {
        ok &= expect_string("legacy.player_name", loaded.player_name, "legacy");
        ok &= expect_int("legacy.current_level", loaded.current_level, 4);
        ok &= expect_int("legacy.highest_level_unlocked", loaded.highest_level_unlocked, 4);
        ok &= expect_int("legacy.level3_completed", loaded.level3_completed, 1);
        ok &= expect_int("legacy.level3_points", loaded.level3_points, 0);
        ok &= expect_int("legacy.level5_completed", loaded.level5_completed, 1);
        ok &= expect_int("legacy.level5_points", loaded.level5_points, 590);
    }

    if (chdir("lvls/level2-chase") != 0) {
        fprintf(stderr, "FAIL unable to enter second nested dir for best-score path case\n");
        return 1;
    }

    if (!game_progress_save_best_score(best_score_path, "tester", 777)) {
        fprintf(stderr, "FAIL save_best_score returned 0 from nested cwd\n");
        ok = 0;
    }

    if (chdir(original_cwd) != 0) {
        fprintf(stderr, "FAIL unable to restore cwd after best-score case\n");
        return 1;
    }

    if (!game_progress_load_best_score(best_score_path, best_name, sizeof(best_name), &best_score)) {
        fprintf(stderr, "FAIL load_best_score returned 0 for round-trip best score\n");
        ok = 0;
    } else {
        ok &= expect_string("best_score.name", best_name, "tester");
        ok &= expect_int("best_score.score", best_score, 777);
    }

    remove(best_score_path);

    if (!game_progress_save_best_score(best_score_path, "", 25)) {
        fprintf(stderr, "FAIL save_best_score returned 0 for blank-name fallback case\n");
        ok = 0;
    } else if (!game_progress_load_best_score(best_score_path, best_name, sizeof(best_name), &best_score)) {
        fprintf(stderr, "FAIL load_best_score returned 0 for blank-name fallback case\n");
        ok = 0;
    } else {
        ok &= expect_string("best_score.blank_name_fallback", best_name, "PLAYER");
        ok &= expect_int("best_score.blank_name_score", best_score, 25);
    }

    remove(save_path);
    remove(legacy_path);
    remove(best_score_path);

    if (!ok) return 1;

    printf("game_progress_test: OK\n");
    return 0;
}
