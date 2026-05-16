#include "headers.h"

/**
 * @file fonctions.c
 * @brief Implementation of asset loading, background scrolling, platforms,
 * player physics, collision, and animation rendering.
 */

/**
 * @brief Load a texture from exactly one path.
 *
 * This helper is intentionally small: it only tries the path it receives. The
 * public chargerImage() wrapper adds the project-root/teskito fallback behavior.
 *
 * @param renderer SDL renderer used to create the texture.
 * @param path Path passed to IMG_Load().
 * @return SDL texture on success, NULL on failure.
 */
static SDL_Texture *chargerImageDepuis(SDL_Renderer *renderer, const char *path)
{
    SDL_Surface *surface = IMG_Load(path);
    if (!surface) {
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

SDL_Texture *chargerImage(SDL_Renderer *renderer, const char *path)
{
    char relative_path[512];
    SDL_Texture *texture = chargerImageDepuis(renderer, path);

    if (!texture) {
        snprintf(relative_path, sizeof(relative_path), "../%s", path);
        texture = chargerImageDepuis(renderer, relative_path);
    }

    if (!texture) {
        SDL_Log("Impossible de charger %s: %s", path, IMG_GetError());
    }

    return texture;
}

/**
 * @brief Draw one tiled background layer.
 *
 * The function repeats the same texture horizontally so the layer never leaves
 * blank space while the camera scrolls. The y and h parameters are the values
 * you tune from the macros in headers.h.
 *
 * @param renderer SDL renderer.
 * @param texture Layer texture.
 * @param viewport Area where the layer is drawn.
 * @param scroll Horizontal scroll value for this layer.
 * @param y Base Y position of the layer.
 * @param h Drawn height of the layer.
 * @param camera_y Vertical camera/parallax offset.
 */
static void afficherCouche(SDL_Renderer *renderer, SDL_Texture *texture,
                           const SDL_Rect *viewport, float scroll, int y,
                           int h, float camera_y)
{
    int tex_w = 0;
    int tex_h = 0;
    int start_x;

    if (!texture || !viewport) {
        return;
    }

    SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h);
    if (tex_w <= 0 || tex_h <= 0) {
        return;
    }

    /*
     * Convert the scroll value to a negative start position. The modulo keeps
     * start_x inside one texture width, then the loop below tiles to the right.
     */
    start_x = -(int)scroll % tex_w;
    if (start_x > 0) {
        start_x -= tex_w;
    }

    for (int x = start_x; x < viewport->w; x += tex_w) {
        SDL_Rect dst = {viewport->x + x, viewport->y + y - (int)camera_y, tex_w, h};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
    }
}

int initBackground(Background *bg, SDL_Renderer *renderer)
{
    if (!bg || !renderer) {
        return 0;
    }

    bg->far = chargerImage(renderer, "lvls/level2-chase/assets/backgrounds/bg_street_far.png");
    bg->mid = chargerImage(renderer, "lvls/level2-chase/assets/backgrounds/bg_street_mid.png");
    bg->near = chargerImage(renderer, "lvls/level2-chase/assets/backgrounds/bg_street_near.png");
    bg->ground = chargerImage(renderer, "lvls/level2-chase/assets/backgrounds/bg_ground.png");
    bg->far_scroll = 0.0f;
    bg->mid_scroll = 0.0f;
    bg->near_scroll = 0.0f;
    bg->ground_scroll = 0.0f;

    return bg->far && bg->mid && bg->near && bg->ground;
}

void scrollingBackground(Background *bg, float camera_dx)
{
    if (!bg) {
        return;
    }

    /*
     * Smaller multipliers make far layers move slowly. This creates the
     * parallax effect: distant scenery feels farther away than the ground.
     */
    bg->far_scroll += camera_dx * 0.20f;
    bg->mid_scroll += camera_dx * 0.45f;
    bg->near_scroll += camera_dx * 0.75f;
    bg->ground_scroll += camera_dx;
}

void afficherBackground(SDL_Renderer *renderer, const Background *bg,
                        const SDL_Rect *viewport, float camera_x,
                        float camera_y)
{
    float far_y;
    float mid_y;
    float near_y;

    if (!renderer || !bg || !viewport) {
        return;
    }

    SDL_RenderSetViewport(renderer, viewport);
    SDL_SetRenderDrawColor(renderer, 95, 154, 188, 255);
    SDL_RenderFillRect(renderer, NULL);

    /*
     * Vertical parallax uses different fractions of camera_y. The foreground
     * reacts more strongly than the far layer, while the ground follows the
     * real camera_y exactly so collisions and visuals stay aligned.
     */
    far_y = camera_y * 0.15f;
    mid_y = camera_y * 0.35f;
    near_y = camera_y * 0.65f;

    afficherCouche(renderer, bg->far, viewport, bg->far_scroll + camera_x * 0.02f,
                   BG_FAR_Y, viewport->h + BG_FAR_H_EXTRA, far_y);
    afficherCouche(renderer, bg->mid, viewport, bg->mid_scroll,
                   BG_MID_Y, viewport->h + BG_MID_H_EXTRA, mid_y);
    afficherCouche(renderer, bg->near, viewport, bg->near_scroll,
                   BG_NEAR_Y, viewport->h + BG_NEAR_H_EXTRA, near_y);
    afficherCouche(renderer, bg->ground, viewport, bg->ground_scroll,
                   BG_GROUND_Y, BG_GROUND_H, camera_y);
}

void libererBackground(Background *bg)
{
    if (!bg) {
        return;
    }

    if (bg->far) SDL_DestroyTexture(bg->far);
    if (bg->mid) SDL_DestroyTexture(bg->mid);
    if (bg->near) SDL_DestroyTexture(bg->near);
    if (bg->ground) SDL_DestroyTexture(bg->ground);
    bg->far = NULL;
    bg->mid = NULL;
    bg->near = NULL;
    bg->ground = NULL;
}

void initPlateformes(Platform platforms[], int count)
{
    /*
     * Each platform uses world coordinates. Rendering later subtracts camera_x
     * and camera_y, so the same data works for mono and split-screen views.
     */
    Platform data[MAX_PLATFORMS] = {
        {{240, 500, 220, 28}, PLATFORM_FIXED, 1, 240.0f, 0.0f, 0.0f},
        {{560, 425, 190, 28}, PLATFORM_MOVING, 1, 560.0f, 130.0f, 2.0f},
        {{890, 350, 180, 28}, PLATFORM_DESTRUCTIBLE, 1, 890.0f, 0.0f, 0.0f},
        {{1180, 470, 230, 28}, PLATFORM_FIXED, 1, 1180.0f, 0.0f, 0.0f},
        {{1510, 390, 180, 28}, PLATFORM_MOVING, 1, 1510.0f, 95.0f, 2.7f},
        {{1840, 315, 190, 28}, PLATFORM_DESTRUCTIBLE, 1, 1840.0f, 0.0f, 0.0f},
        {{2180, 475, 260, 28}, PLATFORM_FIXED, 1, 2180.0f, 0.0f, 0.0f},
        {{2580, 405, 200, 28}, PLATFORM_MOVING, 1, 2580.0f, 160.0f, 1.8f}
    };

    if (!platforms || count <= 0) {
        return;
    }

    for (int i = 0; i < count && i < MAX_PLATFORMS; ++i) {
        platforms[i] = data[i];
    }
}

void updatePlateformes(Platform platforms[], int count, float dt)
{
    (void)dt;
    if (!platforms) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        if (platforms[i].type == PLATFORM_MOVING && platforms[i].active) {
            /*
             * Moving platforms oscillate around base_x. SDL_GetTicks() keeps
             * their movement continuous even when the frame rate changes.
             */
            platforms[i].rect.x = (int)(platforms[i].base_x +
                sinf(SDL_GetTicks() * 0.001f * platforms[i].move_speed) *
                platforms[i].move_range);
        }
    }
}

