/*
 * physics.h - Shared Physics Calculations Module
 *
 * Provides unified physics calculations for gravity, velocity,
 * displacement, and momentum across all subsystems.
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include "core/types.h"
#include "world/world.h"
#include "materials/material.h"

/* =============================================================================
 * Physics Constants
 * ============================================================================= */

#define PHYS_GRAVITY_DEFAULT    0.08f       /* Default gravity acceleration */
#define PHYS_MAX_VELOCITY       4.0f        /* Maximum velocity magnitude */
#define PHYS_MIN_VELOCITY       0.01f       /* Velocity below this is zeroed */
#define PHYS_IMPACT_THRESHOLD   1.5f        /* Velocity threshold for impact effects */

/* =============================================================================
 * Velocity Structure
 *
 * Wrapper for velocity operations with both float and fixed-point support.
 * ============================================================================= */

typedef struct {
    float vx;
    float vy;
} Velocity;

/* Convert between float and fixed-point velocity */
static inline Velocity velocity_from_fixed(Fixed8 vx, Fixed8 vy) {
    return (Velocity){
        .vx = FIXED_TO_FLOAT(vx),
        .vy = FIXED_TO_FLOAT(vy)
    };
}

static inline void velocity_to_fixed(Velocity v, Fixed8* vx, Fixed8* vy) {
    *vx = FIXED_FROM_FLOAT(v.vx);
    *vy = FIXED_FROM_FLOAT(v.vy);
}

/* Get velocity at cell position */
static inline Velocity phys_get_velocity(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return (Velocity){0, 0};
    int idx = IDX(x, y);
    return velocity_from_fixed(world->vel_x[idx], world->vel_y[idx]);
}

/* Set velocity at cell position */
static inline void phys_set_velocity(World* world, int x, int y, Velocity v) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    velocity_to_fixed(v, &world->vel_x[idx], &world->vel_y[idx]);
}

/* Reset velocity at cell position */
static inline void phys_reset_velocity(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    world->vel_x[idx] = 0;
    world->vel_y[idx] = 0;
}

/* =============================================================================
 * Gravity Application
 * ============================================================================= */

typedef struct {
    float gravity_scale;        /* Multiplier for gravity (1.0 = normal, -1.0 = rises) */
    float drag_coeff;           /* Air resistance (0 = no drag, 1 = full stop) */
    float terminal_velocity;    /* Maximum speed */
} GravityParams;

/* Get gravity params from material */
static inline GravityParams phys_gravity_params(const MaterialProps* props) {
    return (GravityParams){
        .gravity_scale = props->gravity_scale,
        .drag_coeff = props->drag_coeff,
        .terminal_velocity = props->terminal_velocity
    };
}

/* Apply gravity to velocity and return updated velocity */
static inline Velocity phys_apply_gravity(Velocity v, GravityParams params, float dt) {
    /* Apply gravity */
    float gravity = PHYS_GRAVITY_DEFAULT * params.gravity_scale;
    v.vy += gravity;

    /* Apply drag */
    float drag_factor = 1.0f - params.drag_coeff;
    v.vx *= drag_factor;
    v.vy *= drag_factor;

    /* Clamp to terminal velocity */
    float term = params.terminal_velocity;
    v.vx = CLAMP(v.vx, -term, term);
    v.vy = CLAMP(v.vy, -term, term);

    /* Zero out very small velocities */
    if (v.vx > -PHYS_MIN_VELOCITY && v.vx < PHYS_MIN_VELOCITY) v.vx = 0;
    if (v.vy > -PHYS_MIN_VELOCITY && v.vy < PHYS_MIN_VELOCITY) v.vy = 0;

    (void)dt; /* For future use with proper time integration */
    return v;
}

/* Apply gravity directly to world cell using fixed-point */
static inline void phys_apply_gravity_fixed(World* world, int x, int y,
                                             const MaterialProps* props) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);

    world->vel_y[idx] += props->gravity_step_fixed;
    world->vel_y[idx] = FIXED_MUL(world->vel_y[idx], props->drag_factor_fixed);
    world->vel_y[idx] = CLAMP(world->vel_y[idx],
                               -props->terminal_velocity_fixed,
                               props->terminal_velocity_fixed);
}

/* =============================================================================
 * Movement Step Calculation
 *
 * Calculate how many cells to move based on velocity.
 * ============================================================================= */

typedef struct {
    int steps;          /* Number of cells to move */
    int direction;      /* -1, 0, or 1 */
} MovementSteps;

