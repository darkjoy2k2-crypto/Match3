#include <genesis.h>

#include "globals.h"
#include "typedefs.h"
#include "sprite_quad_engine.h"

void gameover_state_enter(void) {
    nameCursor = 0;
    enteredName[0] = 'A';
    enteredName[1] = 'A';
    enteredName[2] = 'A';
    enteredName[3] = '\0';

    VDP_clearPlane(BG_A, TRUE);

    sprite_quad_engine_hide();
}

void gameover_state_update(void) {
    if (button_pressed(BUTTON_LEFT) && nameCursor > 0) {
        nameCursor--;
    }

    if (button_pressed(BUTTON_RIGHT) && nameCursor < 2) {
        nameCursor++;
    }

    if (button_pressed(BUTTON_UP)) {
        if (enteredName[nameCursor] < 'Z') enteredName[nameCursor]++;
        else enteredName[nameCursor] = 'A';
    }

    if (button_pressed(BUTTON_DOWN)) {
        if (enteredName[nameCursor] > 'A') enteredName[nameCursor]--;
        else enteredName[nameCursor] = 'Z';
    }

    if (button_pressed(BUTTON_START) || button_pressed(BUTTON_A) || button_pressed(BUTTON_C)) {
        insert_highscore(lastScore, enteredName);
        currentState = STATE_HIGHSCORE;
    }
}

void gameover_state_draw(void) {
    char scoreText[16];

    VDP_clearPlane(BG_A, TRUE);
    VDP_drawText("STATE 3: GAME OVER", 10, 7);

    u32_to_str(lastScore, scoreText);
    VDP_drawText("SCORE:", 12, 10);
    VDP_drawText(scoreText, 19, 10);

    VDP_drawText("ENTER 3-LETTER NAME", 9, 14);
    VDP_drawText(enteredName, 18, 16);

    if (nameCursor == 0) VDP_drawText("^", 18, 17);
    if (nameCursor == 1) VDP_drawText("^", 19, 17);
    if (nameCursor == 2) VDP_drawText("^", 20, 17);

    VDP_drawText("L/R: CURSOR  U/D: CHANGE", 6, 21);
    VDP_drawText("START/A/C: CONFIRM", 10, 23);
}

void gameover_state_exit(void) {
}
