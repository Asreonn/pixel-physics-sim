/*
 * material.h - Data-driven material system
 */
#ifndef MATERIAL_H
#define MATERIAL_H

#include "types.h"

/* =============================================================================
 * Material Properties Structure (data-driven)
 * ============================================================================= */

typedef struct {
    /* Identity */
    MaterialID id;
    const char* name;
    MaterialState state;
    
    /* Visual */
    Color base_color;
    uint8_t color_variation;  /* Random variation in color (0-255) */
    
    /* Mechanical (real-world-ish values) */
    float density;            /* kg/m^3 (scaled) */
    float friction;           /* 0-1, for solids/powders */
    float restitution;        /* bounce factor (future use) */
    float cohesion;           /* 0-1, clumpiness for powders */
    float viscosity;          /* Pa*s (scaled) */
    
    /* Motion parameters (derived from real-world properties) */
    float gravity_scale;      /* multiplier for gravity (1.0 = normal) */
    float drag_coeff;         /* 0-1, air resistance */
    float terminal_velocity;  /* cells/tick (max speed) */
    float flow_rate;          /* 0-1, horizontal flow chance (fluids/gases) */
    
    /* Fixed-point cached values (8.8) */
    Fixed8 gravity_step_fixed;
    Fixed8 drag_factor_fixed;
    Fixed8 terminal_velocity_fixed;
    
    /* Powder-specific */
    float settle_probability; /* 0-1, chance to stop jittering */
    float slide_bias;         /* 0-1, randomness in left/right slide */
    
    /* Thermal (future use, but define now) */
    float conductivity;       /* k - heat transfer rate */
    float heat_capacity;      /* c - thermal mass */
    float ignition_temp;      /* Temperature to catch fire */
    float burn_rate;          /* Fuel consumption rate */
    float smoke_rate;         /* Smoke production when burning */
    float melting_temp;       /* Phase change temp */
    float boiling_temp;       /* Phase change temp */
    
} MaterialProps;

/* =============================================================================
 * Material Table Access
 * ============================================================================= */

/* Initialize material table with default values */
void material_init(void);

/* Get material properties by ID */
const MaterialProps* material_get(MaterialID id);

/* Get material state by ID (convenience) */
MaterialState material_state(MaterialID id);

/* Check if material is a powder type */
bool material_is_powder(MaterialID id);

/* Check if material is a fluid type */
bool material_is_fluid(MaterialID id);

/* Check if material is a solid type */
bool material_is_solid(MaterialID id);

/* Check if material is empty/air */
bool material_is_empty(MaterialID id);

/* Check if material is a gas type */
bool material_is_gas(MaterialID id);

/* Get color for material (with optional variation) */
Color material_color(MaterialID id, uint32_t seed);

#endif /* MATERIAL_H */
