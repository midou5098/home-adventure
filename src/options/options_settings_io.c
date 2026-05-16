#include "options_internal.h"

#include <stdint.h>
#include <stdio.h>

#define SETTINGS_MAGIC 0x46524543u
#define SETTINGS_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
} SettingsFileHeader;

static int file_exists_local(const char* path)
{
    FILE* f = NULL;

    if (!path) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int options_settings_resolve_global_path(char* out_path, size_t out_path_size)
{
    static const char* root_candidates[] = {
        ".",
        "..",
        "../..",
        "../../..",
        "../../../.."
    };
    char marker_path[512];
    size_t i;

    if (!out_path || out_path_size == 0) return 0;

    for (i = 0; i < sizeof(root_candidates) / sizeof(root_candidates[0]); ++i) {
        const char* base = root_candidates[i];
        snprintf(marker_path, sizeof(marker_path), "%s/src/main_menu/main.c", base);
        if (file_exists_local(marker_path)) {
            snprintf(out_path, out_path_size, "%s/settings.dat", base);
            return 1;
        }
    }

    snprintf(out_path, out_path_size, "settings.dat");
    return 1;
}

void options_settings_set_defaults(Settings* out_settings)
{
    if (!out_settings) return;

    out_settings->master = 10;
    out_settings->music = 10;
    out_settings->vfx = 10;
    out_settings->brightness = 10;
    out_settings->fullscreen = 0;
}

void options_settings_normalize(Settings* in_out_settings)
{
    if (!in_out_settings) return;

    in_out_settings->master = clamp_int(in_out_settings->master, 0, 10);
    in_out_settings->music = clamp_int(in_out_settings->music, 0, 10);
    in_out_settings->vfx = clamp_int(in_out_settings->vfx, 0, 10);
    in_out_settings->brightness = clamp_int(in_out_settings->brightness, 0, 10);
    in_out_settings->fullscreen = in_out_settings->fullscreen ? 1 : 0;
}

int options_settings_load_from_path(const char* path, Settings* out_settings)
{
    FILE* f = NULL;
    SettingsFileHeader header = {0};
    Settings loaded = {0};

    if (!path || !out_settings) return 0;

    f = fopen(path, "rb");
    if (!f) return 0;

    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    if (header.magic != SETTINGS_MAGIC || header.version != SETTINGS_VERSION) {
        fclose(f);
        return 0;
    }
    if (fread(&loaded, sizeof(Settings), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);

    options_settings_normalize(&loaded);
    *out_settings = loaded;
    return 1;
}

int options_settings_save_to_path(const char* path, const Settings* settings)
{
    FILE* f = NULL;
    SettingsFileHeader header = {SETTINGS_MAGIC, SETTINGS_VERSION};
    Settings to_save = {0};

    if (!path || !settings) return 0;

    to_save = *settings;
    options_settings_normalize(&to_save);

    f = fopen(path, "wb");
    if (!f) return 0;

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    if (fwrite(&to_save, sizeof(Settings), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int options_settings_load_global(Settings* out_settings)
{
    char path[512];

    if (!out_settings) return 0;
    if (!options_settings_resolve_global_path(path, sizeof(path))) return 0;
    return options_settings_load_from_path(path, out_settings);
}

int options_settings_save_global(const Settings* settings)
{
    char path[512];

    if (!settings) return 0;
    if (!options_settings_resolve_global_path(path, sizeof(path))) return 0;
    return options_settings_save_to_path(path, settings);
}
