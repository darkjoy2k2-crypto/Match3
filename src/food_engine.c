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
#define FRAME_GATE_INTERVAL 5
#define SLIDE_BUFFER_MAX (GRID_WIDTH * GRID_HEIGHT)

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

    if (!is_walkable_cell(dstCol, dstRow)) return FALSE;
    if (nextFruitGrid[dstRow][dstCol] >= 0) return FALSE;
    if (isSlide && is_reverse_slide_blocked(srcCol, srcRow, dstCol, dstRow)) return FALSE;

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
    bool leftFirst = (simFrameCounter & 1u) == 0u;

    movementDetectedThisCheck = FALSE;
    currSlideCount = 0;
    update_row_sim_check_mask();

    clear_grid_rows(nextFruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_u8_grid(movedMask, 0);

    for (row = GRID_HEIGHT - 1; row >= 0; row--) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 startCol;
        s16 endCol;
        s16 step;
        s16 col;

        if (rowMin < 0) continue;

        if (!rowSimCheckMask[row]) {
            for (col = rowMin; col <= rowMax; col++) {
                if (!is_walkable_cell(col, row)) continue;
                nextFruitGrid[row][col] = fruitGrid[row][col];
            }
            continue;
        }

        if (leftFirst) {
            startCol = rowMin;
            endCol = rowMax;
            step = 1;
        } else {
            startCol = rowMax;
            endCol = rowMin;
            step = -1;
        }

        col = startCol;
        while (TRUE) {
            s16 type = fruitGrid[row][col];

            currentCellChecks++;

            if (type >= 0) {
                s16 belowRow = row + 1;

                if (is_walkable_cell(col, belowRow)
                    && fruitGrid[belowRow][col] < 0
                    && nextFruitGrid[belowRow][col] < 0) {
                    (void)try_place(col, row, col, belowRow, type);
                } else {
                    s16 leftCol = col - 1;
                    s16 rightCol = col + 1;
                    bool downLeftFree = is_walkable_cell(leftCol, belowRow)
                        && fruitGrid[belowRow][leftCol] < 0
                        && nextFruitGrid[belowRow][leftCol] < 0;
                    bool downRightFree = is_walkable_cell(rightCol, belowRow)
                        && fruitGrid[belowRow][rightCol] < 0
                        && nextFruitGrid[belowRow][rightCol] < 0;

                    if (downLeftFree && downRightFree) {
                        if (leftFirst) {
                            if (!try_place(col, row, leftCol, belowRow, type)) {
                                (void)try_place(col, row, rightCol, belowRow, type);
                            }
                        } else {
                            if (!try_place(col, row, rightCol, belowRow, type)) {
                                (void)try_place(col, row, leftCol, belowRow, type);
                            }
                        }
                    } else if (downLeftFree) {
                        (void)try_place(col, row, leftCol, belowRow, type);
                    } else if (downRightFree) {
                        (void)try_place(col, row, rightCol, belowRow, type);
                    } else {
                        (void)try_place(col, row, col, row, type);
                    }
                }
            }

            if (col == endCol) break;
            col += step;
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

    clear_grid_rows(nextFruitGrid, -1, 0, GRID_HEIGHT - 1);
    clear_u8_grid(movedMask, 0);

    for (row = GRID_HEIGHT - 1; row >= 0; row--) {
        s16 rowMin = walkableMinCol[row];
        s16 rowMax = walkableMaxCol[row];
        s16 startCol;
        s16 endCol;
        s16 step;
        s16 col;

        if (rowMin < 0) continue;

        if (!rowSimCheckMask[row]) {
            for (col = rowMin; col <= rowMax; col++) {
                if (!is_walkable_cell(col, row)) continue;
                nextFruitGrid[row][col] = fruitGrid[row][col];
            }
            continue;
        }

        if (leftFirst) {
            startCol = rowMin;
            endCol = rowMax;
            step = 1;
        } else {
            startCol = rowMax;
            endCol = rowMin;
            step = -1;
        }

        col = startCol;
        while (TRUE) {
            s16 type = fruitGrid[row][col];

            currentCellChecks++;

            if (type >= 0) {
                s16 belowRow = row + 1;
                s16 leftCol = col - 1;
                s16 rightCol = col + 1;
                bool belowWalkable = is_walkable_cell(col, belowRow);
                bool belowFruit = belowWalkable && (fruitGrid[belowRow][col] >= 0);
                bool leftWalkable = is_walkable_cell(leftCol, row);
                bool rightWalkable = is_walkable_cell(rightCol, row);
                bool leftBlocked = leftWalkable && (fruitGrid[row][leftCol] >= 0);
                bool rightBlocked = rightWalkable && (fruitGrid[row][rightCol] >= 0);
                bool leftFree = leftWalkable
                    && (fruitGrid[row][leftCol] < 0)
                    && (nextFruitGrid[row][leftCol] < 0);
                bool rightFree = rightWalkable
                    && (fruitGrid[row][rightCol] < 0)
                    && (nextFruitGrid[row][rightCol] < 0);
                bool downLeftFree = is_walkable_cell(leftCol, belowRow)
                    && (fruitGrid[belowRow][leftCol] < 0)
                    && (nextFruitGrid[belowRow][leftCol] < 0);
                bool downRightFree = is_walkable_cell(rightCol, belowRow)
                    && (fruitGrid[belowRow][rightCol] < 0)
                    && (nextFruitGrid[belowRow][rightCol] < 0);

                if (belowFruit && leftBlocked && rightFree) {
                    if (downRightFree) {
                        (void)try_place(col, row, rightCol, belowRow, type);
                    } else if (row != 0) {
                        (void)try_place(col, row, rightCol, row, type);
                    } else {
                        (void)try_place(col, row, col, row, type);
                    }
                } else if (belowFruit && rightBlocked && leftFree) {
                    if (downLeftFree) {
                        (void)try_place(col, row, leftCol, belowRow, type);
                    } else if (row != 0) {
                        (void)try_place(col, row, leftCol, row, type);
                    } else {
                        (void)try_place(col, row, col, row, type);
                    }
                } else if (downLeftFree && downRightFree) {
                    if (leftFirst) {
                        if (!try_place(col, row, leftCol, belowRow, type)) {
                            (void)try_place(col, row, rightCol, belowRow, type);
                        }
                    } else {
                        if (!try_place(col, row, rightCol, belowRow, type)) {
                            (void)try_place(col, row, leftCol, belowRow, type);
                        }
                    }
                } else if (downLeftFree) {
                    (void)try_place(col, row, leftCol, belowRow, type);
                } else if (downRightFree) {
                    (void)try_place(col, row, rightCol, belowRow, type);
                } else {
                    (void)try_place(col, row, col, row, type);
                }
            }

            if (col == endCol) break;
            col += step;
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

static bool mark_all_matches(void) {
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

    {
        s16 col;
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
    }

    return anyMatched;
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

    foodBgTileBase = TILE_USER_INDEX + border1.tileset->numTile;
    VDP_loadTileSet(&food_tiles, foodBgTileBase, DMA);
    VDP_waitDMACompletion();
}

void food_engine_update(void) {
    bool movementCheckRan = FALSE;
    bool gravityMovedThisCheck = FALSE;

    currentCellChecks = 0;
    movementFrameCounter++;

    update_match_flight();

    if (movementFrameCounter >= FRAME_GATE_INTERVAL) {
        movementFrameCounter = 0;
        gravityMovedThisCheck = simulate_food_grid();
        if (shakeRequested) {
            (void)center_compact_sparse_rows();
            shakeRequested = FALSE;
        }
        movementCheckRan = TRUE;
        food_engine_spawn_random();
    }

    if (pendingMatchCheck && movementCheckRan && !gravityMovedThisCheck) {
        pendingMatchCheck = FALSE;
        (void)resolve_board_cascade();
    }

    if (boardDirty) {
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
    boardDirty = TRUE;
    movementDetectedThisCheck = FALSE;
    shakeRequested = FALSE;
    prevSlideCount = 0;
    currSlideCount = 0;
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
