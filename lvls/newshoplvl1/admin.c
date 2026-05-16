#include "admin.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CONFIG_MAGIC "SDSH"
#define CONFIG_VERSION 1u
#define TV_CONFIG_MAGIC "TVCF"
#define TV_CONFIG_VERSION 1u
#define AD_CONFIG_MAGIC "ADCF"
#define AD_CONFIG_VERSION 1u
#define ARCADE_CONFIG_MAGIC "ARCF"
#define ARCADE_CONFIG_VERSION 1u
#define ARCADE_POPUP_CONFIG_MAGIC "APCF"
#define ARCADE_POPUP_CONFIG_VERSION 1u
#define NPC_CONFIG_MAGIC "NPCF"
#define NPC_CONFIG_VERSION 2u

typedef struct {
    char magic[4];
    uint32_t version;
    MapConfig config;
} PersistedConfig;

typedef struct {
    char magic[4];
    uint32_t version;
    Rect rect;
} PersistedTvConfig;

typedef struct {
    char magic[4];
    uint32_t version;
    Rect rect;
} PersistedAdConfig;

typedef struct {
    char magic[4];
    uint32_t version;
    Rect rect;
} PersistedArcadeConfig;

typedef struct {
    char magic[4];
    uint32_t version;
    Rect rect;
} PersistedArcadePopupConfig;

typedef struct {
    int32_t skin_index;
    int32_t z_index;
    Rect rect;
    uint8_t flip_x;
    uint8_t reserved[3];
} PersistedNpcEntry;

typedef struct {
    char magic[4];
    uint32_t version;
    int32_t count;
    PersistedNpcEntry entries[ADMIN_MAX_NPCS];
} PersistedNpcConfig;

typedef struct {
    int32_t skin_index;
    Rect rect;
    uint8_t flip_x;
    uint8_t reserved[3];
} PersistedNpcEntryV1;

typedef struct {
    char magic[4];
    uint32_t version;
    int32_t count;
    PersistedNpcEntryV1 entries[ADMIN_MAX_NPCS];
} PersistedNpcConfigV1;

