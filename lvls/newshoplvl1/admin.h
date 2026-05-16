#ifndef SDLVERSIONSHOP_ADMIN_H
#define SDLVERSIONSHOP_ADMIN_H

#include "map.h"

#include <stdbool.h>

#define ADMIN_MAX_NPCS 16

typedef struct {
    int skin_index;
    int z_index;
    Rect rect;
    bool flip_x;
} AdminNpcEntry;

typedef struct {
    int count;
    AdminNpcEntry entries[ADMIN_MAX_NPCS];
} AdminNpcConfig;

bool admin_load_config(MapConfig *config, const char *path);
bool admin_save_config(const MapConfig *config, const char *path);
bool admin_load_tv_rect(Rect *rect, const char *path);
bool admin_save_tv_rect(const Rect *rect, const char *path);
bool admin_load_ad_rect(Rect *rect, const char *path);
bool admin_save_ad_rect(const Rect *rect, const char *path);
bool admin_load_arcade_rect(Rect *rect, const char *path);
bool admin_save_arcade_rect(const Rect *rect, const char *path);
bool admin_load_arcade_popup_rect(Rect *rect, const char *path);
bool admin_save_arcade_popup_rect(const Rect *rect, const char *path);
bool admin_load_npc_config(AdminNpcConfig *config, const char *path);
bool admin_save_npc_config(const AdminNpcConfig *config, const char *path);

#endif
