/*
 * fire.c - Fire and smoke physics implementation
 * 
 * Fire:
 * - Rises upward (like gas but slower)
 * - Has limited lifetime, then becomes smoke or disappears
 * - Spreads to adjacent flammable materials
 * - Produces smoke while burning
 * 
 * Smoke:
 * - Rises upward
 * - Spreads horizontally when blocked
 * - Dissipates SLOWLY over time
 */
#include "fire.h"
#include "material.h"

/* Fire settings */
#define FIRE_RISE_CHANCE 0.6f       /* Chance to rise each tick */
#define FIRE_DIE_CHANCE 0.02f       /* Chance to die each tick (becomes smoke) */
#define FIRE_SPREAD_CHANCE 0.03f    /* Chance to spread to adjacent flammable */
#define FIRE_SMOKE_CHANCE 0.15f     /* Chance to produce smoke above */
#define FIRE_MAX_LIFETIME 120       /* Max ticks before forced death */

/* Smoke settings */
#define SMOKE_DISSIPATE_CHANCE 0.006f   /* Faster dissipation (~1-2 seconds) */
#define SMOKE_RISE_CHANCE 0.85f         /* High chance to rise */
#define SMOKE_SPREAD_CHANCE 0.3f        /* Chance to spread horizontally when blocked */

/* Steam settings */
#define STEAM_RISE_CHANCE 0.9f          /* Very high chance to rise */
#define STEAM_CONDENSE_CHANCE 0.01f     /* Chance to condense to water (when cool) */
#define STEAM_SPREAD_CHANCE 0.4f        /* Spreads more than smoke */

/* Fire color palette (indexed by age) - from hot to cool */
static const Color FIRE_PALETTE[6] = {
    {255, 255, 200, 255},  /* 0: White-yellow (very hot, young) */
    {255, 220, 100, 255},  /* 1: Bright yellow */
    {255, 150, 50, 255},   /* 2: Orange */
    {255, 80, 20, 255},    /* 3: Red-orange */
    {200, 50, 20, 255},    /* 4: Dark red */
    {100, 30, 10, 255},    /* 5: Very dark (dying) */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

bool material_is_flammable(MaterialID mat) {
    return mat == MAT_WOOD;
}

bool fire_try_ignite(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    MaterialID mat = world_get_mat(world, x, y);
    
    if (material_is_flammable(mat)) {
        world_set_mat(world, x, y, MAT_FIRE);
        world_add_flag(world, x, y, FLAG_BURNING);
        return true;
    }
    
    return false;
}

/* Check if fire/gas can move to position */
static bool gas_can_move_to(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);
    
    if (state == STATE_EMPTY) return true;
    
    return false;
}

/* Check if gas can swap with fluid (bubble up) */
static bool gas_can_displace_fluid(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);
    
    if (state == STATE_FLUID) return true;
    
    return false;
}

/* =============================================================================
 * Fire Cell Update
 * ============================================================================= */

/* Get fire color based on lifetime */
Color fire_get_color(uint8_t lifetime) {
    int palette_idx = lifetime / 20;  /* Every 20 ticks, shift to cooler color */
    if (palette_idx > 5) palette_idx = 5;
    return FIRE_PALETTE[palette_idx];
}

