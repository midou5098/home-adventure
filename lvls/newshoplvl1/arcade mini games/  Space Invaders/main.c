#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 720

#define PLAYER_SPEED 420.0f
#define PLAYER_BULLET_SPEED 720.0f
#define ENEMY_BULLET_SPEED 300.0f
#define ENEMY_MOVE_INTERVAL_START 0.65f
#define ENEMY_MOVE_INTERVAL_MIN 0.12f

#define MAX_PLAYER_BULLETS 4
#define MAX_ENEMY_BULLETS 12
#define MAX_BARRIERS 4
#define BARRIER_BLOCK_ROWS 4
#define BARRIER_BLOCK_COLS 6

#define ENEMY_ROWS 5
#define ENEMY_COLS 11
#define ENEMY_COUNT (ENEMY_ROWS * ENEMY_COLS)

typedef struct {
    float x;
    float y;
    float w;
    float h;
} RectF;

typedef struct {
    RectF rect;
    bool alive;
} Bullet;

typedef struct {
    RectF rect;
    bool alive;
    int type;
} Enemy;

typedef struct {
    SDL_Rect rect;
    int hp;
} BarrierBlock;

typedef struct {
    RectF rect;
    int lives;
    int score;
} Player;

typedef struct {
    Player player;
    Bullet player_bullets[MAX_PLAYER_BULLETS];
    Bullet enemy_bullets[MAX_ENEMY_BULLETS];
    Enemy enemies[ENEMY_COUNT];
    BarrierBlock barriers[MAX_BARRIERS][BARRIER_BLOCK_ROWS][BARRIER_BLOCK_COLS];
    int enemy_direction;
    int enemies_alive;
    float enemy_step_timer;
    float enemy_step_interval;
    float enemy_shot_timer;
    float player_shot_cooldown;
    float respawn_timer;
    bool game_over;
    bool victory;
    bool end_dialog_shown;
    SDL_Rect quit_button;
} GameState;

static bool rects_intersect(RectF a, RectF b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

static SDL_Rect to_sdl_rect(RectF rect) {
    SDL_Rect out = {
        .x = (int)rect.x,
        .y = (int)rect.y,
        .w = (int)rect.w,
        .h = (int)rect.h,
    };
    return out;
}

static const char *glyph_rows(char c) {
    switch (c) {
        case 'A': return "01110"
                         "10001"
                         "10001"
                         "11111"
                         "10001"
                         "10001"
                         "10001";
        case 'C': return "01111"
                         "10000"
                         "10000"
                         "10000"
                         "10000"
                         "10000"
                         "01111";
        case 'D': return "11110"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "11110";
        case 'E': return "11111"
                         "10000"
                         "10000"
                         "11110"
                         "10000"
                         "10000"
                         "11111";
        case 'H': return "10001"
                         "10001"
                         "10001"
                         "11111"
                         "10001"
                         "10001"
                         "10001";
        case 'G': return "01111"
                         "10000"
                         "10000"
                         "10111"
                         "10001"
                         "10001"
                         "01111";
        case 'I': return "11111"
                         "00100"
                         "00100"
                         "00100"
                         "00100"
                         "00100"
                         "11111";
        case 'L': return "10000"
                         "10000"
                         "10000"
                         "10000"
                         "10000"
                         "10000"
                         "11111";
        case 'M': return "10001"
                         "11011"
                         "10101"
                         "10001"
                         "10001"
                         "10001"
                         "10001";
        case 'N': return "10001"
                         "11001"
                         "10101"
                         "10011"
                         "10001"
                         "10001"
                         "10001";
        case 'O': return "01110"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "01110";
        case 'P': return "11110"
                         "10001"
                         "10001"
                         "11110"
                         "10000"
                         "10000"
                         "10000";
        case 'Q': return "01110"
                         "10001"
                         "10001"
                         "10001"
                         "10101"
                         "10010"
                         "01101";
        case 'R': return "11110"
                         "10001"
                         "10001"
                         "11110"
                         "10100"
                         "10010"
                         "10001";
        case 'S': return "01111"
                         "10000"
                         "10000"
                         "01110"
                         "00001"
                         "00001"
                         "11110";
        case 'T': return "11111"
                         "00100"
                         "00100"
                         "00100"
                         "00100"
                         "00100"
                         "00100";
        case 'U': return "10001"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "01110";
        case 'V': return "10001"
                         "10001"
                         "10001"
                         "10001"
                         "10001"
                         "01010"
                         "00100";
        case 'W': return "10001"
                         "10001"
                         "10001"
                         "10101"
                         "10101"
                         "10101"
                         "01010";
        case '/': return "00001"
                         "00010"
                         "00100"
                         "00100"
                         "01000"
                         "10000"
                         "00000";
        default: return NULL;
    }
}

static void draw_char(SDL_Renderer *renderer, int x, int y, char c, int scale) {
    if (c == ' ') {
        return;
    }

    const char *rows = glyph_rows(c);
    if (!rows) {
        return;
    }

    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if (rows[row * 5 + col] == '1') {
                SDL_Rect pixel = {
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                };
                SDL_RenderFillRect(renderer, &pixel);
            }
        }
    }
}

