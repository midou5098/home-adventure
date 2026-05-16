#ifndef SDLVERSIONSHOP_BARISTA_H
#define SDLVERSIONSHOP_BARISTA_H

#include "game.h"

typedef struct {
    int frame;
} BaristaState;

void barista_init(BaristaState *barista);
void barista_update(BaristaState *barista, uint32_t now_ms);

#endif
