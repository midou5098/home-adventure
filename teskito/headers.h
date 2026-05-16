#ifndef HEADERS_H
#define HEADERS_H

/**
 * @file headers.h
 * @brief Shared types, constants, and function prototypes for the Teskito
 * background scrolling task.
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_W 1280
#define SCREEN_H 720

/**
 * @name Player Tuning
 * Values used by the player movement, jump, gravity, and animation systems.
 */
/**@{*/
#define PLAYER_W 96
#define PLAYER_H 128
#define PLAYER_SPEED 360.0f
#define JUMP_SPEED 760.0f
#define GRAVITY 1800.0f
#define GROUND_Y 548.0f
#define FRAME_COUNT 36
#define FRAME_COLS 6
/**@}*/

/**
 * @brief Number of platforms created by initPlateformes().
 */
#define MAX_PLATFORMS 8

/**
 * @brief Background layout variables.
 *
 * Change these values to move or resize each background layer without editing
 * the drawing code in fonctions.c.
 *
 * The Y values move layers vertically:
 * - bigger value: lower on the screen
 * - smaller or negative value: higher on the screen
 *
 * The height extra values are added to the current viewport height.
 */
#define BG_FAR_Y -50
#define BG_FAR_H_EXTRA 120
#define BG_MID_Y 0
#define BG_MID_H_EXTRA 120
#define BG_NEAR_Y -358
#define BG_NEAR_H_EXTRA 700
#define BG_GROUND_Y 548
#define BG_GROUND_H 150

/**
 * @brief Platform behavior type.
 */
typedef enum {
    /** Static platform. */
    PLATFORM_FIXED,
    /** Platform moves horizontally using a sine wave. */
    PLATFORM_MOVING,
    /** Platform disappears after the player lands on it. */
    PLATFORM_DESTRUCTIBLE
} PlatformType;

/**
 * @brief Layered scrolling background.
 *
 * Stores one texture per parallax layer and the current horizontal scroll for
 * each layer.
 */
typedef struct {
    /** Farthest background texture. Moves the slowest. */
    SDL_Texture *far;
    /** Middle background texture. */
    SDL_Texture *mid;
    /** Near foreground background texture. */
    SDL_Texture *near;
    /** Ground texture drawn at BG_GROUND_Y. */
    SDL_Texture *ground;
    /** Horizontal scroll offset for far. */
    float far_scroll;
    /** Horizontal scroll offset for mid. */
    float mid_scroll;
    /** Horizontal scroll offset for near. */
    float near_scroll;
    /** Horizontal scroll offset for ground. */
    float ground_scroll;
} Background;

/**
 * @brief A platform used by the player collision system.
 */
typedef struct {
    /** World rectangle before camera offset is applied. */
    SDL_Rect rect;
    /** Platform behavior. */
    PlatformType type;
    /** 1 when visible/collidable, 0 after destruction. */
    int active;
    /** Original X position used by moving platforms. */
    float base_x;
    /** Maximum horizontal distance from base_x. */
    float move_range;
    /** Oscillation speed multiplier. */
    float move_speed;
} Platform;

/**
 * @brief Player state and animation textures.
 *
 * The player switches between idle, run, and jump sprite sheets depending on
 * movement and ground state.
 */
typedef struct {
    /** Idle animation sprite sheet. */
    SDL_Texture *idle;
    /** Run animation sprite sheet. */
    SDL_Texture *run;
    /** Jump animation sprite sheet. */
    SDL_Texture *jump;
    /** Player world X position. */
    float x;
    /** Player world Y position. */
    float y;
    /** Horizontal velocity in pixels per second. */
    float vx;
    /** Vertical velocity in pixels per second. */
    float vy;
    /** 1 when the player can jump. */
    int on_ground;
    /** 1 for right, 0 for left. Used for horizontal sprite flip. */
    int facing_right;
    /** Current frame index in the active sprite sheet. */
    int frame;
    /** Accumulates time until the next animation frame. */
    float frame_timer;
    /** Seconds between animation frames. */
    float frame_delay;
} Player;

