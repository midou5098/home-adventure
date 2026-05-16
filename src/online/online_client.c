#include "online_client.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <zlib.h>

#define ONLINE_CAPTURE_W 1280
#define ONLINE_CAPTURE_H 720
#define ONLINE_MAX_TEXT_PAYLOAD 1024
#define ONLINE_MAX_SEND_QUEUE (8u * 1024u * 1024u)
#define ONLINE_REMOTE_WINDOW_ID 0xFFFFFFFFu
#define ONLINE_FRAME_INTERVAL_MS 40u
#define ONLINE_FRAME_BACKLOG_LIMIT (512u * 1024u)
#define ONLINE_INPUT_RESEND_INTERVAL_MS 100u
#define ONLINE_RAW_FRAME_STREAMING 1

enum {
    ONLINE_PACKET_HELLO = 1,
    ONLINE_PACKET_WELCOME = 2,
    ONLINE_PACKET_LOBBY = 3,
    ONLINE_PACKET_PROFILE = 4,
    ONLINE_PACKET_START = 5,
    ONLINE_PACKET_KICK = 6,
    ONLINE_PACKET_INFO = 7,
    ONLINE_PACKET_INPUT = 8,
    ONLINE_PACKET_FRAME = 9,
    ONLINE_PACKET_SAVE = 10,
    ONLINE_PACKET_END = 11,
    ONLINE_PACKET_ERROR = 12,
    ONLINE_PACKET_PAUSE = 13
};

typedef struct {
    int socket_fd;
    int connected;
    int is_host;
    int in_room;
    int host_gameplay_active;
    int abort_host_gameplay;
    char local_name[24];
    int local_skin;
    int local_control;
    int session_pause_active;
    int pause_state_dirty;
    int pending_pause_state;
    uint32_t last_sent_input_mask;
    Uint32 last_input_resend_ticks;
    uint32_t remote_input_mask;
    uint32_t previous_remote_input_mask;
    uint8_t* incoming;
    size_t incoming_len;
    size_t incoming_cap;
    uint8_t* outgoing;
    size_t outgoing_len;
    size_t outgoing_cap;
    uint8_t* frame_pixels;
    size_t frame_pixels_cap;
    int frame_width;
    int frame_height;
    int frame_pitch;
    int frame_level;
    uint32_t frame_sequence;
    uint8_t* capture_full;
    size_t capture_full_cap;
    uint8_t* capture_scaled;
    size_t capture_scaled_cap;
    uint8_t* compress_buffer;
    size_t compress_cap;
    uint32_t next_frame_sequence;
    Uint32 last_frame_submit_ticks;
    OnlineLobbyState lobby;
} OnlineClientState;

static OnlineClientState g_online_client;

static void online_copy_text(char* dest, size_t dest_size, const char* src)
{
    size_t i = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    while (src[i] != '\0' && i + 1 < dest_size) {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = '\0';
}

static void online_unsanitize_display_text(char* text)
{
    if (!text) return;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] == '_') text[i] = ' ';
    }
}

static void online_set_notice(const char* text)
{
    online_copy_text(g_online_client.lobby.notice, sizeof(g_online_client.lobby.notice), text ? text : "");
    online_unsanitize_display_text(g_online_client.lobby.notice);
}

static void online_reset_lobby_defaults(void)
{
    memset(&g_online_client.lobby, 0, sizeof(g_online_client.lobby));
    online_copy_text(g_online_client.lobby.host_name, sizeof(g_online_client.lobby.host_name), "HOST");
    online_copy_text(g_online_client.lobby.client_name, sizeof(g_online_client.lobby.client_name), "WAITING");
    g_online_client.lobby.host_skin = 1;
    g_online_client.lobby.host_control = 2;
    g_online_client.lobby.client_skin = 2;
    g_online_client.lobby.client_control = 1;
    online_set_notice("Connect_to_a_server");
}

static void online_reset_connection_state(void)
{
    if (g_online_client.socket_fd >= 0) {
        close(g_online_client.socket_fd);
    }

    g_online_client.socket_fd = -1;
    g_online_client.connected = 0;
    g_online_client.is_host = 0;
    g_online_client.in_room = 0;
    g_online_client.host_gameplay_active = 0;
    g_online_client.abort_host_gameplay = 0;
    g_online_client.last_sent_input_mask = 0;
    g_online_client.last_input_resend_ticks = 0;
    g_online_client.remote_input_mask = 0;
    g_online_client.previous_remote_input_mask = 0;
    g_online_client.session_pause_active = 0;
    g_online_client.pause_state_dirty = 0;
    g_online_client.pending_pause_state = 0;
    g_online_client.incoming_len = 0;
    g_online_client.outgoing_len = 0;
    g_online_client.frame_width = 0;
    g_online_client.frame_height = 0;
    g_online_client.frame_pitch = 0;
    g_online_client.frame_level = 0;
    g_online_client.frame_sequence = 0;
    g_online_client.next_frame_sequence = 1;
    g_online_client.last_frame_submit_ticks = 0;
    online_reset_lobby_defaults();
}

