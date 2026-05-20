#include <genesis.h>

#include "resources.h"
#include "globals.h"
#include "typedefs.h"
#include "food_engine.h"

#define FRUIT_TYPE_COUNT 64
#define GRID_WIDTH 20
#define GRID_HEIGHT 15
#define SPAWN_POOL_SIZE 5
#define TOP_ROW_SPAWN_COUNT 6
#define MATCH_FLY_MAX_SPRITES 48
#define MATCH_FLY_SPEED 8
#define MATCH_TARGET_X 0
#define MATCH_TARGET_Y 0
#define MATCH_FLY_FP_SHIFT 8
#define BOARD_INIT_MAX_RETRIES 120
#define CASCADE_MAX_STEPS 128
#define FRAME_GATE_INTERVAL 1
#define SLIDE_BUFFER_MAX (GRID_WIDTH * GRID_HEIGHT)

typedef enum {
    SWAP_ANIM_NONE,
    SWAP_ANIM_IN_PROGRESS
} SwapAnimState;

typedef struct MatchFlightSprite {
    Sprite* sprite;
    s16 x;
    s16 y;
    s32 xFp;
    s32 yFp;
    s16 targetX;
    s16 targetY;
    s32 velXFp;
    s32 velYFp;
    bool active;
} MatchFlightSprite;

static u16 foodBgTileBase = 0;
static bool pendingMatchCheck = FALSE;
static u32 simFrameCounter = 0;
static u16 movementFrameCounter = 0;
static bool boardDirty = TRUE;
static bool movementDetectedThisCheck = FALSE;
static bool shakeRequested = FALSE;
static u16 dutyCycleCounter = 0;
static bool forceGravityCheck = FALSE;
static bool forceMatchCheck = FALSE;
static SwapAnimState lastSwapAnimState = SWAP_ANIM_NONE;
static u16 lastGravityAnimCount = 0;

static s16 fruitGrid[GRID_HEIGHT][GRID_WIDTH];
static s16 nextFruitGrid[GRID_HEIGHT][GRID_WIDTH];
static s16 lastRenderedGrid[GRID_HEIGHT][GRID_WIDTH];
static s16 walkableMinCol[GRID_HEIGHT];
static s16 walkableMaxCol[GRID_HEIGHT];
static u8 matchMask[GRID_HEIGHT][GRID_WIDTH];
static u8 movedMask[GRID_HEIGHT][GRID_WIDTH];
static u8 rowSimCheckMask[GRID_HEIGHT];
static s16 prevSlideFromCol[SLIDE_BUFFER_MAX];
static s16 prevSlideFromRow[SLIDE_BUFFER_MAX];
static s16 prevSlideToCol[SLIDE_BUFFER_MAX];
static s16 prevSlideToRow[SLIDE_BUFFER_MAX];
static s16 currSlideFromCol[SLIDE_BUFFER_MAX];
static s16 currSlideFromRow[SLIDE_BUFFER_MAX];
static s16 currSlideToCol[SLIDE_BUFFER_MAX];
static s16 currSlideToRow[SLIDE_BUFFER_MAX];
static u16 prevSlideCount = 0;
static u16 currSlideCount = 0;
static u16 currentCellChecks = 0;
static u16 lastCellChecks = 0;

static u16 spawnPool[SPAWN_POOL_SIZE] = { 0, 1, 2, 3, 4 };
static const s16 topRowSpawnCols[TOP_ROW_SPAWN_COUNT] = { 7, 8, 9, 10, 11, 12 };
static u16 spawnBag[GRID_WIDTH][SPAWN_POOL_SIZE];
static u8 spawnBagPos[GRID_WIDTH];

static MatchFlightSprite matchFlightSprites[MATCH_FLY_MAX_SPRITES];
static u16 matchFlightActiveCount = 0;
static bool matchFlightInProgress = FALSE;

typedef struct {
    u16 spriteIndex;
    u16 srcCol;
    u16 srcRow;
    u16 dstCol;
    u16 dstRow;
    s16 type;
    bool active;
    bool stampErased;
} GravityAnimFruit;

static GravityAnimFruit gravityAnimFruits[MATCH_FLY_MAX_SPRITES];
static u16 gravityAnimCount = 0;

static SwapAnimState swapAnimState = SWAP_ANIM_NONE;
static u16 swapSprite1Index = 0xFFFFu;
static u16 swapSprite2Index = 0xFFFFu;
static u16 swapCol1 = 0;
static u16 swapRow1 = 0;
static u16 swapCol2 = 0;
static u16 swapRow2 = 0;
static s16 swapType1 = -1;
static s16 swapType2 = -1;
static u16 swapFrameCounter = 0;

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
    if (col < 0 || col >= GRID_WIDTH) return FALSE;
    if (row < 0 || row >= GRID_HEIGHT) return FALSE;
    return bottleMaskRows[row][col] == '1';
}

static void draw_fruit_to_bg(s16 col, s16 row, u16 type) {
    u16 base;
    u16 tileX;
    u16 tileY;

    if (col < 0 || col >= GRID_WIDTH) return;
    if (row < 0 || row >= GRID_HEIGHT) return;
    if (type >= FRUIT_TYPE_COUNT) return;

    base = foodBgTileBase + ((type >> 3) * 32) + ((type & 7) << 1);
    tileX = (u16)(col << 1);
    tileY = (u16)(row << 1);

    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, base + 0), tileX, tileY);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, base + 1), tileX + 1, tileY);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, base + 16), tileX, tileY + 1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL2, FALSE, FALSE, FALSE, base + 17), tileX + 1, tileY + 1);
}

static void erase_fruit_from_bg(s16 col, s16 row) {
    u16 tileX = (u16)(col << 1);
    u16 tileY = (u16)(row << 1);

    VDP_setTileMapXY(BG_A, 0, tileX, tileY);
    VDP_setTileMapXY(BG_A, 0, tileX + 1, tileY);
    VDP_setTileMapXY(BG_A, 0, tileX, tileY + 1);
    VDP_setTileMapXY(BG_A, 0, tileX + 1, tileY + 1);
}

static s16 grid_col_to_px(s16 col) {
    return (s16)(col << 4);
}

static s16 grid_row_to_px(s16 row) {
    return (s16)(row << 4);
}

static u32 isqrt_u32(u32 value) {
    u32 op = value;
    u32 res = 0;
    u32 one = 1UL << 30;

    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= (res + one)) {
            op -= (res + one);
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return res;
}

static void match_flight_reset_slots(void) {
    u16 i;

    for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
        matchFlightSprites[i].x = 0;
        matchFlightSprites[i].y = 0;
        matchFlightSprites[i].xFp = 0;
        matchFlightSprites[i].yFp = 0;
        matchFlightSprites[i].targetX = MATCH_TARGET_X;
        matchFlightSprites[i].targetY = MATCH_TARGET_Y;
        matchFlightSprites[i].velXFp = 0;
        matchFlightSprites[i].velYFp = 0;
        matchFlightSprites[i].active = FALSE;

        if (matchFlightSprites[i].sprite != NULL) {
            SPR_setPosition(matchFlightSprites[i].sprite, -128, -128);
        }
    }

    matchFlightActiveCount = 0;
    matchFlightInProgress = FALSE;
}

static MatchFlightSprite* match_flight_alloc_slot(void) {
    u16 i;

    for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
        if (matchFlightSprites[i].active) continue;
        return &matchFlightSprites[i];
    }

    return NULL;
}

