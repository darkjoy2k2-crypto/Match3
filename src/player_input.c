#include "player_input.h"

static u16 first_pressed_direction(u16 joyState, u16 lastJoyState) {
    u16 newlyPressed = (u16)(joyState & (u16)~lastJoyState);

    if (newlyPressed & BUTTON_LEFT) return BUTTON_LEFT;
    if (newlyPressed & BUTTON_RIGHT) return BUTTON_RIGHT;
    if (newlyPressed & BUTTON_UP) return BUTTON_UP;
    if (newlyPressed & BUTTON_DOWN) return BUTTON_DOWN;

    return 0;
}

static u16 first_held_direction(u16 joyState) {
    if (joyState & BUTTON_LEFT) return BUTTON_LEFT;
    if (joyState & BUTTON_RIGHT) return BUTTON_RIGHT;
    if (joyState & BUTTON_UP) return BUTTON_UP;
    if (joyState & BUTTON_DOWN) return BUTTON_DOWN;

    return 0;
}

void hold_repeat_input_init(HoldRepeatInput* input, u16 initialDelay, u16 repeatDelay) {
    input->activeButton = 0;
    input->holdTimer = 0;
    input->initialDelay = initialDelay;
    input->repeatDelay = repeatDelay;
}

u16 hold_repeat_input_update(HoldRepeatInput* input, u16 joyState, u16 lastJoyState) {
    u16 newDirection = first_pressed_direction(joyState, lastJoyState);

    if (newDirection != 0) {
        input->activeButton = newDirection;
        input->holdTimer = input->initialDelay;
        return newDirection;
    }

    if (input->activeButton != 0) {
        if ((joyState & input->activeButton) == 0) {
            input->activeButton = 0;
        } else {
            if (input->holdTimer > 0) {
                input->holdTimer--;
            }

            if (input->holdTimer == 0) {
                input->holdTimer = input->repeatDelay;
                return input->activeButton;
            }
        }
    }

    if (input->activeButton == 0) {
        u16 heldDirection = first_held_direction(joyState);

        if (heldDirection != 0) {
            input->activeButton = heldDirection;
            input->holdTimer = input->initialDelay;
            return heldDirection;
        }
    }

    return 0;
}
