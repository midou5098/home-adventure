#include "audio.h"

#include "billiards.h"

#include <stdlib.h>
#include <string.h>

enum {
    AUDIO_CHANNEL_FOOTSTEP = 20,
    AUDIO_CHANNEL_CUE = 21,
    AUDIO_CHANNEL_WALL = 22,
    AUDIO_CHANNEL_POCKET = 23,
    AUDIO_CHANNEL_SKIN2_SIDE = 24,
    AUDIO_CHANNEL_TETRIS_BG = 25,
    AUDIO_CHANNEL_KEY_PICKUP = 26,
    AUDIO_MIN_CHANNELS = 27
};

static Mix_Chunk *load_chunk(const char *path, int volume) {
    Mix_Chunk *chunk = Mix_LoadWAV(path);
    if (!chunk) {
        SDL_Log("Failed to load audio '%s': %s", path, Mix_GetError());
        return NULL;
    }

    Mix_VolumeChunk(chunk, volume);
    return chunk;
}

static Mix_Music *load_music_first_available(const char *const *paths, int path_count) {
    for (int i = 0; i < path_count; ++i) {
        Mix_Music *music = Mix_LoadMUS(paths[i]);
        if (music) {
            return music;
        }
        SDL_Log("Failed to load music '%s': %s", paths[i], Mix_GetError());
    }
    return NULL;
}

static int pick_skin2_side_step_index(AudioState *audio) {
    int available[3];
    int available_count = 0;

    for (int i = 0; i < 3; ++i) {
        if (audio->sfx_skin2_side_step[i]) {
            available[available_count++] = i;
        }
    }

    if (available_count <= 0) {
        return -1;
    }

    int chosen = available[rand() % available_count];
    if (available_count > 1 && chosen == audio->last_skin2_side_step_index) {
        for (int tries = 0; tries < 4 && chosen == audio->last_skin2_side_step_index; ++tries) {
            chosen = available[rand() % available_count];
        }
        if (chosen == audio->last_skin2_side_step_index) {
            for (int i = 0; i < available_count; ++i) {
                if (available[i] != audio->last_skin2_side_step_index) {
                    chosen = available[i];
                    break;
                }
            }
        }
    }

    audio->last_skin2_side_step_index = chosen;
    return chosen;
}

bool audio_init(AudioState *audio) {
    memset(audio, 0, sizeof(*audio));
    audio->last_skin2_side_step_index = -1;

    {
        const int current_mix_flags = Mix_Init(0);
        if ((current_mix_flags & MIX_INIT_MP3) == 0) {
            const int mp3_result = Mix_Init(MIX_INIT_MP3);
            if ((mp3_result & MIX_INIT_MP3) != 0) {
                audio->initialized_mp3_decoder = true;
            } else {
                SDL_Log("Mix_Init(MIX_INIT_MP3) failed: %s", Mix_GetError());
            }
        }
    }

    if (Mix_QuerySpec(NULL, NULL, NULL) == 0) {
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
            SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
            if (audio->initialized_mp3_decoder) {
                Mix_Quit();
                audio->initialized_mp3_decoder = false;
            }
            return false;
        }
        audio->opened_audio_device = true;
    }

    if (Mix_AllocateChannels(-1) < AUDIO_MIN_CHANNELS) {
        Mix_AllocateChannels(AUDIO_MIN_CHANNELS);
    }

    audio->sfx_footstep = load_chunk("assets/walking.wav", 23);
    audio->sfx_skin2_side_step[0] = load_chunk(
        "assets/skin2/skin2 walking side way sound effect/soundeffect1.mp3",
        35
    );
    audio->sfx_skin2_side_step[1] = load_chunk(
        "assets/skin2/skin2 walking side way sound effect/soundeffect2.mp3",
        35
    );
    audio->sfx_skin2_side_step[2] = load_chunk(
        "assets/skin2/skin2 walking side way sound effect/soundeffect3.mp3",
        35
    );
    audio->sfx_cue_hit = load_chunk("assets/minigame/first ball hit with baton.mp3", 54);
    audio->sfx_wall_bounce = load_chunk("assets/minigame/ball hiting wall .mp3", 36);
    audio->sfx_pocket = load_chunk("assets/minigame/ball fall in hole.mp3", 61);
    audio->sfx_key_pickup = load_chunk("assets/key_pickup.mp3", 42);
    {
        static const char *const tetris_bg_paths[] = {
            "arcade mini games/tetrise/tetrise bg.mp3",
            "newshoplvl1/arcade mini games/tetrise/tetrise bg.mp3",
            "../newshoplvl1/arcade mini games/tetrise/tetrise bg.mp3"
        };
        for (int i = 0; i < (int)(sizeof(tetris_bg_paths) / sizeof(tetris_bg_paths[0])); ++i) {
            audio->sfx_tetris_bg = load_chunk(tetris_bg_paths[i], 10);
            if (audio->sfx_tetris_bg) {
                break;
            }
        }
    }
    {
        static const char *const music_paths[] = {
            "assets/bgmusic.mp3",
            "newshoplvl1/assets/bgmusic.mp3",
            "../newshoplvl1/assets/bgmusic.mp3"
        };
        audio->bg_music = load_music_first_available(
            music_paths,
            (int)(sizeof(music_paths) / sizeof(music_paths[0]))
        );
        if (audio->bg_music) {
            Mix_VolumeMusic(32);
            if (Mix_PlayMusic(audio->bg_music, -1) < 0) {
                SDL_Log("Failed to start shop background music: %s", Mix_GetError());
            }
        }
    }

    audio->last_quiz_phase = QUIZ_PHASE_QUESTION;
    audio->last_round_id = 0;
    audio->initialized = true;
    return true;
}

