#ifndef HEADER_H
#define HEADER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#define WINDOW_WIDTH 1366
#define WINDOW_HEIGHT 820

#define MAX_PHOTOS 24
#define MIN_ROUNDS 5
#define MAX_ROUNDS MAX_PHOTOS
#define OPTION_COUNT 3
#define SNOWFLAKE_COUNT 260

#define PHOTOS_DIR "photos"
#define BACKGROUND_PATH "background-image/homealonebackground.png"
#define SONG_DIR "song "
#define SUCCESS_SOUND_PATH "success sound effect/updatepelgo-success-221935.mp3"
#define FAILURE_SOUND_PATH "echec sound /alexis_gaming_cam-echec-370391.mp3"
#define FONT_PATH "font/ka1.ttf"

typedef enum MessageType {
    MESSAGE_NONE = 0,
    MESSAGE_SUCCESS,
    MESSAGE_FAILURE,
    MESSAGE_FINAL
} MessageType;

typedef struct PhotoAsset {
    char path[512];
    char name[128];
    SDL_Texture *texture;
    int width;
    int height;
} PhotoAsset;

typedef struct PieceOption {
    int photo_index;
    SDL_Rect src_rect;
    SDL_FRect card_rect;
    SDL_FRect preview_rect;
    bool correct;
} PieceOption;

typedef struct RoundData {
    int puzzle_photo_index;
    SDL_FRect image_rect;
    SDL_FRect piece_rect;
    float norm_x;
    float norm_y;
    float norm_w;
    float norm_h;
    PieceOption options[OPTION_COUNT];
} RoundData;

typedef struct DragState {
    bool active;
    int option_index;
    float offset_x;
    float offset_y;
    float mouse_x;
    float mouse_y;
} DragState;

typedef struct MessageOverlay {
    bool visible;
    bool game_finished;
    MessageType type;
    Uint32 start_ticks;
    char eyebrow[48];
    char title[96];
    char text[192];
    char button[32];
    SDL_Texture *panel_texture;
    int panel_width;
    int panel_height;
} MessageOverlay;

typedef struct Snowflake {
    float x;
    float y;
    float radius;
    float speed_y;
    float drift;
    float phase;
    Uint8 alpha;
    bool front_layer;
} Snowflake;

typedef struct Game {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_tiny;
    TTF_Font *font_small;
    TTF_Font *font_medium;
    TTF_Font *font_large;
    Mix_Music *music;
    Mix_Chunk *success_sound;
    Mix_Chunk *failure_sound;
    SDL_Texture *background_texture;
    int background_width;
    int background_height;
    bool running;
    bool fullscreen;

    PhotoAsset photos[MAX_PHOTOS];
    int photo_count;

    RoundData rounds[MAX_ROUNDS];
    int round_count;
    int current_round;
    int score;
    Uint32 round_start_ticks;
    Uint32 last_frame_ticks;
    bool round_locked;
    bool show_completed_piece;
    bool hover_target;
    bool muted;

    DragState drag;
    MessageOverlay overlay;
    Snowflake snowflakes[SNOWFLAKE_COUNT];
} Game;

int game_init(Game *game);
void game_run(Game *game);
void game_cleanup(Game *game);

#endif
