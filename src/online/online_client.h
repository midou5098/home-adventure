#ifndef ONLINE_CLIENT_H
#define ONLINE_CLIENT_H

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int connected;
    int is_host;
    int in_room;
    int game_started;
    int remote_connected;
    int kicked;
    int connection_lost;
    char join_code[8];
    char notice[96];
    char host_name[24];
    char client_name[24];
    int host_skin;
    int host_control;
    int client_skin;
    int client_control;
    int last_save_level;
    int last_save_score;
    int last_save_lives[2];
    char last_save_note[64];
    char end_reason[64];
} OnlineLobbyState;

typedef struct {
    int available;
    int width;
    int height;
    int pitch;
    int level_number;
    uint32_t sequence;
    const void* pixels;
} OnlineRemoteFrame;

enum {
    ONLINE_INPUT_A      = 1u << 0,
    ONLINE_INPUT_D      = 1u << 1,
    ONLINE_INPUT_W      = 1u << 2,
    ONLINE_INPUT_S      = 1u << 3,
    ONLINE_INPUT_SPACE  = 1u << 4,
    ONLINE_INPUT_F      = 1u << 5,
    ONLINE_INPUT_0      = 1u << 6
};

void online_client_reset(void);
void online_client_disconnect(void);

void online_client_default_server(char* out_host, size_t out_host_size, int* out_port);
void online_client_default_player_name(char* out_name, size_t out_name_size);

int online_client_connect_host(const char* server_host, int server_port, const char* player_name);
int online_client_connect_join(const char* server_host, int server_port, const char* join_code, const char* player_name);
void online_client_pump(void);

int online_client_is_connected(void);
int online_client_is_host(void);
void online_client_copy_lobby_state(OnlineLobbyState* out_state);

void online_client_set_profile(const char* player_name, int skin, int control);
void online_client_start_game(void);
void online_client_kick_client(void);
void online_client_send_pause_state(int paused);
int online_client_consume_pause_state_change(int* out_paused);

void online_client_send_input_mask(uint32_t input_mask);
uint32_t online_client_remote_input_mask(void);
int online_client_remote_scancode_down(SDL_Scancode scancode);

int online_client_begin_host_gameplay(void);
void online_client_end_host_gameplay(const char* reason);
int online_client_should_abort_host_gameplay(void);
void online_client_submit_frame(SDL_Renderer* renderer, int level_number);
void online_client_notify_save(int current_level, int score, int p1_lives, int p2_lives, const char* note);

int online_client_latest_frame(OnlineRemoteFrame* out_frame);

#endif /* ONLINE_CLIENT_H */
