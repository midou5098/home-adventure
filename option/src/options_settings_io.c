#include "options_internal.h"

#include <stdint.h>

#define SETTINGS_MAGIC 0x46524543u
#define SETTINGS_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
} SettingsFileHeader;

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