static void match_flight_start_cell(s16 col, s16 row, s16 type) {
    u16 anim = (u16)(type >> 3);
    u16 frame = (u16)(type & 7);
    MatchFlightSprite* slot = match_flight_alloc_slot();
    s32 dx;
    s32 dy;
    u32 distSq;
    u32 dist;

    if (slot == NULL) return;

    if (slot->sprite == NULL) {
        slot->sprite = SPR_addSprite(
            &food_sprite_mold,
            grid_col_to_px(col),
            grid_row_to_px(row),
            TILE_ATTR(PAL2, FALSE, FALSE, FALSE)
        );
    }

    if (slot->sprite == NULL) return;

    SPR_setAnim(slot->sprite, anim);
    SPR_setAutoAnimation(slot->sprite, FALSE);
    SPR_setFrame(slot->sprite, frame);

    slot->x = grid_col_to_px(col);
    slot->y = grid_row_to_px(row);
    slot->xFp = ((s32)slot->x) << MATCH_FLY_FP_SHIFT;
    slot->yFp = ((s32)slot->y) << MATCH_FLY_FP_SHIFT;
    slot->targetX = MATCH_TARGET_X;
    slot->targetY = MATCH_TARGET_Y;
    slot->velXFp = 0;
    slot->velYFp = 0;

    dx = (s32)slot->targetX - (s32)slot->x;
    dy = (s32)slot->targetY - (s32)slot->y;
    distSq = (u32)((dx * dx) + (dy * dy));
    dist = isqrt_u32(distSq);

    if (dist > 0u) {
        slot->velXFp = ((dx * MATCH_FLY_SPEED) << MATCH_FLY_FP_SHIFT) / (s32)dist;
        slot->velYFp = ((dy * MATCH_FLY_SPEED) << MATCH_FLY_FP_SHIFT) / (s32)dist;
    }

    if ((slot->velXFp == 0) && (dx != 0)) {
        slot->velXFp = (dx > 0) ? 1 : -1;
    }
    if ((slot->velYFp == 0) && (dy != 0)) {
        slot->velYFp = (dy > 0) ? 1 : -1;
    }

    slot->active = TRUE;

    SPR_setPosition(slot->sprite, slot->x, slot->y);

    matchFlightActiveCount++;
    matchFlightInProgress = TRUE;
}

static void update_match_flight(void) {
    u16 i;

    if (!matchFlightInProgress) return;

    for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
        MatchFlightSprite* slot = &matchFlightSprites[i];
        s32 remainingX;
        s32 remainingY;
        s32 stepXFp;
        s32 stepYFp;

        if (!slot->active) continue;

        remainingX = (((s32)slot->targetX) << MATCH_FLY_FP_SHIFT) - slot->xFp;
        remainingY = (((s32)slot->targetY) << MATCH_FLY_FP_SHIFT) - slot->yFp;

        stepXFp = slot->velXFp;
        stepYFp = slot->velYFp;

        if ((stepXFp > 0 && stepXFp > remainingX) || (stepXFp < 0 && stepXFp < remainingX)) {
            stepXFp = remainingX;
        }
        if ((stepYFp > 0 && stepYFp > remainingY) || (stepYFp < 0 && stepYFp < remainingY)) {
            stepYFp = remainingY;
        }

        slot->xFp += stepXFp;
        slot->yFp += stepYFp;

        slot->x = (s16)(slot->xFp >> MATCH_FLY_FP_SHIFT);
        slot->y = (s16)(slot->yFp >> MATCH_FLY_FP_SHIFT);

        SPR_setPosition(slot->sprite, slot->x, slot->y);

        if ((slot->x != slot->targetX) || (slot->y != slot->targetY)) continue;

        slot->active = FALSE;
        SPR_setPosition(slot->sprite, -128, -128);

        if (matchFlightActiveCount > 0) {
            matchFlightActiveCount--;
        }
    }

    if (matchFlightActiveCount == 0) {
        matchFlightInProgress = FALSE;
    }
}

static u16 rngState = 0x1234u;

static u16 random_u16(void) {
    rngState ^= (rngState << 7);
    rngState ^= (rngState >> 9);
    rngState ^= (u16)(gameTicks + 1u);
    return rngState;
}

static void init_spawn_pool(void) {
    u16 poolIndex;

    for (poolIndex = 0; poolIndex < SPAWN_POOL_SIZE; poolIndex++) {
        while (TRUE) {
            u16 candidate = (u16)(random_u16() % FRUIT_TYPE_COUNT);
            bool alreadyUsed = FALSE;
            u16 prevIndex;

            for (prevIndex = 0; prevIndex < poolIndex; prevIndex++) {
                if (spawnPool[prevIndex] != candidate) continue;
                alreadyUsed = TRUE;
                break;
            }

            if (alreadyUsed) continue;

            spawnPool[poolIndex] = candidate;
            break;
        }
    }
}

static void refill_spawn_bag_for_col(s16 col) {
    u16 i;

    for (i = 0; i < SPAWN_POOL_SIZE; i++) {
        spawnBag[col][i] = spawnPool[i];
    }

    for (i = SPAWN_POOL_SIZE - 1; i > 0; i--) {
        u16 j = (u16)(random_u16() % (i + 1));
        u16 tmp = spawnBag[col][i];
        spawnBag[col][i] = spawnBag[col][j];
        spawnBag[col][j] = tmp;
    }

    spawnBagPos[col] = 0;
}

static u16 next_spawn_fruit_for_col(s16 col) {
    if (spawnBagPos[col] >= SPAWN_POOL_SIZE) {
        refill_spawn_bag_for_col(col);
    }

    return spawnBag[col][spawnBagPos[col]++];
}

static bool top_spawn_would_create_match(s16 col, s16 type) {
    s16 left2 = col - 2;
    s16 left1 = col - 1;
    s16 right1 = col + 1;
    s16 right2 = col + 2;

    if (is_walkable_cell(left2, 0) && is_walkable_cell(left1, 0)
        && fruitGrid[0][left2] == type && fruitGrid[0][left1] == type) {
        return TRUE;
    }

    if (is_walkable_cell(left1, 0) && is_walkable_cell(right1, 0)
        && fruitGrid[0][left1] == type && fruitGrid[0][right1] == type) {
        return TRUE;
    }

    if (is_walkable_cell(right1, 0) && is_walkable_cell(right2, 0)
        && fruitGrid[0][right1] == type && fruitGrid[0][right2] == type) {
        return TRUE;
    }

    /* Also avoid immediate vertical triple starting at top row when possible. */
    if (is_walkable_cell(col, 1) && is_walkable_cell(col, 2)
        && fruitGrid[1][col] == type && fruitGrid[2][col] == type) {
        return TRUE;
    }

    return FALSE;
}

static s16 choose_top_col_for_type(s16 type) {
    s16 slotIndex;
    s16 fallbackCol = -1;

    for (slotIndex = 0; slotIndex < TOP_ROW_SPAWN_COUNT; slotIndex++) {
        s16 col = topRowSpawnCols[slotIndex];

        if (!is_walkable_cell(col, 0)) continue;
        if (fruitGrid[0][col] >= 0) continue;

        if (fallbackCol < 0) {
            fallbackCol = col;
        }

        if (!top_spawn_would_create_match(col, type)) {
            return col;
        }
    }

    return fallbackCol;
}

static s16 choose_cleanup_spawn_type(void) {
    u16 counts[FRUIT_TYPE_COUNT];
    s16 row;
    s16 bestType = -1;
    u16 bestCount = 0xFFFFu;

    for (row = 0; row < FRUIT_TYPE_COUNT; row++) {
        counts[row] = 0;
    }

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 col;

        if (rowMin < 0) continue;

        for (col = rowMin; col <= rowMax; col++) {
            s16 type;

            if (!is_walkable_cell(col, row)) continue;

            type = fruitGrid[row][col];
            if (type < 0 || type >= FRUIT_TYPE_COUNT) continue;

            counts[type]++;
        }
    }

    for (row = 0; row < FRUIT_TYPE_COUNT; row++) {
        u16 c = counts[row];

        if (c == 0 || c >= 3) continue;

        if (c < bestCount) {
            bestCount = c;
            bestType = row;
        }
    }

    return bestType;
}

static void init_spawn_bags(void) {
    s16 col;

    for (col = 0; col < GRID_WIDTH; col++) {
        refill_spawn_bag_for_col(col);
    }
}

