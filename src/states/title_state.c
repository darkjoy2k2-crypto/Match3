#include <genesis.h>

#include "globals.h"
#include "typedefs.h"
#include "sprite_quad_engine.h"

static u16 selectedMode = 0;

static void draw_mode_line(void) {
    VDP_clearTextArea(9, 14, 22, 1);

    if (selectedMode == 0u) {
        VDP_drawText("> ENDLESS   CLEANUP", 9, 14);
    } else {
        VDP_drawText("  ENDLESS > CLEANUP", 9, 14);
    }
}

void title_state_enter(void) {
    PAL_setColor(0, 0x000);
    VDP_clearPlane(BG_A, TRUE);

    sprite_quad_engine_hide();

    VDP_drawText("MATCH-3 PROTOTYPE", 10, 8);
    VDP_drawText("STATE 1: TITLE", 12, 11);
    VDP_drawText("MODE", 16, 13);
    draw_mode_line();
    VDP_drawText("START = PLAY", 12, 16);
    VDP_drawText("C = HIGHSCORES", 12, 18);
}

void title_state_update(void) {
    if (button_pressed(BUTTON_LEFT) || button_pressed(BUTTON_UP)) {
        selectedMode = 0;
        draw_mode_line();
    }

    if (button_pressed(BUTTON_RIGHT) || button_pressed(BUTTON_DOWN)) {
        selectedMode = 1;
        draw_mode_line();
    }

    if (button_pressed(BUTTON_START)) {
        currentGameMode = (selectedMode == 0u) ? GAME_MODE_ENDLESS : GAME_MODE_CLEANUP;
        currentState = STATE_GAME;
        return;
    }

    if (button_pressed(BUTTON_C)) {
        currentState = STATE_HIGHSCORE;
        return;
    }
}

void title_state_draw(void) {
}

void title_state_exit(void) {
}