/* Calculate vertical movement steps from velocity */
static inline MovementSteps phys_calc_fall_steps(const World* world, int x, int y,
                                                  int max_steps) {
    if (!IN_BOUNDS(x, y)) return (MovementSteps){0, 0};

    int idx = IDX(x, y);
    Fixed8 vy = world->vel_y[idx];

    int steps = (int)(FIXED_ABS(vy) >> FIXED_SHIFT);
    steps = CLAMP(steps, 0, max_steps);
    if (steps == 0) steps = 1;

    int dir = (vy > 0) ? 1 : ((vy < 0) ? -1 : 1);

    return (MovementSteps){steps, dir};
}

/* Calculate horizontal movement steps from velocity */
static inline MovementSteps phys_calc_horizontal_steps(const World* world, int x, int y,
                                                        int max_steps) {
    if (!IN_BOUNDS(x, y)) return (MovementSteps){0, 0};

    int idx = IDX(x, y);
    Fixed8 vx = world->vel_x[idx];

    int steps = (int)(FIXED_ABS(vx) >> FIXED_SHIFT);
    steps = CLAMP(steps, 0, max_steps);

    int dir = (vx > 0) ? 1 : ((vx < 0) ? -1 : 0);

    return (MovementSteps){steps, dir};
}

/* =============================================================================
 * Collision Response
 * ============================================================================= */

typedef enum {
    COLLISION_NONE      = 0,
    COLLISION_STOP      = 1,    /* Full stop */
    COLLISION_BOUNCE    = 2,    /* Reverse velocity */
    COLLISION_SLIDE     = 3,    /* Transfer to perpendicular axis */
} CollisionType;

/* Handle collision on vertical axis */
static inline void phys_collision_vertical(World* world, int x, int y,
                                            CollisionType type, float restitution) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);

    switch (type) {
        case COLLISION_STOP:
            world->vel_y[idx] = 0;
            break;
        case COLLISION_BOUNCE:
            world->vel_y[idx] = FIXED_MUL(world->vel_y[idx],
                                           FIXED_FROM_FLOAT(-restitution));
            break;
        case COLLISION_SLIDE:
            /* Transfer some vertical velocity to horizontal */
            world->vel_x[idx] += world->vel_y[idx] / 4;
            world->vel_y[idx] = 0;
            break;
        default:
            break;
    }
}

/* Handle collision on horizontal axis */
static inline void phys_collision_horizontal(World* world, int x, int y,
                                              CollisionType type, float restitution) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);

    switch (type) {
        case COLLISION_STOP:
            world->vel_x[idx] = 0;
            break;
        case COLLISION_BOUNCE:
            world->vel_x[idx] = FIXED_MUL(world->vel_x[idx],
                                           FIXED_FROM_FLOAT(-restitution));
            break;
        case COLLISION_SLIDE:
            world->vel_y[idx] += world->vel_x[idx] / 4;
            world->vel_x[idx] = 0;
            break;
        default:
            break;
    }
}

/* =============================================================================
 * Density-Based Displacement
 * ============================================================================= */

/* Check if source can displace target based on density */
static inline bool phys_can_displace(MaterialID source, MaterialID target) {
    const MaterialProps* src = material_get(source);
    const MaterialProps* tgt = material_get(target);
    return src->density > tgt->density;
}

/* Get density at position */
static inline float phys_get_density(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 9999.0f; /* Boundary = infinite density */
    MaterialID mat = world->mat[IDX(x, y)];
    return material_get(mat)->density;
}

/* =============================================================================
 * Impact Effects
 * ============================================================================= */

/* Check if velocity is high enough for impact effects */
static inline bool phys_is_impact(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    int idx = IDX(x, y);
    float vy = FIXED_TO_FLOAT(FIXED_ABS(world->vel_y[idx]));
    return vy > PHYS_IMPACT_THRESHOLD;
}

/* Get impact strength (0.0 to 1.0) */
static inline float phys_impact_strength(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 0.0f;
    int idx = IDX(x, y);
    float vy = FIXED_TO_FLOAT(FIXED_ABS(world->vel_y[idx]));
    if (vy <= PHYS_IMPACT_THRESHOLD) return 0.0f;
    return CLAMP((vy - PHYS_IMPACT_THRESHOLD) / (PHYS_MAX_VELOCITY - PHYS_IMPACT_THRESHOLD),
                 0.0f, 1.0f);
}

/* =============================================================================
 * Pressure Calculation (for fluids)
 * ============================================================================= */

/* Count column height of same material above position */
static inline int phys_column_height(const World* world, int x, int y, MaterialID mat) {
    int count = 0;
    for (int cy = y; cy >= 0; cy--) {
        if (world_get_mat(world, x, cy) == mat) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

/* Calculate pressure difference between two columns */
static inline int phys_pressure_diff(const World* world, int x1, int x2, int y,
                                      MaterialID mat) {
    int h1 = phys_column_height(world, x1, y, mat);
    int h2 = phys_column_height(world, x2, y, mat);
    return h1 - h2;
}

#endif /* PHYSICS_H */
