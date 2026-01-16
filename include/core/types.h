/*
 * types.h - Core types and constants for Pixel-Cell Physics Simulator
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =============================================================================
 * Configuration Constants
 * ============================================================================= */

/* Grid dimensions (can be changed at compile time) */
#ifndef GRID_WIDTH
#define GRID_WIDTH 512
#endif

#ifndef GRID_HEIGHT
#define GRID_HEIGHT 512
#endif

#define GRID_SIZE (GRID_WIDTH * GRID_HEIGHT)

/* Window dimensions (pixels) */
#define WINDOW_WIDTH  GRID_WIDTH
#define WINDOW_HEIGHT GRID_HEIGHT

/* Simulation tick rate (Hz) */
#define TICK_HZ 120
#define TICK_DT (1.0 / TICK_HZ)

/* Gravity acceleration in cells/tick^2 (scaled from 9.81 m/s^2) */
#define GRAVITY_ACCEL 0.08f

/* Chunk size for dirty region tracking */
#define CHUNK_SIZE 32
#define CHUNKS_X ((GRID_WIDTH + CHUNK_SIZE - 1) / CHUNK_SIZE)
#define CHUNKS_Y ((GRID_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE)
#define CHUNK_COUNT (CHUNKS_X * CHUNKS_Y)

/* =============================================================================
 * Cell Flags (per-cell overlay states)
 * ============================================================================= */

typedef uint16_t CellFlags;

#define FLAG_NONE           0x0000
#define FLAG_UPDATED        0x0001  /* Cell was updated this tick (prevent double-update) */
#define FLAG_STATIC         0x0002  /* Cell is static/immovable */
#define FLAG_BURNING        0x0004  /* Cell is on fire */
#define FLAG_WET            0x0008  /* Cell is wet */
#define FLAG_HOT            0x0010  /* Cell is hot */
#define FLAG_ACTIVE         0x0020  /* Cell is active (needs processing) */
#define FLAG_CORRODING      0x0040  /* Cell is being corroded by acid */
#define FLAG_FROZEN         0x0080  /* Cell is frozen */

/* =============================================================================
 * Material States
 * ============================================================================= */

typedef enum {
    STATE_EMPTY = 0,
    STATE_SOLID,
    STATE_POWDER,
    STATE_FLUID,
    STATE_GAS
} MaterialState;

/* =============================================================================
 * Material IDs
 * ============================================================================= */

typedef uint8_t MaterialID;

#define MAT_EMPTY   0
#define MAT_SAND    1
#define MAT_STONE   2
#define MAT_WATER   3
#define MAT_WOOD    4
#define MAT_FIRE    5
#define MAT_SMOKE   6
#define MAT_SOIL    7
#define MAT_ICE     8
#define MAT_STEAM   9
#define MAT_ASH     10
#define MAT_ACID    11

#define MAT_COUNT   12  /* Total number of materials */

/* =============================================================================
 * Color Types
 * ============================================================================= */

typedef struct {
    uint8_t r, g, b, a;
} Color;

/* =============================================================================
 * Fixed-Point Types (8.8)
 * ============================================================================= */

typedef int16_t Fixed8;

#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_FROM_FLOAT(x) ((Fixed8)((x) * (float)FIXED_ONE))
#define FIXED_TO_FLOAT(x) ((float)(x) / (float)FIXED_ONE)
#define FIXED_MUL(a, b) ((Fixed8)(((int32_t)(a) * (int32_t)(b)) >> FIXED_SHIFT))
#define FIXED_ABS(x) ((x) < 0 ? -(x) : (x))

/* =============================================================================
 * Utility Macros
 * ============================================================================= */

/* Convert 2D coordinates to 1D index */
#define IDX(x, y) ((y) * GRID_WIDTH + (x))

/* Check if coordinates are within grid bounds */
#define IN_BOUNDS(x, y) ((x) >= 0 && (x) < GRID_WIDTH && (y) >= 0 && (y) < GRID_HEIGHT)

/* Get chunk index from cell coordinates */
#define CHUNK_IDX(x, y) (((y) / CHUNK_SIZE) * CHUNKS_X + ((x) / CHUNK_SIZE))

/* Min/Max macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) (MIN(MAX(x, lo), hi))

#endif /* TYPES_H */