void audio_shutdown(AudioState *audio) {
    if (!audio->initialized) {
        return;
    }

    if (Mix_QuerySpec(NULL, NULL, NULL) != 0) {
        Mix_HaltChannel(AUDIO_CHANNEL_FOOTSTEP);
        Mix_HaltChannel(AUDIO_CHANNEL_CUE);
        Mix_HaltChannel(AUDIO_CHANNEL_WALL);
        Mix_HaltChannel(AUDIO_CHANNEL_POCKET);
        Mix_HaltChannel(AUDIO_CHANNEL_SKIN2_SIDE);
        Mix_HaltChannel(AUDIO_CHANNEL_TETRIS_BG);
        Mix_HaltChannel(AUDIO_CHANNEL_KEY_PICKUP);
    }

    Mix_FreeChunk(audio->sfx_footstep);
    Mix_FreeChunk(audio->sfx_skin2_side_step[0]);
    Mix_FreeChunk(audio->sfx_skin2_side_step[1]);
    Mix_FreeChunk(audio->sfx_skin2_side_step[2]);
    Mix_FreeChunk(audio->sfx_cue_hit);
    Mix_FreeChunk(audio->sfx_wall_bounce);
    Mix_FreeChunk(audio->sfx_pocket);
    Mix_FreeChunk(audio->sfx_tetris_bg);
    Mix_FreeChunk(audio->sfx_key_pickup);
    if (audio->bg_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(audio->bg_music);
    }

    audio->sfx_footstep = NULL;
    audio->sfx_skin2_side_step[0] = NULL;
    audio->sfx_skin2_side_step[1] = NULL;
    audio->sfx_skin2_side_step[2] = NULL;
    audio->sfx_cue_hit = NULL;
    audio->sfx_wall_bounce = NULL;
    audio->sfx_pocket = NULL;
    audio->sfx_tetris_bg = NULL;
    audio->sfx_key_pickup = NULL;
    audio->bg_music = NULL;

    if (audio->opened_audio_device && Mix_QuerySpec(NULL, NULL, NULL) != 0) {
        Mix_CloseAudio();
    }
    if (audio->initialized_mp3_decoder) {
        Mix_Quit();
    }

    audio->opened_audio_device = false;
    audio->initialized_mp3_decoder = false;
    audio->walking_active = false;
    audio->skin2_sideways_active = false;
    audio->skin2_side_was_playing = false;
    audio->next_skin2_side_step_at = 0;
    audio->last_skin2_side_step_index = -1;
    audio->tetris_music_playing = false;
    audio->initialized = false;
}

void audio_set_walking(AudioState *audio, bool is_walking) {
    if (!audio->initialized || !audio->sfx_footstep || Mix_QuerySpec(NULL, NULL, NULL) == 0) {
        return;
    }

    if (is_walking) {
        if (audio->skin2_sideways_active) {
            if (Mix_Playing(AUDIO_CHANNEL_FOOTSTEP)) {
                Mix_HaltChannel(AUDIO_CHANNEL_FOOTSTEP);
            }
            audio->walking_active = false;
        } else if (!audio->walking_active) {
            audio->walking_active = true;
            if (!Mix_Playing(AUDIO_CHANNEL_FOOTSTEP)) {
                Mix_PlayChannel(AUDIO_CHANNEL_FOOTSTEP, audio->sfx_footstep, -1);
            }
        }
        return;
    }

    if (!audio->walking_active) {
        return;
    }

    audio->walking_active = false;
    if (Mix_Playing(AUDIO_CHANNEL_FOOTSTEP)) {
        Mix_HaltChannel(AUDIO_CHANNEL_FOOTSTEP);
    }
}

