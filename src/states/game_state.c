#include <genesis.h>

#include "resources.h"
#include "globals.h"
#include "typedefs.h"
#include "player_input.h"
#include "sprite_quad_engine.h"
#include "food_engine.h"
#include "states/game_state.h"

#define TILESET_BASE TILE_USER_INDEX

#define LOCK_MOVE_SPEED 3

static u16 lastDrawnSeconds = 0xFFFFu;

static LockModeState lockMode   = LOCK_NONE;
static u16 lockedCol            = 0;
static u16 lockedRow            = 0;
static u16 lockTargetCol        = 0;
static u16 lockTargetRow        = 0;
static s16 lockTargetX          = 0;
static s16 lockTargetY          = 0;
static s8  lockDirDx            = 0;
static s8  lockDirDy            = 0;
static u16 lockBlinkTick        = 0;
static Sprite* arrowSprite      = NULL;
static u8  lockArrowFrame       = 0;
static bool lockSwapConsumed    = FALSE;

static const char* bottleMaskRows[15] = {
    "00000001111110000000",
    "00000001111110000000",
    "00000111111111100000",
    "00011111111111111000",
    "00011111111111111000",
    "00111111111111111100",
    "00111111111111111100",
    "00111111111111111100",
    "00111111111111111100",
    "00111111111111111100",
    "00011111111111111000",
    "00011111111111111000",
    "00000111111111100000",
    "00000000000000000000",
    "00000000000000000000"
};

static bool is_walkable_cell(s16 col, s16 row) {
    if (col < 0 || col >= 20) return FALSE;
    if (row < 0 || row >= 15) return FALSE;
    return (bottleMaskRows[row][col] == '1');
}

void game_state_enter(void) {
    gameTicks = 0;
    timeLeftSeconds = 120;
    lastScore = 0;

    quadCol = 10;
    quadRow = 7;
    if (!is_walkable_cell((s16)quadCol, (s16)quadRow)) {
        quadCol = 10;
        quadRow = 4;
    }

    sync_quad_pos_from_cell();

    hold_repeat_input_init(&moveInput, MOVE_REPEAT_INITIAL_DELAY, MOVE_REPEAT_DELAY);

    VDP_setTextPalette(PAL3);
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    VDP_drawImageEx(
        BG_B,
        &border1,
        TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILESET_BASE),
        0,
        0,
        FALSE,
        TRUE
    );

    VDP_drawText("TIME", 33, 1);
    VDP_drawText("PUSH C TO SHAKE!!!", 11, 27);

    sprite_quad_engine_show(quadX, quadY);
    food_engine_init();
    lastDrawnSeconds = 0xFFFFu;

    lockMode          = LOCK_NONE;
    lockDirDx         = 0;
    lockDirDy         = 0;
    lockBlinkTick     = 0;
    lockSwapConsumed  = FALSE;
    lockedCol         = quadCol;
    lockedRow         = quadRow;

    arrowSprite = SPR_addSprite(&arrow_sprite, -128, -128,
                                TILE_ATTR(PAL1, FALSE, FALSE, FALSE));
    if (arrowSprite != NULL) {
        SPR_setAnim(arrowSprite, 0);
        SPR_setAutoAnimation(arrowSprite, FALSE);
        SPR_setFrame(arrowSprite, 0);
    }
}