static void draw_text(SDL_Renderer *renderer, int x, int y, const char *text, int scale) {
    int cursor_x = x;
    size_t len = strlen(text);

    for (size_t i = 0; i < len; ++i) {
        draw_char(renderer, cursor_x, y, text[i], scale);
        cursor_x += (text[i] == ' ') ? (3 * scale) : (6 * scale);
    }
}

static int text_width(const char *text, int scale) {
    int width = 0;
    size_t len = strlen(text);

    for (size_t i = 0; i < len; ++i) {
        width += (text[i] == ' ') ? (3 * scale) : (6 * scale);
    }
    return width;
}

static bool point_in_rect(int x, int y, SDL_Rect rect) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

static void update_window_title(SDL_Window *window, const GameState *game) {
    char title[160];
    const char *state = "";

    if (game->game_over) {
        state = " | GAME OVER";
    } else if (game->victory) {
        state = " | WINNER";
    } else if (game->respawn_timer > 0.0f) {
        state = " | Respawning...";
    }

    snprintf(
        title,
        sizeof(title),
        "Space Invaders | Score: %d | Lives: %d%s",
        game->player.score,
        game->player.lives,
        state
    );
    SDL_SetWindowTitle(window, title);
}

static void reset_barriers(GameState *game) {
    const int start_x = 120;
    const int gap = 180;
    const int start_y = WINDOW_HEIGHT - 210;
    const int block_w = 16;
    const int block_h = 16;

    for (int b = 0; b < MAX_BARRIERS; ++b) {
        for (int row = 0; row < BARRIER_BLOCK_ROWS; ++row) {
            for (int col = 0; col < BARRIER_BLOCK_COLS; ++col) {
                BarrierBlock *block = &game->barriers[b][row][col];
                block->hp = 3;
                block->rect.x = start_x + b * gap + col * block_w;
                block->rect.y = start_y + row * block_h;
                block->rect.w = block_w - 2;
                block->rect.h = block_h - 2;

                if ((row == 3 && (col < 1 || col > 4)) || (row == 2 && (col == 2 || col == 3))) {
                    block->hp = 0;
                }
            }
        }
    }
}

static void reset_enemies(GameState *game) {
    const float enemy_w = 44.0f;
    const float enemy_h = 28.0f;
    const float start_x = 120.0f;
    const float start_y = 90.0f;
    const float gap_x = 22.0f;
    const float gap_y = 18.0f;

    game->enemies_alive = ENEMY_COUNT;
    game->enemy_direction = 1;
    game->enemy_step_timer = 0.0f;
    game->enemy_step_interval = ENEMY_MOVE_INTERVAL_START;
    game->enemy_shot_timer = 0.8f;

    for (int row = 0; row < ENEMY_ROWS; ++row) {
        for (int col = 0; col < ENEMY_COLS; ++col) {
            int idx = row * ENEMY_COLS + col;
            Enemy *enemy = &game->enemies[idx];
            enemy->alive = true;
            enemy->type = row;
            enemy->rect.x = start_x + col * (enemy_w + gap_x);
            enemy->rect.y = start_y + row * (enemy_h + gap_y);
            enemy->rect.w = enemy_w;
            enemy->rect.h = enemy_h;
        }
    }
}

static void reset_bullets(GameState *game) {
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        game->player_bullets[i].alive = false;
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        game->enemy_bullets[i].alive = false;
    }
}

