#include "barista.h"

void barista_init(BaristaState *barista) {
    barista->frame = 0;
}

void barista_update(BaristaState *barista, uint32_t now_ms) {
    const int frame_count = 36;
    const uint32_t play_duration_ms = 3000;
    const uint32_t pause_duration_ms = 2000;
    const uint32_t cycle_duration_ms = play_duration_ms + pause_duration_ms;

    const uint32_t cycle_time = now_ms % cycle_duration_ms;

    if (cycle_time >= play_duration_ms) {
        barista->frame = frame_count - 1;
        return;
    }

    int frame = (int)(((float)cycle_time / (float)play_duration_ms) * frame_count);
    if (frame < 0) {
        frame = 0;
    }
    if (frame >= frame_count) {
        frame = frame_count - 1;
    }

    barista->frame = frame;
}
