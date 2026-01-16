/*
 * fire.c - Fire and Gas Physics Implementation
 *
 * Implements fire and gas behavior using the modular system:
 *   - cell_ops.h for movement operations
 *   - behavior.h for reactions and state transitions
 *   - grid_iter.h for iteration patterns
 */
#include "subsystems/fire.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"

/* =============================================================================
 * Fire Configuration
 * ============================================================================= */

#define FIRE_RISE_CHANCE    0.6f    /* Chance to rise each tick */
#define FIRE_DIE_CHANCE     0.02f   /* Chance to die each tick */
#define FIRE_SPREAD_CHANCE  0.03f   /* Chance to spread to adjacent */
#define FIRE_SMOKE_CHANCE   0.15f   /* Chance to produce smoke */
#define FIRE_MAX_LIFETIME   120     /* Max ticks before forced death */

/* =============================================================================
 * Gas Configuration
 * ============================================================================= */

#define SMOKE_DISSIPATE_CHANCE 0.006f   /* Chance to disappear */
#define SMOKE_RISE_CHANCE      0.85f    /* Chance to rise */
#define SMOKE_SPREAD_CHANCE    0.3f     /* Chance to spread horizontally */

#define STEAM_RISE_CHANCE      0.9f     /* Very high chance to rise */
#define STEAM_CONDENSE_CHANCE  0.01f    /* Chance to condense */
#define STEAM_CONDENSE_TEMP    80.0f    /* Temperature below which steam condenses */

/* =============================================================================
 * Fire Color Palette
 * ============================================================================= */

static const Color FIRE_PALETTE[6] = {
    {255, 255, 200, 255},   /* White-yellow (young/hot) */
    {255, 220, 100, 255},   /* Bright yellow */
    {255, 150,  50, 255},   /* Orange */
    {255,  80,  20, 255},   /* Red-orange */
    {200,  50,  20, 255},   /* Dark red */
    {100,  30,  10, 255},   /* Very dark (dying) */
};

