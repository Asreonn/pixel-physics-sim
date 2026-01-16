/*
 * powder.c - Powder physics implementation (sand, soil)
 * 
 * Implements Milestone A from overview.md:
 * - Rule-based cellular automaton
 * - Bias-resistant scan order (bottom-up with per-tick direction randomization)
 * - Angle of repose via friction and cohesion
 * - Double-update prevention
 */
#include "powder.h"
#include "material.h"

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

bool powder_can_move_to(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);
    
    /* Can move into empty cells */
    if (state == STATE_EMPTY) return true;
    
    /* Can move into fluids (will displace) */
    if (state == STATE_FLUID) return true;
    
    /* Can move into gas */
    if (state == STATE_GAS) return true;
    
    return false;
}

bool powder_can_displace(const World* world, MaterialID source, int target_x, int target_y) {
    if (!IN_BOUNDS(target_x, target_y)) return false;
    
    MaterialID target = world_get_mat(world, target_x, target_y);
    const MaterialProps* src_props = material_get(source);
    const MaterialProps* tgt_props = material_get(target);
    
    /* Can only displace fluids and gases */
    if (tgt_props->state != STATE_FLUID && tgt_props->state != STATE_GAS) {
        return false;
    }
    
    /* Higher density displaces lower */
    return src_props->density > tgt_props->density;
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool powder_update_cell(Simulation* sim, World* world, int x, int y) {
    /* Skip if already updated this tick */
    if (world_has_flag(world, x, y, FLAG_UPDATED)) {
        return false;
    }
    
    MaterialID mat = world_get_mat(world, x, y);
    
    /* Only process powder materials */
    if (!material_is_powder(mat)) {
        return false;
    }
    
    const MaterialProps* props = material_get(mat);
    
    /* Check for settling (reduces jitter on stable piles) */
    if (simulation_randf(sim) < props->settle_probability) {
        /* Check if we're on stable ground */
        MaterialID below = world_get_mat(world, x, y + 1);
        if (!material_is_empty(below) && !material_is_fluid(below) && material_state(below) != STATE_GAS) {
            /* Check if diagonals are also blocked */
            bool left_blocked = !powder_can_move_to(world, x - 1, y + 1);
            bool right_blocked = !powder_can_move_to(world, x + 1, y + 1);
            
            if (left_blocked && right_blocked) {
                /* Stable position - skip update to reduce jitter */
                return false;
            }
        }
    }
    
    /* =========================================================================
     * Movement Rules (priority order from overview.md A1)
     * 1. Fall down if empty
     * 2. Fall down-left if empty  
     * 3. Fall down-right if empty
     * 4. Stay put
     * ========================================================================= */
    
    int dx = 0, dy = 0;
    bool moved = false;
    
    /* Apply gravity to velocity (powder falls faster) */
    int idx = IDX(x, y);
    Fixed8 gravity_step = props->gravity_step_fixed;
    Fixed8 drag_factor = props->drag_factor_fixed;
    Fixed8 terminal = props->terminal_velocity_fixed;
    
    world->vel_y[idx] = world->vel_y[idx] + gravity_step;
    world->vel_y[idx] = FIXED_MUL(world->vel_y[idx], drag_factor);
    world->vel_y[idx] = CLAMP(world->vel_y[idx], -terminal, terminal);
    
    int fall_steps = (int)(FIXED_ABS(world->vel_y[idx]) >> FIXED_SHIFT);
    fall_steps = CLAMP(fall_steps, 0, 3);
    if (fall_steps == 0) {
        fall_steps = 1;
    }
    
    /* Priority 1: Fall straight down (multi-step) */
    int cur_y = y;
    for (int i = 0; i < fall_steps; i++) {
        if (powder_can_move_to(world, x, cur_y + 1)) {
            cur_y += 1;
            dy = cur_y - y;
            moved = true;
        } else {
            world->vel_y[idx] = 0;
            break;
        }
    }
    
    /* Priority 2 & 3: Fall diagonally (randomize left/right bias) */
    if (!moved && fall_steps == 1) {
        bool try_left_first = simulation_randf(sim) < props->slide_bias;
        
        int left_x = x - 1;
        int right_x = x + 1;
        int down_y = y + 1;
        
        bool can_left = powder_can_move_to(world, left_x, down_y);
        bool can_right = powder_can_move_to(world, right_x, down_y);
        
        /* Apply cohesion: chance to NOT slide when both options available */
        if (can_left && can_right && props->cohesion > 0) {
            if (simulation_randf(sim) < props->cohesion) {
                /* Cohesion prevents sliding */
                can_left = false;
                can_right = false;
            }
        }
        
        if (try_left_first) {
            if (can_left) {
                dx = -1; dy = 1;
                moved = true;
            } else if (can_right) {
                dx = 1; dy = 1;
                moved = true;
            }
        } else {
            if (can_right) {
                dx = 1; dy = 1;
                moved = true;
            } else if (can_left) {
                dx = -1; dy = 1;
                moved = true;
            }
        }
    }
    
    /* Execute movement */
    if (moved) {
        int new_x = x + dx;
        int new_y = y + dy;
        
        MaterialID target = world_get_mat(world, new_x, new_y);
        
        if (material_is_empty(target)) {
            /* Simple swap with empty */
            world_swap_cells(world, x, y, new_x, new_y);
        } else if (powder_can_displace(world, mat, new_x, new_y)) {
            /* Displace fluid/gas: swap positions */
            
            /* Splash effect - if falling fast into fluid, create splashes */
            Fixed8 impact_vel = world->vel_y[idx];
            if (material_is_fluid(target) && FIXED_ABS(impact_vel) > FIXED_FROM_FLOAT(1.5f)) {
                /* Try to splash fluid to the sides */
                int splash_dir = (simulation_rand(sim) & 1) ? -1 : 1;
                int splash_x = new_x + splash_dir;
                int splash_y = new_y - 1;
                
                if (IN_BOUNDS(splash_x, splash_y)) {
                    MaterialID splash_target = world_get_mat(world, splash_x, splash_y);
                    if (material_is_empty(splash_target) || material_is_gas(splash_target)) {
                        /* Move some fluid to splash position */
                        world_set_mat(world, splash_x, splash_y, target);
                        int splash_idx = IDX(splash_x, splash_y);
                        world->vel_x[splash_idx] = FIXED_FROM_FLOAT(splash_dir * 0.8f);
                        world->vel_y[splash_idx] = FIXED_FROM_FLOAT(-0.5f);
                        world->color_seed[splash_idx] = world->color_seed[IDX(new_x, new_y)];
                    }
                }
            }
            
            world_swap_cells(world, x, y, new_x, new_y);
        }
        
        /* Mark both cells as updated to prevent double-processing */
        world_add_flag(world, new_x, new_y, FLAG_UPDATED);
        world_add_flag(world, x, y, FLAG_UPDATED);
        
        /* Update statistics */
        world->cells_updated++;
        
        return true;
    }
    
    return false;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void powder_update(Simulation* sim, World* world) {
    /*
     * Scan order strategy (from overview.md):
     * - Process bottom to top (so falling particles don't get processed twice)
     * - Randomize left-right direction per tick to avoid directional bias
     * - Only process active chunks for performance
     */
    
    /* Determine scan direction for this tick */
    bool scan_left_to_right = simulation_rand(sim) & 1;
    
    /* Process from bottom to top */
    for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
        /* Check which chunk row this is */
        int chunk_y = y / CHUNK_SIZE;
        
        if (scan_left_to_right) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int chunk_x = x / CHUNK_SIZE;
                
                /* Only process cells in active chunks */
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    powder_update_cell(sim, world, x, y);
                }
            }
        } else {
            for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                int chunk_x = x / CHUNK_SIZE;
                
                /* Only process cells in active chunks */
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    powder_update_cell(sim, world, x, y);
                }
            }
        }
    }
}