static int online_ensure_capacity(uint8_t** buffer, size_t* current_cap, size_t needed)
{
    size_t next_cap = 0;
    uint8_t* resized = NULL;

    if (!buffer || !current_cap) return 0;
    if (needed <= *current_cap) return 1;

    next_cap = (*current_cap == 0) ? 4096u : *current_cap;
    while (next_cap < needed) {
        if (next_cap > ((size_t)-1) / 2u) {
            next_cap = needed;
            break;
        }
        next_cap *= 2u;
    }

    resized = (uint8_t*)realloc(*buffer, next_cap);
    if (!resized) return 0;

    *buffer = resized;
    *current_cap = next_cap;
    return 1;
}

static void online_trim_send_queue(size_t consumed)
{
    if (consumed == 0 || consumed > g_online_client.outgoing_len) return;
    if (consumed == g_online_client.outgoing_len) {
        g_online_client.outgoing_len = 0;
        return;
    }

    memmove(g_online_client.outgoing,
            g_online_client.outgoing + consumed,
            g_online_client.outgoing_len - consumed);
    g_online_client.outgoing_len -= consumed;
}

static void online_drop_queued_frame_packets(void)
{
    size_t read_offset = 0;
    size_t write_offset = 0;

    while (read_offset + 8u <= g_online_client.outgoing_len) {
        uint32_t type_be = 0;
        uint32_t len_be = 0;
        uint32_t packet_type = 0;
        uint32_t payload_len = 0;
        size_t packet_size = 0;

        memcpy(&type_be, g_online_client.outgoing + read_offset, 4);
        memcpy(&len_be, g_online_client.outgoing + read_offset + 4u, 4);
        packet_type = ntohl(type_be);
        payload_len = ntohl(len_be);
        packet_size = 8u + (size_t)payload_len;

        if (payload_len > ONLINE_MAX_SEND_QUEUE ||
            packet_size > g_online_client.outgoing_len - read_offset) {
            return;
        }

        if (packet_type != ONLINE_PACKET_FRAME) {
            if (write_offset != read_offset) {
                memmove(g_online_client.outgoing + write_offset,
                        g_online_client.outgoing + read_offset,
                        packet_size);
            }
            write_offset += packet_size;
        }

        read_offset += packet_size;
    }

    if (read_offset == g_online_client.outgoing_len) {
        g_online_client.outgoing_len = write_offset;
    }
}

static int online_append_packet(uint32_t packet_type, const void* payload, uint32_t payload_len)
{
    uint32_t type_be = htonl(packet_type);
    uint32_t len_be = htonl(payload_len);
    size_t needed = 0;

    if (payload_len > ONLINE_MAX_SEND_QUEUE) return 0;
    needed = g_online_client.outgoing_len + 8u + (size_t)payload_len;
    if (needed > ONLINE_MAX_SEND_QUEUE && packet_type == ONLINE_PACKET_FRAME) {
        return 0;
    }
    if (!online_ensure_capacity(&g_online_client.outgoing, &g_online_client.outgoing_cap, needed)) {
        return 0;
    }

    memcpy(g_online_client.outgoing + g_online_client.outgoing_len, &type_be, sizeof(type_be));
    g_online_client.outgoing_len += sizeof(type_be);
    memcpy(g_online_client.outgoing + g_online_client.outgoing_len, &len_be, sizeof(len_be));
    g_online_client.outgoing_len += sizeof(len_be);
    if (payload_len > 0 && payload) {
        memcpy(g_online_client.outgoing + g_online_client.outgoing_len, payload, payload_len);
        g_online_client.outgoing_len += payload_len;
    }
    return 1;
}

static void online_sanitize_token(char* dest, size_t dest_size, const char* src, int uppercase_only)
{
    size_t out_len = 0;

    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src) return;

    for (size_t i = 0; src[i] != '\0' && out_len + 1 < dest_size; ++i) {
        unsigned char c = (unsigned char)src[i];

        if (!(isalnum(c) || c == '_' || c == '-')) continue;
        if (uppercase_only) c = (unsigned char)toupper(c);
        dest[out_len++] = (char)c;
    }

    dest[out_len] = '\0';
}

static int online_text_value(const char* text, const char* key, char* out, size_t out_size)
{
    const char* cursor = NULL;
    size_t key_len = 0;

    if (!text || !key || !out || out_size == 0) return 0;
    out[0] = '\0';
    key_len = strlen(key);
    cursor = text;

    while (*cursor != '\0') {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;

        if (strncmp(cursor, key, key_len) == 0 && cursor[key_len] == '=') {
            const char* value = cursor + key_len + 1;
            size_t len = 0;
            while (value[len] != '\0' && value[len] != ' ') ++len;
            if (len >= out_size) len = out_size - 1;
            memcpy(out, value, len);
            out[len] = '\0';
            return 1;
        }

        while (*cursor != '\0' && *cursor != ' ') ++cursor;
    }

    return 0;
}

static int online_text_value_int(const char* text, const char* key, int fallback)
{
    char value[64];
    if (!online_text_value(text, key, value, sizeof(value))) return fallback;
    return atoi(value);
}

