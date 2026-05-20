#pragma once

#include <genesis.h>

#include "typedefs.h"
#include "player_input.h"

#define QUAD_STEP 16
#define QUAD_OFFSET_X 8
#define QUAD_OFFSET_Y 8
#define MOVE_REPEAT_INITIAL_DELAY 6
#define MOVE_REPEAT_DELAY 2

extern GameState currentState;
extern GameState lastState;
extern GameMode currentGameMode;

extern u16 joyState;
extern u16 lastJoyState;

extern u16 gameTicks;
extern u16 timeLeftSeconds;
extern u32 lastScore;

extern s16 quadX;
extern s16 quadY;
extern u16 quadCol;
extern u16 quadRow;

extern u16 nameCursor;
extern char enteredName[4];

extern HoldRepeatInput moveInput;

extern u16 textPaletteData[16];
extern HighscoreEntry highscores[10];

bool button_pressed(u16 button);
void sync_quad_pos_from_cell(void);
void insert_highscore(u32 score, const char* name3);
void u32_to_str(u32 value, char* out);
void format_timer(char* out, u16 seconds);
