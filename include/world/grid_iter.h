/*
 * grid_iter.h - Grid Iteration Module
 *
 * Provides unified grid traversal patterns for all subsystems.
 * Handles chunk-based active region optimization.
 */
#ifndef GRID_ITER_H
#define GRID_ITER_H

#include "core/types.h"
#include "world/world.h"
#include "engine/simulation.h"

/* =============================================================================
 * Iteration Order
 * ============================================================================= */

typedef enum {
    ITER_TOP_DOWN,      /* y: 0 -> HEIGHT (for rising materials) */
    ITER_BOTTOM_UP,     /* y: HEIGHT -> 0 (for falling materials) */
} IterDirection;

typedef enum {
    ITER_LEFT_RIGHT,    /* x: 0 -> WIDTH */
    ITER_RIGHT_LEFT,    /* x: WIDTH -> 0 */
    ITER_RANDOM,        /* Random per-tick */
} IterHorizontal;

/* =============================================================================
 * Cell Update Callback
 * ============================================================================= */

/* Return true to continue iteration, false to stop */
typedef bool (*CellUpdateFunc)(Simulation* sim, World* world, int x, int y, void* userdata);

/* =============================================================================
 * Grid Iteration Functions
 * ============================================================================= */

/* Iterate over all active cells with specified direction */
static inline void grid_iterate(Simulation* sim, World* world,
                                 IterDirection dir, IterHorizontal horiz,
                                 CellUpdateFunc func, void* userdata) {
    /* Determine horizontal scan direction */
    bool scan_left = (horiz == ITER_LEFT_RIGHT) ||
                     (horiz == ITER_RANDOM && (simulation_rand(sim) & 1));

    /* Determine vertical bounds */
    int y_start, y_end, y_step;
    if (dir == ITER_TOP_DOWN) {
        y_start = 0;
        y_end = GRID_HEIGHT;
        y_step = 1;
    } else {
        y_start = GRID_HEIGHT - 1;
        y_end = -1;
        y_step = -1;
    }

    /* Iterate */
    for (int y = y_start; y != y_end; y += y_step) {
        int chunk_y = y / CHUNK_SIZE;

        if (scan_left) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    if (!func(sim, world, x, y, userdata)) return;
                }
            }
        } else {
            for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    if (!func(sim, world, x, y, userdata)) return;
                }
            }
        }
    }
}

/* Common iteration patterns */
static inline void grid_iterate_falling(Simulation* sim, World* world,
                                         CellUpdateFunc func, void* userdata) {
    grid_iterate(sim, world, ITER_BOTTOM_UP, ITER_RANDOM, func, userdata);
}

static inline void grid_iterate_rising(Simulation* sim, World* world,
                                        CellUpdateFunc func, void* userdata) {
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_RANDOM, func, userdata);
}

/* =============================================================================
 * Multi-Pass Iteration
 *
 * Some subsystems need multiple passes (e.g., fluid dispersion).
 * ============================================================================= */

typedef struct {
    int pass;           /* Current pass number */
    int total_passes;   /* Total number of passes */
} PassInfo;

static inline void grid_iterate_multipass(Simulation* sim, World* world,
                                           IterDirection dir, IterHorizontal horiz,
                                           int passes, bool clear_flags_between,
                                           CellUpdateFunc func, void* userdata) {
    for (int pass = 0; pass < passes; pass++) {
        /* Optionally clear updated flags between passes */
        if (pass > 0 && clear_flags_between) {
            /* Only clear for cells we'll process */
            bool scan_left = (horiz == ITER_LEFT_RIGHT) ||
                             (horiz == ITER_RANDOM && (simulation_rand(sim) & 1));

            int y_start = (dir == ITER_TOP_DOWN) ? 0 : GRID_HEIGHT - 1;
            int y_end = (dir == ITER_TOP_DOWN) ? GRID_HEIGHT : -1;
            int y_step = (dir == ITER_TOP_DOWN) ? 1 : -1;

            for (int y = y_start; y != y_end; y += y_step) {
                int chunk_y = y / CHUNK_SIZE;
                if (scan_left) {
                    for (int x = 0; x < GRID_WIDTH; x++) {
                        int chunk_x = x / CHUNK_SIZE;
                        if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                            world_remove_flag(world, x, y, FLAG_UPDATED);
                        }
                    }
                } else {
                    for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                        int chunk_x = x / CHUNK_SIZE;
                        if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                            world_remove_flag(world, x, y, FLAG_UPDATED);
                        }
                    }
                }
            }
        }

        grid_iterate(sim, world, dir, horiz, func, userdata);
    }
}

