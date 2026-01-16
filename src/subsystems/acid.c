/*
 * acid.c - Acid Physics Implementation
 *
 * Implements acid corrosion using the modular system:
 *   - cell_ops.h for neighbor iteration
 *   - behavior.h for corrosion rules
 *   - grid_iter.h for iteration patterns
 */
#include "subsystems/acid.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"

/* =============================================================================
 * Acid Configuration
 * ============================================================================= */

#define ACID_CORRODE_CHANCE   0.08f   /* Chance to corrode per tick */
#define ACID_SURVIVE_CHANCE   0.5f    /* Chance acid survives reaction */
#define ACID_SMOKE_CHANCE     0.5f    /* Chance to produce smoke */

/* =============================================================================
 * Corrosion Helpers
 * ============================================================================= */

bool material_is_corrodible(MaterialID mat) {
    return bhv_is_corrodible(mat);
}

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool acid_update_cell(Simulation* sim, World* world, int x, int y) {
    MaterialID mat = world_get_mat(world, x, y);

    if (mat != MAT_ACID) {
        return false;
    }

    /* Check all 8 neighbors for corrodible materials */
    for (int i = 0; i < 8; i++) {
        int nx = x + NEIGHBOR8_DX[i];
        int ny = y + NEIGHBOR8_DY[i];

        if (!IN_BOUNDS(nx, ny)) continue;

        MaterialID neighbor = world_get_mat(world, nx, ny);

        if (bhv_is_corrodible(neighbor)) {
            /* Roll for corrosion */
            if (simulation_randf(sim) < ACID_CORRODE_CHANCE) {
                /* Get reaction details */
                ReactionRule reaction = bhv_get_corrosion_reaction(neighbor);

                /* Apply result to target */
                if (simulation_randf(sim) < reaction.byproduct_chance) {
                    world_set_mat(world, nx, ny, reaction.byproduct);
                    world->lifetime[IDX(nx, ny)] = 0;
                } else {
                    world_set_mat(world, nx, ny, MAT_EMPTY);
                }

                /* Acid consumption */
                if (simulation_randf(sim) > ACID_SURVIVE_CHANCE) {
                    world_set_mat(world, x, y, MAT_EMPTY);
                }

                /* Mark cells as updated */
                cell_mark_updated(world, x, y);
                cell_mark_updated(world, nx, ny);
                world->cells_updated++;

                return true;
            }
        }
    }

    return false;
}

/* =============================================================================
 * Grid Update Callback
 * ============================================================================= */

static bool acid_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    acid_update_cell(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void acid_update(Simulation* sim, World* world) {
    /* Process acid corrosion (movement handled by fluid_update) */
    grid_iterate_falling(sim, world, acid_cell_callback, NULL);
}