static void reset_game(GameState *game) {
    game->player.rect.x = WINDOW_WIDTH / 2.0f - 28.0f;
    game->player.rect.y = WINDOW_HEIGHT - 70.0f;
    game->player.rect.w = 56.0f;
    game->player.rect.h = 24.0f;
    game->player.lives = 3;
    game->player.score = 0;

    game->player_shot_cooldown = 0.0f;
    game->respawn_timer = 0.0f;
    game->game_over = false;
    game->victory = false;
    game->end_dialog_shown = false;
    game->quit_button = (SDL_Rect){0, 0, 0, 0};

    reset_bullets(game);
    reset_barriers(game);
    reset_enemies(game);
}

static void spawn_player_bullet(GameState *game) {
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        Bullet *bullet = &game->player_bullets[i];
        if (!bullet->alive) {
            bullet->alive = true;
            bullet->rect.w = 6.0f;
            bullet->rect.h = 16.0f;
            bullet->rect.x = game->player.rect.x + game->player.rect.w / 2.0f - bullet->rect.w / 2.0f;
            bullet->rect.y = game->player.rect.y - bullet->rect.h;
            game->player_shot_cooldown = 0.3f;
            return;
        }
    }
}

static void spawn_enemy_bullet(GameState *game) {
    int live_columns[ENEMY_COLS];
    int live_count = 0;

    for (int col = 0; col < ENEMY_COLS; ++col) {
        for (int row = ENEMY_ROWS - 1; row >= 0; --row) {
            Enemy *enemy = &game->enemies[row * ENEMY_COLS + col];
            if (enemy->alive) {
                live_columns[live_count++] = row * ENEMY_COLS + col;
                break;
            }
        }
    }

    if (live_count == 0) {
        return;
    }

    int chosen = live_columns[rand() % live_count];
    Enemy *enemy = &game->enemies[chosen];

    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        Bullet *bullet = &game->enemy_bullets[i];
        if (!bullet->alive) {
            bullet->alive = true;
            bullet->rect.w = 6.0f;
            bullet->rect.h = 18.0f;
            bullet->rect.x = enemy->rect.x + enemy->rect.w / 2.0f - bullet->rect.w / 2.0f;
            bullet->rect.y = enemy->rect.y + enemy->rect.h;
            return;
        }
    }
}

static void damage_barrier(RectF bullet_rect, BarrierBlock barriers[MAX_BARRIERS][BARRIER_BLOCK_ROWS][BARRIER_BLOCK_COLS], bool *bullet_alive) {
    for (int b = 0; b < MAX_BARRIERS; ++b) {
        for (int row = 0; row < BARRIER_BLOCK_ROWS; ++row) {
            for (int col = 0; col < BARRIER_BLOCK_COLS; ++col) {
                BarrierBlock *block = &barriers[b][row][col];
                if (block->hp <= 0) {
                    continue;
                }

                RectF block_rect = {
                    .x = (float)block->rect.x,
                    .y = (float)block->rect.y,
                    .w = (float)block->rect.w,
                    .h = (float)block->rect.h,
                };

                if (rects_intersect(bullet_rect, block_rect)) {
                    block->hp--;
                    *bullet_alive = false;
                    return;
                }
            }
        }
    }
}

static void clear_enemy_bullets(GameState *game) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        game->enemy_bullets[i].alive = false;
    }
}

static void handle_player_hit(GameState *game) {
    if (game->respawn_timer > 0.0f || game->game_over) {
        return;
    }

    game->player.lives--;
    game->respawn_timer = 1.4f;
    clear_enemy_bullets(game);

    if (game->player.lives <= 0) {
        game->game_over = true;
    } else {
        game->player.rect.x = WINDOW_WIDTH / 2.0f - game->player.rect.w / 2.0f;
    }
}

static void update_enemy_speed(GameState *game) {
    float ratio = (float)game->enemies_alive / (float)ENEMY_COUNT;
    float interval = ENEMY_MOVE_INTERVAL_MIN + (ENEMY_MOVE_INTERVAL_START - ENEMY_MOVE_INTERVAL_MIN) * ratio;
    if (interval < ENEMY_MOVE_INTERVAL_MIN) {
        interval = ENEMY_MOVE_INTERVAL_MIN;
    }
    game->enemy_step_interval = interval;
}