static void online_push_key_event(SDL_Keycode keycode, SDL_Scancode scancode, int down)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.type = event.type;
    event.key.windowID = ONLINE_REMOTE_WINDOW_ID;
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = 0;
    event.key.keysym.sym = keycode;
    event.key.keysym.scancode = scancode;
    event.key.keysym.mod = KMOD_NONE;
    SDL_PushEvent(&event);
}

static void online_apply_remote_input_mask(uint32_t next_mask)
{
    static const struct {
        uint32_t bit;
        SDL_Keycode keycode;
        SDL_Scancode scancode;
    } key_map[] = {
        {ONLINE_INPUT_A, SDLK_a, SDL_SCANCODE_A},
        {ONLINE_INPUT_D, SDLK_d, SDL_SCANCODE_D},
        {ONLINE_INPUT_W, SDLK_w, SDL_SCANCODE_W},
        {ONLINE_INPUT_S, SDLK_s, SDL_SCANCODE_S},
        {ONLINE_INPUT_SPACE, SDLK_SPACE, SDL_SCANCODE_SPACE},
        {ONLINE_INPUT_F, SDLK_f, SDL_SCANCODE_F},
        {ONLINE_INPUT_0, SDLK_0, SDL_SCANCODE_0}
    };

    for (size_t i = 0; i < sizeof(key_map) / sizeof(key_map[0]); ++i) {
        int was_down = (g_online_client.previous_remote_input_mask & key_map[i].bit) != 0;
        int is_down = (next_mask & key_map[i].bit) != 0;
        if (was_down == is_down) continue;
        online_push_key_event(key_map[i].keycode, key_map[i].scancode, is_down);
    }

    g_online_client.previous_remote_input_mask = next_mask;
    g_online_client.remote_input_mask = next_mask;
}

static void online_handle_disconnect(const char* reason)
{
    int was_connected = g_online_client.connected;
    int remote_was_connected = g_online_client.lobby.remote_connected;

    if (!was_connected && g_online_client.socket_fd < 0) return;

    if (g_online_client.socket_fd >= 0) {
        close(g_online_client.socket_fd);
    }
    g_online_client.socket_fd = -1;
    g_online_client.connected = 0;
    g_online_client.in_room = 0;
    g_online_client.outgoing_len = 0;
    g_online_client.incoming_len = 0;
    g_online_client.lobby.connected = 0;
    g_online_client.lobby.game_started = 0;
    g_online_client.lobby.connection_lost = 1;
    online_set_notice(reason ? reason : "Connection_lost");

    if (g_online_client.host_gameplay_active && remote_was_connected) {
        g_online_client.abort_host_gameplay = 1;
    }
}