void afficherPlateformes(SDL_Renderer *renderer, const Platform platforms[],
                         int count, const SDL_Rect *viewport, float camera_x,
                         float camera_y)
{
    if (!renderer || !platforms || !viewport) {
        return;
    }

    SDL_RenderSetViewport(renderer, viewport);

    for (int i = 0; i < count; ++i) {
        SDL_Rect dst;

        if (!platforms[i].active) {
            continue;
        }

        /* Convert world coordinates to current viewport screen coordinates. */
        dst = platforms[i].rect;
        dst.x = viewport->x + (int)(platforms[i].rect.x - camera_x);
        dst.y = viewport->y + (int)(platforms[i].rect.y - camera_y);

        if (dst.x + dst.w < viewport->x || dst.x > viewport->x + viewport->w) {
            continue;
        }

        if (platforms[i].type == PLATFORM_FIXED) {
            SDL_SetRenderDrawColor(renderer, 91, 70, 48, 255);
        } else if (platforms[i].type == PLATFORM_MOVING) {
            SDL_SetRenderDrawColor(renderer, 52, 111, 156, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 156, 82, 54, 255);
        }
        SDL_RenderFillRect(renderer, &dst);

        SDL_SetRenderDrawColor(renderer, 242, 214, 153, 255);
        SDL_RenderDrawRect(renderer, &dst);
    }
}

int initPlayer(Player *player, SDL_Renderer *renderer)
{
    if (!player || !renderer) {
        return 0;
    }

    player->idle = chargerImage(renderer, "lvls/level1-climb/assets/animations/skin1_idle.png");
    player->run = chargerImage(renderer, "lvls/level1-climb/assets/animations/skin1_run.png");
    player->jump = chargerImage(renderer, "lvls/level1-climb/assets/animations/skin1_jump.png");
    player->x = 120.0f;
    player->y = GROUND_Y - PLAYER_H;
    player->vx = 0.0f;
    player->vy = 0.0f;
    player->on_ground = 1;
    player->facing_right = 1;
    player->frame = 0;
    player->frame_timer = 0.0f;
    player->frame_delay = 1.0f / 14.0f;

    return player->idle && player->run && player->jump;
}

