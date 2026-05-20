#include <genesis.h>

#include "resources.h"
#include "globals.h"
#include "state_machine.h"

int main(bool hardReset) {
    (void)hardReset;

    JOY_init();
    SPR_init();
    VDP_setScreenWidth320();
    VDP_setScreenHeight240();

    VDP_loadFont(&font_default, CPU);
    PAL_setPalette(PAL0, border1.palette->data, CPU);
    PAL_setPalette(PAL1, player_quad.palette->data, CPU);
    PAL_setPalette(PAL2, food_sprite_mold.palette->data, CPU);
    PAL_setPalette(PAL3, textPaletteData, CPU);
    VDP_setTextPalette(PAL3);

    state_machine_init();
    SYS_showFrameLoad(TRUE);

    while (1) {
        state_machine_update();
    }

    return 0;
}
