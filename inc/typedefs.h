#pragma once

#include <genesis.h>

typedef struct HighscoreEntry {
    u32 score;
    char name[4];
} HighscoreEntry __attribute__((aligned(4)));

typedef enum GameState {
    STATE_NONE = -1,
    STATE_TITLE = 0,
    STATE_GAME = 1,
    STATE_GAMEOVER = 2,
    STATE_HIGHSCORE = 3,
    STATE_COUNT = 4
} GameState;

typedef enum GameMode {
    GAME_MODE_ENDLESS = 0,
    GAME_MODE_CLEANUP = 1
} GameMode;

typedef struct StateDefinition {
    void (*enter)(void);
    void (*update)(void);
    void (*draw)(void);
    void (*exit)(void);
} StateDefinition __attribute__((aligned(4)));
