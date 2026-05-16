#include "map.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float x1;
    float x2;
    float y;
} HorizontalSegment;

static int clampi(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int compare_range_min(const void *lhs, const void *rhs) {
    const Range *a = (const Range *)lhs;
    const Range *b = (const Range *)rhs;
    if (a->min < b->min) {
        return -1;
    }
    if (a->min > b->min) {
        return 1;
    }
    return 0;
}

static int compare_horizontal_segment(const void *lhs, const void *rhs) {
    const HorizontalSegment *a = (const HorizontalSegment *)lhs;
    const HorizontalSegment *b = (const HorizontalSegment *)rhs;

    if (a->y < b->y) {
        return -1;
    }
    if (a->y > b->y) {
        return 1;
    }
    if (a->x1 < b->x1) {
        return -1;
    }
    if (a->x1 > b->x1) {
        return 1;
    }
    return 0;
}

static void build_tile_blocks(MapState *map) {
    map->tile_block_count = 0;

    const int width = map->config.width;
    const int height = map->config.height;
    const int arm_w = map->config.arm_width;
    const int arm_h = map->config.arm_height;

    if (map->config.shape == MAP_SHAPE_RECT) {
        map->tile_blocks[map->tile_block_count++] = (TileBlock){0, 0, width, height};
        return;
    }

    if (map->config.shape == MAP_SHAPE_T) {
        const int stem_x = (width - arm_w) / 2;
        map->tile_blocks[map->tile_block_count++] = (TileBlock){0, 0, width, arm_h};
        map->tile_blocks[map->tile_block_count++] = (TileBlock){stem_x, 0, arm_w, height};
        return;
    }

    if (map->config.shape == MAP_SHAPE_U) {
        map->tile_blocks[map->tile_block_count++] = (TileBlock){0, 0, arm_w, height};
        map->tile_blocks[map->tile_block_count++] = (TileBlock){width - arm_w, 0, arm_w, height};
        map->tile_blocks[map->tile_block_count++] = (TileBlock){0, height - arm_h, width, arm_h};
        return;
    }

    map->tile_blocks[map->tile_block_count++] = (TileBlock){0, 0, arm_w, height};
    map->tile_blocks[map->tile_block_count++] = (TileBlock){0, height - arm_h, width, arm_h};
}

static void build_walkable_rects(MapState *map) {
    map->walkable_count = 0;
    for (int i = 0; i < map->tile_block_count && i < MAX_WALKABLE_RECTS; ++i) {
        const TileBlock block = map->tile_blocks[i];
        map->walkable[map->walkable_count++] = (Rect){
            .x = (float)(block.x * TILE_SIZE),
            .y = (float)(block.y * TILE_SIZE),
            .w = (float)(block.w * TILE_SIZE),
            .h = (float)(block.h * TILE_SIZE)
        };
    }
}

static void build_occupied_tiles(MapState *map) {
    memset(map->occupied, 0, sizeof(map->occupied));

    for (int i = 0; i < map->tile_block_count; ++i) {
        TileBlock block = map->tile_blocks[i];
        const int y_start = clampi(block.y, 0, map->config.height);
        const int y_end = clampi(block.y + block.h, 0, map->config.height);
        const int x_start = clampi(block.x, 0, map->config.width);
        const int x_end = clampi(block.x + block.w, 0, map->config.width);

        for (int y = y_start; y < y_end; ++y) {
            for (int x = x_start; x < x_end; ++x) {
                if (x >= 0 && x < MAX_WORLD_TILES_X && y >= 0 && y < MAX_WORLD_TILES_Y) {
                    map->occupied[y][x] = true;
                }
            }
        }
    }
}

static bool is_tile_occupied(const MapState *map, int tile_x, int tile_y) {
    if (tile_x < 0 || tile_x >= map->config.width || tile_y < 0 || tile_y >= map->config.height) {
        return false;
    }
    if (tile_x >= MAX_WORLD_TILES_X || tile_y >= MAX_WORLD_TILES_Y) {
        return false;
    }
    return map->occupied[tile_y][tile_x];
}

static void build_wall_segments(MapState *map) {
    map->wall_segment_count = 0;

    for (int y = 0; y < map->config.height; ++y) {
        for (int x = 0; x < map->config.width; ++x) {
            if (!is_tile_occupied(map, x, y)) {
                continue;
            }

            const float base_x = (float)(x * TILE_SIZE);
            const float base_y = (float)(y * TILE_SIZE);

            if (!is_tile_occupied(map, x, y - 1) && map->wall_segment_count < MAX_WALL_SEGMENTS) {
                map->wall_segments[map->wall_segment_count++] = (Seg){
                    .x1 = base_x,
                    .y1 = base_y,
                    .x2 = base_x + TILE_SIZE,
                    .y2 = base_y
                };
            }
            if (!is_tile_occupied(map, x + 1, y) && map->wall_segment_count < MAX_WALL_SEGMENTS) {
                map->wall_segments[map->wall_segment_count++] = (Seg){
                    .x1 = base_x + TILE_SIZE,
                    .y1 = base_y,
                    .x2 = base_x + TILE_SIZE,
                    .y2 = base_y + TILE_SIZE
                };
            }
            if (!is_tile_occupied(map, x, y + 1) && map->wall_segment_count < MAX_WALL_SEGMENTS) {
                map->wall_segments[map->wall_segment_count++] = (Seg){
                    .x1 = base_x,
                    .y1 = base_y + TILE_SIZE,
                    .x2 = base_x + TILE_SIZE,
                    .y2 = base_y + TILE_SIZE
                };
            }
            if (!is_tile_occupied(map, x - 1, y) && map->wall_segment_count < MAX_WALL_SEGMENTS) {
                map->wall_segments[map->wall_segment_count++] = (Seg){
                    .x1 = base_x,
                    .y1 = base_y,
                    .x2 = base_x,
                    .y2 = base_y + TILE_SIZE
                };
            }
        }
    }
}

static void build_top_wall_covers(MapState *map) {
    HorizontalSegment horizontal[MAX_WALL_SEGMENTS];
    int horizontal_count = 0;

    /* Build covers only from top-facing edges (tile occupied, tile above empty).
       This intentionally excludes bottom-facing edges to avoid drawing/solid
       "bottom wall background" strips near the floor. */
    for (int y = 0; y < map->config.height; ++y) {
        for (int x = 0; x < map->config.width; ++x) {
            if (!is_tile_occupied(map, x, y) || is_tile_occupied(map, x, y - 1)) {
                continue;
            }
            if (horizontal_count >= MAX_WALL_SEGMENTS) {
                break;
            }
            horizontal[horizontal_count++] = (HorizontalSegment){
                .x1 = (float)(x * TILE_SIZE),
                .x2 = (float)((x + 1) * TILE_SIZE),
                .y = (float)(y * TILE_SIZE)
            };
        }
        if (horizontal_count >= MAX_WALL_SEGMENTS) {
            break;
        }
    }

    qsort(horizontal, (size_t)horizontal_count, sizeof(horizontal[0]), compare_horizontal_segment);

    HorizontalSegment merged[MAX_WALL_SEGMENTS];
    int merged_count = 0;

    for (int i = 0; i < horizontal_count; ++i) {
        const HorizontalSegment segment = horizontal[i];

        if (merged_count == 0) {
            merged[merged_count++] = segment;
            continue;
        }

        HorizontalSegment *last = &merged[merged_count - 1];
        if (fabsf(last->y - segment.y) < 0.0001f && segment.x1 <= last->x2) {
            if (segment.x2 > last->x2) {
                last->x2 = segment.x2;
            }
        } else {
            merged[merged_count++] = segment;
        }
    }

    map->top_wall_cover_count = 0;
    for (int i = 0; i < merged_count && i < MAX_TOP_WALL_COVERS; ++i) {
        map->top_wall_covers[map->top_wall_cover_count++] = (Rect){
            .x = merged[i].x1,
            .y = merged[i].y,
            .w = merged[i].x2 - merged[i].x1,
            .h = 0.0f
        };
    }

    map->wall_obstacle_count = 0;
    const float front_cover_y = (map->top_wall_cover_count > 0) ? map->top_wall_covers[0].y : 0.0f;
    for (int i = 0; i < map->top_wall_cover_count; ++i) {
        const float y_offset = (fabsf(map->top_wall_covers[i].y - front_cover_y) < 0.0001f)
            ? 0.0f
            : -WALL_COVER_HEIGHT;
        map->wall_obstacles[map->wall_obstacle_count++] = (Rect){
            .x = map->top_wall_covers[i].x,
            .y = map->top_wall_covers[i].y + y_offset,
            .w = map->top_wall_covers[i].w,
            .h = WALL_COVER_HEIGHT
        };
    }
}

static bool rect_contains_rect(Rect outer, Rect inner) {
    return (
        inner.x >= outer.x &&
        inner.y >= outer.y &&
        inner.x + inner.w <= outer.x + outer.w &&
        inner.y + inner.h <= outer.y + outer.h
    );
}

static bool spawn_rect_inside_walkable(const MapState *map, Rect spawn_rect) {
    for (int i = 0; i < map->walkable_count; ++i) {
        if (rect_contains_rect(map->walkable[i], spawn_rect)) {
            return true;
        }
    }
    return false;
}

static bool spawn_rect_overlaps_solids(const MapState *map, Rect spawn_rect) {
    for (int i = 0; i < map->wall_obstacle_count; ++i) {
        if (rect_overlaps(spawn_rect, map->wall_obstacles[i])) {
            return true;
        }
    }
    if (rect_overlaps(spawn_rect, map->billiards_table)) {
        return true;
    }
    if (rect_overlaps(spawn_rect, map->food_buffet)) {
        return true;
    }
    if (rect_overlaps(spawn_rect, map->food_buffet2)) {
        return true;
    }
    for (int i = 0; i < map->decor_table_count; ++i) {
        if (rect_overlaps(spawn_rect, map->decor_tables[i])) {
            return true;
        }
    }
    return false;
}

static bool is_spawn_valid(const MapState *map, float x, float y) {
    const Rect spawn_rect = {
        .x = x,
        .y = y,
        .w = (float)PLAYER_W,
        .h = (float)PLAYER_H
    };

    if (!spawn_rect_inside_walkable(map, spawn_rect)) {
        return false;
    }
    if (spawn_rect_overlaps_solids(map, spawn_rect)) {
        return false;
    }
    return true;
}

static void resolve_spawn_position(MapState *map) {
    const int spawn_max_x = map->world_w - PLAYER_W;
    const int spawn_max_y = map->world_h - PLAYER_H;

    map->spawn_x = (float)clampi(map->config.spawn_x, 0, spawn_max_x > 0 ? spawn_max_x : 0);
    map->spawn_y = (float)clampi(map->config.spawn_y, 0, spawn_max_y > 0 ? spawn_max_y : 0);

    if (is_spawn_valid(map, map->spawn_x, map->spawn_y)) {
        return;
    }

    float best_x = map->spawn_x;
    float best_y = map->spawn_y;
    float best_dist_sq = INFINITY;
    bool found = false;

    for (int y = 0; y <= spawn_max_y; ++y) {
        for (int x = 0; x <= spawn_max_x; ++x) {
            if (!is_spawn_valid(map, (float)x, (float)y)) {
                continue;
            }
            const float dx = (float)x - map->spawn_x;
            const float dy = (float)y - map->spawn_y;
            const float dist_sq = dx * dx + dy * dy;
            if (!found || dist_sq < best_dist_sq) {
                found = true;
                best_dist_sq = dist_sq;
                best_x = (float)x;
                best_y = (float)y;
            }
        }
    }

    if (found) {
        map->spawn_x = best_x;
        map->spawn_y = best_y;
        return;
    }

    /* Last resort: snap to the first walkable tile area. */
    if (map->walkable_count > 0) {
        for (int i = 0; i < map->walkable_count; ++i) {
            const Rect walk = map->walkable[i];
            const float fallback_x = clampf(walk.x, 0.0f, (float)(spawn_max_x > 0 ? spawn_max_x : 0));
            const float fallback_y = clampf(walk.y, 0.0f, (float)(spawn_max_y > 0 ? spawn_max_y : 0));
            if (is_spawn_valid(map, fallback_x, fallback_y)) {
                map->spawn_x = fallback_x;
                map->spawn_y = fallback_y;
                return;
            }
        }
    }
}

static Rect build_barista_zone(Rect barista, Rect food_buffet) {
    const float interaction_size = 54.0f;
    const float interaction_offset_y = 18.0f;

    const float service_front_y = fmaxf(
        barista.y + barista.h + interaction_offset_y,
        food_buffet.y + food_buffet.h
    );

    return (Rect){
        .x = barista.x + fmaxf(0.0f, (barista.w - interaction_size) * 0.5f),
        .y = service_front_y,
        .w = interaction_size,
        .h = interaction_size
    };
}

static Rect build_door_zone(Rect door) {
    const float horizontal_margin = 10.0f;
    const float interaction_depth = 44.0f;

    return (Rect){
        .x = door.x - horizontal_margin,
        .y = door.y + door.h - 8.0f,
        .w = door.w + horizontal_margin * 2.0f,
        .h = interaction_depth
    };
}

Rect map_default_arcade_rect(void) {
    return (Rect){
        .x = 96.0f,
        .y = 382.0f,
        .w = 72.0f,
        .h = 112.0f
    };
}

static Rect build_arcade_zone(Rect arcade) {
    const float horizontal_margin = 18.0f;
    const float interaction_depth = 46.0f;

    return (Rect){
        .x = arcade.x - horizontal_margin,
        .y = arcade.y + arcade.h - 6.0f,
        .w = arcade.w + horizontal_margin * 2.0f,
        .h = interaction_depth
    };
}

void map_set_arcade_rect(MapState *map, Rect rect) {
    if (!map) {
        return;
    }

    map->arcade_rect = rect;
    map->arcade_zone = build_arcade_zone(map->arcade_rect);
}

MapConfig map_default_config(void) {
    MapConfig config;
    memset(&config, 0, sizeof(config));

    config.shape = MAP_SHAPE_L;
    config.width = 25;
    config.height = 18;
    config.arm_width = 12;
    config.arm_height = 9;

    config.spawn_x = 776;
    config.spawn_y = 540;

    config.table_x = 25;
    config.table_y = 30;
    config.table_w = 220;
    config.table_h = 123;

    config.popup_w = 680;
    config.popup_h = 490;

    config.hole_top_left_x = 0;
    config.hole_top_left_y = 14;
    config.hole_top_middle_x = 0;
    config.hole_top_middle_y = 14;
    config.hole_top_right_x = 0;
    config.hole_top_right_y = 14;
    config.hole_bottom_left_x = 0;
    config.hole_bottom_left_y = -9;
    config.hole_bottom_middle_x = 0;
    config.hole_bottom_middle_y = -9;
    config.hole_bottom_right_x = 0;
    config.hole_bottom_right_y = -9;

    config.cue_ball_x = 197;
    config.cue_ball_y = 300;
    config.eight_ball_x = 401;
    config.eight_ball_y = 255;

    config.buffet_x = 420;
    config.buffet_y = 300;
    config.buffet_w = 300;
    config.buffet_h = 80;

    config.buffet2_x = 715;
    config.buffet2_y = 380;
    config.buffet2_w = 64;
    config.buffet2_h = 120;

    config.door_x = 272;
    config.door_y = -1;
    config.door_w = 84;
    config.door_h = 60;

    config.window_x = 533;
    config.window_y = 236;
    config.window_w = 121;
    config.window_h = 37;

    config.barista_x = 534;
    config.barista_y = 257;
    config.barista_w = 40;
    config.barista_h = 64;

    config.decor_table_count = 2;
    config.decor_tables[0] = (Rect){242, 498, 84, 78};
    config.decor_tables[1] = (Rect){390, 498, 84, 78};

    return config;
}

void map_build(MapState *map, const MapConfig *config) {
    memset(map, 0, sizeof(*map));
    map->config = *config;

    map->config.width = clampi(map->config.width, 4, MAX_WORLD_TILES_X);
    map->config.height = clampi(map->config.height, 4, MAX_WORLD_TILES_Y);
    map->config.arm_width = clampi(map->config.arm_width, 1, map->config.width);
    map->config.arm_height = clampi(map->config.arm_height, 1, map->config.height);

    if (map->config.shape == MAP_SHAPE_U) {
        map->config.arm_width = clampi(map->config.arm_width, 1, map->config.width / 2);
    }

    map->world_w = map->config.width * TILE_SIZE;
    map->world_h = map->config.height * TILE_SIZE;
    map->offset_x = (WINDOW_W - map->world_w) * 0.5f;
    map->offset_y = (WINDOW_H - map->world_h) * 0.5f;

    build_tile_blocks(map);
    build_walkable_rects(map);
    build_occupied_tiles(map);
    build_wall_segments(map);
    build_top_wall_covers(map);

    map->billiards_table = (Rect){
        (float)map->config.table_x,
        (float)map->config.table_y,
        (float)map->config.table_w,
        (float)map->config.table_h
    };

    map->billiards_popup = (Rect){
        (float)((map->world_w - map->config.popup_w) / 2),
        (float)((map->world_h - map->config.popup_h) / 2),
        (float)map->config.popup_w,
        (float)map->config.popup_h
    };

    map->food_buffet = (Rect){
        (float)map->config.buffet_x,
        (float)map->config.buffet_y,
        (float)map->config.buffet_w,
        (float)map->config.buffet_h
    };

    map->food_buffet2 = (Rect){
        (float)map->config.buffet2_x,
        (float)map->config.buffet2_y,
        (float)map->config.buffet2_w,
        (float)map->config.buffet2_h
    };

    map->door = (Rect){
        (float)map->config.door_x,
        (float)map->config.door_y,
        (float)map->config.door_w,
        (float)map->config.door_h
    };
    map->door_zone = build_door_zone(map->door);

    map->wall_window = (Rect){
        (float)map->config.window_x,
        (float)map->config.window_y,
        (float)map->config.window_w,
        (float)map->config.window_h
    };

    map->barista_rect = (Rect){
        (float)map->config.barista_x,
        (float)map->config.barista_y,
        (float)map->config.barista_w,
        (float)map->config.barista_h
    };
    map_set_arcade_rect(map, map_default_arcade_rect());

    map->decor_table_count = clampi(map->config.decor_table_count, 0, MAX_DECOR_TABLES);
    for (int i = 0; i < map->decor_table_count; ++i) {
        map->decor_tables[i] = map->config.decor_tables[i];
    }

    map->barista_zone = build_barista_zone(map->barista_rect, map->food_buffet);

    resolve_spawn_position(map);
}

void map_draw_floor(SDL_Renderer *renderer, const MapState *map) {
    SDL_SetRenderDrawColor(renderer, 0x7b, 0x7f, 0x87, 0xFF);

    for (int y = 0; y < map->config.height; ++y) {
        for (int x = 0; x < map->config.width; ++x) {
            if (!is_tile_occupied(map, x, y)) {
                continue;
            }

            const int base_x = (int)lroundf(map->offset_x + x * TILE_SIZE);
            const int base_y = (int)lroundf(map->offset_y + y * TILE_SIZE);

            for (int local_y = 0; local_y < TILE_SIZE; local_y += FLOOR_TILE_SIZE) {
                for (int local_x = 0; local_x < TILE_SIZE; local_x += FLOOR_TILE_SIZE) {
                    SDL_Rect floor_tile = {
                        .x = base_x + local_x,
                        .y = base_y + local_y,
                        .w = FLOOR_TILE_SIZE,
                        .h = FLOOR_TILE_SIZE
                    };
                    SDL_RenderFillRect(renderer, &floor_tile);

                    SDL_SetRenderDrawColor(renderer, 0x5f, 0x63, 0x6a, 0xFF);
                    SDL_Rect grid_tile = {
                        .x = floor_tile.x,
                        .y = floor_tile.y,
                        .w = FLOOR_TILE_SIZE - 1,
                        .h = FLOOR_TILE_SIZE - 1
                    };
                    SDL_RenderDrawRect(renderer, &grid_tile);
                    SDL_SetRenderDrawColor(renderer, 0x7b, 0x7f, 0x87, 0xFF);
                }
            }
        }
    }
}

void map_draw_wall_segments(SDL_Renderer *renderer, const MapState *map) {
    SDL_SetRenderDrawColor(renderer, 0x8b, 0x73, 0x55, 0xFF);

    for (int i = 0; i < map->wall_segment_count; ++i) {
        const Seg segment = map->wall_segments[i];
        if (
            fabsf(segment.y1 - segment.y2) < 0.0001f &&
            segment.y1 >= (float)map->world_h - 0.0001f
        ) {
            continue;
        }
        const int x1 = (int)lroundf(map->offset_x + segment.x1);
        const int y1 = (int)lroundf(map->offset_y + segment.y1);
        const int x2 = (int)lroundf(map->offset_x + segment.x2);
        const int y2 = (int)lroundf(map->offset_y + segment.y2);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}

SDL_Rect map_world_to_screen_rect(const MapState *map, Rect world_rect) {
    SDL_Rect out;
    out.x = (int)lroundf(map->offset_x + world_rect.x);
    out.y = (int)lroundf(map->offset_y + world_rect.y);
    out.w = (int)lroundf(world_rect.w);
    out.h = (int)lroundf(world_rect.h);
    return out;
}

int map_get_solid_obstacles(const MapState *map, Rect *out, int max_count) {
    int count = 0;

    for (int i = 0; i < map->wall_obstacle_count && count < max_count; ++i) {
        out[count++] = map->wall_obstacles[i];
    }

    if (count < max_count) {
        out[count++] = map->billiards_table;
    }
    if (count < max_count) {
        out[count++] = map->food_buffet;
    }
    if (count < max_count) {
        out[count++] = map->food_buffet2;
    }
    if (count < max_count) {
        out[count++] = map->arcade_rect;
    }

    for (int i = 0; i < map->decor_table_count && count < max_count; ++i) {
        out[count++] = map->decor_tables[i];
    }

    return count;
}

static void merge_ranges(Range *ranges, int *count) {
    if (*count <= 1) {
        return;
    }

    qsort(ranges, (size_t)*count, sizeof(ranges[0]), compare_range_min);

    int merged_count = 1;
    for (int i = 1; i < *count; ++i) {
        Range *last = &ranges[merged_count - 1];
        if (ranges[i].min <= last->max) {
            if (ranges[i].max > last->max) {
                last->max = ranges[i].max;
            }
        } else {
            ranges[merged_count++] = ranges[i];
        }
    }

    *count = merged_count;
}

static float clamp_to_ranges(float value, const Range *ranges, int count, float fallback) {
    if (count == 0) {
        return fallback;
    }

    for (int i = 0; i < count; ++i) {
        if (value >= ranges[i].min && value <= ranges[i].max) {
            return value;
        }
    }

    float closest = fallback;
    float closest_distance = INFINITY;

    for (int i = 0; i < count; ++i) {
        const float clamped = clampf(value, ranges[i].min, ranges[i].max);
        const float distance = fabsf(clamped - value);
        if (distance < closest_distance) {
            closest_distance = distance;
            closest = clamped;
        }
    }

    return closest;
}

static int get_allowed_x_ranges(
    const MapState *map,
    float player_y,
    float player_h,
    float player_w,
    Range *out,
    int max_count
) {
    int count = 0;

    for (int i = 0; i < map->walkable_count && count < max_count; ++i) {
        const Rect rect = map->walkable[i];
        const bool fits_vertically =
            player_y >= rect.y &&
            player_y + player_h <= rect.y + rect.h;

        if (!fits_vertically) {
            continue;
        }

        out[count++] = (Range){
            .min = rect.x,
            .max = rect.x + rect.w - player_w
        };
    }

    merge_ranges(out, &count);
    return count;
}

static int get_allowed_y_ranges(
    const MapState *map,
    float player_x,
    float player_w,
    float player_h,
    Range *out,
    int max_count
) {
    int count = 0;

    for (int i = 0; i < map->walkable_count && count < max_count; ++i) {
        const Rect rect = map->walkable[i];
        const bool fits_horizontally =
            player_x >= rect.x &&
            player_x + player_w <= rect.x + rect.w;

        if (!fits_horizontally) {
            continue;
        }

        out[count++] = (Range){
            .min = rect.y,
            .max = rect.y + rect.h - player_h
        };
    }

    merge_ranges(out, &count);
    return count;
}

static void resolve_solid_collision_x(
    const MapState *map,
    float *x,
    float y,
    float player_w,
    float player_h,
    float dx
) {
    Rect solids[MAX_SOLID_OBSTACLES];
    const int solid_count = map_get_solid_obstacles(map, solids, MAX_SOLID_OBSTACLES);

    Rect player_rect = {.x = *x, .y = y, .w = player_w, .h = player_h};
    for (int i = 0; i < solid_count; ++i) {
        if (!rect_overlaps(player_rect, solids[i])) {
            continue;
        }

        if (dx > 0.0f) {
            *x = solids[i].x - player_w;
        } else if (dx < 0.0f) {
            *x = solids[i].x + solids[i].w;
        }

        player_rect.x = *x;
    }
}

static void resolve_solid_collision_y(
    const MapState *map,
    float x,
    float *y,
    float player_w,
    float player_h,
    float dy
) {
    Rect solids[MAX_SOLID_OBSTACLES];
    const int solid_count = map_get_solid_obstacles(map, solids, MAX_SOLID_OBSTACLES);

    Rect player_rect = {.x = x, .y = *y, .w = player_w, .h = player_h};
    for (int i = 0; i < solid_count; ++i) {
        if (!rect_overlaps(player_rect, solids[i])) {
            continue;
        }

        if (dy > 0.0f) {
            *y = solids[i].y - player_h;
        } else if (dy < 0.0f) {
            *y = solids[i].y + solids[i].h;
        }

        player_rect.y = *y;
    }
}

void map_move_player(
    const MapState *map,
    float *x,
    float *y,
    float player_w,
    float player_h,
    float dx,
    float dy
) {
    if (dx != 0.0f) {
        const float next_x = *x + dx;
        Range ranges[32];
        const int count = get_allowed_x_ranges(map, *y, player_h, player_w, ranges, 32);
        *x = clamp_to_ranges(next_x, ranges, count, *x);
        resolve_solid_collision_x(map, x, *y, player_w, player_h, dx);
    }

    if (dy != 0.0f) {
        const float next_y = *y + dy;
        Range ranges[32];
        const int count = get_allowed_y_ranges(map, *x, player_w, player_h, ranges, 32);
        *y = clamp_to_ranges(next_y, ranges, count, *y);
        resolve_solid_collision_y(map, *x, y, player_w, player_h, dy);
    }
}

InteractionTarget map_get_interaction_target(
    const MapState *map,
    Rect player_rect,
    bool popup_visible
) {
    if (popup_visible) {
        return TARGET_NONE;
    }

    if (rect_touch_or_overlap(player_rect, map->barista_zone)) {
        return TARGET_BARISTA;
    }

    if (rect_touch_or_overlap(player_rect, map->arcade_zone)) {
        return TARGET_ARCADE;
    }

    if (rect_touch_or_overlap(player_rect, map->door_zone)) {
        return TARGET_EXIT_DOOR;
    }

    if (rect_touch_or_overlap(player_rect, map->billiards_table)) {
        return TARGET_BILLIARDS;
    }

    return TARGET_NONE;
}
