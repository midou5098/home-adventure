#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GRID_WIDTH 10
#define GRID_HEIGHT 10
#define MINE_COUNT 15
#define TILE_SIZE 40
#define OUTER_PADDING 24
#define PANEL_GAP 18
#define HEADER_HEIGHT 96
#define BOARD_WIDTH (GRID_WIDTH * TILE_SIZE)
#define BOARD_HEIGHT (GRID_HEIGHT * TILE_SIZE)
#define WINDOW_WIDTH (BOARD_WIDTH + OUTER_PADDING * 2)
#define WINDOW_HEIGHT (HEADER_HEIGHT + BOARD_HEIGHT + OUTER_PADDING * 2 + PANEL_GAP)
#define HEADER_X OUTER_PADDING
#define HEADER_Y OUTER_PADDING
#define BOARD_X OUTER_PADDING
#define BOARD_Y (HEADER_Y + HEADER_HEIGHT + PANEL_GAP)

typedef struct {
    bool has_mine;
    bool revealed;
    bool flagged;
    uint8_t adjacent_mines;
} Cell;

typedef enum {
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST
} GameState;

typedef struct {
    Cell cells[GRID_HEIGHT][GRID_WIDTH];
    GameState state;
    bool first_click;
    int revealed_safe_cells;
    int flags_used;
} Game;

typedef struct {
    int x;
    int y;
} Cursor;

static const bool DIGIT_SEGMENTS[10][7] = {
    {true, true, true, true, true, true, false},
    {false, true, true, false, false, false, false},
    {true, true, false, true, true, false, true},
    {true, true, true, true, false, false, true},
    {false, true, true, false, false, true, true},
    {true, false, true, true, false, true, true},
    {true, false, true, true, true, true, true},
    {true, true, true, false, false, false, false},
    {true, true, true, true, true, true, true},
    {true, true, true, true, false, true, true},
};

static bool in_bounds(int x, int y) {
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

static void reset_game(Game *game) {
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            game->cells[y][x] = (Cell){0};
        }
    }

    game->state = GAME_PLAYING;
    game->first_click = true;
    game->revealed_safe_cells = 0;
    game->flags_used = 0;
}

static void place_mines(Game *game, int safe_x, int safe_y) {
    int placed = 0;

    while (placed < MINE_COUNT) {
        int x = rand() % GRID_WIDTH;
        int y = rand() % GRID_HEIGHT;

        if (game->cells[y][x].has_mine) {
            continue;
        }

        if (abs(x - safe_x) <= 1 && abs(y - safe_y) <= 1) {
            continue;
        }

        game->cells[y][x].has_mine = true;
        placed++;
    }

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (game->cells[y][x].has_mine) {
                continue;
            }

            uint8_t count = 0;
            for (int ny = y - 1; ny <= y + 1; ++ny) {
                for (int nx = x - 1; nx <= x + 1; ++nx) {
                    if ((nx != x || ny != y) && in_bounds(nx, ny) && game->cells[ny][nx].has_mine) {
                        count++;
                    }
                }
            }
            game->cells[y][x].adjacent_mines = count;
        }
    }
}

static void reveal_all_mines(Game *game) {
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (game->cells[y][x].has_mine) {
                game->cells[y][x].revealed = true;
            }
        }
    }
}

