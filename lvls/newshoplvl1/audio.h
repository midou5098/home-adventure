#ifndef SDLVERSIONSHOP_AUDIO_H
#define SDLVERSIONSHOP_AUDIO_H

#include "game.h"

struct BilliardsState;

typedef struct {
    bool initialized;
    bool opened_audio_device;
    bool initialized_mp3_decoder;
    bool walking_active;
    bool skin2_sideways_active;
    bool skin2_side_was_playing;
    uint32_t next_skin2_side_step_at;
    int last_skin2_side_step_index;

    Mix_Chunk *sfx_footstep;
    Mix_Chunk *sfx_skin2_side_step[3];
    Mix_Chunk *sfx_cue_hit;
    Mix_Chunk *sfx_wall_bounce;
    Mix_Chunk *sfx_pocket;
    Mix_Chunk *sfx_tetris_bg;
    Mix_Chunk *sfx_key_pickup;
    Mix_Music *bg_music;
    bool tetris_music_playing;

    QuizPhase last_quiz_phase;
    int last_round_id;
    bool pocket_sound_played;
    bool wall_sound_played;
} AudioState;

bool audio_init(AudioState *audio);
void audio_shutdown(AudioState *audio);
void audio_set_walking(AudioState *audio, bool is_walking);
void audio_set_skin2_sideways(AudioState *audio, bool is_active);
void audio_set_tetris_music(AudioState *audio, bool is_active);
void audio_play_key_pickup(AudioState *audio);
void audio_reset_billiards(AudioState *audio);
void audio_update_billiards(AudioState *audio, const struct BilliardsState *billiards);

#endif
