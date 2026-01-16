/*
 * cell_ops.h - Common Cell Operations Module
 *
 * Provides unified interface for cell manipulation, movement validation,
 * and state management across all subsystems.
 */
#ifndef CELL_OPS_H
#define CELL_OPS_H

#include "core/types.h"
#include "world/world.h"
#include "materials/material.h"

/* =============================================================================
 * Movement Direction Enumeration
 * ============================================================================= */

typedef enum {
    DIR_NONE        = 0,
    DIR_UP          = (1 << 0),
    DIR_DOWN        = (1 << 1),
    DIR_LEFT        = (1 << 2),
    DIR_RIGHT       = (1 << 3),
    DIR_UP_LEFT     = (1 << 4),
    DIR_UP_RIGHT    = (1 << 5),
    DIR_DOWN_LEFT   = (1 << 6),
    DIR_DOWN_RIGHT  = (1 << 7),
} Direction;

/* Direction groups for common patterns */
#define DIR_HORIZONTAL  (DIR_LEFT | DIR_RIGHT)
#define DIR_VERTICAL    (DIR_UP | DIR_DOWN)
#define DIR_DIAGONAL    (DIR_UP_LEFT | DIR_UP_RIGHT | DIR_DOWN_LEFT | DIR_DOWN_RIGHT)
#define DIR_CARDINAL    (DIR_HORIZONTAL | DIR_VERTICAL)
#define DIR_ALL         (DIR_CARDINAL | DIR_DIAGONAL)

/* Direction to offset conversion */
typedef struct {
    int dx;
    int dy;
} DirOffset;

/* Get offset for a single direction */
static inline DirOffset dir_to_offset(Direction dir) {
    switch (dir) {
        case DIR_UP:         return (DirOffset){ 0, -1};
        case DIR_DOWN:       return (DirOffset){ 0,  1};
        case DIR_LEFT:       return (DirOffset){-1,  0};
        case DIR_RIGHT:      return (DirOffset){ 1,  0};
        case DIR_UP_LEFT:    return (DirOffset){-1, -1};
        case DIR_UP_RIGHT:   return (DirOffset){ 1, -1};
        case DIR_DOWN_LEFT:  return (DirOffset){-1,  1};
        case DIR_DOWN_RIGHT: return (DirOffset){ 1,  1};
        default:             return (DirOffset){ 0,  0};
    }
}

/* =============================================================================
 * Cell State Classifiers
 *
 * Quick checks for cell properties without repeated material lookups.
 * ============================================================================= */

typedef enum {
    CELL_EMPTY      = 0,
    CELL_SOLID      = 1,
    CELL_POWDER     = 2,
    CELL_FLUID      = 3,
    CELL_GAS        = 4,
} CellType;

/* Get cell type at position */
static inline CellType cell_get_type(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return CELL_SOLID; /* Out of bounds = solid wall */
    MaterialID mat = world->mat[IDX(x, y)];
    return (CellType)material_state(mat);
}

/* Quick type checks */
static inline bool cell_is_empty(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_EMPTY;
}

static inline bool cell_is_solid(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_SOLID;
}

static inline bool cell_is_powder(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_POWDER;
}

static inline bool cell_is_fluid(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_FLUID;
}

static inline bool cell_is_gas(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_GAS;
}

/* Composite checks */
static inline bool cell_is_passable(const World* world, int x, int y) {
    CellType type = cell_get_type(world, x, y);
    return type == CELL_EMPTY || type == CELL_GAS;
}

static inline bool cell_is_displaceable(const World* world, int x, int y) {
    CellType type = cell_get_type(world, x, y);
    return type == CELL_EMPTY || type == CELL_FLUID || type == CELL_GAS;
}

static inline bool cell_is_movable(const World* world, int x, int y) {
    CellType type = cell_get_type(world, x, y);
    return type == CELL_POWDER || type == CELL_FLUID || type == CELL_GAS;
}

/* =============================================================================
 * Movement Validation
 *
 * Check if a cell can move to a target position based on material rules.
 * ============================================================================= */

typedef enum {
    MOVE_BLOCKED    = 0,    /* Cannot move */
    MOVE_INTO_EMPTY = 1,    /* Move into empty space */
    MOVE_SWAP       = 2,    /* Swap with target (displacement) */
} MoveResult;

/* Check if source material can move to target position */
static inline MoveResult cell_can_move(const World* world,
                                        MaterialID source_mat,
                                        int target_x, int target_y) {
    if (!IN_BOUNDS(target_x, target_y)) return MOVE_BLOCKED;

    MaterialID target_mat = world->mat[IDX(target_x, target_y)];
    MaterialState target_state = material_state(target_mat);

    /* Can always move into empty */
    if (target_state == STATE_EMPTY) return MOVE_INTO_EMPTY;

    /* Cannot move into solids */
    if (target_state == STATE_SOLID) return MOVE_BLOCKED;

    /* Density-based displacement */
    const MaterialProps* src_props = material_get(source_mat);
    const MaterialProps* tgt_props = material_get(target_mat);

    if (src_props->density > tgt_props->density) {
        return MOVE_SWAP;
    }

    return MOVE_BLOCKED;
}

