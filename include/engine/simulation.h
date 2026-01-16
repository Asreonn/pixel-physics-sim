/*
 * simulation.h - Fixed timestep simulation loop
 */
#ifndef SIMULATION_H
#define SIMULATION_H

#include "core/types.h"
#include "world/world.h"

/* =============================================================================
 * Simulation State
 * ============================================================================= */

typedef struct {
    /* Timing */
    double tick_hz;           /* Simulation frequency */
    double dt;                /* Fixed timestep (1/tick_hz) */
    double accumulator;       /* Time accumulator for fixed step */
    uint64_t tick_count;      /* Total ticks simulated */
    
    /* RNG state for deterministic simulation */
    uint32_t rng_state;
    uint32_t tick_seed;       /* Per-tick seed for reproducibility */
    
    /* Performance tracking */
    double tick_time_ms;      /* Last tick duration in ms */
    double avg_tick_time_ms;  /* Running average */
    
    /* Profiling - subsystem times in microseconds */
    double profile_powder_us;
    double profile_fluid_us;
    double profile_fire_us;
    double profile_gas_us;
    double profile_total_us;
    
    /* Simulation state */
    bool paused;
    bool step_once;           /* Execute single step when paused */
    
} Simulation;

/* =============================================================================
 * Simulation Functions
 * ============================================================================= */

/* Create and initialize simulation state */
Simulation* simulation_create(double tick_hz);

/* Destroy simulation */
void simulation_destroy(Simulation* sim);

/* Update simulation with real time delta (handles accumulator) */
void simulation_update(Simulation* sim, World* world, double real_dt);

/* Execute a single simulation tick */
void simulation_tick(Simulation* sim, World* world);

/* Pause/unpause simulation */
void simulation_set_paused(Simulation* sim, bool paused);

/* Toggle pause state */
void simulation_toggle_pause(Simulation* sim);

/* Step one tick when paused */
void simulation_step_once(Simulation* sim);

/* Get RNG value for this tick (deterministic) */
uint32_t simulation_rand(Simulation* sim);

/* Get random float [0, 1) */
float simulation_randf(Simulation* sim);

/* Get random int in range [min, max] inclusive */
int simulation_rand_range(Simulation* sim, int min, int max);

/* Reset simulation state */
void simulation_reset(Simulation* sim);

#endif /* SIMULATION_H */