static void move_enemies(GameState *game) {
    float left_edge = WINDOW_WIDTH;
    float right_edge = 0.0f;

    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *enemy = &game->enemies[i];
        if (!enemy->alive) {
            continue;
        }

        if (enemy->rect.x < left_edge) {
            left_edge = enemy->rect.x;
        }
        if (enemy->rect.x + enemy->rect.w > right_edge) {
            right_edge = enemy->rect.x + enemy->rect.w;
        }
    }

    float step_x = 18.0f * (float)game->enemy_direction;
    bool should_drop = (game->enemy_direction > 0 && right_edge + step_x >= WINDOW_WIDTH - 50.0f) ||
                       (game->enemy_direction < 0 && left_edge + step_x <= 50.0f);

    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *enemy = &game->enemies[i];
        if (!enemy->alive) {
            continue;
        }

        if (should_drop) {
            enemy->rect.y += 24.0f;
        } else {
            enemy->rect.x += step_x;
        }

        if (enemy->rect.y + enemy->rect.h >= game->player.rect.y - 12.0f) {
            game->game_over = true;
        }
    }

    if (should_drop) {
        game->enemy_direction *= -1;
    }
}

static void update_game(GameState *game, float dt, const Uint8 *keys) {
    if (game->game_over || game->victory) {
        return;
    }

    if (game->player_shot_cooldown > 0.0f) {
        game->player_shot_cooldown -= dt;
    }
    if (game->respawn_timer > 0.0f) {
        game->respawn_timer -= dt;
        if (game->respawn_timer < 0.0f) {
            game->respawn_timer = 0.0f;
        }
    }

    if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
        game->player.rect.x -= PLAYER_SPEED * dt;
    }
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
        game->player.rect.x += PLAYER_SPEED * dt;
    }

    if (game->player.rect.x < 30.0f) {
        game->player.rect.x = 30.0f;
    }
    if (game->player.rect.x + game->player.rect.w > WINDOW_WIDTH - 30.0f) {
        game->player.rect.x = WINDOW_WIDTH - 30.0f - game->player.rect.w;
    }

    game->enemy_step_timer += dt;
    if (game->enemy_step_timer >= game->enemy_step_interval) {
        game->enemy_step_timer = 0.0f;
        move_enemies(game);
    }

    game->enemy_shot_timer -= dt;
    if (game->enemy_shot_timer <= 0.0f && game->enemies_alive > 0) {
        spawn_enemy_bullet(game);
        float pressure = 0.15f + 0.9f * ((float)game->enemies_alive / (float)ENEMY_COUNT);
        game->enemy_shot_timer = pressure;
    }

    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        Bullet *bullet = &game->player_bullets[i];
        if (!bullet->alive) {
            continue;
        }

        bullet->rect.y -= PLAYER_BULLET_SPEED * dt;
        if (bullet->rect.y + bullet->rect.h < 0.0f) {
            bullet->alive = false;
            continue;
        }

        damage_barrier(bullet->rect, game->barriers, &bullet->alive);
        if (!bullet->alive) {
            continue;
        }

        for (int e = 0; e < ENEMY_COUNT; ++e) {
            Enemy *enemy = &game->enemies[e];
            if (!enemy->alive) {
                continue;
            }

            if (rects_intersect(bullet->rect, enemy->rect)) {
                enemy->alive = false;
                bullet->alive = false;
                game->enemies_alive--;
                game->player.score += (ENEMY_ROWS - enemy->type) * 10;
                update_enemy_speed(game);

                if (game->enemies_alive <= 0) {
                    game->victory = true;
                }
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        Bullet *bullet = &game->enemy_bullets[i];
        if (!bullet->alive) {
            continue;
        }

        bullet->rect.y += ENEMY_BULLET_SPEED * dt;
        if (bullet->rect.y > WINDOW_HEIGHT) {
            bullet->alive = false;
            continue;
        }

        damage_barrier(bullet->rect, game->barriers, &bullet->alive);
        if (!bullet->alive) {
            continue;
        }

        if (game->respawn_timer <= 0.0f && rects_intersect(bullet->rect, game->player.rect)) {
            bullet->alive = false;
            handle_player_hit(game);
        }
    }
}