/* Simplified movement checks for specific material types */
static inline bool cell_powder_can_enter(const World* world, int x, int y) {
    CellType type = cell_get_type(world, x, y);
    return type == CELL_EMPTY || type == CELL_FLUID || type == CELL_GAS;
}

static inline bool cell_fluid_can_enter(const World* world, int x, int y) {
    CellType type = cell_get_type(world, x, y);
    return type == CELL_EMPTY || type == CELL_GAS;
}

static inline bool cell_gas_can_enter(const World* world, int x, int y) {
    return cell_get_type(world, x, y) == CELL_EMPTY;
}

/* =============================================================================
 * Cell Movement Operations
 *
 * Execute cell movement with proper state updates.
 * ============================================================================= */

/* Move cell and mark as updated */
static inline bool cell_move(World* world, int from_x, int from_y, int to_x, int to_y) {
    if (!IN_BOUNDS(from_x, from_y) || !IN_BOUNDS(to_x, to_y)) return false;

    world_swap_cells(world, from_x, from_y, to_x, to_y);
    world_add_flag(world, to_x, to_y, FLAG_UPDATED);
    world_add_flag(world, from_x, from_y, FLAG_UPDATED);
    world->cells_updated++;

    return true;
}

/* Try to move in a direction, returns true if moved */
static inline bool cell_try_move(World* world, int x, int y, Direction dir) {
    DirOffset off = dir_to_offset(dir);
    int nx = x + off.dx;
    int ny = y + off.dy;

    MaterialID mat = world_get_mat(world, x, y);
    MoveResult result = cell_can_move(world, mat, nx, ny);

    if (result != MOVE_BLOCKED) {
        return cell_move(world, x, y, nx, ny);
    }
    return false;
}

/* =============================================================================
 * Neighbor Iteration Helpers
 *
 * Iterate over neighbors with callback pattern.
 * ============================================================================= */

/* Neighbor info passed to callback */
typedef struct {
    int x, y;               /* Neighbor position */
    int dx, dy;             /* Offset from center */
    MaterialID mat;         /* Material at neighbor */
    CellType type;          /* Cell type */
    int index;              /* Iteration index */
} NeighborInfo;

/* Callback type for neighbor iteration */
typedef bool (*NeighborCallback)(World* world, int cx, int cy,
                                  const NeighborInfo* neighbor, void* userdata);

/* 4-directional neighbor offsets */
static const int NEIGHBOR4_DX[4] = {-1, 1, 0, 0};
static const int NEIGHBOR4_DY[4] = {0, 0, -1, 1};

/* 8-directional neighbor offsets */
static const int NEIGHBOR8_DX[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
static const int NEIGHBOR8_DY[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

/* Iterate 4 cardinal neighbors */
static inline void cell_foreach_neighbor4(World* world, int cx, int cy,
                                           NeighborCallback cb, void* userdata) {
    for (int i = 0; i < 4; i++) {
        int nx = cx + NEIGHBOR4_DX[i];
        int ny = cy + NEIGHBOR4_DY[i];

        if (!IN_BOUNDS(nx, ny)) continue;

        NeighborInfo info = {
            .x = nx, .y = ny,
            .dx = NEIGHBOR4_DX[i], .dy = NEIGHBOR4_DY[i],
            .mat = world->mat[IDX(nx, ny)],
            .type = cell_get_type(world, nx, ny),
            .index = i
        };

        if (!cb(world, cx, cy, &info, userdata)) break;
    }
}

/* Iterate 8 surrounding neighbors */
static inline void cell_foreach_neighbor8(World* world, int cx, int cy,
                                           NeighborCallback cb, void* userdata) {
    for (int i = 0; i < 8; i++) {
        int nx = cx + NEIGHBOR8_DX[i];
        int ny = cy + NEIGHBOR8_DY[i];

        if (!IN_BOUNDS(nx, ny)) continue;

        NeighborInfo info = {
            .x = nx, .y = ny,
            .dx = NEIGHBOR8_DX[i], .dy = NEIGHBOR8_DY[i],
            .mat = world->mat[IDX(nx, ny)],
            .type = cell_get_type(world, nx, ny),
            .index = i
        };

        if (!cb(world, cx, cy, &info, userdata)) break;
    }
}

/* =============================================================================
 * Update State Helpers
 * ============================================================================= */

/* Check if cell was already processed this tick */
static inline bool cell_was_updated(const World* world, int x, int y) {
    return world_has_flag(world, x, y, FLAG_UPDATED);
}

/* Mark cell as processed */
static inline void cell_mark_updated(World* world, int x, int y) {
    world_add_flag(world, x, y, FLAG_UPDATED);
}

/* Skip if already updated, returns true if should skip */
static inline bool cell_skip_if_updated(const World* world, int x, int y) {
    return cell_was_updated(world, x, y);
}

#endif /* CELL_OPS_H */
