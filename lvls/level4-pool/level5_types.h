#ifndef LEVEL5_TYPES_H
#define LEVEL5_TYPES_H

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <stdbool.h>

#include "../shared/session.h"

typedef struct {
    float x;
    float y;
} Vec2;

typedef struct {
    SDL_Texture *texture;
    int width;
    int height;
} TextureAsset;

typedef struct {
    TextureAsset texture;
    int columns;
    int rows;
    int frame_count;
    int fps;
} SpriteSheet;

typedef struct {
    float x;
    float floor_top;
    float y;
    float base_y;
    float width;
    float height;
    SDL_Color color1;
    SDL_Color color2;
    float bob_offset;
    float tilt;
    float drift;
    bool hidden;
    bool flip_x;
} Platform;

typedef enum {
    SHEET_BURGLAR = 0,
    SHEET_BURGLAR_HAPPY,
    SHEET_BURGLAR_SMOKE,
    SHEET_BURGLAR2,
    SHEET_BURGLAR2_HAPPY,
    SHEET_BURGLAR2_SMOKE,
    SHEET_KEVIN,
    SHEET_FRIEND_BACK,
    SHEET_FRIEND_FRONT,
    SHEET_INNER_TUBE,
    SHEET_COUNT
} SheetId;

typedef struct {
    float x;
    float y;
    float radius;
    int health;
    int max_health;
    int stock_health;
    int stock_max_health;
    int respawn_fade_frames;
    int respawn_tube_lock_frames;
    bool respawn_airborne;
    float angle;
    float display_angle;
    float power;
    bool alive;
    bool defeated;
    float vx;
    float vy;
    bool on_float;
    bool scripted_doomed;
    bool death_cue_played;
    float rider_offset_x;
    int knock_timer;
    const char *name;
    SDL_Color skin_color;
    SDL_Color hat_color;
    SDL_Color body_color;
    SDL_Color float_color1;
    SDL_Color float_color2;
    SheetId default_sheet;
    Platform platform;
} Character;

typedef struct {
    float x;
    float y;
    float width;
    float height;
} BlockConfig;

typedef struct {
    float x;
    float y;
    float width;
    float height;
    int z_index;
} TvConfig;

typedef struct {
    float x;
    float y;
    float width;
    float height;
    int z_index;
} AdConfig;

typedef struct {
    float altitude;
    float size;
} AirplaneConfig;

typedef struct {
    float x;
    float float_y;
    int render_height;
    int sprite_cols;
    int sprite_rows;
    int sprite_frames;
} PlacementConfig;

typedef struct {
    PlacementConfig player;
    PlacementConfig player2;
    PlacementConfig enemies[3];
    BlockConfig blocks[3];
    TvConfig tv;
    AdConfig ad;
    AirplaneConfig airplane;
} LevelConfig;

typedef struct {
    float x;
    float y;
    float width;
    float height;
    SDL_Color color;
    float bob_offset;
    float tilt_offset;
} Block;

typedef enum {
    OWNER_NONE = 0,
    OWNER_PLAYER,
    OWNER_ENEMY,
    OWNER_SCRIPTED_ENEMY
} ProjectileOwner;

typedef struct {
    bool active;
    float x;
    float y;
    float vx;
    float vy;
    float radius;
    int bounces;
    ProjectileOwner owner;
    Character *scripted_target;
    float base_power;
    float current_power;
} Projectile;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float life;
    float size;
    float alpha;
} SplashParticle;

typedef enum {
    GAME_AIMING = 0,
    GAME_ENEMY_THINKING,
    GAME_PROJECTILE,
    GAME_WON,
    GAME_LOST
} GameState;

typedef enum {
    TURN_PLAYER = 0,
    TURN_ENEMY
} TurnState;

typedef enum {
    INTRO_FADE_IN = 0,
    INTRO_TITLE,
    INTRO_DONE
} IntroState;

typedef enum {
    RESPAWN_FLIGHT_TO_LEFT = 0,
    RESPAWN_FLIGHT_TO_RIGHT
} RespawnFlightStage;