static void online_handle_text_packet(uint32_t packet_type, const uint8_t* payload, size_t payload_len)
{
    char text[ONLINE_MAX_TEXT_PAYLOAD];
    char value[128];
    size_t copy_len = payload_len;

    if (copy_len >= sizeof(text)) copy_len = sizeof(text) - 1;
    memcpy(text, payload, copy_len);
    text[copy_len] = '\0';

    if (packet_type == ONLINE_PACKET_WELCOME) {
        if (online_text_value(text, "code", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.join_code, sizeof(g_online_client.lobby.join_code), value);
        }
        g_online_client.lobby.connected = 1;
        g_online_client.in_room = 1;
        online_set_notice("Connected");
        return;
    }

    if (packet_type == ONLINE_PACKET_LOBBY) {
        if (online_text_value(text, "code", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.join_code, sizeof(g_online_client.lobby.join_code), value);
        }
        if (online_text_value(text, "host_name", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.host_name, sizeof(g_online_client.lobby.host_name), value);
        }
        if (online_text_value(text, "client_name", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.client_name, sizeof(g_online_client.lobby.client_name), value);
        }
        g_online_client.lobby.host_skin = online_text_value_int(text, "host_skin", g_online_client.lobby.host_skin);
        g_online_client.lobby.host_control = online_text_value_int(text, "host_control", g_online_client.lobby.host_control);
        g_online_client.lobby.client_skin = online_text_value_int(text, "client_skin", g_online_client.lobby.client_skin);
        g_online_client.lobby.client_control = online_text_value_int(text, "client_control", g_online_client.lobby.client_control);
        g_online_client.local_skin = g_online_client.is_host
                                         ? g_online_client.lobby.host_skin
                                         : g_online_client.lobby.client_skin;
        g_online_client.lobby.remote_connected = online_text_value_int(text, "client_connected", 0);
        g_online_client.lobby.game_started = online_text_value_int(text, "game_started", 0);
        if (online_text_value(text, "notice", value, sizeof(value))) {
            online_set_notice(value);
        }
        g_online_client.in_room = 1;
        g_online_client.lobby.connected = 1;
        return;
    }

    if (packet_type == ONLINE_PACKET_INFO) {
        if (online_text_value(text, "message", value, sizeof(value))) {
            online_set_notice(value);
            if (strcmp(value, "CLIENT_DISCONNECTED") == 0) {
                g_online_client.lobby.remote_connected = 0;
                if (g_online_client.host_gameplay_active) {
                    g_online_client.abort_host_gameplay = 1;
                }
            } else if (strcmp(value, "HOST_DISCONNECTED") == 0) {
                online_copy_text(g_online_client.lobby.end_reason,
                                 sizeof(g_online_client.lobby.end_reason),
                                 "Host left");
            }
        }
        return;
    }

    if (packet_type == ONLINE_PACKET_ERROR) {
        if (online_text_value(text, "message", value, sizeof(value))) {
            online_set_notice(value);
        } else {
            online_set_notice("Server_error");
        }
        return;
    }

    if (packet_type == ONLINE_PACKET_KICK) {
        g_online_client.lobby.kicked = 1;
        online_set_notice("Kicked_by_host");
        return;
    }

    if (packet_type == ONLINE_PACKET_START) {
        g_online_client.lobby.game_started = 1;
        online_set_notice("Game_started");
        return;
    }

    if (packet_type == ONLINE_PACKET_SAVE) {
        g_online_client.lobby.last_save_level = online_text_value_int(text, "level", g_online_client.lobby.last_save_level);
        g_online_client.lobby.last_save_score = online_text_value_int(text, "score", g_online_client.lobby.last_save_score);
        g_online_client.lobby.last_save_lives[0] = online_text_value_int(text, "p1", g_online_client.lobby.last_save_lives[0]);
        g_online_client.lobby.last_save_lives[1] = online_text_value_int(text, "p2", g_online_client.lobby.last_save_lives[1]);
        if (online_text_value(text, "note", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.last_save_note,
                             sizeof(g_online_client.lobby.last_save_note),
                             value);
            online_unsanitize_display_text(g_online_client.lobby.last_save_note);
        }
        return;
    }

    if (packet_type == ONLINE_PACKET_END) {
        if (online_text_value(text, "reason", value, sizeof(value))) {
            online_copy_text(g_online_client.lobby.end_reason,
                             sizeof(g_online_client.lobby.end_reason),
                             value);
            online_unsanitize_display_text(g_online_client.lobby.end_reason);
        } else {
            online_copy_text(g_online_client.lobby.end_reason,
                             sizeof(g_online_client.lobby.end_reason),
                             "Session ended");
        }
        g_online_client.lobby.game_started = 0;
        online_set_notice(g_online_client.lobby.end_reason);
        return;
    }

    if (packet_type == ONLINE_PACKET_INPUT) {
        uint32_t next_mask = (uint32_t)online_text_value_int(text, "mask", 0);
        online_apply_remote_input_mask(next_mask);
        return;
    }

    if (packet_type == ONLINE_PACKET_PAUSE) {
        int paused = online_text_value_int(text, "state", g_online_client.session_pause_active);

        g_online_client.session_pause_active = paused ? 1 : 0;
        g_online_client.pending_pause_state = g_online_client.session_pause_active;
        g_online_client.pause_state_dirty = 1;
    }
}

static void online_handle_frame_packet(const uint8_t* payload, size_t payload_len)
{
    uint32_t width_be = 0;
    uint32_t height_be = 0;
    uint32_t level_be = 0;
    uint32_t sequence_be = 0;
    uint32_t raw_size_be = 0;
    uint32_t compressed_size_be = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t level = 0;
    uint32_t sequence = 0;
    uint32_t raw_size = 0;
    uint32_t compressed_size = 0;
    uLongf dest_len = 0;

    if (!payload || payload_len < 24u) return;

    memcpy(&width_be, payload + 0, 4);
    memcpy(&height_be, payload + 4, 4);
    memcpy(&level_be, payload + 8, 4);
    memcpy(&sequence_be, payload + 12, 4);
    memcpy(&raw_size_be, payload + 16, 4);
    memcpy(&compressed_size_be, payload + 20, 4);

    width = ntohl(width_be);
    height = ntohl(height_be);
    level = ntohl(level_be);
    sequence = ntohl(sequence_be);
    raw_size = ntohl(raw_size_be);
    compressed_size = ntohl(compressed_size_be);

    if (width == 0 || height == 0 || raw_size == 0) return;
    if (payload_len < 24u + compressed_size) return;
    if (!online_ensure_capacity(&g_online_client.frame_pixels, &g_online_client.frame_pixels_cap, raw_size)) return;

    if (compressed_size == raw_size) {
        memcpy(g_online_client.frame_pixels, payload + 24, raw_size);
    } else {
        dest_len = (uLongf)raw_size;
        if (uncompress(g_online_client.frame_pixels,
                       &dest_len,
                       payload + 24,
                       (uLongf)compressed_size) != Z_OK) {
            return;
        }
        if ((uint32_t)dest_len != raw_size) return;
    }

    g_online_client.frame_width = (int)width;
    g_online_client.frame_height = (int)height;
    g_online_client.frame_pitch = (int)(width * 4u);
    g_online_client.frame_level = (int)level;
    g_online_client.frame_sequence = sequence;
}