static void reveal_cell(Game *game, int start_x, int start_y) {
    if (!in_bounds(start_x, start_y)) {
        return;
    }

    Cell *start = &game->cells[start_y][start_x];
    if (start->flagged || start->revealed || game->state != GAME_PLAYING) {
        return;
    }

    if (game->first_click) {
        place_mines(game, start_x, start_y);
        game->first_click = false;
    }

    if (start->has_mine) {
        start->revealed = true;
        game->state = GAME_LOST;
        reveal_all_mines(game);
        return;
    }

    int queue_x[GRID_WIDTH * GRID_HEIGHT];
    int queue_y[GRID_WIDTH * GRID_HEIGHT];
    bool queued[GRID_HEIGHT][GRID_WIDTH] = {{false}};
    int head = 0;
    int tail = 0;

    queue_x[tail] = start_x;
    queue_y[tail] = start_y;
    queued[start_y][start_x] = true;
    tail++;

    while (head < tail) {
        int x = queue_x[head];
        int y = queue_y[head];
        head++;

        if (!in_bounds(x, y)) {
            continue;
        }

        Cell *cell = &game->cells[y][x];
        if (cell->revealed || cell->flagged || cell->has_mine) {
            continue;
        }

        cell->revealed = true;
        game->revealed_safe_cells++;

        if (cell->adjacent_mines != 0) {
            continue;
        }

        for (int ny = y - 1; ny <= y + 1; ++ny) {
            for (int nx = x - 1; nx <= x + 1; ++nx) {
                if (nx == x && ny == y) {
                    continue;
                }

                if (in_bounds(nx, ny) && !queued[ny][nx]) {
                    queue_x[tail] = nx;
                    queue_y[tail] = ny;
                    queued[ny][nx] = true;
                    tail++;
                }
            }
        }
    }

    if (game->revealed_safe_cells == GRID_WIDTH * GRID_HEIGHT - MINE_COUNT) {
        game->state = GAME_WON;
    }
}

static void toggle_flag(Game *game, int x, int y) {
    if (!in_bounds(x, y) || game->state != GAME_PLAYING) {
        return;
    }

    Cell *cell = &game->cells[y][x];
    if (cell->revealed) {
        return;
    }

    cell->flagged = !cell->flagged;
    game->flags_used += cell->flagged ? 1 : -1;
}

