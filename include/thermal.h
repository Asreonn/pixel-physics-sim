/*
 * thermal.h - Temperature simulation and phase changes
 */
#ifndef THERMAL_H
#define THERMAL_H

#include "types.h"
#include "world.h"
#include "simulation.h"

/* =============================================================================
 * Temperature System
 * 
 * Features:
 * - Heat diffusion between cells
 * - Phase changes (ice->water->steam)
 * - Fire produces heat
 * - Materials have thermal conductivity and heat capacity
 * ============================================================================= */

/* Temperature constants */
#define AMBIENT_TEMP 20.0f          /* Room temperature in Celsius */
#define HEAT_DIFFUSION_RATE 0.15f   /* How fast heat spreads */
#define FIRE_TEMPERATURE 800.0f     /* Temperature of fire */

/* Main thermal update function */
void thermal_update(Simulation* sim, World* world);

/* Check and apply phase changes for a cell */
void thermal_check_phase_change(Simulation* sim, World* world, int x, int y);

#endif /* THERMAL_H */
