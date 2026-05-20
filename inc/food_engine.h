#pragma once

#include <genesis.h>

/**
 * @brief Initialize the food engine
 */
void food_engine_init(void);

/**
 * @brief Update the grid-based food simulation and BG_A diff rendering
 */
void food_engine_update(void);

/**
 * @brief Reserved draw hook for the food engine
 */
void food_engine_draw(void);

/**
 * @brief Spawn a random fruit at the top of the bottle
 */
void food_engine_spawn_random(void);

/**
 * @brief Cleanup the food simulation and reset the grid
 */
void food_engine_cleanup(void);

/**
 * @brief Get fruit type at grid position (-1 if empty)
 * @param col Grid column (0-19)
 * @param row Grid row (0-14)
 * @return Fruit type (0-5) or -1 if empty
 */
s16 food_engine_get_cell(u16 col, u16 row);

/**
 * @brief Get number of cell checks performed in the last frame
 * @return Cell check count
 */
u16 food_engine_get_last_cell_checks(void);

/**
 * @brief Start a swap animation between two adjacent fruits
 * @param col1 Column of first cell
 * @param row1 Row of first cell
 * @param col2 Column of second cell
 * @param row2 Row of second cell
 * @note Pauses gravity/slide during animation. Call only for adjacent walkable cells.
 */
void food_engine_start_swap_animation(u16 col1, u16 row1, u16 col2, u16 row2);

/**
 * @brief Check if a swap animation is currently in progress
 * @return TRUE if swap animation active, FALSE otherwise
 */
bool food_engine_is_swap_animating(void);

/**
 * @brief Legacy swap (no animation) - should not be used; kept for reference
 * @deprecated Use food_engine_start_swap_animation() instead
 */
void food_engine_swap_cells(u16 col1, u16 row1, u16 col2, u16 row2);

/**
 * @brief Trigger one manual row-center compaction pass on next movement tick.
 * @note Used by cleanup gameplay when player presses C ("shake").
 */
void food_engine_trigger_shake(void);