bool admin_load_config(MapConfig *config, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    PersistedConfig persisted;
    const size_t read_count = fread(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    if (read_count != 1) {
        return false;
    }
    if (memcmp(persisted.magic, CONFIG_MAGIC, sizeof(persisted.magic)) != 0) {
        return false;
    }
    if (persisted.version != CONFIG_VERSION) {
        return false;
    }

    *config = persisted.config;
    return true;
}

bool admin_save_config(const MapConfig *config, const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedConfig persisted;
    memcpy(persisted.magic, CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = CONFIG_VERSION;
    persisted.config = *config;

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}

bool admin_load_tv_rect(Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    PersistedTvConfig persisted;
    const size_t read_count = fread(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    if (read_count != 1) {
        return false;
    }
    if (memcmp(persisted.magic, TV_CONFIG_MAGIC, sizeof(persisted.magic)) != 0) {
        return false;
    }
    if (persisted.version != TV_CONFIG_VERSION) {
        return false;
    }

    *rect = persisted.rect;
    return true;
}

bool admin_save_tv_rect(const Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedTvConfig persisted;
    memcpy(persisted.magic, TV_CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = TV_CONFIG_VERSION;
    persisted.rect = *rect;

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}

bool admin_load_ad_rect(Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    PersistedAdConfig persisted;
    const size_t read_count = fread(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    if (read_count != 1) {
        return false;
    }
    if (memcmp(persisted.magic, AD_CONFIG_MAGIC, sizeof(persisted.magic)) != 0) {
        return false;
    }
    if (persisted.version != AD_CONFIG_VERSION) {
        return false;
    }

    *rect = persisted.rect;
    return true;
}

bool admin_save_ad_rect(const Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedAdConfig persisted;
    memcpy(persisted.magic, AD_CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = AD_CONFIG_VERSION;
    persisted.rect = *rect;

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}

bool admin_load_arcade_rect(Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    PersistedArcadeConfig persisted;
    const size_t read_count = fread(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    if (read_count != 1) {
        return false;
    }
    if (memcmp(persisted.magic, ARCADE_CONFIG_MAGIC, sizeof(persisted.magic)) != 0) {
        return false;
    }
    if (persisted.version != ARCADE_CONFIG_VERSION) {
        return false;
    }

    *rect = persisted.rect;
    return true;
}

bool admin_save_arcade_rect(const Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedArcadeConfig persisted;
    memcpy(persisted.magic, ARCADE_CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = ARCADE_CONFIG_VERSION;
    persisted.rect = *rect;

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}

bool admin_load_arcade_popup_rect(Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    PersistedArcadePopupConfig persisted;
    const size_t read_count = fread(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    if (read_count != 1) {
        return false;
    }
    if (memcmp(persisted.magic, ARCADE_POPUP_CONFIG_MAGIC, sizeof(persisted.magic)) != 0) {
        return false;
    }
    if (persisted.version != ARCADE_POPUP_CONFIG_VERSION) {
        return false;
    }

    *rect = persisted.rect;
    return true;
}

bool admin_save_arcade_popup_rect(const Rect *rect, const char *path) {
    if (!rect) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedArcadePopupConfig persisted;
    memcpy(persisted.magic, ARCADE_POPUP_CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = ARCADE_POPUP_CONFIG_VERSION;
    persisted.rect = *rect;

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}

bool admin_load_npc_config(AdminNpcConfig *config, const char *path) {
    if (!config) {
        return false;
    }

    memset(config, 0, sizeof(*config));

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    {
        char magic[4];
        uint32_t version = 0;
        if (
            fread(magic, sizeof(magic), 1, file) != 1 ||
            fread(&version, sizeof(version), 1, file) != 1
        ) {
            fclose(file);
            return false;
        }

        if (memcmp(magic, NPC_CONFIG_MAGIC, sizeof(magic)) != 0) {
            fclose(file);
            return false;
        }

        rewind(file);
        if (version == NPC_CONFIG_VERSION) {
            PersistedNpcConfig persisted;
            if (fread(&persisted, sizeof(persisted), 1, file) != 1) {
                fclose(file);
                return false;
            }

            if (persisted.count < 0) {
                persisted.count = 0;
            }
            if (persisted.count > ADMIN_MAX_NPCS) {
                persisted.count = ADMIN_MAX_NPCS;
            }
            config->count = persisted.count;

            for (int i = 0; i < config->count; ++i) {
                config->entries[i].skin_index = persisted.entries[i].skin_index;
                config->entries[i].z_index = persisted.entries[i].z_index;
                config->entries[i].rect = persisted.entries[i].rect;
                config->entries[i].flip_x = persisted.entries[i].flip_x != 0;
            }
            fclose(file);
            return true;
        }

        if (version == 1u) {
            PersistedNpcConfigV1 persisted;
            if (fread(&persisted, sizeof(persisted), 1, file) != 1) {
                fclose(file);
                return false;
            }

            if (persisted.count < 0) {
                persisted.count = 0;
            }
            if (persisted.count > ADMIN_MAX_NPCS) {
                persisted.count = ADMIN_MAX_NPCS;
            }
            config->count = persisted.count;

            for (int i = 0; i < config->count; ++i) {
                config->entries[i].skin_index = persisted.entries[i].skin_index;
                config->entries[i].z_index = 0;
                config->entries[i].rect = persisted.entries[i].rect;
                config->entries[i].flip_x = persisted.entries[i].flip_x != 0;
            }
            fclose(file);
            return true;
        }
    }

    fclose(file);
    return false;
}

bool admin_save_npc_config(const AdminNpcConfig *config, const char *path) {
    if (!config) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }

    PersistedNpcConfig persisted;
    memset(&persisted, 0, sizeof(persisted));
    memcpy(persisted.magic, NPC_CONFIG_MAGIC, sizeof(persisted.magic));
    persisted.version = NPC_CONFIG_VERSION;
    persisted.count = config->count;
    if (persisted.count < 0) {
        persisted.count = 0;
    }
    if (persisted.count > ADMIN_MAX_NPCS) {
        persisted.count = ADMIN_MAX_NPCS;
    }

    for (int i = 0; i < persisted.count; ++i) {
        persisted.entries[i].skin_index = config->entries[i].skin_index;
        persisted.entries[i].z_index = config->entries[i].z_index;
        persisted.entries[i].rect = config->entries[i].rect;
        persisted.entries[i].flip_x = config->entries[i].flip_x ? 1u : 0u;
    }

    const size_t write_count = fwrite(&persisted, sizeof(persisted), 1, file);
    fclose(file);

    return write_count == 1;
}