static void set_draw_color(SDL_Renderer *renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_Rect rect = {x, y, w, h};
    set_draw_color(renderer, color);
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_rect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_Rect rect = {x, y, w, h};
    set_draw_color(renderer, color);
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_segment_digit(SDL_Renderer *renderer, int digit, int x, int y, int scale, SDL_Color color) {
    if (digit < 0 || digit > 9) {
        return;
    }

    const int t = scale;
    const int l = scale * 3;
    SDL_Rect segments[7] = {
        {x + t, y, l, t},
        {x + t + l, y + t, t, l},
        {x + t + l, y + (2 * t) + l, t, l},
        {x + t, y + (2 * l) + (2 * t), l, t},
        {x, y + (2 * t) + l, t, l},
        {x, y + t, t, l},
        {x + t, y + t + l, l, t},
    };

    set_draw_color(renderer, color);
    for (int i = 0; i < 7; ++i) {
        if (DIGIT_SEGMENTS[digit][i]) {
            SDL_RenderFillRect(renderer, &segments[i]);
        }
    }
}

static void draw_number(SDL_Renderer *renderer, int value, int x, int y, int scale, SDL_Color color) {
    char buffer[16];
    SDL_snprintf(buffer, sizeof(buffer), "%d", value);

    int cursor = x;
    for (int i = 0; buffer[i] != '\0'; ++i) {
        if (buffer[i] == '-') {
            SDL_Rect segment = {cursor + scale, y + scale * 4, scale * 3, scale};
            set_draw_color(renderer, color);
            SDL_RenderFillRect(renderer, &segment);
            cursor += scale * 6;
            continue;
        }

        draw_segment_digit(renderer, buffer[i] - '0', cursor, y, scale, color);
        cursor += scale * 6;
    }
}

static SDL_Color adjacent_color(uint8_t count) {
    static const SDL_Color colors[9] = {
        {0, 0, 0, 255},
        {37, 99, 235, 255},
        {22, 163, 74, 255},
        {220, 38, 38, 255},
        {91, 33, 182, 255},
        {180, 83, 9, 255},
        {8, 145, 178, 255},
        {55, 65, 81, 255},
        {17, 24, 39, 255},
    };
    return colors[count];
}

static void draw_mine(SDL_Renderer *renderer, int x, int y) {
    fill_rect(renderer, x + 10, y + 10, 12, 12, (SDL_Color){30, 41, 59, 255});
    fill_rect(renderer, x + 14, y + 5, 4, 22, (SDL_Color){30, 41, 59, 255});
    fill_rect(renderer, x + 5, y + 14, 22, 4, (SDL_Color){30, 41, 59, 255});
}

static void draw_flag(SDL_Renderer *renderer, int x, int y) {
    fill_rect(renderer, x + 14, y + 8, 3, 18, (SDL_Color){55, 65, 81, 255});
    fill_rect(renderer, x + 10, y + 24, 11, 3, (SDL_Color){55, 65, 81, 255});
    fill_rect(renderer, x + 16, y + 8, 10, 8, (SDL_Color){220, 38, 38, 255});
}

static const uint8_t *glyph_rows(char c) {
    static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t colon[7] = {0, 4, 0, 0, 0, 4, 0};
    static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t glyph_d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t glyph_f[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
    static const uint8_t glyph_i[7] = {31, 4, 4, 4, 4, 4, 31};
    static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t glyph_l[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t glyph_m[7] = {17, 27, 21, 17, 17, 17, 17};
    static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t glyph_w[7] = {17, 17, 17, 17, 21, 27, 17};
    static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};

    switch (c) {
        case 'A': return glyph_a;
        case 'C': return glyph_c;
        case 'D': return glyph_d;
        case 'E': return glyph_e;
        case 'F': return glyph_f;
        case 'G': return glyph_g;
        case 'I': return glyph_i;
        case 'K': return glyph_k;
        case 'L': return glyph_l;
        case 'M': return glyph_m;
        case 'N': return glyph_n;
        case 'O': return glyph_o;
        case 'P': return glyph_p;
        case 'R': return glyph_r;
        case 'S': return glyph_s;
        case 'T': return glyph_t;
        case 'U': return glyph_u;
        case 'V': return glyph_v;
        case 'W': return glyph_w;
        case 'Y': return glyph_y;
        case '-': return dash;
        case ':': return colon;
        case ' ': return blank;
        default: return blank;
    }
}

static void draw_text(SDL_Renderer *renderer, const char *text, int x, int y, int scale, SDL_Color color) {
    set_draw_color(renderer, color);

    for (int i = 0; text[i] != '\0'; ++i) {
        const uint8_t *rows = glyph_rows(text[i]);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (1 << (4 - col))) == 0) {
                    continue;
                }

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

static int text_width(const char *text, int scale) {
    int length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length * scale * 6 - (length > 0 ? scale : 0);
}

static void draw_panel(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color bg, SDL_Color border) {
    fill_rect(renderer, x, y, w, h, bg);
    draw_rect(renderer, x, y, w, h, border);
    draw_rect(renderer, x + 1, y + 1, w - 2, h - 2, (SDL_Color){255, 255, 255, 60});
}

static void move_cursor(Cursor *cursor, int dx, int dy) {
    cursor->x += dx;
    cursor->y += dy;

    if (cursor->x < 0) {
        cursor->x = 0;
    } else if (cursor->x >= GRID_WIDTH) {
        cursor->x = GRID_WIDTH - 1;
    }

    if (cursor->y < 0) {
        cursor->y = 0;
    } else if (cursor->y >= GRID_HEIGHT) {
        cursor->y = GRID_HEIGHT - 1;
    }
}

static void set_cursor_from_mouse(Cursor *cursor, int mouse_x, int mouse_y) {
    if (mouse_x < BOARD_X || mouse_x >= BOARD_X + BOARD_WIDTH || mouse_y < BOARD_Y || mouse_y >= BOARD_Y + BOARD_HEIGHT) {
        return;
    }

    cursor->x = (mouse_x - BOARD_X) / TILE_SIZE;
    cursor->y = (mouse_y - BOARD_Y) / TILE_SIZE;
}

static bool game_over_button_clicked(int mouse_x, int mouse_y) {
    const int dialog_w = 240;
    const int dialog_h = 150;
    const int dialog_x = (WINDOW_WIDTH - dialog_w) / 2;
    const int dialog_y = (WINDOW_HEIGHT - dialog_h) / 2;
    const int button_w = 120;
    const int button_h = 34;
    const int button_x = dialog_x + (dialog_w - button_w) / 2;
    const int button_y = dialog_y + 88;

    return mouse_x >= button_x && mouse_x < button_x + button_w && mouse_y >= button_y && mouse_y < button_y + button_h;
}

static void render_game(SDL_Window *window, SDL_Renderer *renderer, const Game *game, const Cursor *cursor, bool show_rules, bool update_window_surface) {
    fill_rect(renderer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (SDL_Color){214, 223, 232, 255});
    draw_panel(renderer, HEADER_X, HEADER_Y, BOARD_WIDTH, HEADER_HEIGHT, (SDL_Color){193, 205, 218, 255}, (SDL_Color){107, 114, 128, 255});
    draw_panel(renderer, BOARD_X - 6, BOARD_Y - 6, BOARD_WIDTH + 12, BOARD_HEIGHT + 12, (SDL_Color){166, 180, 196, 255}, (SDL_Color){100, 116, 139, 255});

    draw_number(renderer, MINE_COUNT - game->flags_used, HEADER_X + 18, HEADER_Y + 20, 4, (SDL_Color){185, 28, 28, 255});
    draw_text(renderer, "ARROWS MOVE", HEADER_X + 118, HEADER_Y + 18, 2, (SDL_Color){15, 23, 42, 255});
    draw_text(renderer, "SPACE OPEN", HEADER_X + 118, HEADER_Y + 38, 2, (SDL_Color){15, 23, 42, 255});
    draw_text(renderer, "F FLAG  I RULES", HEADER_X + 132, HEADER_Y + 58, 2, (SDL_Color){15, 23, 42, 255});
    draw_number(renderer, game->revealed_safe_cells, HEADER_X + BOARD_WIDTH - 70, HEADER_Y + 20, 4, (SDL_Color){15, 23, 42, 255});

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            const Cell *cell = &game->cells[y][x];
            int screen_x = BOARD_X + x * TILE_SIZE;
            int screen_y = BOARD_Y + y * TILE_SIZE;

            SDL_Color tile_color = cell->revealed ? (SDL_Color){229, 235, 241, 255} : (SDL_Color){148, 163, 184, 255};
            if (cell->revealed && cell->has_mine && game->state == GAME_LOST) {
                tile_color = (SDL_Color){248, 113, 113, 255};
            }

            fill_rect(renderer, screen_x, screen_y, TILE_SIZE, TILE_SIZE, tile_color);
            draw_rect(renderer, screen_x, screen_y, TILE_SIZE, TILE_SIZE, (SDL_Color){100, 116, 139, 255});

            if (!cell->revealed) {
                fill_rect(renderer, screen_x + 4, screen_y + 4, TILE_SIZE - 8, TILE_SIZE - 8, (SDL_Color){190, 202, 216, 255});
                if (cell->flagged) {
                    draw_flag(renderer, screen_x + 4, screen_y + 4);
                }
            } else if (cell->has_mine) {
                draw_mine(renderer, screen_x + 4, screen_y + 4);
            } else if (cell->adjacent_mines > 0) {
                draw_number(renderer, cell->adjacent_mines, screen_x + 11, screen_y + 9, 2, adjacent_color(cell->adjacent_mines));
            }

            if (cursor->x == x && cursor->y == y) {
                fill_rect(renderer, screen_x + 3, screen_y + 3, TILE_SIZE - 6, TILE_SIZE - 6, (SDL_Color){239, 68, 68, 70});
                draw_rect(renderer, screen_x + 1, screen_y + 1, TILE_SIZE - 2, TILE_SIZE - 2, (SDL_Color){127, 29, 29, 255});
                draw_rect(renderer, screen_x + 2, screen_y + 2, TILE_SIZE - 4, TILE_SIZE - 4, (SDL_Color){248, 113, 113, 255});
            }
        }
    }

    if (show_rules) {
        fill_rect(renderer, BOARD_X + 20, BOARD_Y + 70, BOARD_WIDTH - 40, 230, (SDL_Color){226, 232, 240, 245});
        draw_rect(renderer, BOARD_X + 20, BOARD_Y + 70, BOARD_WIDTH - 40, 230, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "RULES", BOARD_X + 52, BOARD_Y + 92, 3, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "OPEN ALL SAFE CELLS", BOARD_X + 52, BOARD_Y + 132, 2, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "AVOID ALL MINES", BOARD_X + 52, BOARD_Y + 156, 2, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "NUMBERS SHOW", BOARD_X + 52, BOARD_Y + 192, 2, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "MINES IN NEAR CELLS", BOARD_X + 52, BOARD_Y + 216, 2, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, "PRESS I TO CLOSE", BOARD_X + 52, BOARD_Y + 252, 2, (SDL_Color){55, 65, 81, 255});
    }

    if (game->state != GAME_PLAYING) {
        const int dialog_w = 240;
        const int dialog_h = 150;
        const int dialog_x = (WINDOW_WIDTH - dialog_w) / 2;
        const int dialog_y = (WINDOW_HEIGHT - dialog_h) / 2;
        const int button_w = 120;
        const int button_h = 34;
        const int button_x = dialog_x + (dialog_w - button_w) / 2;
        const int button_y = dialog_y + 88;

        fill_rect(renderer, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, (SDL_Color){15, 23, 42, 90});
        fill_rect(renderer, dialog_x, dialog_y, dialog_w, dialog_h, (SDL_Color){241, 245, 249, 255});
        draw_rect(renderer, dialog_x, dialog_y, dialog_w, dialog_h, (SDL_Color){15, 23, 42, 255});
        draw_text(renderer, game->state == GAME_WON ? "YOU WIN" : "YOU LOST", dialog_x + 42, dialog_y + 28, 3, (SDL_Color){15, 23, 42, 255});
        fill_rect(renderer, button_x, button_y, button_w, button_h, (SDL_Color){220, 38, 38, 255});
        draw_rect(renderer, button_x, button_y, button_w, button_h, (SDL_Color){127, 29, 29, 255});
        draw_text(renderer, "QUIT", button_x + (button_w - text_width("QUIT", 2)) / 2, button_y + 10, 2, (SDL_Color){248, 250, 252, 255});
    }

    SDL_RenderPresent(renderer);
    if (update_window_surface) {
        SDL_UpdateWindowSurface(window);
    }
}

