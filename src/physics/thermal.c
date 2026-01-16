/*
 * thermal.c - Temperature Simulation and Phase Changes
 *
 * Implements thermal simulation using the modular system:
 *   - cell_ops.h for neighbor iteration
 *   - behavior.h for phase transitions
 *   - grid_iter.h for iteration patterns
 */
#include "physics/thermal.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"
#include <math.h>

/* =============================================================================
 * Phase Change Logic
 * ============================================================================= */

void thermal_check_phase_change(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    float temp = world->temp_next[idx];
    const MaterialProps* props = material_get(mat);

    /* =========================================================================
     * Ice -> Water (Melting)
     * ========================================================================= */
    if (mat == MAT_ICE && bhv_can_melt(mat) && temp > props->melting_temp) {
        StateTransition trans = bhv_get_melt_transition(mat);
        float melt_chance = trans.probability + (temp - props->melting_temp) * 0.002f;

        if (simulation_randf(sim) < melt_chance) {
            world_set_mat(world, x, y, trans.result);
            world->temp_next[idx] -= 10.0f; /* Absorb heat */
        }
    }

    /* =========================================================================
     * Water -> Ice (Freezing)
     * ========================================================================= */
    if (mat == MAT_WATER && bhv_can_freeze(mat) && temp < 0.0f) {
        StateTransition trans = bhv_get_freeze_transition(mat);
        float freeze_chance = trans.probability + (-temp) * 0.001f;

        if (simulation_randf(sim) < freeze_chance) {
            world_set_mat(world, x, y, trans.result);
            world->temp_next[idx] += 5.0f; /* Release heat */
        }
    }

    /* =========================================================================
     * Water -> Steam (Boiling)
     * ========================================================================= */
    if (mat == MAT_WATER && bhv_can_boil(mat) && temp > props->boiling_temp) {
        StateTransition trans = bhv_get_boil_transition(mat);
        float boil_chance = trans.probability + (temp - props->boiling_temp) * 0.005f;

        if (simulation_randf(sim) < boil_chance) {
            world_set_mat(world, x, y, trans.result);
            world->lifetime[idx] = 0;
            world->temp_next[idx] -= 50.0f; /* Absorb lot of heat */
        }
    }

    /* =========================================================================
     * Steam -> Water (Condensation)
     * ========================================================================= */
    if (mat == MAT_STEAM && bhv_can_condense(mat) && temp < 80.0f) {
        StateTransition trans = bhv_get_condense_transition(mat);
        float condense_chance = trans.probability + (80.0f - temp) * 0.001f;

        if (simulation_randf(sim) < condense_chance) {
            world_set_mat(world, x, y, trans.result);
            world->lifetime[idx] = 0;
            world->temp_next[idx] += 20.0f; /* Release heat */
        }
    }
}

/* =============================================================================
 * Heat Diffusion Callback
 * ============================================================================= */

static bool thermal_diffusion_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)sim;
    (void)userdata;

    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    float temp = world->temp[idx];

    /* Fire produces constant heat */
    if (mat == MAT_FIRE) {
        world->temp_next[idx] = FIRE_TEMPERATURE;
        return true;
    }

    /* Empty cells cool to ambient quickly */
    if (mat == MAT_EMPTY) {
        world->temp_next[idx] = temp + (AMBIENT_TEMP - temp) * 0.1f;
        return true;
    }

    const MaterialProps* props = material_get(mat);
    float conductivity = props->conductivity;

    /* No heat transfer for non-conductive materials */
    if (conductivity <= 0.001f) {
        world->temp_next[idx] = temp;
        return true;
    }

    /* Calculate heat exchange with 4-directional neighbors */
    float heat_in = 0.0f;
    int neighbor_count = 0;

    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];

        if (!IN_BOUNDS(nx, ny)) continue;

        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        float ntemp = world->temp[nidx];
        float ncond = material_get(nmat)->conductivity;

        /* Effective conductivity is geometric mean */
        float eff_cond = (conductivity * ncond > 0) ? sqrtf(conductivity * ncond) : 0;

        /* Heat flows from hot to cold */
        heat_in += (ntemp - temp) * eff_cond;
        neighbor_count++;
    }

    /* Apply heat diffusion */
    if (neighbor_count > 0) {
        float delta_temp = heat_in * HEAT_DIFFUSION_RATE / neighbor_count;
        float thermal_mass = (props->heat_capacity < 0.1f) ? 0.1f : props->heat_capacity;
        world->temp_next[idx] = temp + delta_temp / thermal_mass;
    } else {
        world->temp_next[idx] = temp;
    }

    /* Gradual cooling to ambient */
    world->temp_next[idx] += (AMBIENT_TEMP - world->temp_next[idx]) * AMBIENT_COOLING_RATE;

    /* Clamp temperature */
    world->temp_next[idx] = CLAMP(world->temp_next[idx], MIN_TEMPERATURE, MAX_TEMPERATURE);

    return true;
}

/* =============================================================================
 * Phase Change Callback
 * ============================================================================= */

static bool thermal_phase_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    thermal_check_phase_change(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Thermal Update
 * ============================================================================= */

void thermal_update(Simulation* sim, World* world) {
    /* Pass 1: Heat diffusion */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_diffusion_callback, NULL);

    /* Pass 2: Phase changes */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_phase_callback, NULL);

    /* Swap temperature buffers */
    float* tmp = world->temp;
    world->temp = world->temp_next;
    world->temp_next = tmp;
}
