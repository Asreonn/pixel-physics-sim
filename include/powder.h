/*
 * powder.h - Powder physics (sand, soil) with rule-based cellular automaton
 */
#ifndef POWDER_H
#define POWDER_H

#include "types.h"
#include "world.h"
#include "simulation.h"

/* =============================================================================
 * Powder Update System
 * 
 * Implements sand behavior per overview.md section A1:
 * - Scan order randomization to avoid directional artifacts
 * - Movement rules with priority (down > down-left/right > stay)
 * - Angle of repose for realistic pile formation
 * - Double-update prevention
 * ============================================================================= */

/* Main powder update function - called by simulation_tick */
void powder_update(Simulation* sim, World* world);

/* Update a single powder cell at (x, y) */
bool powder_update_cell(Simulation* sim, World* world, int x, int y);

/* Check if powder can move to target position */
bool powder_can_move_to(const World* world, int x, int y);

/* Check if powder can displace target (density-based) */
bool powder_can_displace(const World* world, MaterialID source, int target_x, int target_y);

#endif /* POWDER_H */