static void draw_player(SDL_Renderer *renderer, const GameState *game) {
    if (game->game_over) {
        return;
    }
    if (game->respawn_timer > 0.0f) {
        int phase = (int)(game->respawn_timer * 12.0f);
        if (phase % 2 == 0) {
            return;
        }
    }

    SDL_SetRenderDrawColor(renderer, 88, 241, 144, 255);
    SDL_Rect body = to_sdl_rect(game->player.rect);
    SDL_RenderFillRect(renderer, &body);

    SDL_Rect left_wing = {
        body.x - 10, body.y + 8, 12, 10
    };
    SDL_Rect right_wing = {
        body.x + body.w - 2, body.y + 8, 12, 10
    };
    SDL_Rect cockpit = {
        body.x + body.w / 2 - 8, body.y - 8, 16, 10
    };
    SDL_RenderFillRect(renderer, &left_wing);
    SDL_RenderFillRect(renderer, &right_wing);
    SDL_RenderFillRect(renderer, &cockpit);
}

static void draw_enemies(SDL_Renderer *renderer, const GameState *game) {
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        const Enemy *enemy = &game->enemies[i];
        if (!enemy->alive) {
            continue;
        }

        switch (enemy->type) {
            case 0:
                SDL_SetRenderDrawColor(renderer, 255, 99, 72, 255);
                break;
            case 1:
            case 2:
                SDL_SetRenderDrawColor(renderer, 255, 177, 66, 255);
                break;
            default:
                SDL_SetRenderDrawColor(renderer, 111, 196, 255, 255);
                break;
        }

        SDL_Rect body = to_sdl_rect(enemy->rect);
        SDL_Rect eye_left = {body.x + 8, body.y + 7, 7, 7};
        SDL_Rect eye_right = {body.x + body.w - 15, body.y + 7, 7, 7};
        SDL_Rect legs_left = {body.x + 6, body.y + body.h - 4, 7, 8};
        SDL_Rect legs_right = {body.x + body.w - 13, body.y + body.h - 4, 7, 8};

        SDL_RenderFillRect(renderer, &body);
        SDL_SetRenderDrawColor(renderer, 19, 24, 38, 255);
        SDL_RenderFillRect(renderer, &eye_left);
        SDL_RenderFillRect(renderer, &eye_right);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &legs_left);
        SDL_RenderFillRect(renderer, &legs_right);
    }
}

static void draw_barriers(SDL_Renderer *renderer, const GameState *game) {
    for (int b = 0; b < MAX_BARRIERS; ++b) {
        for (int row = 0; row < BARRIER_BLOCK_ROWS; ++row) {
            for (int col = 0; col < BARRIER_BLOCK_COLS; ++col) {
                const BarrierBlock *block = &game->barriers[b][row][col];
                if (block->hp <= 0) {
                    continue;
                }

                if (block->hp == 3) {
                    SDL_SetRenderDrawColor(renderer, 140, 255, 160, 255);
                } else if (block->hp == 2) {
                    SDL_SetRenderDrawColor(renderer, 245, 211, 92, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 240, 113, 120, 255);
                }
                SDL_RenderFillRect(renderer, &block->rect);
            }
        }
    }
}

static void draw_bullets(SDL_Renderer *renderer, const GameState *game) {
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    for (int i = 0; i < MAX_PLAYER_BULLETS; ++i) {
        if (!game->player_bullets[i].alive) {
            continue;
        }
        SDL_Rect rect = to_sdl_rect(game->player_bullets[i].rect);
        SDL_RenderFillRect(renderer, &rect);
    }

    SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255);
    for (int i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!game->enemy_bullets[i].alive) {
            continue;
        }
        SDL_Rect rect = to_sdl_rect(game->enemy_bullets[i].rect);
        SDL_RenderFillRect(renderer, &rect);
    }
}

static void render_background(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 12, 21, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 32, 38, 61, 255);
    for (int i = 0; i < 48; ++i) {
        SDL_Rect star = {
            (i * 197) % WINDOW_WIDTH,
            (i * 131) % WINDOW_HEIGHT,
            2 + (i % 3),
            2 + (i % 3),
        };
        SDL_RenderFillRect(renderer, &star);
    }

    SDL_SetRenderDrawColor(renderer, 70, 255, 120, 255);
    SDL_Rect ground = {24, WINDOW_HEIGHT - 26, WINDOW_WIDTH - 48, 4};
    SDL_RenderFillRect(renderer, &ground);
}

