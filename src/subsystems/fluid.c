/*
 * fluid.c - Fluid Physics Implementation (water, acid)
 *
 * Implements fluid behavior using the modular system:
 *   - cell_ops.h for movement operations
 *   - physics.h for gravity and pressure
 *   - behavior.h for material classification
 *   - grid_iter.h for iteration patterns
 */
#include "subsystems/fluid.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "physics/physics.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"
#include <stdlib.h>

/* =============================================================================
 * Fluid Configuration
 * ============================================================================= */

#define FLUID_DISPERSION_PASSES  2       /* Number of update passes */
#define FLUID_PRESSURE_THRESHOLD 1       /* Minimum pressure diff to flow */
#define FLUID_PRESSURE_CHANCE    0.3f    /* Chance to equalize pressure */

/* =============================================================================
 * Movement Helpers
 * ============================================================================= */

bool fluid_can_move_to(const World* world, int x, int y) {
    return cell_fluid_can_enter(world, x, y);
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool fluid_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);

    if (!material_is_fluid(mat)) {
        return false;
    }

    const MaterialProps* props = material_get(mat);

    /* =========================================================================
     * Apply Gravity
     * ========================================================================= */
    phys_apply_gravity_fixed(world, x, y, props);

    int idx = IDX(x, y);
    Fixed8 vy = world->vel_y[idx];

    MovementSteps steps = phys_calc_fall_steps(world, x, y, 2);

    int new_x = x, new_y = y;
    bool moved = false;

    /* =========================================================================
     * Movement: Priority 1 - Fall Down
     * ========================================================================= */
    if (vy > 0) {
        for (int i = 0; i < steps.steps; i++) {
            if (fluid_can_move_to(world, new_x, new_y + 1)) {
                new_y++;
                moved = true;
            } else {
                phys_collision_vertical(world, x, y, COLLISION_STOP, 0);
                break;
            }
        }
    }

    /* =========================================================================
     * Movement: Priority 2 - Horizontal Flow
     * ========================================================================= */
    if (!moved || vy <= 0) {
        if (simulation_randf(sim) < props->flow_rate) {
            bool can_left = fluid_can_move_to(world, x - 1, y);
            bool can_right = fluid_can_move_to(world, x + 1, y);

            if (can_left && can_right) {
                new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
                moved = true;
            } else if (can_left) {
                new_x = x - 1;
                moved = true;
            } else if (can_right) {
                new_x = x + 1;
                moved = true;
            }
        }
    }

    /* =========================================================================
     * Movement: Priority 3 - Pressure Equalization
     * ========================================================================= */
    if (!moved && simulation_randf(sim) < FLUID_PRESSURE_CHANCE) {
        int my_pressure = phys_column_height(world, x, y, mat);

        /* Check left neighbor */
        if (fluid_can_move_to(world, x - 1, y)) {
            int left_pressure = phys_column_height(world, x - 1, y, mat);
            if (left_pressure < my_pressure - FLUID_PRESSURE_THRESHOLD) {
                new_x = x - 1;
                moved = true;
            }
        }

        /* Check right neighbor if not moved */
        if (!moved && fluid_can_move_to(world, x + 1, y)) {
            int right_pressure = phys_column_height(world, x + 1, y, mat);
            if (right_pressure < my_pressure - FLUID_PRESSURE_THRESHOLD) {
                new_x = x + 1;
                moved = true;
            }
        }
    }

    /* =========================================================================
     * Apply Horizontal Drag
     * ========================================================================= */
    world->vel_x[idx] = FIXED_MUL(world->vel_x[idx], props->drag_factor_fixed);

    /* =========================================================================
     * Execute Movement
     * ========================================================================= */
    if (moved && (new_x != x || new_y != y)) {
        cell_move(world, x, y, new_x, new_y);
        return true;
    }

    return false;
}

/* =============================================================================
 * Grid Update Callback
 * ============================================================================= */

static bool fluid_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    int* pass = (int*)userdata;

    /* Clear updated flag on subsequent passes */
    if (*pass > 0) {
        world_remove_flag(world, x, y, FLAG_UPDATED);
    }

    fluid_update_cell(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void fluid_update(Simulation* sim, World* world) {
    /* Multiple passes for better dispersion */
    for (int pass = 0; pass < FLUID_DISPERSION_PASSES; pass++) {
        grid_iterate_falling(sim, world, fluid_cell_callback, &pass);
    }
}