static void online_process_incoming_packets(void)
{
    size_t offset = 0;
    const uint8_t* latest_frame_payload = NULL;
    size_t latest_frame_payload_len = 0;

    while (g_online_client.incoming_len - offset >= 8u) {
        uint32_t type_be = 0;
        uint32_t len_be = 0;
        uint32_t packet_type = 0;
        uint32_t payload_len = 0;

        memcpy(&type_be, g_online_client.incoming + offset, 4);
        memcpy(&len_be, g_online_client.incoming + offset + 4, 4);
        packet_type = ntohl(type_be);
        payload_len = ntohl(len_be);

        if (payload_len > 32u * 1024u * 1024u) {
            online_handle_disconnect("Protocol_error");
            return;
        }
        if (g_online_client.incoming_len - offset < 8u + payload_len) break;

        if (packet_type == ONLINE_PACKET_FRAME) {
            latest_frame_payload = g_online_client.incoming + offset + 8u;
            latest_frame_payload_len = payload_len;
        } else {
            online_handle_text_packet(packet_type, g_online_client.incoming + offset + 8u, payload_len);
        }

        offset += 8u + payload_len;
    }

    if (latest_frame_payload) {
        online_handle_frame_packet(latest_frame_payload, latest_frame_payload_len);
    }

    if (offset > 0) {
        if (offset < g_online_client.incoming_len) {
            memmove(g_online_client.incoming,
                    g_online_client.incoming + offset,
                    g_online_client.incoming_len - offset);
        }
        g_online_client.incoming_len -= offset;
    }
}

static int online_connect_socket(const char* server_host, int server_port)
{
    struct addrinfo hints;
    struct addrinfo* result = NULL;
    struct addrinfo* cursor = NULL;
    char port_text[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_text, sizeof(port_text), "%d", server_port);
    if (getaddrinfo(server_host, port_text, &hints, &result) != 0) {
        return -1;
    }

    for (cursor = result; cursor != NULL; cursor = cursor->ai_next) {
        int flags = 0;

        fd = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, cursor->ai_addr, cursor->ai_addrlen) == 0) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }
            break;
        }

        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

static int online_connect_common(const char* server_host,
                                 int server_port,
                                 const char* player_name,
                                 int is_host,
                                 const char* join_code)
{
    char safe_name[24];
    char safe_code[8];
    char payload[128];

    online_sanitize_token(safe_name, sizeof(safe_name), player_name, 0);
    if (safe_name[0] == '\0') {
        online_copy_text(safe_name, sizeof(safe_name), is_host ? "HOST" : "CLIENT");
    }
    online_sanitize_token(safe_code, sizeof(safe_code), join_code ? join_code : "", 1);

    online_reset_connection_state();
    g_online_client.socket_fd = online_connect_socket(server_host, server_port);
    if (g_online_client.socket_fd < 0) {
        online_set_notice("Unable_to_reach_server");
        return 0;
    }

    g_online_client.connected = 1;
    g_online_client.is_host = is_host ? 1 : 0;
    online_copy_text(g_online_client.local_name, sizeof(g_online_client.local_name), safe_name);
    g_online_client.local_skin = is_host ? 1 : 2;
    g_online_client.local_control = is_host ? 2 : 1;
    g_online_client.lobby.connected = 1;
    online_set_notice("Connecting");

    if (is_host) {
        snprintf(payload, sizeof(payload), "role=host name=%s", safe_name);
    } else {
        snprintf(payload, sizeof(payload), "role=join code=%s name=%s", safe_code, safe_name);
    }
    online_append_packet(ONLINE_PACKET_HELLO, payload, (uint32_t)strlen(payload));

    snprintf(payload, sizeof(payload), "name=%s skin=%d control=%d",
             safe_name,
             g_online_client.local_skin,
             g_online_client.local_control);
    online_append_packet(ONLINE_PACKET_PROFILE, payload, (uint32_t)strlen(payload));
    return 1;
}

void online_client_reset(void)
{
    if (g_online_client.capture_full) free(g_online_client.capture_full);
    if (g_online_client.capture_scaled) free(g_online_client.capture_scaled);
    if (g_online_client.compress_buffer) free(g_online_client.compress_buffer);
    if (g_online_client.frame_pixels) free(g_online_client.frame_pixels);
    if (g_online_client.incoming) free(g_online_client.incoming);
    if (g_online_client.outgoing) free(g_online_client.outgoing);

    memset(&g_online_client, 0, sizeof(g_online_client));
    g_online_client.socket_fd = -1;
    online_reset_connection_state();
}

void online_client_disconnect(void)
{
    online_handle_disconnect("Disconnected");
    g_online_client.host_gameplay_active = 0;
    g_online_client.abort_host_gameplay = 0;
    g_online_client.session_pause_active = 0;
    g_online_client.pause_state_dirty = 0;
    g_online_client.pending_pause_state = 0;
}

void online_client_default_server(char* out_host, size_t out_host_size, int* out_port)
{
    const char* host_env = getenv("HOME_ADVENTURE_SERVER_HOST");
    const char* port_env = getenv("HOME_ADVENTURE_SERVER_PORT");
    int port = 9090;

    if (port_env && port_env[0] != '\0') {
        int parsed = atoi(port_env);
        if (parsed > 0 && parsed < 65536) port = parsed;
    }

    if (out_host && out_host_size > 0) {
        online_copy_text(out_host, out_host_size, (host_env && host_env[0] != '\0') ? host_env : "127.0.0.1");
    }
    if (out_port) *out_port = port;
}

