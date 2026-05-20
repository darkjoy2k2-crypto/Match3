#pragma once

typedef enum LockModeState {
    LOCK_NONE = 0,
    LOCK_ACTIVE
} LockModeState;

void game_state_enter(void);
void game_state_update(void);
void game_state_draw(void);
void game_state_exit(void);