Color fire_get_color(uint8_t lifetime) {
    int idx = lifetime / 20;
    if (idx > 5) idx = 5;
    return FIRE_PALETTE[idx];
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

bool material_is_flammable(MaterialID mat) {
    return bhv_is_flammable(mat);
}

bool fire_try_ignite(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;

    MaterialID mat = world_get_mat(world, x, y);

    if (bhv_is_flammable(mat)) {
        world_set_mat(world, x, y, MAT_FIRE);
        world_add_flag(world, x, y, FLAG_BURNING);
        return true;
    }

    return false;
}

/* =============================================================================
 * Gas Movement Helper
 * ============================================================================= */

static bool gas_try_move(Simulation* sim, World* world, int x, int y) {
    int new_x = x, new_y = y;
    bool moved = false;

    /* Priority 1: Rise straight up */
    if (cell_gas_can_enter(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    /* Priority 2: Rise diagonally */
    else {
        bool can_ul = cell_gas_can_enter(world, x - 1, y - 1);
        bool can_ur = cell_gas_can_enter(world, x + 1, y - 1);

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
        bool can_l = cell_gas_can_enter(world, x - 1, y);
        bool can_r = cell_gas_can_enter(world, x + 1, y);

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
        cell_move(world, x, y, new_x, new_y);
        return true;
    }

    return false;
}

/* =============================================================================
 * Fire Cell Update
 * ============================================================================= */

bool fire_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
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

    /* =========================================================================
     * Fire Death Check
     * ========================================================================= */
    bool should_die = (simulation_randf(sim) < FIRE_DIE_CHANCE) ||
                      (world->lifetime[idx] >= FIRE_MAX_LIFETIME);

    if (should_die) {
        FireDeathProducts products = bhv_get_fire_death();
        float r = simulation_randf(sim);

        if (r < products.ash_chance) {
            world_set_mat(world, x, y, products.ash);
        } else if (r < products.ash_chance + products.smoke_chance) {
            world_set_mat(world, x, y, products.smoke);
        } else {
            world_set_mat(world, x, y, MAT_EMPTY);
        }

        world->lifetime[idx] = 0;
        world_remove_flag(world, x, y, FLAG_BURNING);
        cell_mark_updated(world, x, y);
        world->cells_updated++;
        return true;
    }

    /* =========================================================================
     * Smoke Production
     * ========================================================================= */
    if (simulation_randf(sim) < FIRE_SMOKE_CHANCE) {
        if (IN_BOUNDS(x, y - 1) && cell_is_empty(world, x, y - 1)) {
            world_set_mat(world, x, y - 1, MAT_SMOKE);
            cell_mark_updated(world, x, y - 1);
        }
    }

    /* =========================================================================
     * Fire Spread
     * ========================================================================= */
    for (int i = 0; i < 8; i++) {
        if (simulation_randf(sim) < FIRE_SPREAD_CHANCE) {
            int nx = x + NEIGHBOR8_DX[i];
            int ny = y + NEIGHBOR8_DY[i];

            if (IN_BOUNDS(nx, ny)) {
                MaterialID neighbor = world_get_mat(world, nx, ny);
                if (bhv_is_flammable(neighbor)) {
                    fire_try_ignite(world, nx, ny);
                }
            }
        }
    }

    /* =========================================================================
     * Fire Movement (rises)
     * ========================================================================= */
    if (simulation_randf(sim) < FIRE_RISE_CHANCE) {
        if (gas_try_move(sim, world, x, y)) {
            return true;
        }
    }

    cell_mark_updated(world, x, y);
    return true;
}

/* =============================================================================
 * Gas/Smoke Cell Update
 * ============================================================================= */

bool gas_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);

    if (state != STATE_GAS || mat == MAT_FIRE) {
        return false;
    }

    int idx = IDX(x, y);

    /* Increment lifetime */
    if (world->lifetime[idx] < 255) {
        world->lifetime[idx]++;
    }

    /* =========================================================================
     * Smoke Dissipation
     * ========================================================================= */
    if (mat == MAT_SMOKE) {
        float dissipate_chance = SMOKE_DISSIPATE_CHANCE * (1.0f + world->lifetime[idx] / 100.0f);
        if (simulation_randf(sim) < dissipate_chance) {
            world_set_mat(world, x, y, MAT_EMPTY);
            world->lifetime[idx] = 0;
            cell_mark_updated(world, x, y);
            world->cells_updated++;
            return true;
        }
    }

    /* =========================================================================
     * Steam Condensation
     * ========================================================================= */
    if (mat == MAT_STEAM) {
        float temp = world->temp[idx];
        if (temp < STEAM_CONDENSE_TEMP) {
            float condense_chance = STEAM_CONDENSE_CHANCE * (STEAM_CONDENSE_TEMP - temp) / STEAM_CONDENSE_TEMP;
            if (simulation_randf(sim) < condense_chance) {
                world_set_mat(world, x, y, MAT_WATER);
                world->lifetime[idx] = 0;
                cell_mark_updated(world, x, y);
                world->cells_updated++;
                return true;
            }
        }
    }

    /* =========================================================================
     * Gas Movement
     * ========================================================================= */
    float rise_chance = (mat == MAT_STEAM) ? STEAM_RISE_CHANCE : SMOKE_RISE_CHANCE;
    if (simulation_randf(sim) > rise_chance) {
        return false;
    }

    int new_x = x, new_y = y;
    bool moved = false;

    /* Priority 1: Rise straight up */
    if (cell_gas_can_enter(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    /* Priority 2: Rise diagonally */
    else {
        bool can_ul = cell_gas_can_enter(world, x - 1, y - 1);
        bool can_ur = cell_gas_can_enter(world, x + 1, y - 1);

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
    if (!moved && simulation_randf(sim) < SMOKE_SPREAD_CHANCE) {
        bool can_l = cell_gas_can_enter(world, x - 1, y);
        bool can_r = cell_gas_can_enter(world, x + 1, y);

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

    /* Priority 4: Bubble through fluid */
    if (!moved && cell_is_fluid(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }

    if (moved) {
        cell_move(world, x, y, new_x, new_y);
        return true;
    }

    return false;
}

/* =============================================================================
 * Grid Update Callbacks
 * ============================================================================= */

static bool fire_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    fire_update_cell(sim, world, x, y);
    return true;
}

static bool gas_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    gas_update_cell(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Update Functions
 * ============================================================================= */

void fire_update(Simulation* sim, World* world) {
    /* Fire rises, process bottom to top */
    grid_iterate_falling(sim, world, fire_cell_callback, NULL);
}

void gas_update(Simulation* sim, World* world) {
    /* Gas rises, process top to bottom */
    grid_iterate_rising(sim, world, gas_cell_callback, NULL);
}
