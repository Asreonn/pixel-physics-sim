/*
 * thermal.c - Temperature simulation and phase changes
 */
#include "thermal.h"
#include "material.h"
#include <math.h>

/* =============================================================================
 * Phase Change Logic
 * ============================================================================= */

void thermal_check_phase_change(Simulation* sim, World* world, int x, int y) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    float temp = world->temp_next[idx];
    const MaterialProps* props = material_get(mat);
    
    /* Ice melting -> Water */
    if (mat == MAT_ICE && temp > props->melting_temp) {
        /* Higher chance to melt the hotter it is */
        float melt_chance = 0.01f + (temp - props->melting_temp) * 0.002f;
        if (simulation_randf(sim) < melt_chance) {
            world_set_mat(world, x, y, MAT_WATER);
            /* Melting absorbs heat */
            world->temp_next[idx] -= 10.0f;
        }
    }
    
    /* Water freezing -> Ice */
    if (mat == MAT_WATER && temp < 0.0f) {
        float freeze_chance = 0.005f + (-temp) * 0.001f;
        if (simulation_randf(sim) < freeze_chance) {
            world_set_mat(world, x, y, MAT_ICE);
            /* Freezing releases heat */
            world->temp_next[idx] += 5.0f;
        }
    }
    
    /* Water boiling -> Steam */
    if (mat == MAT_WATER && temp > props->boiling_temp) {
        float boil_chance = 0.02f + (temp - props->boiling_temp) * 0.005f;
        if (simulation_randf(sim) < boil_chance) {
            world_set_mat(world, x, y, MAT_STEAM);
            world->lifetime[idx] = 0;
            /* Boiling absorbs a lot of heat */
            world->temp_next[idx] -= 50.0f;
        }
    }
    
    /* Steam condensing -> Water (handled in fire.c/gas_update_cell) */
    /* But we can also check here for extra realism */
    if (mat == MAT_STEAM && temp < 80.0f) {
        float condense_chance = 0.01f + (80.0f - temp) * 0.001f;
        if (simulation_randf(sim) < condense_chance) {
            world_set_mat(world, x, y, MAT_WATER);
            world->lifetime[idx] = 0;
            /* Condensation releases heat */
            world->temp_next[idx] += 20.0f;
        }
    }
}

/* =============================================================================
 * Main Thermal Update
 * ============================================================================= */

void thermal_update(Simulation* sim, World* world) {
    /* Heat diffusion pass - only process active chunks for performance */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        int chunk_y = y / CHUNK_SIZE;
        for (int x = 0; x < GRID_WIDTH; x++) {
            int chunk_x = x / CHUNK_SIZE;
            
            /* Skip inactive chunks */
            if (!world_is_chunk_active(world, chunk_x, chunk_y)) {
                continue;
            }
            int idx = IDX(x, y);
            MaterialID mat = world->mat[idx];
            float temp = world->temp[idx];
            
            /* Fire produces heat */
            if (mat == MAT_FIRE) {
                world->temp_next[idx] = FIRE_TEMPERATURE;
                continue;
            }
            
            /* Empty cells cool to ambient quickly */
            if (mat == MAT_EMPTY) {
                world->temp_next[idx] = temp + (AMBIENT_TEMP - temp) * 0.1f;
                continue;
            }
            
            const MaterialProps* props = material_get(mat);
            float conductivity = props->conductivity;
            
            /* If conductivity is 0, no heat transfer */
            if (conductivity <= 0.001f) {
                world->temp_next[idx] = temp;
                continue;
            }
            
            /* Calculate heat exchange with neighbors */
            float heat_in = 0.0f;
            int neighbor_count = 0;
            
            /* 4-directional neighbors */
            int dx[] = {-1, 1, 0, 0};
            int dy[] = {0, 0, -1, 1};
            
            for (int i = 0; i < 4; i++) {
                int nx = x + dx[i];
                int ny = y + dy[i];
                
                if (!IN_BOUNDS(nx, ny)) continue;
                
                int nidx = IDX(nx, ny);
                MaterialID nmat = world->mat[nidx];
                float ntemp = world->temp[nidx];
                
                /* Get neighbor conductivity */
                float ncond = material_get(nmat)->conductivity;
                
                /* Effective conductivity is geometric mean */
                float eff_cond = (conductivity * ncond > 0) ? 
                                 sqrtf(conductivity * ncond) : 0;
                
                /* Heat flows from hot to cold */
                heat_in += (ntemp - temp) * eff_cond;
                neighbor_count++;
            }
            
            /* Apply heat diffusion */
            if (neighbor_count > 0) {
                float delta_temp = heat_in * HEAT_DIFFUSION_RATE / neighbor_count;
                
                /* Thermal mass affects how much temperature changes */
                float thermal_mass = props->heat_capacity;
                if (thermal_mass < 0.1f) thermal_mass = 0.1f;
                
                world->temp_next[idx] = temp + delta_temp / thermal_mass;
            } else {
                world->temp_next[idx] = temp;
            }
            
            /* Gradually cool to ambient (heat loss to environment) */
            world->temp_next[idx] += (AMBIENT_TEMP - world->temp_next[idx]) * 0.001f;
            
            /* Clamp temperature to reasonable range */
            if (world->temp_next[idx] < -100.0f) world->temp_next[idx] = -100.0f;
            if (world->temp_next[idx] > 2000.0f) world->temp_next[idx] = 2000.0f;
        }
    }
    
    /* Phase change pass - only process active chunks */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        int chunk_y = y / CHUNK_SIZE;
        for (int x = 0; x < GRID_WIDTH; x++) {
            int chunk_x = x / CHUNK_SIZE;
            if (!world_is_chunk_active(world, chunk_x, chunk_y)) {
                continue;
            }
            thermal_check_phase_change(sim, world, x, y);
        }
    }
    
    /* Swap temperature buffers */
    float* tmp = world->temp;
    world->temp = world->temp_next;
    world->temp_next = tmp;
}
