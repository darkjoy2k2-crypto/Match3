#include <genesis.h>
#include <string.h>

#include "globals.h"
#include "typedefs.h"
#include "sprite_quad_engine.h"

void highscore_state_enter(void) {
    VDP_clearPlane(BG_A, TRUE);

    sprite_quad_engine_hide();
}

void highscore_state_update(void) {
    if (button_pressed(BUTTON_B) || button_pressed(BUTTON_C) || button_pressed(BUTTON_START)) {
        currentState = STATE_TITLE;
    }
}

void highscore_state_draw(void) {
    char line[32];
    char scoreText[16];

    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("STATE 4: HIGHSCORES", 10, 3);

    for (u16 i = 0; i < 10; i++) {
        u32_to_str(highscores[i].score, scoreText);

        line[0] = (char)('0' + ((i + 1) / 10));
        line[1] = (char)('0' + ((i + 1) % 10));
        line[2] = '.';
        line[3] = ' ';
        line[4] = highscores[i].name[0];
        line[5] = highscores[i].name[1];
        line[6] = highscores[i].name[2];
        line[7] = ' ';
        line[8] = '-';
        line[9] = ' ';
        line[10] = '\0';

        strcat(line, scoreText);
        VDP_drawText(line, 10, (u16)(6 + i));
    }

    VDP_drawText("START/B/C: BACK TO TITLE", 8, 22);
}

void highscore_state_exit(void) {
}