static void draw_help_overlay(SDL_Renderer *renderer) {
    const char *help_text = "HOW TO PLAY  A/D MOVE  SPACE SHOOT  ESC QUIT";
    const int scale = 2;
    const int padding = 18;
    const int width = text_width(help_text, scale);
    const int box_height = 7 * scale + 10;
    SDL_Rect box = {
        WINDOW_WIDTH - width - padding * 2,
        12,
        width + padding,
        box_height,
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 12, 20, 32, 190);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 128, 216, 255, 255);
    SDL_RenderDrawRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 220, 236, 255, 255);
    draw_text(renderer, box.x + 8, box.y + 5, help_text, scale);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void draw_end_overlay(SDL_Renderer *renderer, GameState *game, int mouse_x, int mouse_y) {
    const char *title = game->victory ? "WINNER" : "GAME OVER";
    const int title_scale = 5;
    const int button_scale = 4;
    const int panel_w = 420;
    const int panel_h = 220;
    SDL_Rect panel = {
        WINDOW_WIDTH / 2 - panel_w / 2,
        WINDOW_HEIGHT / 2 - panel_h / 2,
        panel_w,
        panel_h,
    };
    int title_w = text_width(title, title_scale);
    int button_w = 180;
    int button_h = 54;
    bool hovered;

    game->quit_button = (SDL_Rect){
        panel.x + panel.w / 2 - button_w / 2,
        panel.y + panel.h - 84,
        button_w,
        button_h,
    };
    hovered = point_in_rect(mouse_x, mouse_y, game->quit_button);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 4, 8, 18, 190);
    SDL_Rect shade = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &shade);

    SDL_SetRenderDrawColor(renderer, 12, 20, 32, 245);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, game->victory ? 70 : 240, game->victory ? 255 : 113, game->victory ? 120 : 120, 255);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Rect accent = {panel.x + 18, panel.y + 18, panel.w - 36, 6};
    SDL_RenderFillRect(renderer, &accent);

    SDL_SetRenderDrawColor(renderer, 220, 236, 255, 255);
    draw_text(renderer, panel.x + panel.w / 2 - title_w / 2, panel.y + 56, title, title_scale);

    SDL_SetRenderDrawColor(renderer, hovered ? 95 : 34, hovered ? 232 : 188, hovered ? 255 : 255, 255);
    SDL_RenderFillRect(renderer, &game->quit_button);
    SDL_SetRenderDrawColor(renderer, 9, 18, 30, 255);
    SDL_RenderDrawRect(renderer, &game->quit_button);
    SDL_SetRenderDrawColor(renderer, 9, 18, 30, 255);
    draw_text(
        renderer,
        game->quit_button.x + game->quit_button.w / 2 - text_width("QUIT", button_scale) / 2,
        game->quit_button.y + game->quit_button.h / 2 - (7 * button_scale) / 2,
        "QUIT",
        button_scale
    );
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int main(void) {
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Space Invaders",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    GameState game = {0};
    reset_game(&game);
    update_window_title(window, &game);

    bool running = true;
    Uint64 last_counter = SDL_GetPerformanceCounter();
    int mouse_x = 0;
    int mouse_y = 0;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION) {
                mouse_x = event.motion.x;
                mouse_y = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                mouse_x = event.button.x;
                mouse_y = event.button.y;
                if ((game.game_over || game.victory) && point_in_rect(mouse_x, mouse_y, game.quit_button)) {
                    running = false;
                }
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                if ((game.game_over || game.victory) &&
                    (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER)) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (!game.game_over && !game.victory && game.respawn_timer <= 0.0f && game.player_shot_cooldown <= 0.0f) {
                        spawn_player_bullet(&game);
                    }
                }
            }
        }

        Uint64 current_counter = SDL_GetPerformanceCounter();
        float dt = (float)(current_counter - last_counter) / (float)SDL_GetPerformanceFrequency();
        last_counter = current_counter;

        if (dt > 0.05f) {
            dt = 0.05f;
        }

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        update_game(&game, dt, keys);
        update_window_title(window, &game);

        render_background(renderer);
        draw_help_overlay(renderer);
        draw_enemies(renderer, &game);
        draw_barriers(renderer, &game);
        draw_bullets(renderer, &game);
        draw_player(renderer, &game);
        if (game.game_over || game.victory) {
            game.end_dialog_shown = true;
            draw_end_overlay(renderer, &game, mouse_x, mouse_y);
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
