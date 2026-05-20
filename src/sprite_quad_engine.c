#include <genesis.h>

#include "resources.h"

#define QUAD_ANIM_FRAME_COUNT 1
#define QUAD_ANIM_STEP_TICKS 12

typedef struct QuadSpriteEngine {
    Sprite* sprite;
    u16 animTick;
    u16 animFrame;
} QuadSpriteEngine;

static QuadSpriteEngine quadEngine = { NULL, 0, 0 };

void sprite_quad_engine_show(s16 x, s16 y) {
    if (quadEngine.sprite == NULL) {
        quadEngine.sprite = SPR_addSprite(&player_quad, x, y, TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
        if (quadEngine.sprite == NULL) return;

        SPR_setAnim(quadEngine.sprite, 0);
        SPR_setAutoAnimation(quadEngine.sprite, FALSE);
    }

    quadEngine.animTick = 0;
    quadEngine.animFrame = 0;
    SPR_setFrame(quadEngine.sprite, 0);
    SPR_setPosition(quadEngine.sprite, x, y);
}

void sprite_quad_engine_hide(void) {
    if (quadEngine.sprite != NULL) {
        SPR_setPosition(quadEngine.sprite, -32, -32);
    }
}

void sprite_quad_engine_set_position(s16 x, s16 y) {
    if (quadEngine.sprite != NULL) {
        SPR_setPosition(quadEngine.sprite, x, y);
    }
}

void sprite_quad_engine_update(void) {
    if (quadEngine.sprite == NULL) return;

    quadEngine.animTick++;
    if (quadEngine.animTick >= QUAD_ANIM_STEP_TICKS) {
        quadEngine.animTick = 0;
        quadEngine.animFrame++;
        if (quadEngine.animFrame >= QUAD_ANIM_FRAME_COUNT) {
            quadEngine.animFrame = 0;
        }
        SPR_setFrame(quadEngine.sprite, quadEngine.animFrame);
    }
}