void online_client_default_player_name(char* out_name, size_t out_name_size)
{
    const char* user = getenv("USER");
    char safe_name[24];

    if (!out_name || out_name_size == 0) return;
    online_sanitize_token(safe_name, sizeof(safe_name), user ? user : "PLAYER", 0);
    if (safe_name[0] == '\0') {
        online_copy_text(out_name, out_name_size, "PLAYER");
        return;
    }
    online_copy_text(out_name, out_name_size, safe_name);
}

int online_client_connect_host(const char* server_host, int server_port, const char* player_name)
{
    return online_connect_common(server_host, server_port, player_name, 1, NULL);
}

int online_client_connect_join(const char* server_host, int server_port, const char* join_code, const char* player_name)
{
    return online_connect_common(server_host, server_port, player_name, 0, join_code);
}

void online_client_pump(void)
{
    uint8_t read_buffer[8192];

    if (!g_online_client.connected || g_online_client.socket_fd < 0) return;

    while (1) {
        ssize_t bytes = recv(g_online_client.socket_fd, read_buffer, sizeof(read_buffer), 0);
        if (bytes > 0) {
            size_t needed = g_online_client.incoming_len + (size_t)bytes;
            if (!online_ensure_capacity(&g_online_client.incoming, &g_online_client.incoming_cap, needed)) {
                online_handle_disconnect("Out_of_memory");
                return;
            }
            memcpy(g_online_client.incoming + g_online_client.incoming_len, read_buffer, (size_t)bytes);
            g_online_client.incoming_len += (size_t)bytes;
            continue;
        }
        if (bytes == 0) {
            online_handle_disconnect("Server_closed_connection");
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        online_handle_disconnect("Network_read_failed");
        return;
    }

    online_process_incoming_packets();

    while (g_online_client.outgoing_len > 0) {
        ssize_t bytes = send(g_online_client.socket_fd, g_online_client.outgoing, g_online_client.outgoing_len, 0);
        if (bytes > 0) {
            online_trim_send_queue((size_t)bytes);
            continue;
        }
        if (bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        online_handle_disconnect("Network_write_failed");
        return;
    }
}

int online_client_is_connected(void)
{
    return g_online_client.connected;
}

int online_client_is_host(void)
{
    return g_online_client.is_host;
}

void online_client_copy_lobby_state(OnlineLobbyState* out_state)
{
    if (!out_state) return;
    *out_state = g_online_client.lobby;
}

void online_client_set_profile(const char* player_name, int skin, int control)
{
    char safe_name[24];
    char payload[128];

    online_sanitize_token(safe_name, sizeof(safe_name), player_name, 0);
    if (safe_name[0] == '\0') {
        online_copy_text(safe_name, sizeof(safe_name), g_online_client.is_host ? "HOST" : "CLIENT");
    }

    if (skin < 1 || skin > 2) skin = g_online_client.is_host ? 1 : 2;
    if (control < 1 || control > 2) control = g_online_client.is_host ? 2 : 1;

    online_copy_text(g_online_client.local_name, sizeof(g_online_client.local_name), safe_name);
    g_online_client.local_skin = skin;
    g_online_client.local_control = control;

    if (g_online_client.is_host) {
        online_copy_text(g_online_client.lobby.host_name, sizeof(g_online_client.lobby.host_name), safe_name);
        g_online_client.lobby.host_skin = skin;
        g_online_client.lobby.host_control = control;
    } else {
        online_copy_text(g_online_client.lobby.client_name, sizeof(g_online_client.lobby.client_name), safe_name);
        g_online_client.lobby.client_skin = skin;
        g_online_client.lobby.client_control = control;
    }

    if (!g_online_client.connected) return;

    snprintf(payload, sizeof(payload), "name=%s skin=%d control=%d", safe_name, skin, control);
    online_append_packet(ONLINE_PACKET_PROFILE, payload, (uint32_t)strlen(payload));
}

void online_client_start_game(void)
{
    if (!g_online_client.connected || !g_online_client.is_host) return;
    g_online_client.lobby.game_started = 1;
    online_append_packet(ONLINE_PACKET_START, NULL, 0);
}

void online_client_kick_client(void)
{
    if (!g_online_client.connected || !g_online_client.is_host) return;
    online_append_packet(ONLINE_PACKET_KICK, NULL, 0);
}

void online_client_send_pause_state(int paused)
{
    char payload[32];

    if (!g_online_client.connected) return;

    g_online_client.session_pause_active = paused ? 1 : 0;
    snprintf(payload, sizeof(payload), "state=%d", g_online_client.session_pause_active);
    online_append_packet(ONLINE_PACKET_PAUSE, payload, (uint32_t)strlen(payload));
}

int online_client_consume_pause_state_change(int* out_paused)
{
    if (!g_online_client.pause_state_dirty) return 0;

    if (out_paused) {
        *out_paused = g_online_client.pending_pause_state ? 1 : 0;
    }
    g_online_client.pause_state_dirty = 0;
    return 1;
}

void online_client_send_input_mask(uint32_t input_mask)
{
    char payload[64];
    Uint32 now = 0;
    int input_changed = 0;

    input_mask &= (ONLINE_INPUT_A |
                   ONLINE_INPUT_D |
                   ONLINE_INPUT_W |
                   ONLINE_INPUT_S |
                   ONLINE_INPUT_SPACE |
                   ONLINE_INPUT_F |
                   ONLINE_INPUT_0);

    if (!g_online_client.connected || g_online_client.is_host) return;
    now = SDL_GetTicks();
    input_changed = (input_mask != g_online_client.last_sent_input_mask);
    if (!input_changed) {
        if (input_mask == 0) return;
        if (g_online_client.last_input_resend_ticks > 0 &&
            now - g_online_client.last_input_resend_ticks < ONLINE_INPUT_RESEND_INTERVAL_MS) {
            return;
        }
    }

    g_online_client.last_sent_input_mask = input_mask;
    g_online_client.last_input_resend_ticks = now;
    snprintf(payload, sizeof(payload), "mask=%u", input_mask);
    online_append_packet(ONLINE_PACKET_INPUT, payload, (uint32_t)strlen(payload));
}

uint32_t online_client_remote_input_mask(void)
{
    return g_online_client.remote_input_mask;
}

int online_client_remote_scancode_down(SDL_Scancode scancode)
{
    switch (scancode) {
        case SDL_SCANCODE_A: return (g_online_client.remote_input_mask & ONLINE_INPUT_A) != 0;
        case SDL_SCANCODE_D: return (g_online_client.remote_input_mask & ONLINE_INPUT_D) != 0;
        case SDL_SCANCODE_W: return (g_online_client.remote_input_mask & ONLINE_INPUT_W) != 0;
        case SDL_SCANCODE_S: return (g_online_client.remote_input_mask & ONLINE_INPUT_S) != 0;
        case SDL_SCANCODE_SPACE: return (g_online_client.remote_input_mask & ONLINE_INPUT_SPACE) != 0;
        case SDL_SCANCODE_F: return (g_online_client.remote_input_mask & ONLINE_INPUT_F) != 0;
        case SDL_SCANCODE_0: return (g_online_client.remote_input_mask & ONLINE_INPUT_0) != 0;
        default: return 0;
    }
}

int online_client_begin_host_gameplay(void)
{
    if (!g_online_client.connected || !g_online_client.is_host) return 0;
    g_online_client.host_gameplay_active = 1;
    g_online_client.abort_host_gameplay = 0;
    g_online_client.remote_input_mask = 0;
    g_online_client.previous_remote_input_mask = 0;
    g_online_client.session_pause_active = 0;
    g_online_client.pause_state_dirty = 0;
    g_online_client.pending_pause_state = 0;
    g_online_client.last_frame_submit_ticks = 0;
    g_online_client.next_frame_sequence = 1;
    g_online_client.last_sent_input_mask = 0;
    g_online_client.last_input_resend_ticks = 0;
    return 1;
}

void online_client_end_host_gameplay(const char* reason)
{
    char payload[96];

    if (g_online_client.connected && g_online_client.is_host) {
        snprintf(payload, sizeof(payload), "reason=%s", (reason && reason[0] != '\0') ? reason : "SESSION_ENDED");
        online_append_packet(ONLINE_PACKET_END, payload, (uint32_t)strlen(payload));
        online_client_pump();
    }

    g_online_client.host_gameplay_active = 0;
    g_online_client.abort_host_gameplay = 0;
    g_online_client.remote_input_mask = 0;
    g_online_client.previous_remote_input_mask = 0;
    g_online_client.session_pause_active = 0;
    g_online_client.pause_state_dirty = 0;
    g_online_client.pending_pause_state = 0;
    g_online_client.last_sent_input_mask = 0;
    g_online_client.last_input_resend_ticks = 0;
}

int online_client_should_abort_host_gameplay(void)
{
    return g_online_client.abort_host_gameplay;
}

void online_client_submit_frame(SDL_Renderer* renderer, int level_number)
{
    int src_w = 0;
    int src_h = 0;
    size_t full_size = 0;
    size_t scaled_size = 0;
    Uint32 now = 0;
#if !ONLINE_RAW_FRAME_STREAMING
    uLongf compressed_size = 0;
#endif
    size_t frame_data_size = 0;
    size_t frame_buffer_size = 0;
    uint32_t header[6];
    uint8_t* src_pixels = NULL;
    uint8_t* dst_pixels = NULL;
    size_t payload_size = 0;

    if (!renderer ||
        !g_online_client.connected ||
        !g_online_client.is_host ||
        !g_online_client.host_gameplay_active ||
        !g_online_client.lobby.remote_connected ||
        !g_online_client.lobby.game_started) {
        return;
    }

    now = SDL_GetTicks();
    if (g_online_client.last_frame_submit_ticks > 0 &&
        now - g_online_client.last_frame_submit_ticks < ONLINE_FRAME_INTERVAL_MS) {
        return;
    }
    online_drop_queued_frame_packets();
    if (g_online_client.outgoing_len > ONLINE_FRAME_BACKLOG_LIMIT) {
        return;
    }

    if (SDL_GetRendererOutputSize(renderer, &src_w, &src_h) != 0 || src_w <= 0 || src_h <= 0) {
        src_w = 1280;
        src_h = 720;
    }

    full_size = (size_t)src_w * (size_t)src_h * 4u;
    scaled_size = (size_t)ONLINE_CAPTURE_W * (size_t)ONLINE_CAPTURE_H * 4u;
    if (!online_ensure_capacity(&g_online_client.capture_full, &g_online_client.capture_full_cap, full_size)) return;
    if (!online_ensure_capacity(&g_online_client.capture_scaled, &g_online_client.capture_scaled_cap, scaled_size)) return;
#if ONLINE_RAW_FRAME_STREAMING
    frame_buffer_size = scaled_size + 24u;
#else
    frame_buffer_size = compressBound((uLong)scaled_size) + 24u;
#endif
    if (!online_ensure_capacity(&g_online_client.compress_buffer,
                                &g_online_client.compress_cap,
                                frame_buffer_size)) return;

    if (SDL_RenderReadPixels(renderer,
                             NULL,
                             SDL_PIXELFORMAT_ARGB8888,
                             g_online_client.capture_full,
                             src_w * 4) != 0) {
        return;
    }

    src_pixels = g_online_client.capture_full;
    dst_pixels = g_online_client.capture_scaled;
    if (src_w == ONLINE_CAPTURE_W && src_h == ONLINE_CAPTURE_H) {
        memcpy(dst_pixels, src_pixels, scaled_size);
    } else {
        for (int y = 0; y < ONLINE_CAPTURE_H; ++y) {
            int src_y = (y * src_h) / ONLINE_CAPTURE_H;
            uint32_t* dst_row = (uint32_t*)(dst_pixels + (size_t)y * ONLINE_CAPTURE_W * 4u);
            uint32_t* src_row = (uint32_t*)(src_pixels + (size_t)src_y * src_w * 4u);
            for (int x = 0; x < ONLINE_CAPTURE_W; ++x) {
                int src_x = (x * src_w) / ONLINE_CAPTURE_W;
                dst_row[x] = src_row[src_x];
            }
        }
    }

#if ONLINE_RAW_FRAME_STREAMING
    memcpy(g_online_client.compress_buffer + 24u, g_online_client.capture_scaled, scaled_size);
    frame_data_size = scaled_size;
#else
    compressed_size = (uLongf)(g_online_client.compress_cap - 24u);
    if (compress2(g_online_client.compress_buffer + 24u,
                  &compressed_size,
                  g_online_client.capture_scaled,
                  (uLong)scaled_size,
                  1) != Z_OK) {
        return;
    }
    frame_data_size = (size_t)compressed_size;
#endif

    header[0] = htonl(ONLINE_CAPTURE_W);
    header[1] = htonl(ONLINE_CAPTURE_H);
    header[2] = htonl((uint32_t)level_number);
    header[3] = htonl(g_online_client.next_frame_sequence++);
    header[4] = htonl((uint32_t)scaled_size);
    header[5] = htonl((uint32_t)frame_data_size);
    memcpy(g_online_client.compress_buffer, header, sizeof(header));

    payload_size = 24u + frame_data_size;
    if (!online_append_packet(ONLINE_PACKET_FRAME,
                              g_online_client.compress_buffer,
                              (uint32_t)payload_size)) {
        return;
    }

    g_online_client.last_frame_submit_ticks = now;
}

void online_client_notify_save(int current_level, int score, int p1_lives, int p2_lives, const char* note)
{
    char safe_note[64];
    char payload[192];

    if (!g_online_client.connected || !g_online_client.is_host) return;

    online_sanitize_token(safe_note, sizeof(safe_note), note ? note : "SAVE", 1);
    if (safe_note[0] == '\0') online_copy_text(safe_note, sizeof(safe_note), "SAVE");

    snprintf(payload, sizeof(payload),
             "level=%d score=%d p1=%d p2=%d note=%s",
             current_level,
             score,
             p1_lives,
             p2_lives,
             safe_note);
    online_append_packet(ONLINE_PACKET_SAVE, payload, (uint32_t)strlen(payload));
}

int online_client_latest_frame(OnlineRemoteFrame* out_frame)
{
    if (!out_frame) return 0;
    memset(out_frame, 0, sizeof(*out_frame));

    if (g_online_client.frame_sequence == 0 ||
        !g_online_client.frame_pixels ||
        g_online_client.frame_width <= 0 ||
        g_online_client.frame_height <= 0) {
        return 0;
    }

    out_frame->available = 1;
    out_frame->width = g_online_client.frame_width;
    out_frame->height = g_online_client.frame_height;
    out_frame->pitch = g_online_client.frame_pitch;
    out_frame->level_number = g_online_client.frame_level;
    out_frame->sequence = g_online_client.frame_sequence;
    out_frame->pixels = g_online_client.frame_pixels;
    return 1;
}
