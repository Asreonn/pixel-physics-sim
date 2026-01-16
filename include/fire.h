/*
 * fire.h - Fire and smoke physics
 */
#ifndef FIRE_H
#define FIRE_H

#include "types.h"
#include "world.h"
#include "simulation.h"

/* =============================================================================
 * Fire and Gas Update System
 * 
 * Fire behavior:
 * - Burns fuel (wood), produces heat
 * - Spreads to adjacent flammable materials
 * - Produces smoke
 * - Dies when fuel exhausted
 * 
 * Smoke/Gas behavior:
 * - Rises upward (opposite of gravity)
 * - Spreads horizontally when blocked
 * - Dissipates over time
 * ============================================================================= */

/* Main fire update function */
void fire_update(Simulation* sim, World* world);

/* Main gas/smoke update function */
void gas_update(Simulation* sim, World* world);

/* Update a single fire cell */
bool fire_update_cell(Simulation* sim, World* world, int x, int y);

/* Update a single gas/smoke cell */
bool gas_update_cell(Simulation* sim, World* world, int x, int y);

/* Try to ignite a cell (if flammable) */
bool fire_try_ignite(World* world, int x, int y);

/* Check if material is flammable */
bool material_is_flammable(MaterialID mat);

/* Get fire color based on lifetime (for animated fire) */
Color fire_get_color(uint8_t lifetime);

#endif /* FIRE_H */
