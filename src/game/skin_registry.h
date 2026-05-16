#ifndef SKIN_REGISTRY_H
#define SKIN_REGISTRY_H

#define SKIN_REGISTRY_MAX 64
#define SKIN_REGISTRY_PATH_MAX 256
#define SKIN_REGISTRY_NAME_MAX 64

typedef struct {
    int id;
    char folder_path[SKIN_REGISTRY_PATH_MAX];
    char idle_path[SKIN_REGISTRY_PATH_MAX];
    char run_path[SKIN_REGISTRY_PATH_MAX];
    char choose_idle_path[SKIN_REGISTRY_PATH_MAX];
    char choose_select_path[SKIN_REGISTRY_PATH_MAX];
    char display_name[SKIN_REGISTRY_NAME_MAX];
} SkinDefinition;

int skin_registry_load(SkinDefinition* out_skins, int max_skins);
const SkinDefinition* skin_registry_find_by_id(const SkinDefinition* skins, int skin_count, int id);
const SkinDefinition* skin_registry_first(const SkinDefinition* skins, int skin_count);

#endif /* SKIN_REGISTRY_H */
