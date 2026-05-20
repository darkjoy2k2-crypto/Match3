#include "state_machine.h"

#include "globals.h"
#include "typedefs.h"
#include "states/title_state.h"
#include "states/game_state.h"
#include "states/gameover_state.h"
#include "states/highscore_state.h"

static const StateDefinition stateTable[STATE_COUNT] = {
    { title_state_enter, title_state_update, title_state_draw, title_state_exit },
    { game_state_enter, game_state_update, game_state_draw, game_state_exit },
    { gameover_state_enter, gameover_state_update, gameover_state_draw, gameover_state_exit },
    { highscore_state_enter, highscore_state_update, highscore_state_draw, highscore_state_exit }
};

void state_machine_init(void) {
    currentState = STATE_TITLE;
    lastState = STATE_NONE;
    joyState = JOY_readJoypad(JOY_1);
    lastJoyState = joyState;
}

void state_machine_update(void) {
    joyState = JOY_readJoypad(JOY_1);

    if (currentState != lastState) {
        if (lastState != STATE_NONE && stateTable[lastState].exit != NULL) {
            stateTable[lastState].exit();
        }

        if (stateTable[currentState].enter != NULL) {
            stateTable[currentState].enter();
        }

        lastState = currentState;
    }

    if (stateTable[currentState].update != NULL) {
        stateTable[currentState].update();
    }

    if (stateTable[currentState].draw != NULL) {
        stateTable[currentState].draw();
    }

    lastJoyState = joyState;
    SPR_update();
    SYS_doVBlankProcess();
}
