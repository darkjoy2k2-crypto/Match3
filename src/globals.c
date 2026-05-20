#include "globals.h"

u32 lastScore = 0;

GameState currentState = STATE_TITLE;
GameState lastState = STATE_NONE;
GameMode currentGameMode = GAME_MODE_ENDLESS;

u16 joyState = 0;
u16 lastJoyState = 0;

u16 gameTicks = 0;
u16 timeLeftSeconds = 120;

s16 quadX = 160;
s16 quadY = 112;
u16 quadCol = 10;
u16 quadRow = 7;

u16 nameCursor = 0;
char enteredName[4] = "AAA";

HoldRepeatInput moveInput;

u16 textPaletteData[16] = {
    0x000, 0xEEE, 0xEEE, 0xEEE,
    0xEEE, 0xEEE, 0xEEE, 0xEEE,
    0xEEE, 0xEEE, 0xEEE, 0xEEE,
    0xEEE, 0xEEE, 0xEEE, 0xEEE
};

HighscoreEntry highscores[10] = {
    {120, "AAA"},
    {110, "BBB"},
    {100, "CCC"},
    {90, "DDD"},
    {80, "EEE"},
    {70, "FFF"},
    {60, "GGG"},
    {50, "HHH"},
    {40, "III"},
    {30, "JJJ"}
};

bool button_pressed(u16 button) {
    return ((joyState & button) && !(lastJoyState & button));
}

void sync_quad_pos_from_cell(void) {
    quadX = (s16)((quadCol * QUAD_STEP) + QUAD_OFFSET_X);
    quadY = (s16)((quadRow * QUAD_STEP) + QUAD_OFFSET_Y);
}

void insert_highscore(u32 score, const char* name3) {
    u16 pos = 10;

    for (u16 i = 0; i < 10; i++) {
        if (score > highscores[i].score) {
            pos = i;
            break;
        }
    }

    if (pos >= 10) return;

    for (s16 j = 9; j > (s16)pos; j--) {
        highscores[j] = highscores[j - 1];
    }

    highscores[pos].score = score;
    highscores[pos].name[0] = name3[0];
    highscores[pos].name[1] = name3[1];
    highscores[pos].name[2] = name3[2];
    highscores[pos].name[3] = '\0';
}

void u32_to_str(u32 value, char* out) {
    char tmp[11];
    s16 pos = 0;

    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (value > 0) {
        tmp[pos++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (s16 i = 0; i < pos; i++) {
        out[i] = tmp[pos - 1 - i];
    }
    out[pos] = '\0';
}

void format_timer(char* out, u16 seconds) {
    u16 mm = (u16)(seconds / 60);
    u16 ss = (u16)(seconds % 60);

    out[0] = (char)('0' + mm / 10);
    out[1] = (char)('0' + mm % 10);
    out[2] = ':';
    out[3] = (char)('0' + ss / 10);
    out[4] = (char)('0' + ss % 10);
    out[5] = '\0';
}
