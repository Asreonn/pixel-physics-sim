/*
 * acid.h - Acid physics (corrosion)
 */
#ifndef ACID_H
#define ACID_H

#include "types.h"
#include "world.h"
#include "simulation.h"

/* =============================================================================
 * Acid System
 * 
 * Acid behavior:
 * - Flows like water
 * - Corrodes certain materials on contact
 * - Both acid and target are consumed in the reaction
 * - Produces smoke as byproduct
 * ============================================================================= */

/* Main acid update function (corrosion only, movement handled by fluid.c) */
void acid_update(Simulation* sim, World* world);

/* Update a single acid cell for corrosion */
bool acid_update_cell(Simulation* sim, World* world, int x, int y);

/* Check if a material can be corroded by acid */
bool material_is_corrodible(MaterialID mat);

#endif /* ACID_H */