/**
 * @brief Load an image file as an SDL texture.
 *
 * The loader tries the given path first, then tries the same path prefixed with
 * "../" so the program works when launched from the project root or from the
 * teskito directory.
 *
 * @param renderer SDL renderer used to create the texture.
 * @param path Relative asset path.
 * @return Loaded texture, or NULL on failure.
 */
SDL_Texture *chargerImage(SDL_Renderer *renderer, const char *path);

/**
 * @brief Load all background textures and reset scroll positions.
 *
 * @param bg Background structure to initialize.
 * @param renderer SDL renderer used for texture creation.
 * @return 1 on success, 0 on failure.
 */
int initBackground(Background *bg, SDL_Renderer *renderer);

/**
 * @brief Update horizontal parallax scroll values.
 *
 * @param bg Background to update.
 * @param camera_dx Horizontal camera movement since the previous frame.
 */
void scrollingBackground(Background *bg, float camera_dx);

/**
 * @brief Draw the background inside a viewport.
 *
 * Uses horizontal and vertical camera offsets to support left/right scrolling
 * and up/down scrolling while jumping.
 *
 * @param renderer SDL renderer.
 * @param bg Background to draw.
 * @param viewport Screen area where the background is drawn.
 * @param camera_x Horizontal camera offset.
 * @param camera_y Vertical camera offset.
 */
void afficherBackground(SDL_Renderer *renderer, const Background *bg,
                        const SDL_Rect *viewport, float camera_x,
                        float camera_y);

/**
 * @brief Destroy background textures.
 *
 * @param bg Background to clean up.
 */
void libererBackground(Background *bg);

/**
 * @brief Initialize fixed, moving, and destructible platforms.
 *
 * @param platforms Platform array.
 * @param count Number of items in the platform array.
 */
void initPlateformes(Platform platforms[], int count);

/**
 * @brief Update platform animation.
 *
 * @param platforms Platform array.
 * @param count Number of items in the platform array.
 * @param dt Delta time in seconds.
 */
void updatePlateformes(Platform platforms[], int count, float dt);

/**
 * @brief Draw active platforms inside a viewport.
 *
 * @param renderer SDL renderer.
 * @param platforms Platform array.
 * @param count Number of platforms.
 * @param viewport Screen area where platforms are drawn.
 * @param camera_x Horizontal camera offset.
 * @param camera_y Vertical camera offset.
 */
void afficherPlateformes(SDL_Renderer *renderer, const Platform platforms[],
                         int count, const SDL_Rect *viewport, float camera_x,
                         float camera_y);

/**
 * @brief Load player idle, run, and jump sprite sheets.
 *
 * @param player Player structure to initialize.
 * @param renderer SDL renderer used for texture creation.
 * @return 1 on success, 0 on failure.
 */
int initPlayer(Player *player, SDL_Renderer *renderer);

/**
 * @brief Read keyboard state and update player velocity/jump state.
 *
 * @param player Player to control.
 * @param keys SDL keyboard state from SDL_GetKeyboardState().
 */
void gererInputPlayer(Player *player, const Uint8 *keys);

/**
 * @brief Update player physics, collisions, and animation frame.
 *
 * @param player Player to update.
 * @param platforms Platform array for collision.
 * @param count Number of platforms.
 * @param dt Delta time in seconds.
 */
void updatePlayer(Player *player, Platform platforms[], int count, float dt);

/**
 * @brief Draw the player with the current idle/run/jump animation.
 *
 * @param renderer SDL renderer.
 * @param player Player to draw.
 * @param viewport Screen area where the player is drawn.
 * @param camera_x Horizontal camera offset.
 * @param camera_y Vertical camera offset.
 */
void afficherPlayer(SDL_Renderer *renderer, const Player *player,
                    const SDL_Rect *viewport, float camera_x, float camera_y);

/**
 * @brief Destroy player animation textures.
 *
 * @param player Player to clean up.
 */
void libererPlayer(Player *player);

#endif