int main(void) {
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Minesweeper",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );
    if (window == NULL) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool update_window_surface = false;
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (renderer == NULL) {
        SDL_Surface *window_surface = SDL_GetWindowSurface(window);
        if (window_surface != NULL) {
            renderer = SDL_CreateSoftwareRenderer(window_surface);
            update_window_surface = renderer != NULL;
        }
    }
    if (renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Game game;
    Cursor cursor = {0, 0};
    bool show_rules = false;
    reset_game(&game);

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouse_x = event.button.x;
                int mouse_y = event.button.y;

                if (game.state != GAME_PLAYING) {
                    if (event.button.button == SDL_BUTTON_LEFT && game_over_button_clicked(mouse_x, mouse_y)) {
                        running = false;
                    }
                    continue;
                }

                if (mouse_x < BOARD_X || mouse_x >= BOARD_X + BOARD_WIDTH || mouse_y < BOARD_Y || mouse_y >= BOARD_Y + BOARD_HEIGHT) {
                    continue;
                }

                set_cursor_from_mouse(&cursor, mouse_x, mouse_y);

                if (event.button.button == SDL_BUTTON_LEFT) {
                    reveal_cell(&game, cursor.x, cursor.y);
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    toggle_flag(&game, cursor.x, cursor.y);
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat) {
                    continue;
                }

                SDL_Keycode key = event.key.keysym.sym;
                if (game.state != GAME_PLAYING) {
                    if (key == SDLK_RETURN || key == SDLK_SPACE || key == SDLK_q || key == SDLK_ESCAPE) {
                        running = false;
                    }
                } else if (key == SDLK_i) {
                    show_rules = !show_rules;
                } else if (key == SDLK_LEFT || key == SDLK_a || key == SDLK_h) {
                    move_cursor(&cursor, -1, 0);
                } else if (key == SDLK_RIGHT || key == SDLK_d || key == SDLK_l) {
                    move_cursor(&cursor, 1, 0);
                } else if (key == SDLK_UP || key == SDLK_w || key == SDLK_k) {
                    move_cursor(&cursor, 0, -1);
                } else if (key == SDLK_DOWN || key == SDLK_s || key == SDLK_j) {
                    move_cursor(&cursor, 0, 1);
                } else if (key == SDLK_RETURN || key == SDLK_SPACE) {
                    reveal_cell(&game, cursor.x, cursor.y);
                } else if (key == SDLK_f) {
                    toggle_flag(&game, cursor.x, cursor.y);
                }
            }
        }

        render_game(window, renderer, &game, &cursor, show_rules, update_window_surface);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
