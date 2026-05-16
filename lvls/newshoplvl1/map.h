#ifndef SDLVERSIONSHOP_MAP_H
#define SDLVERSIONSHOP_MAP_H

#include "game.h"

#include <stdbool.h>

#define MAX_TILE_BLOCKS 8
#define MAX_WALKABLE_RECTS 16
#define MAX_WALL_SEGMENTS 1024
#define MAX_TOP_WALL_COVERS 8
#define MAX_SOLID_OBSTACLES 64
#define MAX_WORLD_TILES_X 128
#define MAX_WORLD_TILES_Y 128

typedef struct {
    int x;
    int y;
    int w;
    int h;
} TileBlock;

typedef struct {
    MapShape shape;
    int width;
    int height;
    int arm_width;
    int arm_height;

    int spawn_x;
    int spawn_y;

    int table_x;
    int table_y;
    int table_w;
    int table_h;

    int popup_w;
    int popup_h;

    int hole_top_left_x;
    int hole_top_left_y;
    int hole_top_middle_x;
    int hole_top_middle_y;
    int hole_top_right_x;
    int hole_top_right_y;
    int hole_bottom_left_x;
    int hole_bottom_left_y;
    int hole_bottom_middle_x;
    int hole_bottom_middle_y;
    int hole_bottom_right_x;
    int hole_bottom_right_y;

    int cue_ball_x;
    int cue_ball_y;
    int eight_ball_x;
    int eight_ball_y;

    int buffet_x;
    int buffet_y;
    int buffet_w;
    int buffet_h;

    int buffet2_x;
    int buffet2_y;
    int buffet2_w;
    int buffet2_h;

    int door_x;
    int door_y;
    int door_w;
    int door_h;

    int window_x;
    int window_y;
    int window_w;
    int window_h;

    int barista_x;
    int barista_y;
    int barista_w;
    int barista_h;

    int decor_table_count;
    Rect decor_tables[MAX_DECOR_TABLES];
} MapConfig;

typedef struct {
    MapConfig config;

    int world_w;
    int world_h;
    float offset_x;
    float offset_y;

    TileBlock tile_blocks[MAX_TILE_BLOCKS];
    int tile_block_count;

    Rect walkable[MAX_WALKABLE_RECTS];
    int walkable_count;

    bool occupied[MAX_WORLD_TILES_Y][MAX_WORLD_TILES_X];

    Seg wall_segments[MAX_WALL_SEGMENTS];
    int wall_segment_count;

    Rect top_wall_covers[MAX_TOP_WALL_COVERS];
    int top_wall_cover_count;

    Rect wall_obstacles[MAX_TOP_WALL_COVERS];
    int wall_obstacle_count;

    Rect billiards_table;
    Rect billiards_popup;
    Rect food_buffet;
    Rect food_buffet2;
    Rect decor_tables[MAX_DECOR_TABLES];
    int decor_table_count;
    Rect door;
    Rect door_zone;
    Rect wall_window;
    Rect barista_rect;
    Rect barista_zone;
    Rect arcade_rect;
    Rect arcade_zone;

    float spawn_x;
    float spawn_y;
} MapState;

MapConfig map_default_config(void);
void map_build(MapState *map, const MapConfig *config);
Rect map_default_arcade_rect(void);
void map_set_arcade_rect(MapState *map, Rect rect);

void map_draw_floor(SDL_Renderer *renderer, const MapState *map);
void map_draw_wall_segments(SDL_Renderer *renderer, const MapState *map);

SDL_Rect map_world_to_screen_rect(const MapState *map, Rect world_rect);

int map_get_solid_obstacles(const MapState *map, Rect *out, int max_count);

void map_move_player(
    const MapState *map,
    float *x,
    float *y,
    float player_w,
    float player_h,
    float dx,
    float dy
);

InteractionTarget map_get_interaction_target(
    const MapState *map,
    Rect player_rect,
    bool popup_visible
);

#endif
