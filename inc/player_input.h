#pragma once

#include <genesis.h>

typedef struct HoldRepeatInput {
    u16 activeButton;
    u16 holdTimer;
    u16 initialDelay;
    u16 repeatDelay;
} HoldRepeatInput __attribute__((aligned(2)));

void hold_repeat_input_init(HoldRepeatInput* input, u16 initialDelay, u16 repeatDelay);
u16 hold_repeat_input_update(HoldRepeatInput* input, u16 joyState, u16 lastJoyState);
