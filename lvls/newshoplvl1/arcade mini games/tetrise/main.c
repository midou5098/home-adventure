#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_WIDTH 10
#define BOARD_HEIGHT 20
#define CELL_SIZE 30
#define SIDE_PANEL_WIDTH 180
#define WINDOW_WIDTH (BOARD_WIDTH * CELL_SIZE + SIDE_PANEL_WIDTH)
#define WINDOW_HEIGHT (BOARD_HEIGHT * CELL_SIZE)

#define FALL_DELAY_START 650
#define FALL_DELAY_MIN 120
#define LOCK_FLASH_MS 70
#define PIECE_LIMIT 15
#define MUSIC_PATH "tetrise bg.mp3"

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point blocks[4];
} Shape;

typedef struct {
    int type;
    int rotation;
    int x;
    int y;
} Piece;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Button;

static const Shape g_shapes[7][4] = {
    {
        {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}},
        {{{2, 0}, {2, 1}, {2, 2}, {2, 3}}},
        {{{0, 2}, {1, 2}, {2, 2}, {3, 2}}},
        {{{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    },
    {
        {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}},
        {{{1, 0}, {2, 0}, {1, 1}, {1, 2}}},
        {{{0, 1}, {1, 1}, {2, 1}, {2, 2}}},
        {{{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
    },
    {
        {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}},
        {{{1, 0}, {1, 1}, {1, 2}, {2, 2}}},
        {{{0, 1}, {1, 1}, {2, 1}, {0, 2}}},
        {{{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
    },
    {
        {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
        {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
        {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
        {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    },
    {
        {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}},
        {{{1, 0}, {1, 1}, {2, 1}, {2, 2}}},
        {{{1, 1}, {2, 1}, {0, 2}, {1, 2}}},
        {{{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
    },
    {
        {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}},
        {{{1, 0}, {1, 1}, {2, 1}, {1, 2}}},
        {{{0, 1}, {1, 1}, {2, 1}, {1, 2}}},
        {{{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
    },
    {
        {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}},
        {{{2, 0}, {1, 1}, {2, 1}, {1, 2}}},
        {{{0, 1}, {1, 1}, {1, 2}, {2, 2}}},
        {{{1, 0}, {0, 1}, {1, 1}, {0, 2}}},
    },
};

static const Color g_piece_colors[8] = {
    {16, 18, 24},
    {0, 240, 240},
    {0, 102, 204},
    {255, 140, 0},
    {240, 220, 70},
    {80, 220, 120},
    {170, 80, 220},
    {220, 70, 70},
};

static const char *g_piece_names[7] = {"I", "J", "L", "O", "S", "T", "Z"};

typedef struct {
    int cells[BOARD_HEIGHT][BOARD_WIDTH];
    Piece current;
    Piece next;
    uint32_t score;
    uint32_t lines;
    uint32_t level;
    uint32_t pieces_placed;
    bool running;
    bool paused;
    uint32_t last_fall_tick;
} GameState;

static const Button g_quit_button = {78, 338, 144, 34};

static Mix_Music *load_background_music(void) {
    Mix_Music *music = Mix_LoadMUS(MUSIC_PATH);
    if (music == NULL) {
        fprintf(stderr, "Mix_LoadMUS failed for %s: %s\n", MUSIC_PATH, Mix_GetError());
    }
    return music;
}

static const uint8_t *glyph_for_char(char c) {
    static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t colon[7] = {0, 4, 4, 0, 4, 4, 0};
    static const uint8_t slash[7] = {1, 1, 2, 4, 8, 16, 16};
    static const uint8_t zero[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t one[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t two[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t three[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t four[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t five[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t six[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t seven[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t eight[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t nine[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t g[7] = {14, 17, 16, 19, 17, 17, 14};
    static const uint8_t i[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t m[7] = {17, 27, 21, 17, 17, 17, 17};
    static const uint8_t n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t v[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t w[7] = {17, 17, 17, 17, 21, 21, 10};
    static const uint8_t x[7] = {17, 17, 10, 4, 10, 17, 17};

    switch (c) {
        case ' ': return space;
        case ':': return colon;
        case '/': return slash;
        case '0': return zero;
        case '1': return one;
        case '2': return two;
        case '3': return three;
        case '4': return four;
        case '5': return five;
        case '6': return six;
        case '7': return seven;
        case '8': return eight;
        case '9': return nine;
        case 'A': return a;
        case 'D': return d;
        case 'E': return e;
        case 'G': return g;
        case 'I': return i;
        case 'M': return m;
        case 'N': return n;
        case 'O': return o;
        case 'P': return p;
        case 'Q': return q;
        case 'R': return r;
        case 'S': return s;
        case 'T': return t;
        case 'U': return u;
        case 'V': return v;
        case 'W': return w;
        case 'X': return x;
        default: return space;
    }
}

static void draw_text(SDL_Renderer *renderer, int x, int y, int scale, Color color, const char *text) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int i = 0; text[i] != '\0'; ++i) {
        const uint8_t *glyph = glyph_for_char(text[i]);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((glyph[row] >> (4 - col)) & 1) {
                    SDL_Rect pixel = {
                        x + i * scale * 6 + col * scale,
                        y + row * scale,
                        scale,
                        scale
                    };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
    }
}

static bool point_in_button(int x, int y, Button button) {
    return x >= button.x && x < button.x + button.w &&
           y >= button.y && y < button.y + button.h;
}

static int random_piece_type(void) {
    return rand() % 7;
}

static Piece make_piece(int type) {
    Piece piece;
    piece.type = type;
    piece.rotation = 0;
    piece.x = 3;
    piece.y = 0;
    return piece;
}

static int fall_delay_for_level(uint32_t level) {
    int delay = FALL_DELAY_START - (int)(level * 45);
    if (delay < FALL_DELAY_MIN) {
        delay = FALL_DELAY_MIN;
    }
    return delay;
}

static bool piece_fits(const GameState *game, Piece piece) {
    const Shape *shape = &g_shapes[piece.type][piece.rotation];
    for (int i = 0; i < 4; ++i) {
        int x = piece.x + shape->blocks[i].x;
        int y = piece.y + shape->blocks[i].y;
        if (x < 0 || x >= BOARD_WIDTH || y >= BOARD_HEIGHT) {
            return false;
        }
        if (y >= 0 && game->cells[y][x] != 0) {
            return false;
        }
    }
    return true;
}

static bool try_move_piece(GameState *game, int dx, int dy) {
    Piece moved = game->current;
    moved.x += dx;
    moved.y += dy;
    if (!piece_fits(game, moved)) {
        return false;
    }
    game->current = moved;
    return true;
}

static bool try_rotate_piece(GameState *game, int delta) {
    static const int kicks[][2] = {
        {0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}
    };

    Piece rotated = game->current;
    rotated.rotation = (rotated.rotation + delta + 4) % 4;

    for (size_t i = 0; i < sizeof(kicks) / sizeof(kicks[0]); ++i) {
        Piece candidate = rotated;
        candidate.x += kicks[i][0];
        candidate.y += kicks[i][1];
        if (piece_fits(game, candidate)) {
            game->current = candidate;
            return true;
        }
    }

    return false;
}

static int ghost_drop_distance(const GameState *game) {
    Piece ghost = game->current;
    int distance = 0;
    while (piece_fits(game, ghost)) {
        ghost.y += 1;
        distance += 1;
    }
    return distance - 1;
}

static void lock_piece(GameState *game) {
    const Shape *shape = &g_shapes[game->current.type][game->current.rotation];
    for (int i = 0; i < 4; ++i) {
        int x = game->current.x + shape->blocks[i].x;
        int y = game->current.y + shape->blocks[i].y;
        if (y >= 0 && y < BOARD_HEIGHT && x >= 0 && x < BOARD_WIDTH) {
            game->cells[y][x] = game->current.type + 1;
        }
    }
}

static int clear_lines(GameState *game) {
    int cleared = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            if (game->cells[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (!full) {
            continue;
        }

        cleared += 1;
        for (int row = y; row > 0; --row) {
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                game->cells[row][x] = game->cells[row - 1][x];
            }
        }
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            game->cells[0][x] = 0;
        }
        y += 1;
    }
    return cleared;
}

static void update_score(GameState *game, int cleared) {
    static const int rewards[] = {0, 100, 300, 500, 800};
    game->lines += (uint32_t)cleared;
    game->score += (uint32_t)rewards[cleared] * (game->level + 1);
    game->level = game->lines / 10;
}

static void update_window_title(SDL_Window *window, const GameState *game) {
    char title[256];
    if (game->running) {
        snprintf(title, sizeof(title),
                 "Tetris C/SDL2  Score:%u  Lines:%u  Level:%u  Pieces:%u/%d  Next:%s%s",
                 game->score,
                 game->lines,
                 game->level,
                 game->pieces_placed,
                 PIECE_LIMIT,
                 g_piece_names[game->next.type],
                 game->paused ? "  [PAUSED]" : "");
    } else {
        snprintf(title, sizeof(title),
                 "Tetris C/SDL2  Game Over  Score:%u  Pieces:%u/%d  Click QUIT to exit",
                 game->score,
                 game->pieces_placed,
                 PIECE_LIMIT);
    }
    SDL_SetWindowTitle(window, title);
}

static void spawn_next_piece(GameState *game) {
    game->current = game->next;
    game->current.x = 3;
    game->current.y = 0;
    game->current.rotation = 0;
    game->next = make_piece(random_piece_type());
    if (!piece_fits(game, game->current)) {
        game->running = false;
    }
}

static void reset_game(GameState *game, SDL_Window *window) {
    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            game->cells[y][x] = 0;
        }
    }
    game->score = 0;
    game->lines = 0;
    game->level = 0;
    game->pieces_placed = 0;
    game->running = true;
    game->paused = false;
    game->current = make_piece(random_piece_type());
    game->next = make_piece(random_piece_type());
    game->last_fall_tick = SDL_GetTicks();
    if (!piece_fits(game, game->current)) {
        game->running = false;
    }
    update_window_title(window, game);
}

static void settle_piece(GameState *game, SDL_Window *window) {
    lock_piece(game);
    game->pieces_placed += 1;
    int cleared = clear_lines(game);
    if (cleared > 0) {
        update_score(game, cleared);
    }
    if (game->pieces_placed >= PIECE_LIMIT) {
        game->running = false;
    } else {
        spawn_next_piece(game);
    }
    game->last_fall_tick = SDL_GetTicks();
    update_window_title(window, game);
}

static void hard_drop(GameState *game, SDL_Window *window) {
    int distance = ghost_drop_distance(game);
    if (distance > 0) {
        game->score += (uint32_t)(distance * 2);
        game->current.y += distance;
    }
    settle_piece(game, window);
}

static void draw_rect(SDL_Renderer *renderer, int x, int y, int w, int h, Color color) {
    SDL_Rect rect = {x, y, w, h};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_block(SDL_Renderer *renderer, int board_x, int board_y, Color color, bool ghost) {
    SDL_Rect rect = {
        board_x * CELL_SIZE + 1,
        board_y * CELL_SIZE + 1,
        CELL_SIZE - 2,
        CELL_SIZE - 2
    };

    if (ghost) {
        SDL_SetRenderDrawColor(renderer, color.r / 2, color.g / 2, color.b / 2, 255);
        SDL_RenderDrawRect(renderer, &rect);
        return;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer,
                           (uint8_t)(color.r > 40 ? color.r - 40 : 0),
                           (uint8_t)(color.g > 40 ? color.g - 40 : 0),
                           (uint8_t)(color.b > 40 ? color.b - 40 : 0),
                           255);
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_board(SDL_Renderer *renderer, const GameState *game) {
    draw_rect(renderer, 0, 0, BOARD_WIDTH * CELL_SIZE, WINDOW_HEIGHT, (Color){24, 27, 35});

    SDL_SetRenderDrawColor(renderer, 38, 42, 52, 255);
    for (int x = 0; x <= BOARD_WIDTH; ++x) {
        SDL_RenderDrawLine(renderer, x * CELL_SIZE, 0, x * CELL_SIZE, WINDOW_HEIGHT);
    }
    for (int y = 0; y <= BOARD_HEIGHT; ++y) {
        SDL_RenderDrawLine(renderer, 0, y * CELL_SIZE, BOARD_WIDTH * CELL_SIZE, y * CELL_SIZE);
    }

    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            int cell = game->cells[y][x];
            if (cell != 0) {
                draw_block(renderer, x, y, g_piece_colors[cell], false);
            }
        }
    }

    if (game->running) {
        int distance = ghost_drop_distance(game);
        const Shape *shape = &g_shapes[game->current.type][game->current.rotation];
        for (int i = 0; i < 4; ++i) {
            int x = game->current.x + shape->blocks[i].x;
            int y = game->current.y + shape->blocks[i].y + distance;
            if (y >= 0) {
                draw_block(renderer, x, y, g_piece_colors[game->current.type + 1], true);
            }
        }
        for (int i = 0; i < 4; ++i) {
            int x = game->current.x + shape->blocks[i].x;
            int y = game->current.y + shape->blocks[i].y;
            if (y >= 0) {
                draw_block(renderer, x, y, g_piece_colors[game->current.type + 1], false);
            }
        }
    }
}

static void draw_preview(SDL_Renderer *renderer, const Piece *piece, int origin_x, int origin_y) {
    const Shape *shape = &g_shapes[piece->type][0];
    Color color = g_piece_colors[piece->type + 1];
    for (int i = 0; i < 4; ++i) {
        SDL_Rect rect = {
            origin_x + shape->blocks[i].x * 24,
            origin_y + shape->blocks[i].y * 24,
            20,
            20
        };
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
        SDL_RenderFillRect(renderer, &rect);
    }
}

static void draw_button(SDL_Renderer *renderer, Button button, Color fill, Color outline, const char *label) {
    SDL_Rect rect = {button.x, button.y, button.w, button.h};
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, outline.r, outline.g, outline.b, 255);
    SDL_RenderDrawRect(renderer, &rect);
    draw_text(renderer, button.x + 40, button.y + 11, 2, (Color){235, 235, 235}, label);
}

static void draw_side_panel(SDL_Renderer *renderer, const GameState *game) {
    int panel_x = BOARD_WIDTH * CELL_SIZE;
    draw_rect(renderer, panel_x, 0, SIDE_PANEL_WIDTH, WINDOW_HEIGHT, (Color){18, 20, 28});

    SDL_Rect border = {panel_x + 12, 12, SIDE_PANEL_WIDTH - 24, WINDOW_HEIGHT - 24};
    SDL_SetRenderDrawColor(renderer, 70, 76, 92, 255);
    SDL_RenderDrawRect(renderer, &border);

    SDL_Rect preview_box = {panel_x + 28, 50, SIDE_PANEL_WIDTH - 56, 110};
    SDL_SetRenderDrawColor(renderer, 52, 57, 70, 255);
    SDL_RenderDrawRect(renderer, &preview_box);
    draw_text(renderer, panel_x + 43, 28, 2, (Color){220, 220, 220}, "NEXT");
    draw_preview(renderer, &game->next, panel_x + 42, 74);

    int meter_x = panel_x + 32;
    int meter_y = 210;
    int meter_w = SIDE_PANEL_WIDTH - 64;
    int level_fill = (int)((game->lines % 10) * meter_w / 10);

    SDL_Rect meter_bg = {meter_x, meter_y, meter_w, 20};
    SDL_SetRenderDrawColor(renderer, 44, 49, 62, 255);
    SDL_RenderFillRect(renderer, &meter_bg);
    SDL_SetRenderDrawColor(renderer, 0, 180, 180, 255);
    SDL_Rect meter_fill = {meter_x, meter_y, level_fill, 20};
    SDL_RenderFillRect(renderer, &meter_fill);

    SDL_SetRenderDrawColor(renderer, 90, 95, 110, 255);
    SDL_RenderDrawRect(renderer, &meter_bg);
    draw_text(renderer, panel_x + 36, 188, 2, (Color){220, 220, 220}, "LEVEL");

    SDL_Rect info_boxes[3] = {
        {panel_x + 28, 260, SIDE_PANEL_WIDTH - 56, 52},
        {panel_x + 28, 326, SIDE_PANEL_WIDTH - 56, 52},
        {panel_x + 28, 392, SIDE_PANEL_WIDTH - 56, 52},
    };
    for (int i = 0; i < 3; ++i) {
        SDL_SetRenderDrawColor(renderer, 52, 57, 70, 255);
        SDL_RenderDrawRect(renderer, &info_boxes[i]);
    }

    int score_height = (int)((game->score % 1000) * 52 / 1000);
    int lines_height = (int)((game->lines % 10) * 52 / 10);
    int level_height = (int)(((game->level % 10) + 1) * 52 / 10);

    draw_rect(renderer, info_boxes[0].x + 2, info_boxes[0].y + info_boxes[0].h - score_height - 2,
              info_boxes[0].w - 4, score_height, (Color){220, 110, 60});
    draw_rect(renderer, info_boxes[1].x + 2, info_boxes[1].y + info_boxes[1].h - lines_height - 2,
              info_boxes[1].w - 4, lines_height, (Color){90, 180, 110});
    draw_rect(renderer, info_boxes[2].x + 2, info_boxes[2].y + info_boxes[2].h - level_height - 2,
              info_boxes[2].w - 4, level_height, (Color){120, 120, 240});

    draw_text(renderer, panel_x + 38, 242, 2, (Color){230, 230, 230}, "SCORE");
    draw_text(renderer, panel_x + 42, 308, 2, (Color){230, 230, 230}, "LINES");
    draw_text(renderer, panel_x + 37, 374, 2, (Color){230, 230, 230}, "PIECES");

    char pieces_text[16];
    snprintf(pieces_text, sizeof(pieces_text), "%u/%d", game->pieces_placed, PIECE_LIMIT);
    draw_text(renderer, panel_x + 52, 460, 2, (Color){230, 230, 230}, pieces_text);

    draw_text(renderer, panel_x + 24, 500, 2, (Color){220, 220, 220}, "HOW TO PLAY");
    draw_text(renderer, panel_x + 24, 526, 1, (Color){200, 200, 200}, "A/D MOVE");
    draw_text(renderer, panel_x + 24, 540, 1, (Color){200, 200, 200}, "S DOWN");
    draw_text(renderer, panel_x + 24, 554, 1, (Color){200, 200, 200}, "UP/X ROT");
    draw_text(renderer, panel_x + 24, 568, 1, (Color){200, 200, 200}, "SPACE DROP");
    draw_text(renderer, panel_x + 24, 582, 1, (Color){200, 200, 200}, "P PAUSE");

    if (game->paused || !game->running) {
        SDL_Rect overlay = {28, WINDOW_HEIGHT / 2 - 60, BOARD_WIDTH * CELL_SIZE - 56, 120};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 8, 10, 14, 180);
        SDL_RenderFillRect(renderer, &overlay);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDrawRect(renderer, &overlay);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (game->paused) {
            draw_text(renderer, 98, WINDOW_HEIGHT / 2 - 18, 3, (Color){240, 240, 240}, "PAUSED");
        } else {
            draw_text(renderer, 73, WINDOW_HEIGHT / 2 - 42, 3, (Color){240, 240, 240}, "GAME OVER");
            draw_button(renderer, g_quit_button, (Color){110, 42, 42}, (Color){220, 120, 120}, "QUIT");
        }
    }
}

static void handle_keydown(SDL_Keycode key, GameState *game, SDL_Window *window) {
    if (!game->running && (key == SDLK_0 || key == SDLK_KP_0 || key == SDLK_f)) {
        SDL_Event quit_event;
        quit_event.type = SDL_QUIT;
        SDL_PushEvent(&quit_event);
        return;
    }
    if (key == SDLK_p && game->running) {
        game->paused = !game->paused;
        game->last_fall_tick = SDL_GetTicks();
        if (Mix_PlayingMusic() != 0) {
            if (game->paused) {
                Mix_PauseMusic();
            } else {
                Mix_ResumeMusic();
            }
        }
        update_window_title(window, game);
        return;
    }
    if (!game->running || game->paused) {
        return;
    }

    bool moved = false;
    switch (key) {
        case SDLK_LEFT:
        case SDLK_a:
            moved = try_move_piece(game, -1, 0);
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            moved = try_move_piece(game, 1, 0);
            break;
        case SDLK_DOWN:
        case SDLK_s:
            if (try_move_piece(game, 0, 1)) {
                game->score += 1;
                moved = true;
            } else {
                settle_piece(game, window);
                return;
            }
            break;
        case SDLK_UP:
        case SDLK_x:
            moved = try_rotate_piece(game, 1);
            break;
        case SDLK_z:
            moved = try_rotate_piece(game, -1);
            break;
        case SDLK_SPACE:
            hard_drop(game, window);
            return;
        default:
            return;
    }

    if (moved) {
        update_window_title(window, game);
    }
}

static void handle_mouse_button(SDL_MouseButtonEvent event, bool *quit, const GameState *game) {
    if (event.button != SDL_BUTTON_LEFT || game->running) {
        return;
    }
    if (point_in_button(event.x, event.y, g_quit_button)) {
        *quit = true;
    }
}

int main(void) {
    srand((unsigned int)time(NULL));
    Mix_Music *background_music = NULL;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    if ((Mix_Init(MIX_INIT_MP3) & MIX_INIT_MP3) != 0) {
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == 0) {
            background_music = load_background_music();
            if (background_music != NULL) {
                Mix_VolumeMusic(MIX_MAX_VOLUME / 3);
                if (Mix_PlayMusic(background_music, -1) != 0) {
                    fprintf(stderr, "Mix_PlayMusic failed: %s\n", Mix_GetError());
                    Mix_FreeMusic(background_music);
                    background_music = NULL;
                }
            }
        } else {
            fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        }
    } else {
        fprintf(stderr, "Mix_Init failed to enable MP3 support: %s\n", Mix_GetError());
    }

    SDL_Window *window = SDL_CreateWindow(
        "Tetris C/SDL2",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        if (background_music != NULL) {
            Mix_FreeMusic(background_music);
        }
        if (Mix_QuerySpec(NULL, NULL, NULL) != 0) {
            Mix_CloseAudio();
        }
        Mix_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        if (background_music != NULL) {
            Mix_FreeMusic(background_music);
        }
        if (Mix_QuerySpec(NULL, NULL, NULL) != 0) {
            Mix_CloseAudio();
        }
        Mix_Quit();
        SDL_Quit();
        return EXIT_FAILURE;
    }

    GameState game = {0};
    reset_game(&game, window);

    bool quit = false;
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                handle_keydown(event.key.keysym.sym, &game, window);
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                handle_mouse_button(event.button, &quit, &game);
            }
        }

        if (game.running && !game.paused) {
            uint32_t now = SDL_GetTicks();
            if (now - game.last_fall_tick >= (uint32_t)fall_delay_for_level(game.level)) {
                if (!try_move_piece(&game, 0, 1)) {
                    SDL_Delay(LOCK_FLASH_MS);
                    settle_piece(&game, window);
                } else {
                    game.last_fall_tick = now;
                }
                update_window_title(window, &game);
            }
        }

        SDL_SetRenderDrawColor(renderer, 10, 12, 18, 255);
        SDL_RenderClear(renderer);
        draw_board(renderer, &game);
        draw_side_panel(renderer, &game);
        SDL_RenderPresent(renderer);
    }

    Mix_HaltMusic();
    if (background_music != NULL) {
        Mix_FreeMusic(background_music);
    }
    if (Mix_QuerySpec(NULL, NULL, NULL) != 0) {
        Mix_CloseAudio();
    }
    Mix_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