static void init_walkable_ranges(void) {
    s16 row;

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 col;
        s16 minCol = -1;
        s16 maxCol = -1;

        for (col = 0; col < GRID_WIDTH; col++) {
            if (!is_walkable_cell(col, row)) continue;

            if (minCol < 0) minCol = col;
            maxCol = col;
        }

        walkableMinCol[row] = minCol;
        walkableMaxCol[row] = maxCol;
    }
}

static void clear_grid_rows(s16 grid[GRID_HEIGHT][GRID_WIDTH], s16 value, s16 firstRow, s16 lastRow) {
    s16 row;
    s16 col;

    if (firstRow < 0) firstRow = 0;
    if (lastRow >= GRID_HEIGHT) lastRow = GRID_HEIGHT - 1;
    if (firstRow > lastRow) return;

    for (row = firstRow; row <= lastRow; row++) {
        for (col = 0; col < GRID_WIDTH; col++) {
            grid[row][col] = value;
        }
    }
}

static void clear_u8_grid(u8 grid[GRID_HEIGHT][GRID_WIDTH], u8 value) {
    s16 row;
    s16 col;

    for (row = 0; row < GRID_HEIGHT; row++) {
        for (col = 0; col < GRID_WIDTH; col++) {
            grid[row][col] = value;
        }
    }
}

static bool is_reverse_slide_blocked(s16 srcCol, s16 srcRow, s16 dstCol, s16 dstRow) {
    u16 i;

    for (i = 0; i < prevSlideCount; i++) {
        if (prevSlideFromCol[i] != dstCol) continue;
        if (prevSlideFromRow[i] != dstRow) continue;
        if (prevSlideToCol[i] != srcCol) continue;
        if (prevSlideToRow[i] != srcRow) continue;
        return TRUE;
    }

    return FALSE;
}

