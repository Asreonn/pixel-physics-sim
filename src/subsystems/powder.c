/*
 * powder.c - Powder Physics Implementation
 *
 * Implements powder behavior (sand, soil, ash) using the modular system:
 *   - cell_ops.h for movement operations
 *   - physics.h for gravity and velocity
 *   - behavior.h for material classification
 *   - grid_iter.h for iteration patterns
 */
#include "subsystems/powder.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "physics/physics.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"

/* =============================================================================
 * Powder Movement Helpers
 * ============================================================================= */

bool powder_can_move_to(const World* world, int x, int y) {
    return cell_powder_can_enter(world, x, y);
}

bool powder_can_displace(const World* world, MaterialID source, int target_x, int target_y) {
    if (!IN_BOUNDS(target_x, target_y)) return false;

    MaterialID target = world_get_mat(world, target_x, target_y);
    CellType type = cell_get_type(world, target_x, target_y);

    /* Can only displace fluids and gases */
    if (type != CELL_FLUID && type != CELL_GAS) {
        return false;
    }

    /* Higher density displaces lower */
    return phys_can_displace(source, target);
}

/* =============================================================================
 * Splash Effect Helper
 * ============================================================================= */

static void powder_create_splash(Simulation* sim, World* world,
                                  int x, int y, MaterialID fluid_mat) {
    /* Try to splash fluid to the sides */
    int splash_dir = (simulation_rand(sim) & 1) ? -1 : 1;
    int splash_x = x + splash_dir;
    int splash_y = y - 1;

    if (!IN_BOUNDS(splash_x, splash_y)) return;

    if (cell_is_passable(world, splash_x, splash_y)) {
        world_set_mat(world, splash_x, splash_y, fluid_mat);
        int splash_idx = IDX(splash_x, splash_y);
        world->vel_x[splash_idx] = FIXED_FROM_FLOAT(splash_dir * 0.8f);
        world->vel_y[splash_idx] = FIXED_FROM_FLOAT(-0.5f);
        world->color_seed[splash_idx] = world->color_seed[IDX(x, y)];
    }
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool powder_update_cell(Simulation* sim, World* world, int x, int y) {
    /* Skip if already updated this tick */
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);

    /* Only process powder materials */
    if (!material_is_powder(mat)) {
        return false;
    }

    const MaterialProps* props = material_get(mat);

    /* =========================================================================
     * Settling Check (reduces jitter on stable piles)
     * ========================================================================= */
    if (simulation_randf(sim) < props->settle_probability) {
        CellType below_type = cell_get_type(world, x, y + 1);

        if (below_type != CELL_EMPTY && below_type != CELL_FLUID && below_type != CELL_GAS) {
            /* Check if diagonals are also blocked */
            bool left_blocked = !powder_can_move_to(world, x - 1, y + 1);
            bool right_blocked = !powder_can_move_to(world, x + 1, y + 1);

            if (left_blocked && right_blocked) {
                return false; /* Stable - skip update */
            }
        }
    }

    /* =========================================================================
     * Apply Gravity
     * ========================================================================= */
    phys_apply_gravity_fixed(world, x, y, props);

    MovementSteps fall = phys_calc_fall_steps(world, x, y, 3);

    /* =========================================================================
     * Movement: Priority 1 - Fall Straight Down
     * ========================================================================= */
    int dx = 0, dy = 0;
    bool moved = false;

    int cur_y = y;
    for (int i = 0; i < fall.steps; i++) {
        if (powder_can_move_to(world, x, cur_y + 1)) {
            cur_y++;
            dy = cur_y - y;
            moved = true;
        } else {
            phys_collision_vertical(world, x, y, COLLISION_STOP, 0);
            break;
        }
    }

    /* =========================================================================
     * Movement: Priority 2 & 3 - Diagonal Slide
     * ========================================================================= */
    if (!moved && fall.steps == 1) {
        bool try_left_first = simulation_randf(sim) < props->slide_bias;

        bool can_left = powder_can_move_to(world, x - 1, y + 1);
        bool can_right = powder_can_move_to(world, x + 1, y + 1);

        /* Apply cohesion: chance to NOT slide when both options available */
        if (can_left && can_right && props->cohesion > 0) {
            if (simulation_randf(sim) < props->cohesion) {
                can_left = false;
                can_right = false;
            }
        }

        if (try_left_first) {
            if (can_left)       { dx = -1; dy = 1; moved = true; }
            else if (can_right) { dx = 1;  dy = 1; moved = true; }
        } else {
            if (can_right)      { dx = 1;  dy = 1; moved = true; }
            else if (can_left)  { dx = -1; dy = 1; moved = true; }
        }
    }

    /* =========================================================================
     * Execute Movement
     * ========================================================================= */
    if (moved) {
        int new_x = x + dx;
        int new_y = y + dy;

        MaterialID target = world_get_mat(world, new_x, new_y);

        if (cell_is_empty(world, new_x, new_y)) {
            /* Simple swap with empty */
            cell_move(world, x, y, new_x, new_y);
        } else if (powder_can_displace(world, mat, new_x, new_y)) {
            /* Displacement with potential splash */
            if (cell_is_fluid(world, new_x, new_y) && phys_is_impact(world, x, y)) {
                powder_create_splash(sim, world, new_x, new_y, target);
            }
            cell_move(world, x, y, new_x, new_y);
        }

        return true;
    }

    return false;
}

/* =============================================================================
 * Grid Update Callback
 * ============================================================================= */

static bool powder_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    powder_update_cell(sim, world, x, y);
    return true; /* Continue iteration */
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void powder_update(Simulation* sim, World* world) {
    /* Process bottom to top with randomized horizontal direction */
    grid_iterate_falling(sim, world, powder_cell_callback, NULL);
}