bool fire_update_cell(Simulation* sim, World* world, int x, int y) {
    if (world_has_flag(world, x, y, FLAG_UPDATED)) {
        return false;
    }
    
    MaterialID mat = world_get_mat(world, x, y);
    
    if (mat != MAT_FIRE) {
        return false;
    }
    
    int idx = IDX(x, y);
    
    /* Increment lifetime */
    if (world->lifetime[idx] < 255) {
        world->lifetime[idx]++;
    }
    
    /* === FIRE DEATH === */
    /* Fire has a chance to die, or forced death at max lifetime */
    bool should_die = (simulation_randf(sim) < FIRE_DIE_CHANCE) || 
                      (world->lifetime[idx] >= FIRE_MAX_LIFETIME);
    
    if (should_die) {
        /* Determine what fire becomes: ash (30%), smoke (50%), empty (20%) */
        float r = simulation_randf(sim);
        if (r < 0.3f) {
            world_set_mat(world, x, y, MAT_ASH);
        } else if (r < 0.8f) {
            world_set_mat(world, x, y, MAT_SMOKE);
        } else {
            world_set_mat(world, x, y, MAT_EMPTY);
        }
        world->lifetime[idx] = 0;
        world_remove_flag(world, x, y, FLAG_BURNING);
        world_add_flag(world, x, y, FLAG_UPDATED);
        world->cells_updated++;
        return true;
    }
    
    /* === SMOKE PRODUCTION === */
    /* Produce smoke above while burning */
    if (simulation_randf(sim) < FIRE_SMOKE_CHANCE) {
        if (IN_BOUNDS(x, y - 1)) {
            MaterialID above = world_get_mat(world, x, y - 1);
            if (material_is_empty(above)) {
                world_set_mat(world, x, y - 1, MAT_SMOKE);
                world_add_flag(world, x, y - 1, FLAG_UPDATED);
            }
        }
    }
    
    /* === FIRE SPREAD === */
    /* Try to spread fire to adjacent flammable cells */
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    for (int i = 0; i < 8; i++) {
        if (simulation_randf(sim) < FIRE_SPREAD_CHANCE) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if (IN_BOUNDS(nx, ny)) {
                MaterialID neighbor = world_get_mat(world, nx, ny);
                if (material_is_flammable(neighbor)) {
                    fire_try_ignite(world, nx, ny);
                }
            }
        }
    }
    
    /* === FIRE MOVEMENT (rises upward) === */
    if (simulation_randf(sim) < FIRE_RISE_CHANCE) {
        int new_x = x, new_y = y;
        bool moved = false;
        
        /* Priority 1: Rise straight up */
        if (gas_can_move_to(world, x, y - 1)) {
            new_y = y - 1;
            moved = true;
        }
        /* Priority 2: Rise diagonally */
        else {
            bool can_ul = gas_can_move_to(world, x - 1, y - 1);
            bool can_ur = gas_can_move_to(world, x + 1, y - 1);
            
            if (can_ul && can_ur) {
                new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
                new_y = y - 1;
                moved = true;
            } else if (can_ul) {
                new_x = x - 1;
                new_y = y - 1;
                moved = true;
            } else if (can_ur) {
                new_x = x + 1;
                new_y = y - 1;
                moved = true;
            }
        }
        
        /* Priority 3: Spread horizontally */
        if (!moved) {
            bool can_l = gas_can_move_to(world, x - 1, y);
            bool can_r = gas_can_move_to(world, x + 1, y);
            
            if (can_l && can_r) {
                new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
                moved = true;
            } else if (can_l) {
                new_x = x - 1;
                moved = true;
            } else if (can_r) {
                new_x = x + 1;
                moved = true;
            }
        }
        
        if (moved) {
            world_swap_cells(world, x, y, new_x, new_y);
            world_add_flag(world, new_x, new_y, FLAG_UPDATED);
            world_add_flag(world, x, y, FLAG_UPDATED);
            world->cells_updated++;
            return true;
        }
    }
    
    world_add_flag(world, x, y, FLAG_UPDATED);
    return true;
}

/* =============================================================================
 * Gas/Smoke Cell Update
 * ============================================================================= */