void game_state_update(void) {
    bool aHeld     = (joyState     & BUTTON_A) != 0;
    bool aPressed  = button_pressed(BUTTON_A);
    bool aReleased = ((lastJoyState & BUTTON_A) != 0) && !aHeld;

    /* --- Lock mode enter --- */
    if (aPressed && lockMode == LOCK_NONE) {
        lockMode      = LOCK_ACTIVE;
        lockedCol     = quadCol;
        lockedRow     = quadRow;
        lockTargetCol = quadCol;
        lockTargetRow = quadRow;
        lockTargetX   = quadX;
        lockTargetY   = quadY;
        lockDirDx     = 0;
        lockDirDy     = 0;
        lockBlinkTick = 0;
    }

    /* --- Lock mode exit (A released) --- */
    if (aReleased && lockMode == LOCK_ACTIVE) {
        if (lockDirDx != 0 || lockDirDy != 0) {
            /* Commit swap: start animation */
            food_engine_start_swap_animation(lockedCol, lockedRow, lockTargetCol, lockTargetRow);
            quadCol = lockTargetCol;
            quadRow = lockTargetRow;
            sync_quad_pos_from_cell();
            lockSwapConsumed = TRUE;
        }
        /* else: nothing – hand stays at locked cell */
        lockMode  = LOCK_NONE;
        lockDirDx = 0;
        lockDirDy = 0;
        if (arrowSprite != NULL) SPR_setPosition(arrowSprite, -128, -128);
    }

    if (button_pressed(BUTTON_C)) {
        food_engine_trigger_shake();
    }

    if (lockMode == LOCK_NONE) {
        /* Block movement after swap until all direction buttons are released */
        if (lockSwapConsumed) {
            if (!(joyState & (BUTTON_LEFT | BUTTON_RIGHT | BUTTON_UP | BUTTON_DOWN))) {
                lockSwapConsumed = FALSE;
                hold_repeat_input_init(&moveInput, MOVE_REPEAT_INITIAL_DELAY, MOVE_REPEAT_DELAY);
            }
        } else {
            /* Normal cell-by-cell movement */
            u16 moveButton;
            s16 nextCol = (s16)quadCol;
            s16 nextRow = (s16)quadRow;

            moveButton = hold_repeat_input_update(&moveInput, joyState, lastJoyState);

            if (moveButton == BUTTON_LEFT)  nextCol--;
            if (moveButton == BUTTON_RIGHT) nextCol++;
            if (moveButton == BUTTON_UP)    nextRow--;
            if (moveButton == BUTTON_DOWN)  nextRow++;

            if (is_walkable_cell(nextCol, nextRow)) {
                quadCol = (u16)nextCol;
                quadRow = (u16)nextRow;
                sync_quad_pos_from_cell();
            }
        }
    } else { /* LOCK_ACTIVE */
        /* Continuously read held direction – target follows currently held button */
        s8 dx = 0;
        s8 dy = 0;
        u8 frame = lockArrowFrame;

        if      (joyState & BUTTON_LEFT)  { dx = -1; frame = 0; }
        else if (joyState & BUTTON_RIGHT) { dx =  1; frame = 2; }
        else if (joyState & BUTTON_UP)    { dy = -1; frame = 1; }
        else if (joyState & BUTTON_DOWN)  { dy =  1; frame = 3; }

        if (dx != lockDirDx || dy != lockDirDy) {
            if (dx == 0 && dy == 0) {
                /* Direction released: hand returns to locked cell */
                lockTargetCol = lockedCol;
                lockTargetRow = lockedRow;
                lockTargetX   = (s16)(lockedCol * QUAD_STEP + QUAD_OFFSET_X);
                lockTargetY   = (s16)(lockedRow * QUAD_STEP + QUAD_OFFSET_Y);
                lockDirDx     = 0;
                lockDirDy     = 0;
            } else {
                s16 tc = (s16)lockedCol + dx;
                s16 tr = (s16)lockedRow + dy;
                if (is_walkable_cell(tc, tr)) {
                    lockTargetCol  = (u16)tc;
                    lockTargetRow  = (u16)tr;
                    lockTargetX    = (s16)(tc * QUAD_STEP + QUAD_OFFSET_X);
                    lockTargetY    = (s16)(tr * QUAD_STEP + QUAD_OFFSET_Y);
                    lockDirDx      = dx;
                    lockDirDy      = dy;
                    lockArrowFrame = frame;
                }
                /* else: wall – keep current target, ignore this direction */
            }
        }

        /* Move hand 3 px/frame toward current target */
        s16 remX = lockTargetX - quadX;
        s16 remY = lockTargetY - quadY;

        if      (remX >  LOCK_MOVE_SPEED) quadX += LOCK_MOVE_SPEED;
        else if (remX < -LOCK_MOVE_SPEED) quadX -= LOCK_MOVE_SPEED;
        else                              quadX   = lockTargetX;

        if      (remY >  LOCK_MOVE_SPEED) quadY += LOCK_MOVE_SPEED;
        else if (remY < -LOCK_MOVE_SPEED) quadY -= LOCK_MOVE_SPEED;
        else                              quadY   = lockTargetY;

        /* Sync cell coords when hand arrives */
        if (quadX == lockTargetX && quadY == lockTargetY) {
            quadCol = lockTargetCol;
            quadRow = lockTargetRow;
        }

        /* Arrow blinks 1/frame on locked cell while direction is held */
        lockBlinkTick++;
        if (arrowSprite != NULL) {
            if ((lockDirDx != 0 || lockDirDy != 0) && (lockBlinkTick & 1u) == 0u) {
                SPR_setFrame(arrowSprite, lockArrowFrame);
                SPR_setPosition(arrowSprite,
                    (s16)(lockedCol * QUAD_STEP),
                    (s16)(lockedRow * QUAD_STEP));
            } else {
                SPR_setPosition(arrowSprite, -128, -128);
            }
        }
    }

    sprite_quad_engine_set_position(quadX, quadY);
    sprite_quad_engine_update();

    food_engine_update();

    gameTicks++;
    if (gameTicks >= 50u) {
        gameTicks = 0;
        if (timeLeftSeconds > 0) {
            timeLeftSeconds--;
        }
    }

    lastScore = (u32)(120 - timeLeftSeconds);

    if (timeLeftSeconds == 0) {
        currentState = STATE_GAMEOVER;
    }
}

void game_state_draw(void) {
    char timerText[8];

    if (timeLeftSeconds != lastDrawnSeconds) {
        VDP_clearTextArea(34, 2, 5, 1);
        format_timer(timerText, timeLeftSeconds);
        VDP_drawText(timerText, 34, 2);
        lastDrawnSeconds = timeLeftSeconds;
    }
}

void game_state_exit(void) {
    if (arrowSprite != NULL) SPR_setPosition(arrowSprite, -128, -128);
    food_engine_cleanup();
}