void gererInputPlayer(Player *player, const Uint8 *keys)
{
    if (!player || !keys) {
        return;
    }

    player->vx = 0.0f;
    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        player->vx = -PLAYER_SPEED;
        player->facing_right = 0;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        player->vx = PLAYER_SPEED;
        player->facing_right = 1;
    }
    if ((keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) &&
        player->on_ground) {
        player->vy = -JUMP_SPEED;
        player->on_ground = 0;
    }
}

/**
 * @brief Check whether the player lands on top of a platform this frame.
 *
 * The previous bottom position prevents side collisions from being treated as
 * landings. Only a downward crossing from above counts as standing on the
 * platform.
 *
 * @param player Player after movement.
 * @param platform Platform to test.
 * @param previous_bottom Player bottom before vertical movement.
 * @return Non-zero when the player lands on the platform.
 */
static int collisionDessus(const Player *player, const Platform *platform,
                           float previous_bottom)
{
    float left = player->x + 24.0f;
    float right = player->x + PLAYER_W - 24.0f;
    float bottom = player->y + PLAYER_H;

    return platform->active &&
           previous_bottom <= platform->rect.y &&
           bottom >= platform->rect.y &&
           right > platform->rect.x &&
           left < platform->rect.x + platform->rect.w;
}

void updatePlayer(Player *player, Platform platforms[], int count, float dt)
{
    float previous_bottom;

    if (!player) {
        return;
    }

    /*
     * Store the old bottom before gravity moves the player. Collision uses this
     * to detect a clean landing from above.
     */
    previous_bottom = player->y + PLAYER_H;
    player->x += player->vx * dt;
    player->vy += GRAVITY * dt;
    player->y += player->vy * dt;
    player->on_ground = 0;

    /* Ground collision: keeps the player from falling below the floor. */
    if (player->y + PLAYER_H >= GROUND_Y) {
        player->y = GROUND_Y - PLAYER_H;
        player->vy = 0.0f;
        player->on_ground = 1;
    }

    for (int i = 0; platforms && i < count; ++i) {
        if (collisionDessus(player, &platforms[i], previous_bottom)) {
            player->y = (float)(platforms[i].rect.y - PLAYER_H);
            player->vy = 0.0f;
            player->on_ground = 1;
            if (platforms[i].type == PLATFORM_DESTRUCTIBLE) {
                /* Destructible platforms disappear after one successful landing. */
                platforms[i].active = 0;
            }
        }
    }

    if (player->x < 0.0f) {
        player->x = 0.0f;
    }

    /* Animation frame timing is shared by idle, run, and jump sprite sheets. */
    player->frame_timer += dt;
    if (player->frame_timer >= player->frame_delay) {
        player->frame_timer = 0.0f;
        player->frame = (player->frame + 1) % FRAME_COUNT;
    }
}

void afficherPlayer(SDL_Renderer *renderer, const Player *player,
                    const SDL_Rect *viewport, float camera_x, float camera_y)
{
    SDL_Texture *texture;
    SDL_Rect src;
    SDL_Rect dst;
    SDL_RendererFlip flip;
    int tex_w = 0;
    int tex_h = 0;
    int rows = FRAME_COUNT / FRAME_COLS;
    int col;
    int row;

    if (!renderer || !player || !viewport) {
        return;
    }

    /*
     * Choose animation state:
     * - jump has priority while airborne;
     * - run is used while moving on the ground;
     * - idle is the default.
     */
    texture = player->idle;
    if (!player->on_ground) {
        texture = player->jump;
    } else if (fabsf(player->vx) > 1.0f) {
        texture = player->run;
    }

    if (!texture) {
        return;
    }

    /*
     * Sprite sheets have 36 frames arranged in 6 columns. The current frame is
     * converted to source rectangle coordinates before drawing.
     */
    SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h);
    col = player->frame % FRAME_COLS;
    row = player->frame / FRAME_COLS;
    src.x = col * (tex_w / FRAME_COLS);
    src.y = row * (tex_h / rows);
    src.w = tex_w / FRAME_COLS;
    src.h = tex_h / rows;

    dst.x = viewport->x + (int)(player->x - camera_x);
    dst.y = viewport->y + (int)(player->y - camera_y);
    dst.w = PLAYER_W;
    dst.h = PLAYER_H;
    flip = player->facing_right ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;

    SDL_RenderSetViewport(renderer, viewport);
    SDL_RenderCopyEx(renderer, texture, &src, &dst, 0.0, NULL, flip);
}

void libererPlayer(Player *player)
{
    if (!player) {
        return;
    }

    if (player->idle) SDL_DestroyTexture(player->idle);
    if (player->run) SDL_DestroyTexture(player->run);
    if (player->jump) SDL_DestroyTexture(player->jump);
    player->idle = NULL;
    player->run = NULL;
    player->jump = NULL;
}