static void food_engine_start_gravity_animation(u16 srcCol, u16 srcRow, u16 dstCol, u16 dstRow, s16 type) {
    u16 anim;
    u16 frame;
    MatchFlightSprite* slot;
    s32 dx;
    s32 dy;
    u32 distSq;
    u32 dist;
    u16 i;

    if (type < 0 || type >= FRUIT_TYPE_COUNT) return;

    slot = match_flight_alloc_slot();
    if (slot == NULL) return;

    anim = (u16)(type >> 3);
    frame = (u16)(type & 7);

    /* Step 2: Create sprite at source position */
    s16 startX = grid_col_to_px((s16)srcCol);
    s16 startY = grid_row_to_px((s16)srcRow);
    s16 targetX = grid_col_to_px((s16)dstCol);
    s16 targetY = grid_row_to_px((s16)dstRow);

    slot->x = startX;
    slot->y = startY;
    slot->xFp = ((s32)slot->x) << MATCH_FLY_FP_SHIFT;
    slot->yFp = ((s32)slot->y) << MATCH_FLY_FP_SHIFT;
    slot->targetX = targetX;
    slot->targetY = targetY;
    slot->velXFp = 0;
    slot->velYFp = 0;

    dx = (s32)targetX - (s32)startX;
    dy = (s32)targetY - (s32)startY;
    distSq = (u32)((dx * dx) + (dy * dy));
    dist = isqrt_u32(distSq);

    if (dist > 0u) {
        slot->velXFp = ((dx * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
        slot->velYFp = ((dy * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
    }

    if ((slot->velXFp == 0) && (dx != 0)) {
        slot->velXFp = (dx > 0) ? 1 : -1;
    }
    if ((slot->velYFp == 0) && (dy != 0)) {
        slot->velYFp = (dy > 0) ? 1 : -1;
    }

    slot->sprite = SPR_addSprite(
        &food_sprite_mold,
        startX,
        startY,
        TILE_ATTR(PAL2, FALSE, FALSE, TRUE)
    );

    if (slot->sprite != NULL) {
        SPR_setAnim(slot->sprite, anim);
        SPR_setAutoAnimation(slot->sprite, FALSE);
        SPR_setFrame(slot->sprite, frame);
        SPR_setHFlip(slot->sprite, FALSE);
        SPR_setVFlip(slot->sprite, FALSE);
        SPR_setPosition(slot->sprite, startX, startY);
    }

    slot->active = TRUE;

    /* Prevent render_grid_diff from drawing at destination while animating */
    lastRenderedGrid[dstRow][dstCol] = -1;

    i = (u16)(slot - matchFlightSprites);
    if (gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
        gravityAnimFruits[gravityAnimCount].spriteIndex = i;
        gravityAnimFruits[gravityAnimCount].srcCol = srcCol;
        gravityAnimFruits[gravityAnimCount].srcRow = srcRow;
        gravityAnimFruits[gravityAnimCount].dstCol = dstCol;
        gravityAnimFruits[gravityAnimCount].dstRow = dstRow;
        gravityAnimFruits[gravityAnimCount].type = type;
        gravityAnimFruits[gravityAnimCount].active = TRUE;
        gravityAnimFruits[gravityAnimCount].stampErased = FALSE;
        gravityAnimCount++;
    }

    matchFlightActiveCount++;
    matchFlightInProgress = TRUE;
}

static void update_gravity_animations(void) {
    u16 i;
    u16 writePos = 0;

    for (i = 0; i < gravityAnimCount; i++) {
        if (!gravityAnimFruits[i].active) continue;

        u16 spriteIdx = gravityAnimFruits[i].spriteIndex;
        if (spriteIdx >= MATCH_FLY_MAX_SPRITES) {
            gravityAnimFruits[i].active = FALSE;
            continue;
        }

        MatchFlightSprite* slot = &matchFlightSprites[spriteIdx];

        /* On first sprite movement, erase stamp from source position */
        if (!gravityAnimFruits[i].stampErased) {
            erase_fruit_from_bg((s16)gravityAnimFruits[i].srcCol, (s16)gravityAnimFruits[i].srcRow);
            lastRenderedGrid[gravityAnimFruits[i].srcRow][gravityAnimFruits[i].srcCol] = -1;
            gravityAnimFruits[i].stampErased = TRUE;
        }

        if (!slot->active) {
            /* Step 4: Sprite animation complete - stamp fruit at destination on bg_a */
            u16 dstCol = gravityAnimFruits[i].dstCol;
            u16 dstRow = gravityAnimFruits[i].dstRow;

            if (fruitGrid[dstRow][dstCol] >= 0) {
                draw_fruit_to_bg((s16)dstCol, (s16)dstRow, (u16)fruitGrid[dstRow][dstCol]);
                lastRenderedGrid[dstRow][dstCol] = fruitGrid[dstRow][dstCol];
            }

            /* Step 5: Remove sprite */
            if (slot->sprite != NULL) {
                SPR_setPosition(slot->sprite, -128, -128);
                SPR_releaseSprite(slot->sprite);
                slot->sprite = NULL;
            }

            gravityAnimFruits[i].active = FALSE;
            continue;
        }

        /* Step 3: Sprite still moving */
        gravityAnimFruits[writePos++] = gravityAnimFruits[i];
    }

    gravityAnimCount = writePos;
}

static void record_slide(s16 srcCol, s16 srcRow, s16 dstCol, s16 dstRow) {
    if (currSlideCount >= SLIDE_BUFFER_MAX) return;

    currSlideFromCol[currSlideCount] = srcCol;
    currSlideFromRow[currSlideCount] = srcRow;
    currSlideToCol[currSlideCount] = dstCol;
    currSlideToRow[currSlideCount] = dstRow;
    currSlideCount++;
}

static void commit_slide_buffer(void) {
    u16 i;

    prevSlideCount = currSlideCount;
    for (i = 0; i < currSlideCount; i++) {
        prevSlideFromCol[i] = currSlideFromCol[i];
        prevSlideFromRow[i] = currSlideFromRow[i];
        prevSlideToCol[i] = currSlideToCol[i];
        prevSlideToRow[i] = currSlideToRow[i];
    }
}


static bool try_place(s16 srcCol, s16 srcRow, s16 dstCol, s16 dstRow, s16 type) {
    bool isSlide = (srcCol != dstCol);
    bool isGravity = (srcRow != dstRow);

    if (!is_walkable_cell(dstCol, dstRow)) return FALSE;
    if (nextFruitGrid[dstRow][dstCol] >= 0) return FALSE;
    if (isSlide && is_reverse_slide_blocked(srcCol, srcRow, dstCol, dstRow)) return FALSE;

    if (isGravity && type >= 0 && gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
        food_engine_start_gravity_animation((u16)srcCol, (u16)srcRow, (u16)dstCol, (u16)dstRow, type);
    }

    nextFruitGrid[dstRow][dstCol] = type;
    movedMask[dstRow][dstCol] = 1;
    if ((srcCol != dstCol) || (srcRow != dstRow)) {
        boardDirty = TRUE;
        movementDetectedThisCheck = TRUE;
    }
    if (isSlide) {
        record_slide(srcCol, srcRow, dstCol, dstRow);
    }
    (void)srcCol;
    (void)srcRow;
    return TRUE;
}

static bool row_has_gap(s16 row) {
    s16 rowMin = walkableMinCol[row];
    s16 rowMax = walkableMaxCol[row];
    s16 col;

    if (rowMin < 0) return FALSE;

    for (col = rowMin; col <= rowMax; col++) {
        if (!is_walkable_cell(col, row)) continue;
        if (fruitGrid[row][col] < 0) return TRUE;
    }

    return FALSE;
}

static bool first_row_has_empty_cells(void) {
    s16 rowMin = walkableMinCol[0];
    s16 rowMax = walkableMaxCol[0];
    s16 col;

    if (rowMin < 0) return FALSE;

    for (col = rowMin; col <= rowMax; col++) {
        if (!is_walkable_cell(col, 0)) continue;
        if (fruitGrid[0][col] < 0) return TRUE;
    }

    return FALSE;
}

static void update_row_sim_check_mask(void) {
    s16 row;
    bool enableFromHere = FALSE;

    for (row = GRID_HEIGHT - 1; row >= 0; row--) {
        if (walkableMinCol[row] < 0) {
            rowSimCheckMask[row] = 0;
            continue;
        }

        if (!enableFromHere && row_has_gap(row)) {
            enableFromHere = TRUE;
        }

        rowSimCheckMask[row] = enableFromHere ? 1 : 0;
    }
}

static bool center_compact_sparse_rows(void) {
    s16 row;
    bool anyMoved = FALSE;

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 width;
        s16 col;
        s16 values[GRID_WIDTH];
        s16 count = 0;
        s16 writeStart;
        s16 writeEnd;
        s16 writePos;

        if (rowMin < 0) continue;

        width = (s16)(rowMax - rowMin + 1);
        if (width <= 0) continue;

        for (col = rowMin; col <= rowMax; col++) {
            s16 type;

            if (!is_walkable_cell(col, row)) continue;
            type = fruitGrid[row][col];
            if (type < 0) continue;

            values[count++] = type;
        }

        if (count <= 1) continue;

        if (count > 5) continue;

        writeStart = (s16)(rowMin + ((width - count) >> 1));
        writeEnd = (s16)(writeStart + count - 1);
        writePos = 0;

        for (col = rowMin; col <= rowMax; col++) {
            s16 newValue = -1;

            if (!is_walkable_cell(col, row)) continue;

            if (col >= writeStart && col <= writeEnd) {
                newValue = values[writePos++];
            }

            if (fruitGrid[row][col] == newValue) continue;

            fruitGrid[row][col] = newValue;
            anyMoved = TRUE;
        }
    }

    if (anyMoved) {
        boardDirty = TRUE;
        movementDetectedThisCheck = TRUE;
    }

    return anyMoved;
}

static bool simulate_food_grid(void) {
    s16 row;
    bool straightFallDetected = FALSE;
    bool diagonalFallDetected = FALSE;

    movementDetectedThisCheck = FALSE;
    currSlideCount = 0;
    update_row_sim_check_mask();

    clear_grid_rows(nextFruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_u8_grid(movedMask, 0);

    /* Pass 1: Try straight down gravity only */
    for (row = GRID_HEIGHT - 1; row >= 0; row--) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 col;

        if (rowMin < 0) continue;

        if (!rowSimCheckMask[row]) {
            for (col = rowMin; col <= rowMax; col++) {
                if (!is_walkable_cell(col, row)) continue;
                nextFruitGrid[row][col] = fruitGrid[row][col];
            }
            continue;
        }

        for (col = rowMin; col <= rowMax; col++) {
            s16 type = fruitGrid[row][col];

            currentCellChecks++;

            if (type >= 0) {
                s16 belowRow = row + 1;

                /* Priority 1: Straight down (gravity) with animation */
                if (is_walkable_cell(col, belowRow)
                    && fruitGrid[belowRow][col] < 0
                    && nextFruitGrid[belowRow][col] < 0) {
                    (void)try_place(col, row, col, belowRow, type);
                    straightFallDetected = TRUE;
                    movedMask[row][col] = 1;  /* Mark as moved in this pass */
                } else {
                    /* Mark for potential diagonal in pass 2 */
                    nextFruitGrid[row][col] = type;
                }
            }
        }
    }

    /* Pass 2: Try diagonal falls only if no straight falls detected */
    if (!straightFallDetected) {
        for (row = GRID_HEIGHT - 1; row >= 0; row--) {
            s16 rowMin = walkableMinCol[row];
            s16 rowMax = walkableMaxCol[row];
            s16 col;

            if (rowMin < 0) continue;

            for (col = rowMin; col <= rowMax; col++) {
                s16 type = fruitGrid[row][col];

                if (type >= 0 && nextFruitGrid[row][col] == type) {
                    /* This fruit hasn't moved yet, try diagonal */
                    s16 belowRow = row + 1;
                    bool placed = FALSE;

                    /* Try down-left: right neighbor must be blocked */
                    s16 diag_col_left = col - 1;
                    bool rightBlocked = (!is_walkable_cell(col + 1, row) || fruitGrid[row][col + 1] >= 0);
                    if (rightBlocked
                        && is_walkable_cell(diag_col_left, belowRow)
                        && fruitGrid[belowRow][diag_col_left] < 0
                        && nextFruitGrid[belowRow][diag_col_left] < 0
                        && gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
                        food_engine_start_gravity_animation((u16)col, (u16)row, (u16)diag_col_left, (u16)belowRow, type);
                        nextFruitGrid[belowRow][diag_col_left] = type;
                        boardDirty = TRUE;
                        movementDetectedThisCheck = TRUE;
                        diagonalFallDetected = TRUE;
                        movedMask[row][col] = 1;  /* Mark as moved in this pass */
                        placed = TRUE;
                    }

                    /* Try down-right if down-left didn't work */
                    if (!placed) {
                        s16 diag_col_right = col + 1;
                        bool leftBlocked = (!is_walkable_cell(col - 1, row) || fruitGrid[row][col - 1] >= 0);
                        if (leftBlocked
                            && is_walkable_cell(diag_col_right, belowRow)
                            && fruitGrid[belowRow][diag_col_right] < 0
                            && nextFruitGrid[belowRow][diag_col_right] < 0
                            && gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
                            food_engine_start_gravity_animation((u16)col, (u16)row, (u16)diag_col_right, (u16)belowRow, type);
                            nextFruitGrid[belowRow][diag_col_right] = type;
                            boardDirty = TRUE;
                            movementDetectedThisCheck = TRUE;
                            diagonalFallDetected = TRUE;
                            movedMask[row][col] = 1;  /* Mark as moved in this pass */
                        }
                    }
                }
            }
        }
    }

    /* Pass 3: Try slides only if fruit moved in Pass 1 or 2 (not after another slide) */
    if (!straightFallDetected && !diagonalFallDetected) {
        for (row = GRID_HEIGHT - 1; row >= 0; row--) {
            s16 rowMin = walkableMinCol[row];
            s16 rowMax = walkableMaxCol[row];
            s16 col;

            if (rowMin < 0) continue;

            for (col = rowMin; col <= rowMax; col++) {
                s16 type = nextFruitGrid[row][col];

                /* Check fruits in nextFruitGrid that moved in Pass 1 or 2 */
                if (type >= 0 && movedMask[row][col] == 1) {
                    /* This fruit just moved (gravity or diagonal), try to slide it */
                    s16 belowRow = row + 1;

                    /* Check if ANY diagonal is possible */
                    bool diag_left_possible = FALSE;
                    bool diag_right_possible = FALSE;

                    s16 diag_col_left = col - 1;
                    bool rightBlocked = (!is_walkable_cell(col + 1, row) || nextFruitGrid[row][col + 1] >= 0);
                    if (rightBlocked
                        && is_walkable_cell(diag_col_left, belowRow)
                        && nextFruitGrid[belowRow][diag_col_left] < 0) {
                        diag_left_possible = TRUE;
                    }

                    s16 diag_col_right = col + 1;
                    bool leftBlocked = (!is_walkable_cell(col - 1, row) || nextFruitGrid[row][col - 1] >= 0);
                    if (leftBlocked
                        && is_walkable_cell(diag_col_right, belowRow)
                        && nextFruitGrid[belowRow][diag_col_right] < 0) {
                        diag_right_possible = TRUE;
                    }

                    /* Only slide if NO diagonal is possible */
                    if (!diag_left_possible && !diag_right_possible) {
                        /* Only slide if gravity is blocked */
                        bool gravityBlocked = !is_walkable_cell(col, belowRow)
                                            || nextFruitGrid[belowRow][col] >= 0;

                        if (gravityBlocked) {
                            bool placed = FALSE;

                            /* Try slide left first */
                            s16 slide_col_left = col - 1;
                            if (is_walkable_cell(slide_col_left, row)
                                && nextFruitGrid[row][slide_col_left] < 0
                                && gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
                                food_engine_start_gravity_animation((u16)col, (u16)row, (u16)slide_col_left, (u16)row, type);
                                nextFruitGrid[row][slide_col_left] = type;
                                nextFruitGrid[row][col] = -1;
                                boardDirty = TRUE;
                                movementDetectedThisCheck = TRUE;
                                placed = TRUE;
                            }

                            /* Try slide right if left didn't work */
                            if (!placed) {
                                s16 slide_col_right = col + 1;
                                if (is_walkable_cell(slide_col_right, row)
                                    && nextFruitGrid[row][slide_col_right] < 0
                                    && gravityAnimCount < MATCH_FLY_MAX_SPRITES) {
                                    food_engine_start_gravity_animation((u16)col, (u16)row, (u16)slide_col_right, (u16)row, type);
                                    nextFruitGrid[row][slide_col_right] = type;
                                    nextFruitGrid[row][col] = -1;
                                    boardDirty = TRUE;
                                    movementDetectedThisCheck = TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    clear_grid_rows(fruitGrid, -1, 0, GRID_HEIGHT - 1);

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 col;

        if (rowMin < 0) continue;

        for (col = rowMin; col <= rowMax; col++) {
            if (!is_walkable_cell(col, row)) continue;
            fruitGrid[row][col] = nextFruitGrid[row][col];
        }
    }

    commit_slide_buffer();
    simFrameCounter++;
    return movementDetectedThisCheck;
}

static u16 matchCheckPhase = 0; /* 0=horizontal, 1=vertical */
static u16 matchCheckCounter = 0; /* Frame counter for phase timing */

static bool mark_horizontal_matches(void) {
    s16 row;
    bool anyMatched = FALSE;

    clear_u8_grid(matchMask, 0);

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 col;

        if (rowMin < 0) continue;
        col = rowMin;

        while (col <= rowMax) {
            s16 runStart;
            s16 runLen;
            s16 type;

            if (!is_walkable_cell(col, row) || fruitGrid[row][col] < 0) {
                col++;
                continue;
            }

            runStart = col;
            runLen = 1;
            type = fruitGrid[row][col];
            col++;

            while (col <= rowMax && is_walkable_cell(col, row) && fruitGrid[row][col] == type) {
                runLen++;
                col++;
            }

            if (runLen >= 3) {
                s16 markCol;
                for (markCol = runStart; markCol < (runStart + runLen); markCol++) {
                    matchMask[row][markCol] = 1;
                }
                anyMatched = TRUE;
            }
        }
    }

    return anyMatched;
}

static bool mark_vertical_matches(void) {
    s16 col;
    bool anyMatched = FALSE;

    clear_u8_grid(matchMask, 0);

    for (col = 0; col < GRID_WIDTH; col++) {
        s16 row = 0;

        while (row < GRID_HEIGHT) {
            s16 runStart;
            s16 runLen;
            s16 type;

            if (!is_walkable_cell(col, row) || fruitGrid[row][col] < 0) {
                row++;
                continue;
            }

            runStart = row;
            runLen = 1;
            type = fruitGrid[row][col];
            row++;

            while (row < GRID_HEIGHT && is_walkable_cell(col, row) && fruitGrid[row][col] == type) {
                runLen++;
                row++;
            }

            if (runLen >= 3) {
                s16 markRow;
                for (markRow = runStart; markRow < (runStart + runLen); markRow++) {
                    matchMask[markRow][col] = 1;
                }
                anyMatched = TRUE;
            }
        }
    }

    return anyMatched;
}

static bool mark_all_matches(void) {
    if (matchCheckPhase == 0) {
        return mark_horizontal_matches();
    } else {
        return mark_vertical_matches();
    }
}

static bool remove_marked_cells(void) {
    s16 row;
    bool anyRemoved = FALSE;

    for (row = 0; row < GRID_HEIGHT; row++) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 col;

        if (rowMin < 0) continue;

        for (col = rowMin; col <= rowMax; col++) {
            if (!is_walkable_cell(col, row)) continue;
            if (!matchMask[row][col]) continue;

            currentCellChecks++;

            if (fruitGrid[row][col] >= 0) {
                match_flight_start_cell(col, row, fruitGrid[row][col]);
            }

            fruitGrid[row][col] = -1;
            anyRemoved = TRUE;
        }
    }

    return anyRemoved;
}

static bool resolve_board_cascade(void) {
    bool anyResolved = FALSE;

    if (mark_all_matches()) {
        anyResolved = remove_marked_cells();
    }

    if (anyResolved) {
        s16 row;
        for (row = 0; row < GRID_HEIGHT; row++) {
            rowSimCheckMask[row] = 0;
        }
        movementFrameCounter = 0;
        boardDirty = TRUE;
        pendingMatchCheck = TRUE;

        /* Switch phase after match resolved */
        matchCheckPhase = (matchCheckPhase == 0) ? 1 : 0;
        matchCheckCounter = 0;
    }

    return anyResolved;
}

static void initialize_board_without_matches(void) {
    s16 row;
    s16 col;
    u16 retries = 0;

    for (row = 0; row < GRID_HEIGHT; row++) {
        for (col = 0; col < GRID_WIDTH; col++) {
            if (!is_walkable_cell(col, row)) {
                fruitGrid[row][col] = -1;
                continue;
            }

            fruitGrid[row][col] = (s16)next_spawn_fruit_for_col(col);
        }
    }

    while (retries < BOARD_INIT_MAX_RETRIES) {
        bool changed = FALSE;

        if (!mark_all_matches()) {
            return;
        }

        for (row = 0; row < GRID_HEIGHT; row++) {
            for (col = 0; col < GRID_WIDTH; col++) {
                if (!matchMask[row][col]) continue;
                fruitGrid[row][col] = (s16)next_spawn_fruit_for_col(col);
                changed = TRUE;
            }
        }

        if (!changed) {
            return;
        }

        retries++;
    }
}

static void render_grid_diff(void) {
    u16 row;
    u16 col;

    for (row = 0; row < GRID_HEIGHT; row++) {
        for (col = 0; col < GRID_WIDTH; col++) {
            s16 oldValue;
            s16 newValue;

            currentCellChecks++;

            if (!is_walkable_cell((s16)col, (s16)row)) {
                if (lastRenderedGrid[row][col] >= 0) {
                    erase_fruit_from_bg((s16)col, (s16)row);
                    lastRenderedGrid[row][col] = -1;
                }
                continue;
            }

            oldValue = lastRenderedGrid[row][col];
            newValue = fruitGrid[row][col];

            if (oldValue == newValue) continue;

            if (oldValue >= 0 && newValue < 0) {
                erase_fruit_from_bg((s16)col, (s16)row);
            }
            if (newValue >= 0) {
                draw_fruit_to_bg((s16)col, (s16)row, (u16)newValue);
            }

            lastRenderedGrid[row][col] = newValue;
        }
    }
}

static void update_swap_animation(void) {
    if (swapAnimState == SWAP_ANIM_NONE) return;
    if (swapSprite1Index >= MATCH_FLY_MAX_SPRITES) return;

    bool isMove = (swapSprite2Index == 0xFFFFu);

    MatchFlightSprite* slot1 = &matchFlightSprites[swapSprite1Index];
    MatchFlightSprite* slot2 = isMove ? NULL : &matchFlightSprites[swapSprite2Index];

    /* Check if animation(s) completed */
    bool slot1Done = !slot1->active;
    bool slot2Done = isMove ? TRUE : !slot2->active;

    if (slot1Done && slot2Done) {
        /* Animation complete: stamp fruits on background at new positions */
        lastRenderedGrid[swapRow1][swapCol1] = -1;
        if (!isMove) {
            lastRenderedGrid[swapRow2][swapCol2] = -1;
        }

        /* Stamp new positions with fruits */
        if (fruitGrid[swapRow1][swapCol1] >= 0) {
            draw_fruit_to_bg((s16)swapCol1, (s16)swapRow1, (u16)fruitGrid[swapRow1][swapCol1]);
        }
        if (fruitGrid[swapRow2][swapCol2] >= 0) {
            draw_fruit_to_bg((s16)swapCol2, (s16)swapRow2, (u16)fruitGrid[swapRow2][swapCol2]);
        }

        /* Move sprites to default off-screen position */
        if (slot1->sprite != NULL) {
            SPR_setPosition(slot1->sprite, -128, -128);
            SPR_releaseSprite(slot1->sprite);
            slot1->sprite = NULL;
        }
        if (!isMove && slot2->sprite != NULL) {
            SPR_setPosition(slot2->sprite, -128, -128);
            SPR_releaseSprite(slot2->sprite);
            slot2->sprite = NULL;
        }

        pendingMatchCheck = TRUE;
        boardDirty = TRUE;
        swapAnimState = SWAP_ANIM_NONE;
        swapSprite1Index = 0xFFFFu;
        swapSprite2Index = 0xFFFFu;
        swapFrameCounter = 0;
    }
}

void food_engine_start_swap_animation(u16 col1, u16 row1, u16 col2, u16 row2) {
    s16 type1;
    s16 type2;
    s32 dx;
    s32 dy;
    u32 distSq;
    u32 dist;
    MatchFlightSprite* slot1 = NULL;
    MatchFlightSprite* slot2 = NULL;
    u16 i;

    /* Validate input */
    if (!is_walkable_cell((s16)col1, (s16)row1) || !is_walkable_cell((s16)col2, (s16)row2)) return;
    if (swapAnimState == SWAP_ANIM_IN_PROGRESS) return;

    type1 = fruitGrid[row1][col1];
    type2 = fruitGrid[row2][col2];

    /* Both empty - nothing to swap */
    if (type1 < 0 && type2 < 0) return;

    /* Swap grid immediately */
    s16 tmp = fruitGrid[row1][col1];
    fruitGrid[row1][col1] = fruitGrid[row2][col2];
    fruitGrid[row2][col2] = tmp;

    /* Erase from background */
    erase_fruit_from_bg((s16)col1, (s16)row1);
    erase_fruit_from_bg((s16)col2, (s16)row2);

    swapCol1 = col1;
    swapRow1 = row1;
    swapCol2 = col2;
    swapRow2 = row2;
    swapType1 = type1;
    swapType2 = type2;
    swapSprite1Index = 0xFFFFu;
    swapSprite2Index = 0xFFFFu;

    /* Only animate fruits, not empty spaces */
    if (type1 >= 0) {
        slot1 = match_flight_alloc_slot();
        if (slot1 != NULL) {
            slot1->active = TRUE;
            matchFlightActiveCount++;

            s16 startX1 = grid_col_to_px((s16)col1);
            s16 startY1 = grid_row_to_px((s16)row1);
            s16 targetX1 = grid_col_to_px((s16)col2);
            s16 targetY1 = grid_row_to_px((s16)row2);

            slot1->x = startX1;
            slot1->y = startY1;
            slot1->xFp = ((s32)slot1->x) << MATCH_FLY_FP_SHIFT;
            slot1->yFp = ((s32)slot1->y) << MATCH_FLY_FP_SHIFT;
            slot1->targetX = targetX1;
            slot1->targetY = targetY1;

            dx = (s32)targetX1 - (s32)startX1;
            dy = (s32)targetY1 - (s32)startY1;
            distSq = (u32)((dx * dx) + (dy * dy));
            dist = isqrt_u32(distSq);

            if (dist > 0u) {
                slot1->velXFp = ((dx * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
                slot1->velYFp = ((dy * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
            } else {
                slot1->velXFp = 0;
                slot1->velYFp = 0;
            }

            slot1->sprite = SPR_addSprite(
                &food_sprite_mold,
                startX1,
                startY1,
                TILE_ATTR(PAL2, FALSE, FALSE, TRUE)
            );

            if (slot1->sprite != NULL) {
                u16 anim = (u16)(type1 >> 3);
                u16 frame = (u16)(type1 & 7);
                SPR_setAnim(slot1->sprite, anim);
                SPR_setAutoAnimation(slot1->sprite, FALSE);
                SPR_setFrame(slot1->sprite, frame);
                SPR_setHFlip(slot1->sprite, FALSE);
                SPR_setVFlip(slot1->sprite, FALSE);
                SPR_setPosition(slot1->sprite, startX1, startY1);
            }

            for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
                if (&matchFlightSprites[i] == slot1) swapSprite1Index = i;
            }
        }
    }

    if (type2 >= 0) {
        slot2 = match_flight_alloc_slot();
        if (slot2 != NULL) {
            slot2->active = TRUE;
            matchFlightActiveCount++;

            s16 startX2 = grid_col_to_px((s16)col2);
            s16 startY2 = grid_row_to_px((s16)row2);
            s16 targetX2 = grid_col_to_px((s16)col1);
            s16 targetY2 = grid_row_to_px((s16)row1);

            slot2->x = startX2;
            slot2->y = startY2;
            slot2->xFp = ((s32)slot2->x) << MATCH_FLY_FP_SHIFT;
            slot2->yFp = ((s32)slot2->y) << MATCH_FLY_FP_SHIFT;
            slot2->targetX = targetX2;
            slot2->targetY = targetY2;

            dx = (s32)targetX2 - (s32)startX2;
            dy = (s32)targetY2 - (s32)startY2;
            distSq = (u32)((dx * dx) + (dy * dy));
            dist = isqrt_u32(distSq);

            if (dist > 0u) {
                slot2->velXFp = ((dx * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
                slot2->velYFp = ((dy * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
            } else {
                slot2->velXFp = 0;
                slot2->velYFp = 0;
            }

            slot2->sprite = SPR_addSprite(
                &food_sprite_mold,
                startX2,
                startY2,
                TILE_ATTR(PAL2, FALSE, FALSE, TRUE)
            );

            if (slot2->sprite != NULL) {
                u16 anim = (u16)(type2 >> 3);
                u16 frame = (u16)(type2 & 7);
                SPR_setAnim(slot2->sprite, anim);
                SPR_setAutoAnimation(slot2->sprite, FALSE);
                SPR_setFrame(slot2->sprite, frame);
                SPR_setHFlip(slot2->sprite, FALSE);
                SPR_setVFlip(slot2->sprite, FALSE);
                SPR_setPosition(slot2->sprite, startX2, startY2);
            }

            for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
                if (&matchFlightSprites[i] == slot2) swapSprite2Index = i;
            }
        }
    }

    matchFlightInProgress = TRUE;
    swapFrameCounter = 0;
    swapAnimState = SWAP_ANIM_IN_PROGRESS;
}

bool food_engine_is_swap_animating(void) {
    return swapAnimState == SWAP_ANIM_IN_PROGRESS;
}

void food_engine_start_move_animation(u16 col1, u16 row1, u16 col2, u16 row2) {
    s16 type1;
    s16 type2;
    s32 dx;
    s32 dy;
    u32 distSq;
    u32 dist;
    MatchFlightSprite* slot1 = NULL;
    u16 i;

    if (!is_walkable_cell((s16)col1, (s16)row1) || !is_walkable_cell((s16)col2, (s16)row2)) return;
    if (swapAnimState == SWAP_ANIM_IN_PROGRESS) return;

    type1 = fruitGrid[row1][col1];
    type2 = fruitGrid[row2][col2];

    /* Move requires exactly one fruit and one empty space */
    if ((type1 < 0 && type2 < 0) || (type1 >= 0 && type2 >= 0)) return;

    /* Determine which position has the fruit */
    s16 fruitCol = (type1 >= 0) ? (s16)col1 : (s16)col2;
    s16 fruitRow = (type1 >= 0) ? (s16)row1 : (s16)row2;
    s16 emptyCol = (type1 >= 0) ? (s16)col2 : (s16)col1;
    s16 emptyRow = (type1 >= 0) ? (s16)row2 : (s16)row1;
    s16 fruitType = (type1 >= 0) ? type1 : type2;

    /* Update grid immediately */
    fruitGrid[emptyRow][emptyCol] = fruitType;
    fruitGrid[fruitRow][fruitCol] = -1;

    /* Erase from background */
    erase_fruit_from_bg(fruitCol, fruitRow);

    /* Allocate sprite slot */
    slot1 = match_flight_alloc_slot();
    if (slot1 == NULL) return;

    slot1->active = TRUE;
    matchFlightActiveCount++;

    s16 startX = grid_col_to_px(fruitCol);
    s16 startY = grid_row_to_px(fruitRow);
    s16 targetX = grid_col_to_px(emptyCol);
    s16 targetY = grid_row_to_px(emptyRow);

    slot1->x = startX;
    slot1->y = startY;
    slot1->xFp = ((s32)slot1->x) << MATCH_FLY_FP_SHIFT;
    slot1->yFp = ((s32)slot1->y) << MATCH_FLY_FP_SHIFT;
    slot1->targetX = targetX;
    slot1->targetY = targetY;

    dx = (s32)targetX - (s32)startX;
    dy = (s32)targetY - (s32)startY;
    distSq = (u32)((dx * dx) + (dy * dy));
    dist = isqrt_u32(distSq);

    if (dist > 0u) {
        slot1->velXFp = ((dx * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
        slot1->velYFp = ((dy * 2) << MATCH_FLY_FP_SHIFT) / (s32)dist;
    } else {
        slot1->velXFp = 0;
        slot1->velYFp = 0;
    }

    slot1->sprite = SPR_addSprite(
        &food_sprite_mold,
        startX,
        startY,
        TILE_ATTR(PAL2, FALSE, FALSE, TRUE)
    );

    if (slot1->sprite != NULL) {
        u16 anim = (u16)(fruitType >> 3);
        u16 frame = (u16)(fruitType & 7);
        SPR_setAnim(slot1->sprite, anim);
        SPR_setAutoAnimation(slot1->sprite, FALSE);
        SPR_setFrame(slot1->sprite, frame);
        SPR_setHFlip(slot1->sprite, FALSE);
        SPR_setVFlip(slot1->sprite, FALSE);
        SPR_setPosition(slot1->sprite, startX, startY);
    }

    /* Use swap animation tracking for move as well */
    for (i = 0; i < MATCH_FLY_MAX_SPRITES; i++) {
        if (&matchFlightSprites[i] == slot1) swapSprite1Index = i;
    }
    swapSprite2Index = 0xFFFFu;

    /* Store positions: swapCol1/swapRow1 = old (empty), swapCol2/swapRow2 = new (fruit) */
    swapCol1 = (u16)fruitCol;
    swapRow1 = (u16)fruitRow;
    swapCol2 = (u16)emptyCol;
    swapRow2 = (u16)emptyRow;
    swapType1 = -1;           /* Old position becomes empty */
    swapType2 = fruitType;    /* New position gets fruit */

    matchFlightInProgress = TRUE;
    swapFrameCounter = 0;
    swapAnimState = SWAP_ANIM_IN_PROGRESS;
}

void food_engine_init(void) {
    clear_grid_rows(fruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_grid_rows(nextFruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_grid_rows(lastRenderedGrid, -1, 0, GRID_HEIGHT - 1);

    init_walkable_ranges();
    init_spawn_pool();
    init_spawn_bags();
    initialize_board_without_matches();
    match_flight_reset_slots();

    currentCellChecks = 0;
    lastCellChecks = 0;
    pendingMatchCheck = FALSE;
    simFrameCounter = 0;
    movementFrameCounter = 0;
    boardDirty = TRUE;
    movementDetectedThisCheck = FALSE;
    shakeRequested = FALSE;
    prevSlideCount = 0;
    currSlideCount = 0;

    swapAnimState = SWAP_ANIM_NONE;
    swapSprite1Index = 0xFFFFu;
    swapSprite2Index = 0xFFFFu;
    swapCol1 = 0;
    swapRow1 = 0;
    swapCol2 = 0;
    swapRow2 = 0;
    swapType1 = -1;
    swapType2 = -1;

    gravityAnimCount = 0;

    foodBgTileBase = TILE_USER_INDEX + border1.tileset->numTile;
    VDP_loadTileSet(&food_tiles, foodBgTileBase, DMA);
    VDP_waitDMACompletion();
}

void food_engine_update(void) {
    currentCellChecks = 0;
    movementDetectedThisCheck = FALSE;

    /* Detect animation completion: swap or gravity */
    bool swapJustCompleted = (lastSwapAnimState == SWAP_ANIM_IN_PROGRESS && swapAnimState == SWAP_ANIM_NONE);
    bool gravityJustCompleted = (lastGravityAnimCount > 0 && gravityAnimCount == 0);

    if (swapJustCompleted || gravityJustCompleted) {
        forceGravityCheck = TRUE;
    }

    lastSwapAnimState = swapAnimState;
    lastGravityAnimCount = gravityAnimCount;

    /* If swap is animating, update sprites but skip game logic */
    if (swapAnimState == SWAP_ANIM_IN_PROGRESS) {
        if (matchFlightActiveCount > 0) {
            update_match_flight();
        }
        if (gravityAnimCount > 0) {
            update_gravity_animations();
        }
        update_swap_animation();
        if (boardDirty) {
            render_grid_diff();
            boardDirty = FALSE;
        }
        lastCellChecks = currentCellChecks;
        return;
    }

    /* If gravity animations are in progress, update them but skip game logic */
    if (gravityAnimCount > 0) {
        if (matchFlightActiveCount > 0) {
            update_match_flight();
        }
        update_gravity_animations();
        lastCellChecks = currentCellChecks;
        return;
    }

    /* Only update sprites if they're active */
    if (matchFlightActiveCount > 0) {
        update_match_flight();
    }

    /* Force immediate match detection after move/swap settles */
    if (forceMatchCheck) {
        bool horizontalFound = FALSE;
        bool verticalFound = FALSE;

        /* Check horizontal matches */
        matchCheckPhase = 0;
        horizontalFound = mark_horizontal_matches();
        if (horizontalFound) {
            if (remove_marked_cells()) {
                /* Matches removed, trigger gravity cascade */
                forceGravityCheck = TRUE;
                movementFrameCounter = 0;
                boardDirty = TRUE;
                matchCheckPhase = 1;
                forceMatchCheck = FALSE;
                if (boardDirty && gravityAnimCount == 0) {
                    render_grid_diff();
                    boardDirty = FALSE;
                }
                lastCellChecks = currentCellChecks;
                return;
            }
        }

        /* Check vertical matches */
        matchCheckPhase = 1;
        verticalFound = mark_vertical_matches();
        if (verticalFound) {
            if (remove_marked_cells()) {
                /* Matches removed, trigger gravity cascade */
                forceGravityCheck = TRUE;
                movementFrameCounter = 0;
                boardDirty = TRUE;
                matchCheckPhase = 0;
                forceMatchCheck = FALSE;
                if (boardDirty && gravityAnimCount == 0) {
                    render_grid_diff();
                    boardDirty = FALSE;
                }
                lastCellChecks = currentCellChecks;
                return;
            }
        }

        /* No matches found, allow spawn and return to duty cycle */
        forceMatchCheck = FALSE;
        /* Spawn only when first row has empty cells and no animations pending */
        if (first_row_has_empty_cells() && gravityAnimCount == 0 && matchFlightActiveCount == 0) {
            food_engine_spawn_random();
        }
        if (boardDirty && gravityAnimCount == 0) {
            render_grid_diff();
            boardDirty = FALSE;
        }
        lastCellChecks = currentCellChecks;
        return;
    }

    /* Force immediate gravity check after animation completion or match resolution */
    if (forceGravityCheck) {
        movementFrameCounter++;
        if (movementFrameCounter >= FRAME_GATE_INTERVAL) {
            movementFrameCounter = 0;
            (void)simulate_food_grid();
            if (shakeRequested) {
                (void)center_compact_sparse_rows();
                shakeRequested = FALSE;
            }
        }

        /* If gravity detected movement, continue forcing until it settles */
        if (!movementDetectedThisCheck) {
            forceGravityCheck = FALSE;
            forceMatchCheck = TRUE;  /* Trigger full match check after gravity settles */
        }

        if (boardDirty && gravityAnimCount == 0) {
            render_grid_diff();
            boardDirty = FALSE;
        }
        lastCellChecks = currentCellChecks;
        return;
    }

    /* Normal duty cycle operation */
    dutyCycleCounter++;
    if (dutyCycleCounter >= 50) dutyCycleCounter = 0;

    if (dutyCycleCounter == 12) {
        /* Gravity simulation check */
        movementFrameCounter++;
        if (movementFrameCounter >= FRAME_GATE_INTERVAL) {
            movementFrameCounter = 0;
            if (simulate_food_grid()) {
                /* Movement detected, force immediate re-check next frame */
                forceGravityCheck = TRUE;
            }
            if (shakeRequested) {
                (void)center_compact_sparse_rows();
                shakeRequested = FALSE;
            }
        }
    }

    if (dutyCycleCounter == 25) {
        /* Horizontal match-3 detection */
        if (pendingMatchCheck && gravityAnimCount == 0) {
            matchCheckPhase = 0;
            if (resolve_board_cascade()) {
                /* Match resolved, force immediate gravity check */
                forceGravityCheck = TRUE;
            }
        }
    }

    if (dutyCycleCounter == 0 || dutyCycleCounter == 50) {
        /* Vertical match-3 detection (tick 50 wraps to 0) */
        if (pendingMatchCheck && gravityAnimCount == 0) {
            matchCheckPhase = 1;
            if (resolve_board_cascade()) {
                /* Match resolved, force immediate gravity check */
                forceGravityCheck = TRUE;
            }
        }
    }

    /* Spawn only when first row has empty cells - during normal duty cycle only */
    if (dutyCycleCounter == 0 && first_row_has_empty_cells() && gravityAnimCount == 0 && matchFlightActiveCount == 0) {
        food_engine_spawn_random();
    }

    if (boardDirty && gravityAnimCount == 0) {
        render_grid_diff();
        boardDirty = FALSE;
    }
    lastCellChecks = currentCellChecks;
}

void food_engine_draw(void) {
}

void food_engine_spawn_random(void) {
    s16 slotIndex;
    bool spawned = FALSE;

    if (currentGameMode == GAME_MODE_CLEANUP) {
        s16 typeToReplenish = choose_cleanup_spawn_type();

        if (typeToReplenish >= 0) {
            s16 spawnCol = choose_top_col_for_type(typeToReplenish);

            if (spawnCol >= 0) {
                fruitGrid[0][spawnCol] = typeToReplenish;
                spawned = TRUE;
            }
        }

        if (spawned) {
            pendingMatchCheck = TRUE;
            boardDirty = TRUE;
        }
        return;
    }

    for (slotIndex = 0; slotIndex < TOP_ROW_SPAWN_COUNT; slotIndex++) {
        s16 col = topRowSpawnCols[slotIndex];
        s16 chosenType = -1;
        s16 fallbackType = -1;
        u16 attempt;

        if (!is_walkable_cell(col, 0)) continue;
        if (fruitGrid[0][col] >= 0) continue;

        for (attempt = 0; attempt < (SPAWN_POOL_SIZE << 1); attempt++) {
            s16 candidate = (s16)next_spawn_fruit_for_col(col);

            if (attempt == 0) {
                fallbackType = candidate;
            }

            if (top_spawn_would_create_match(col, candidate)) {
                continue;
            }

            chosenType = candidate;
            break;
        }

        if (chosenType < 0) {
            chosenType = fallbackType;
        }

        fruitGrid[0][col] = chosenType;
        spawned = TRUE;
    }

    if (spawned) {
        pendingMatchCheck = TRUE;
        boardDirty = TRUE;
    }
}

void food_engine_cleanup(void) {
    clear_grid_rows(fruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_grid_rows(nextFruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_grid_rows(lastRenderedGrid, -1, 0, GRID_HEIGHT - 1);

    currentCellChecks = 0;
    lastCellChecks = 0;
    pendingMatchCheck = FALSE;
    simFrameCounter = 0;
    movementFrameCounter = 0;
    dutyCycleCounter = 0;
    forceGravityCheck = FALSE;
    forceMatchCheck = FALSE;
    lastSwapAnimState = SWAP_ANIM_NONE;
    lastGravityAnimCount = 0;
    boardDirty = TRUE;
    movementDetectedThisCheck = FALSE;
    shakeRequested = FALSE;
    prevSlideCount = 0;
    currSlideCount = 0;

    swapAnimState = SWAP_ANIM_NONE;
    swapSprite1Index = 0xFFFFu;
    swapSprite2Index = 0xFFFFu;

    gravityAnimCount = 0;

    match_flight_reset_slots();
}

s16 food_engine_get_cell(u16 col, u16 row) {
    if (col >= GRID_WIDTH || row >= GRID_HEIGHT) return -1;
    return fruitGrid[row][col];
}

u16 food_engine_get_last_cell_checks(void) {
    return lastCellChecks;
}

void food_engine_swap_cells(u16 col1, u16 row1, u16 col2, u16 row2) {
    s16 tmp;

    if (!is_walkable_cell((s16)col1, (s16)row1)) return;
    if (!is_walkable_cell((s16)col2, (s16)row2)) return;

    if ((col1 == col2 && ((row1 + 1u) == row2 || (row2 + 1u) == row1))
        || (row1 == row2 && ((col1 + 1u) == col2 || (col2 + 1u) == col1))) {
        tmp = fruitGrid[row1][col1];
        fruitGrid[row1][col1] = fruitGrid[row2][col2];
        fruitGrid[row2][col2] = tmp;

        pendingMatchCheck = TRUE;
        boardDirty = TRUE;
    }
}

void food_engine_trigger_shake(void) {
    shakeRequested = TRUE;
}
