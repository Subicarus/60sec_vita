#pragma once
#include "platform.h"

void game_init(uint32_t seed);
void game_update(float dt, const PadState *pad, int pause_pressed);
void game_draw(void);