void audio_set_skin2_sideways(AudioState *audio, bool is_active) {
    if (!audio->initialized || Mix_QuerySpec(NULL, NULL, NULL) == 0) {
        return;
    }

    if (!is_active) {
        if (audio->skin2_sideways_active && Mix_Playing(AUDIO_CHANNEL_SKIN2_SIDE)) {
            Mix_HaltChannel(AUDIO_CHANNEL_SKIN2_SIDE);
        }
        audio->skin2_sideways_active = false;
        audio->skin2_side_was_playing = false;
        audio->next_skin2_side_step_at = 0;
        return;
    }

    if (
        !audio->sfx_skin2_side_step[0] &&
        !audio->sfx_skin2_side_step[1] &&
        !audio->sfx_skin2_side_step[2]
    ) {
        return;
    }

    {
        const uint32_t now_ms = SDL_GetTicks();
        const bool channel_playing = Mix_Playing(AUDIO_CHANNEL_SKIN2_SIDE) != 0;

        if (!audio->skin2_sideways_active) {
            audio->skin2_sideways_active = true;
            audio->next_skin2_side_step_at = now_ms;
        }

        if (channel_playing) {
            audio->skin2_side_was_playing = true;
            return;
        }

        if (audio->skin2_side_was_playing) {
            audio->skin2_side_was_playing = false;
            audio->next_skin2_side_step_at = now_ms + 1000u;
            return;
        }

        if (now_ms < audio->next_skin2_side_step_at) {
            return;
        }

        const int selected = pick_skin2_side_step_index(audio);
        if (selected >= 0 && audio->sfx_skin2_side_step[selected]) {
            Mix_PlayChannel(AUDIO_CHANNEL_SKIN2_SIDE, audio->sfx_skin2_side_step[selected], 0);
            audio->skin2_side_was_playing = true;
        }
    }
}

void audio_set_tetris_music(AudioState *audio, bool is_active) {
    if (!audio->initialized || !audio->sfx_tetris_bg || Mix_QuerySpec(NULL, NULL, NULL) == 0) {
        return;
    }

    if (is_active) {
        if (!audio->tetris_music_playing || !Mix_Playing(AUDIO_CHANNEL_TETRIS_BG)) {
            Mix_PlayChannel(AUDIO_CHANNEL_TETRIS_BG, audio->sfx_tetris_bg, -1);
            audio->tetris_music_playing = true;
        }
        return;
    }

    if (audio->tetris_music_playing || Mix_Playing(AUDIO_CHANNEL_TETRIS_BG)) {
        Mix_HaltChannel(AUDIO_CHANNEL_TETRIS_BG);
    }
    audio->tetris_music_playing = false;
}

void audio_play_key_pickup(AudioState *audio) {
    if (!audio->initialized || !audio->sfx_key_pickup || Mix_QuerySpec(NULL, NULL, NULL) == 0) {
        return;
    }

    Mix_PlayChannel(AUDIO_CHANNEL_KEY_PICKUP, audio->sfx_key_pickup, 0);
}

void audio_reset_billiards(AudioState *audio) {
    audio->last_round_id = 0;
    audio->pocket_sound_played = false;
    audio->wall_sound_played = false;
    audio->last_quiz_phase = QUIZ_PHASE_QUESTION;
}

void audio_update_billiards(AudioState *audio, const BilliardsState *billiards) {
    if (!audio->initialized || !billiards_visible(billiards)) {
        audio_reset_billiards(audio);
        return;
    }

    if (audio->last_round_id != billiards->round_id) {
        audio->last_round_id = billiards->round_id;
        audio->last_quiz_phase = billiards->phase;
        audio->pocket_sound_played = false;
        audio->wall_sound_played = false;
        return;
    }

    if (billiards->phase != audio->last_quiz_phase) {
        if (billiards->phase == QUIZ_PHASE_CUE && audio->sfx_cue_hit) {
            Mix_PlayChannel(AUDIO_CHANNEL_CUE, audio->sfx_cue_hit, 0);
        } else if (billiards->phase == QUIZ_PHASE_EIGHT_BALL && audio->sfx_cue_hit) {
            Mix_PlayChannel(AUDIO_CHANNEL_CUE, audio->sfx_cue_hit, 0);
        }
        audio->last_quiz_phase = billiards->phase;
    }

    float progress = 0.0f;
    if (billiards->phase == QUIZ_PHASE_EIGHT_BALL) {
        progress = billiards_get_phase_progress(
            billiards,
            SDL_GetTicks(),
            BILLIARDS_QUIZ_EIGHT_BALL_DURATION_MS
        );
    }

    if (!audio->wall_sound_played && !billiards->did_answer_correctly && audio->sfx_wall_bounce) {
        if (
            (billiards->phase == QUIZ_PHASE_EIGHT_BALL && progress > 0.56f) ||
            billiards->phase == QUIZ_PHASE_RESULT
        ) {
            Mix_PlayChannel(AUDIO_CHANNEL_WALL, audio->sfx_wall_bounce, 0);
            audio->wall_sound_played = true;
        }
    }

    if (!audio->pocket_sound_played && billiards->did_answer_correctly && audio->sfx_pocket) {
        if (
            (billiards->phase == QUIZ_PHASE_EIGHT_BALL && progress > 0.70f) ||
            billiards->phase == QUIZ_PHASE_RESULT
        ) {
            Mix_PlayChannel(AUDIO_CHANNEL_POCKET, audio->sfx_pocket, 0);
            audio->pocket_sound_played = true;
        }
    }
}
