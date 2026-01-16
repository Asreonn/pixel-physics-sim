/*
 * fluid.h - Fluid physics (water) with cellular automaton approach
 */
#ifndef FLUID_H
#define FLUID_H

#include "types.h"
#include "world.h"
#include "simulation.h"

/* =============================================================================
 * Fluid Update System
 * 
 * Simple cellular automaton for water:
 * - Falls down due to gravity
 * - Spreads horizontally when blocked below
 * - Equalizes water levels
 * ============================================================================= */

/* Main fluid update function - called by simulation_tick */
void fluid_update(Simulation* sim, World* world);

/* Update a single fluid cell at (x, y) */
bool fluid_update_cell(Simulation* sim, World* world, int x, int y);

/* Check if fluid can move to target position */
bool fluid_can_move_to(const World* world, int x, int y);

#endif /* FLUID_H */