typedef struct {
    bool active;
    int player_index;
    RespawnFlightStage stage;
    bool drop_started;
    float flight_altitude;
} RespawnSequence;

typedef struct {
    bool active;
    float x;
    float y;
} MouseAim;

typedef struct {
    TextureAsset background;
    TextureAsset wall;
    TextureAsset tv;
    TextureAsset airplane;
    TextureAsset airplane_high_speed;
    TextureAsset player_respawn_skin1;
    TextureAsset player_respawn_skin2;
    TextureAsset ad_frames[AD_MAX_FRAMES];
    int ad_frame_count;
    SpriteSheet sheets[SHEET_COUNT];
    TTF_Font *chapter_font;
    SDL_Texture *chapter_title_tex;
    int chapter_title_w;
    int chapter_title_h;
    Mix_Music *music_background;
    Mix_Chunk *sfx_win;
    Mix_Chunk *sfx_splash;
    Mix_Chunk *sfx_fire;
    Mix_Chunk *sfx_among;
    Mix_Chunk *sfx_oh_no;
    Mix_Chunk *sfx_airplane_start_move;
    Mix_Chunk *sfx_respawn_drop;
    bool image_ok;
    bool image_owned;
    bool audio_ok;
    bool mixer_owned;
    bool audio_device_owned;
} Assets;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    Assets assets;
    GameSession *session;

    LevelConfig config;

    Character player;
    Character player2;
    Character enemies[3];
    Block blocks[3];
    Projectile projectile;
    SplashParticle splashes[MAX_SPLASHES];
    int splash_count;
    float airplane_x;
    bool airplane_facing_right;
    RespawnSequence respawn_sequence;

    float camera_x;
    GameState game_state;
    TurnState current_turn;
    int enemy_shoot_timer;
    int enemy_turn_index;
    int enemy_miss_streak;
    bool enemy_shot_hit_current;
    bool duo_mode;
    int active_player_index;
    int player_start_health[2];
    int player_skin_number[2];
    int time_frames;
    double fps_accum_seconds;
    int fps_accum_frames;
    float fps_value;
    bool admin_mode;
    int win_animation_start;
    bool win_audio_played;
    bool opening_shot_done;
    int kevin_throw_until;
    int among_focus_until;
    bool pending_among_reveal;
    MouseAim mouse_aim;
    int skip_key_count;
    int throw_key_count;
    IntroState intro_state;
    int intro_state_start_frame;
    float intro_white_alpha;
    float intro_title_alpha;

    int editor_target;
    int editor_field;
    char status_text[MAX_STATUS_TEXT];
    int status_until;

    char base_path[1024];
    char assets_path[1024];
    char config_path[1024];

    bool running;
    bool smoke_test;
    int outcome_frame;
    bool pause_menu_active;
    bool pause_menu_ready;
} Game;

typedef struct {
    SpriteSheet *sheet;
    int frame_index;
} SheetState;

typedef struct {
    const char *label;
    void *value_ptr;
    bool is_integer;
    double min_value;
    double max_value;
    double step;
} EditorFieldRef;

typedef struct {
    float horizontal_force;
    float upward_force;
    float height_factor;
} HitForce;

static const LevelConfig DEFAULT_LEVEL_CONFIG = {
    .player = {430.0f, FLOOR_Y, 92, 0, 0, 0},
    .player2 = {590.0f, FLOOR_Y, 92, 0, 0, 0},
    .enemies = {
        {1320.0f, FLOOR_Y, 64, 6, 6, 36},
        {1930.0f, FLOOR_Y, 64, 6, 6, 36},
        {1039.0f, FLOOR_Y, 64, 6, 6, 30},
    },
    .blocks = {
        {760.0f, 335.0f, 69.0f, 200.0f},
        {1702.0f, 350.0f, 69.0f, 150.0f},
        {2134.0f, 335.0f, 69.0f, 120.0f},
    },
    .tv = {1540.0f, 185.0f, 240.0f, 170.0f, 2},
    .ad = {1562.0f, 224.0f, 196.0f, 96.0f, 1},
    .airplane = {110.0f, 1.0f},
};

#endif
