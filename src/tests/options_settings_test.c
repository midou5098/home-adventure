#include "options_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TEST_SETTINGS_MAGIC 0x46524543u
#define TEST_SETTINGS_VERSION 1u

static int expect_int(const char* label, int got, int want)
{
    if (got == want) return 1;
    fprintf(stderr, "FAIL %s: got=%d want=%d\n", label, got, want);
    return 0;
}

int main(void)
{
    const char* path = "/tmp/menu_options_settings_test.dat";
    Settings settings = {0};
    Settings loaded = {0};
    FILE* f = NULL;
    uint32_t header[2] = {0, 0};
    int ok = 1;

    remove(path);

    options_settings_set_defaults(&settings);
    ok &= expect_int("defaults.master", settings.master, 10);
    ok &= expect_int("defaults.music", settings.music, 10);
    ok &= expect_int("defaults.vfx", settings.vfx, 10);
    ok &= expect_int("defaults.brightness", settings.brightness, 10);
    ok &= expect_int("defaults.fullscreen", settings.fullscreen, 0);

    settings.master = 99;
    settings.music = -2;
    settings.vfx = 11;
    settings.brightness = -42;
    settings.fullscreen = 5;
    options_settings_normalize(&settings);

    ok &= expect_int("normalize.master", settings.master, 10);
    ok &= expect_int("normalize.music", settings.music, 0);
    ok &= expect_int("normalize.vfx", settings.vfx, 10);
    ok &= expect_int("normalize.brightness", settings.brightness, 0);
    ok &= expect_int("normalize.fullscreen", settings.fullscreen, 1);

    if (!options_settings_save_to_path(path, &settings)) {
        fprintf(stderr, "FAIL save_to_path returned 0\n");
        ok = 0;
    }

    if (!options_settings_load_from_path(path, &loaded)) {
        fprintf(stderr, "FAIL load_from_path returned 0\n");
        ok = 0;
    }

    if (memcmp(&settings, &loaded, sizeof(Settings)) != 0) {
        fprintf(stderr, "FAIL loaded settings differ from saved values\n");
        ok = 0;
    }

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "FAIL unable to reopen temp test file\n");
        ok = 0;
    } else {
        if (fread(header, sizeof(header), 1, f) != 1) {
            fprintf(stderr, "FAIL unable to read settings header\n");
            ok = 0;
        } else {
            ok &= expect_int("header.magic", (int)header[0], (int)TEST_SETTINGS_MAGIC);
            ok &= expect_int("header.version", (int)header[1], (int)TEST_SETTINGS_VERSION);
        }
        fclose(f);
    }

    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "FAIL unable to open temp test file for truncate case\n");
        ok = 0;
    } else {
        fputc(0xAB, f);
        fclose(f);
        if (options_settings_load_from_path(path, &loaded)) {
            fprintf(stderr, "FAIL truncated file should not load successfully\n");
            ok = 0;
        }
    }

    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "FAIL unable to open temp test file for bad magic case\n");
        ok = 0;
    } else {
        header[0] = 0xDEADBEEFu;
        header[1] = TEST_SETTINGS_VERSION;
        fwrite(header, sizeof(header), 1, f);
        fwrite(&settings, sizeof(settings), 1, f);
        fclose(f);
        if (options_settings_load_from_path(path, &loaded)) {
            fprintf(stderr, "FAIL bad magic file should not load successfully\n");
            ok = 0;
        }
    }

    remove(path);
    if (!ok) return 1;

    printf("options_settings_test: OK\n");
    return 0;
}