/* =============================================================================
 * Chunk-Based Iteration
 *
 * Iterate by chunk for better cache locality.
 * ============================================================================= */

/* Callback receives chunk bounds */
typedef void (*ChunkUpdateFunc)(Simulation* sim, World* world,
                                 int chunk_x, int chunk_y,
                                 int x_start, int y_start,
                                 int x_end, int y_end,
                                 void* userdata);

static inline void grid_iterate_chunks(Simulation* sim, World* world,
                                        ChunkUpdateFunc func, void* userdata) {
    for (int cy = 0; cy < CHUNKS_Y; cy++) {
        for (int cx = 0; cx < CHUNKS_X; cx++) {
            if (!world_is_chunk_active(world, cx, cy)) continue;

            int x_start = cx * CHUNK_SIZE;
            int y_start = cy * CHUNK_SIZE;
            int x_end = MIN(x_start + CHUNK_SIZE, GRID_WIDTH);
            int y_end = MIN(y_start + CHUNK_SIZE, GRID_HEIGHT);

            func(sim, world, cx, cy, x_start, y_start, x_end, y_end, userdata);
        }
    }

    (void)sim; /* May be used in future */
}

/* =============================================================================
 * Material-Specific Iteration
 *
 * Only process cells of specific material type.
 * ============================================================================= */

typedef struct {
    MaterialID material;        /* Target material */
    CellUpdateFunc func;        /* Actual update function */
    void* userdata;             /* User data */
} MaterialIterContext;

static inline bool _material_filter_callback(Simulation* sim, World* world,
                                              int x, int y, void* userdata) {
    MaterialIterContext* ctx = (MaterialIterContext*)userdata;

    if (world_get_mat(world, x, y) != ctx->material) {
        return true; /* Skip, continue iteration */
    }

    return ctx->func(sim, world, x, y, ctx->userdata);
}

static inline void grid_iterate_material(Simulation* sim, World* world,
                                          MaterialID mat, IterDirection dir,
                                          CellUpdateFunc func, void* userdata) {
    MaterialIterContext ctx = {
        .material = mat,
        .func = func,
        .userdata = userdata
    };

    grid_iterate(sim, world, dir, ITER_RANDOM, _material_filter_callback, &ctx);
}

/* =============================================================================
 * State-Specific Iteration
 *
 * Only process cells of specific material state.
 * ============================================================================= */

typedef struct {
    MaterialState state;        /* Target state */
    CellUpdateFunc func;        /* Actual update function */
    void* userdata;             /* User data */
} StateIterContext;

static inline bool _state_filter_callback(Simulation* sim, World* world,
                                           int x, int y, void* userdata) {
    StateIterContext* ctx = (StateIterContext*)userdata;

    MaterialID mat = world_get_mat(world, x, y);
    if (material_state(mat) != ctx->state) {
        return true; /* Skip, continue iteration */
    }

    return ctx->func(sim, world, x, y, ctx->userdata);
}

static inline void grid_iterate_state(Simulation* sim, World* world,
                                       MaterialState state, IterDirection dir,
                                       CellUpdateFunc func, void* userdata) {
    StateIterContext ctx = {
        .state = state,
        .func = func,
        .userdata = userdata
    };

    grid_iterate(sim, world, dir, ITER_RANDOM, _state_filter_callback, &ctx);
}

#endif /* GRID_ITER_H */