bool gas_update_cell(Simulation* sim, World* world, int x, int y) {
    if (world_has_flag(world, x, y, FLAG_UPDATED)) {
        return false;
    }
    
    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);
    
    if (state != STATE_GAS) {
        return false;
    }
    
    /* Skip fire - handled separately */
    if (mat == MAT_FIRE) {
        return false;
    }
    
    int idx = IDX(x, y);
    
    /* === SMOKE DISSIPATION === */
    if (mat == MAT_SMOKE) {
        /* Increment lifetime for visual fading */
        if (world->lifetime[idx] < 255) {
            world->lifetime[idx]++;
        }
        
        /* Chance to disappear, higher chance as it ages */
        float dissipate_chance = SMOKE_DISSIPATE_CHANCE * (1.0f + world->lifetime[idx] / 100.0f);
        if (simulation_randf(sim) < dissipate_chance) {
            world_set_mat(world, x, y, MAT_EMPTY);
            world->lifetime[idx] = 0;
            world_add_flag(world, x, y, FLAG_UPDATED);
            world->cells_updated++;
            return true;
        }
    }
    
    /* === STEAM BEHAVIOR === */
    if (mat == MAT_STEAM) {
        /* Steam rises faster and can condense */
        if (world->lifetime[idx] < 255) {
            world->lifetime[idx]++;
        }
        
        /* Check temperature - condense if cool (thermal system will set temp) */
        if (world->temp[idx] < 80.0f) {
            if (simulation_randf(sim) < STEAM_CONDENSE_CHANCE * (100.0f - world->temp[idx]) / 100.0f) {
                world_set_mat(world, x, y, MAT_WATER);
                world->lifetime[idx] = 0;
                world_add_flag(world, x, y, FLAG_UPDATED);
                world->cells_updated++;
                return true;
            }
        }
    }
    
    /* === GAS MOVEMENT === */
    float rise_chance = (mat == MAT_STEAM) ? STEAM_RISE_CHANCE : SMOKE_RISE_CHANCE;
    if (simulation_randf(sim) > rise_chance) {
        return false;
    }
    
    int new_x = x, new_y = y;
    bool moved = false;
    
    /* Priority 1: Rise straight up */
    if (gas_can_move_to(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    /* Priority 2: Rise diagonally */
    else {
        bool can_ul = gas_can_move_to(world, x - 1, y - 1);
        bool can_ur = gas_can_move_to(world, x + 1, y - 1);
        
        if (can_ul && can_ur) {
            new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
            new_y = y - 1;
            moved = true;
        } else if (can_ul) {
            new_x = x - 1;
            new_y = y - 1;
            moved = true;
        } else if (can_ur) {
            new_x = x + 1;
            new_y = y - 1;
            moved = true;
        }
    }
    
    /* Priority 3: Spread horizontally when blocked above */
    if (!moved && simulation_randf(sim) < SMOKE_SPREAD_CHANCE) {
        bool can_l = gas_can_move_to(world, x - 1, y);
        bool can_r = gas_can_move_to(world, x + 1, y);
        
        if (can_l && can_r) {
            new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
            moved = true;
        } else if (can_l) {
            new_x = x - 1;
            moved = true;
        } else if (can_r) {
            new_x = x + 1;
            moved = true;
        }
    }
    
    /* Priority 4: Bubble up through water */
    if (!moved && gas_can_displace_fluid(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    
    if (moved) {
        world_swap_cells(world, x, y, new_x, new_y);
        world_add_flag(world, new_x, new_y, FLAG_UPDATED);
        world_add_flag(world, x, y, FLAG_UPDATED);
        world->cells_updated++;
        return true;
    }
    
    return false;
}

/* =============================================================================
 * Main Update Functions
 * ============================================================================= */

void fire_update(Simulation* sim, World* world) {
    /* Process from bottom to top (fire rises) */
    for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
        int chunk_y = y / CHUNK_SIZE;
        
        for (int x = 0; x < GRID_WIDTH; x++) {
            int chunk_x = x / CHUNK_SIZE;
            
            if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                fire_update_cell(sim, world, x, y);
            }
        }
    }
}

void gas_update(Simulation* sim, World* world) {
    bool scan_left_to_right = simulation_rand(sim) & 1;
    
    /* Process from top to bottom (gas rises, process top first) */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        int chunk_y = y / CHUNK_SIZE;
        
        if (scan_left_to_right) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    gas_update_cell(sim, world, x, y);
                }
            }
        } else {
            for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    gas_update_cell(sim, world, x, y);
                }
            }
        }
    }
}
