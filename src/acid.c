/*
 * acid.c - Acid physics (corrosion)
 */
#include "acid.h"
#include "material.h"

/* Corrosion settings */
#define ACID_CORRODE_CHANCE 0.08f    /* Chance to corrode per tick */
#define ACID_SMOKE_CHANCE 0.5f       /* Chance to produce smoke when corroding */

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

bool material_is_corrodible(MaterialID mat) {
    /* Acid can corrode: Stone, Wood, Sand, Soil */
    switch (mat) {
        case MAT_STONE:
        case MAT_WOOD:
        case MAT_SAND:
        case MAT_SOIL:
            return true;
        default:
            return false;
    }
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool acid_update_cell(Simulation* sim, World* world, int x, int y) {
    /* Only process acid cells */
    MaterialID mat = world_get_mat(world, x, y);
    if (mat != MAT_ACID) {
        return false;
    }
    
    /* Check all 8 neighbors for corrodible materials */
    int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
    
    for (int i = 0; i < 8; i++) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        
        if (!IN_BOUNDS(nx, ny)) continue;
        
        MaterialID neighbor = world_get_mat(world, nx, ny);
        
        if (material_is_corrodible(neighbor)) {
            /* Roll for corrosion */
            if (simulation_randf(sim) < ACID_CORRODE_CHANCE) {
                /* Corrode the target - turn it to empty or smoke */
                if (simulation_randf(sim) < ACID_SMOKE_CHANCE) {
                    world_set_mat(world, nx, ny, MAT_SMOKE);
                    world->lifetime[IDX(nx, ny)] = 0;
                } else {
                    world_set_mat(world, nx, ny, MAT_EMPTY);
                }
                
                /* Acid is consumed in the reaction */
                /* 50% chance acid survives (weakened) */
                if (simulation_randf(sim) < 0.5f) {
                    world_set_mat(world, x, y, MAT_EMPTY);
                }
                
                /* Mark cells as updated */
                world_add_flag(world, x, y, FLAG_UPDATED);
                world_add_flag(world, nx, ny, FLAG_UPDATED);
                world->cells_updated++;
                
                return true;
            }
        }
    }
    
    return false;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void acid_update(Simulation* sim, World* world) {
    /* Process acid corrosion (movement is handled by fluid_update) */
    bool scan_left_to_right = simulation_rand(sim) & 1;
    
    for (int y = GRID_HEIGHT - 1; y >= 0; y--) {
        int chunk_y = y / CHUNK_SIZE;
        
        if (scan_left_to_right) {
            for (int x = 0; x < GRID_WIDTH; x++) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    acid_update_cell(sim, world, x, y);
                }
            }
        } else {
            for (int x = GRID_WIDTH - 1; x >= 0; x--) {
                int chunk_x = x / CHUNK_SIZE;
                if (world_is_chunk_active(world, chunk_x, chunk_y)) {
                    acid_update_cell(sim, world, x, y);
                }
            }
        }
    }
}
