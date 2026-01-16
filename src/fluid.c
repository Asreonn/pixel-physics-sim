/*
 * fluid.c - Realistic fluid physics (water, acid)
 */
#include "fluid.h"
#include "material.h"
#include <stdlib.h>

/* Water behavior settings (real-world scaled) */
#define WATER_DISPERSION_RATE 2  /* Slower than before */
#define PRESSURE_EQUALIZE_CHANCE 0.3f  /* Chance to equalize pressure */

/* Count water column height above a point */
static int count_fluid_column(const World* world, int x, int y_start, MaterialID fluid_type) {
    int count = 0;
    for (int y = y_start; y >= 0; y--) {
        MaterialID mat = world_get_mat(world, x, y);
        if (mat == fluid_type) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

bool fluid_can_move_to(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);
    
    if (state == STATE_EMPTY) return true;
    if (state == STATE_GAS) return true;
    
    return false;
}

/* Apply gravity to velocity (fixed-point) */
static void apply_gravity(World* world, int x, int y, const MaterialProps* props) {
    int idx = IDX(x, y);
    world->vel_y[idx] = world->vel_y[idx] + props->gravity_step_fixed;
    world->vel_y[idx] = FIXED_MUL(world->vel_y[idx], props->drag_factor_fixed);
    world->vel_y[idx] = CLAMP(world->vel_y[idx], -props->terminal_velocity_fixed, props->terminal_velocity_fixed);
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool fluid_update_cell(Simulation* sim, World* world, int x, int y) {
    if (world_has_flag(world, x, y, FLAG_UPDATED)) {
        return false;
    }
    
    MaterialID mat = world_get_mat(world, x, y);
    
    if (!material_is_fluid(mat)) {
        return false;
    }
    
    const MaterialProps* props = material_get(mat);
    
    /* Apply gravity to velocity */
    apply_gravity(world, x, y, props);
    
    int idx = IDX(x, y);
    Fixed8 vy = world->vel_y[idx];
    
    /* Convert velocity to movement steps */
    int steps = (int)(FIXED_ABS(vy) >> FIXED_SHIFT);
    steps = CLAMP(steps, 0, 2);
    if (steps == 0) {
        steps = 1;
    }
    
    int new_x = x, new_y = y;
    bool moved = false;
    
    /* Try to move down based on velocity */
    if (vy > 0) {
        for (int i = 0; i < steps; i++) {
            if (fluid_can_move_to(world, new_x, new_y + 1)) {
                new_y += 1;
                moved = true;
            } else {
                world->vel_y[idx] = 0;
                break;
            }
        }
    }
    
    /* If can't move down, try horizontal flow */
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
    
    /* Pressure equalization - try to level out with neighbors */
    if (!moved && simulation_randf(sim) < PRESSURE_EQUALIZE_CHANCE) {
        int my_pressure = count_fluid_column(world, x, y, mat);
        
        /* Check left neighbor */
        if (fluid_can_move_to(world, x - 1, y)) {
            int left_pressure = count_fluid_column(world, x - 1, y, mat);
            if (left_pressure < my_pressure - 1) {
                new_x = x - 1;
                moved = true;
            }
        }
        
        /* Check right neighbor if not moved yet */
        if (!moved && fluid_can_move_to(world, x + 1, y)) {
            int right_pressure = count_fluid_column(world, x + 1, y, mat);
            if (right_pressure < my_pressure - 1) {
                new_x = x + 1;
                moved = true;
            }
        }
    }
    
    /* Apply drag to horizontal velocity (simple damping) */
    world->vel_x[idx] = FIXED_MUL(world->vel_x[idx], props->drag_factor_fixed);
    
    /* Execute movement */
    if (moved && (new_x != x || new_y != y)) {
        world_swap_cells(world, x, y, new_x, new_y);
        world_add_flag(world, new_x, new_y, FLAG_UPDATED);
        world_add_flag(world, x, y, FLAG_UPDATED);
        world->cells_updated++;
        return true;
    }
    
    return false;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void fluid_update(Simulation* sim, World* world) {
    for (int pass = 0; pass < WATER_DISPERSION_RATE; pass++) {
        bool scan_left_to_right = simulation_rand(sim) & 1;
        
        for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
            int chunk_y = y / CHUNK_SIZE;
            
            if (scan_left_to_right) {
                for (int x = 0; x < GRID_WIDTH; x++) {
                    int chunk_x = x / CHUNK_SIZE;
                    if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                        if (pass > 0) {
                            world_remove_flag(world, x, y, FLAG_UPDATED);
                        }
                        fluid_update_cell(sim, world, x, y);
                    }
                }
            } else {
                for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                    int chunk_x = x / CHUNK_SIZE;
                    if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                        if (pass > 0) {
                            world_remove_flag(world, x, y, FLAG_UPDATED);
                        }
                        fluid_update_cell(sim, world, x, y);
                    }
                }
            }
        }
    }
}
