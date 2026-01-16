/*
 * behavior.h - Material Behavior Classifiers Module
 *
 * Provides unified material behavior classification, reaction rules,
 * and state transition definitions.
 */
#ifndef BEHAVIOR_H
#define BEHAVIOR_H

#include "core/types.h"
#include "materials/material.h"

/* =============================================================================
 * Behavior Flags
 *
 * Bitmask flags for material capabilities and behaviors.
 * ============================================================================= */

typedef uint32_t BehaviorFlags;

/* Movement behaviors */
#define BHV_NONE            0x00000000
#define BHV_FALLS           0x00000001  /* Affected by gravity (downward) */
#define BHV_RISES           0x00000002  /* Negative gravity (upward) */
#define BHV_FLOWS           0x00000004  /* Spreads horizontally */
#define BHV_SLIDES          0x00000008  /* Can slide diagonally */
#define BHV_STATIC          0x00000010  /* Never moves */

/* Interaction behaviors */
#define BHV_FLAMMABLE       0x00000100  /* Can catch fire */
#define BHV_CONDUCTS_HEAT   0x00000200  /* Transfers heat to neighbors */
#define BHV_CORRODIBLE      0x00000400  /* Can be dissolved by acid */
#define BHV_CORROSIVE       0x00000800  /* Dissolves other materials */
#define BHV_EXTINGUISHES    0x00001000  /* Puts out fire on contact */

/* State change behaviors */
#define BHV_MELTS           0x00010000  /* Can melt to liquid */
#define BHV_FREEZES         0x00020000  /* Can freeze to solid */
#define BHV_BOILS           0x00040000  /* Can boil to gas */
#define BHV_CONDENSES       0x00080000  /* Can condense to liquid */
#define BHV_BURNS_OUT       0x00100000  /* Disappears when burning ends */

/* Lifetime behaviors */
#define BHV_DISSIPATES      0x01000000  /* Fades over time */
#define BHV_SPREADS         0x02000000  /* Spreads to neighbors */
#define BHV_PRODUCES_SMOKE  0x04000000  /* Creates smoke */
#define BHV_PRODUCES_HEAT   0x08000000  /* Generates heat */

/* =============================================================================
 * Behavior Lookup Table
 * ============================================================================= */

/* Get behavior flags for material */
static inline BehaviorFlags behavior_get(MaterialID mat) {
    static const BehaviorFlags BEHAVIOR_TABLE[MAT_COUNT] = {
        /* MAT_EMPTY */
        BHV_NONE,

        /* MAT_SAND */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT,

        /* MAT_STONE */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE,

        /* MAT_WATER */
        BHV_FALLS | BHV_FLOWS | BHV_CONDUCTS_HEAT | BHV_FREEZES | BHV_BOILS | BHV_EXTINGUISHES,

        /* MAT_WOOD */
        BHV_STATIC | BHV_FLAMMABLE | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE,

        /* MAT_FIRE */
        BHV_RISES | BHV_SPREADS | BHV_PRODUCES_SMOKE | BHV_PRODUCES_HEAT | BHV_BURNS_OUT,

        /* MAT_SMOKE */
        BHV_RISES | BHV_FLOWS | BHV_DISSIPATES,

        /* MAT_SOIL */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE,

        /* MAT_ICE */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_MELTS,

        /* MAT_STEAM */
        BHV_RISES | BHV_FLOWS | BHV_CONDENSES | BHV_DISSIPATES,

        /* MAT_ASH */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT,

        /* MAT_ACID */
        BHV_FALLS | BHV_FLOWS | BHV_CORROSIVE | BHV_CONDUCTS_HEAT,
    };

    if (mat >= MAT_COUNT) return BHV_NONE;
    return BEHAVIOR_TABLE[mat];
}

/* Check for specific behavior flag */
static inline bool behavior_has(MaterialID mat, BehaviorFlags flag) {
    return (behavior_get(mat) & flag) != 0;
}

/* =============================================================================
 * Behavior Queries (convenience functions)
 * ============================================================================= */

/* Movement queries */
static inline bool bhv_falls(MaterialID mat)    { return behavior_has(mat, BHV_FALLS); }
static inline bool bhv_rises(MaterialID mat)    { return behavior_has(mat, BHV_RISES); }
static inline bool bhv_flows(MaterialID mat)    { return behavior_has(mat, BHV_FLOWS); }
static inline bool bhv_slides(MaterialID mat)   { return behavior_has(mat, BHV_SLIDES); }
static inline bool bhv_is_static(MaterialID mat){ return behavior_has(mat, BHV_STATIC); }

/* Interaction queries */
static inline bool bhv_is_flammable(MaterialID mat)     { return behavior_has(mat, BHV_FLAMMABLE); }
static inline bool bhv_conducts_heat(MaterialID mat)    { return behavior_has(mat, BHV_CONDUCTS_HEAT); }
static inline bool bhv_is_corrodible(MaterialID mat)    { return behavior_has(mat, BHV_CORRODIBLE); }
static inline bool bhv_is_corrosive(MaterialID mat)     { return behavior_has(mat, BHV_CORROSIVE); }
static inline bool bhv_extinguishes(MaterialID mat)     { return behavior_has(mat, BHV_EXTINGUISHES); }

/* State change queries */
static inline bool bhv_can_melt(MaterialID mat)     { return behavior_has(mat, BHV_MELTS); }
static inline bool bhv_can_freeze(MaterialID mat)   { return behavior_has(mat, BHV_FREEZES); }
static inline bool bhv_can_boil(MaterialID mat)     { return behavior_has(mat, BHV_BOILS); }
static inline bool bhv_can_condense(MaterialID mat) { return behavior_has(mat, BHV_CONDENSES); }

/* Lifetime queries */
static inline bool bhv_dissipates(MaterialID mat)   { return behavior_has(mat, BHV_DISSIPATES); }
static inline bool bhv_spreads(MaterialID mat)      { return behavior_has(mat, BHV_SPREADS); }
static inline bool bhv_produces_smoke(MaterialID mat) { return behavior_has(mat, BHV_PRODUCES_SMOKE); }
static inline bool bhv_produces_heat(MaterialID mat)  { return behavior_has(mat, BHV_PRODUCES_HEAT); }

/* =============================================================================
 * State Transitions
 *
 * Define what material becomes under certain conditions.
 * ============================================================================= */

typedef struct {
    MaterialID result;          /* Resulting material */
    float threshold;            /* Temperature or other threshold */
    float probability;          /* Base probability per tick */
} StateTransition;

/* Get melting transition for material */
static inline StateTransition bhv_get_melt_transition(MaterialID mat) {
    switch (mat) {
        case MAT_ICE:
            return (StateTransition){MAT_WATER, 0.0f, 0.01f};
        default:
            return (StateTransition){MAT_EMPTY, 9999.0f, 0.0f};
    }
}

/* Get freezing transition for material */
static inline StateTransition bhv_get_freeze_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WATER:
            return (StateTransition){MAT_ICE, 0.0f, 0.005f};
        default:
            return (StateTransition){MAT_EMPTY, -9999.0f, 0.0f};
    }
}

/* Get boiling transition for material */
static inline StateTransition bhv_get_boil_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WATER:
            return (StateTransition){MAT_STEAM, 100.0f, 0.02f};
        default:
            return (StateTransition){MAT_EMPTY, 9999.0f, 0.0f};
    }
}

/* Get condensation transition for material */
static inline StateTransition bhv_get_condense_transition(MaterialID mat) {
    switch (mat) {
        case MAT_STEAM:
            return (StateTransition){MAT_WATER, 80.0f, 0.01f};
        default:
            return (StateTransition){MAT_EMPTY, -9999.0f, 0.0f};
    }
}

/* Get combustion products for material */
static inline StateTransition bhv_get_burn_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WOOD:
            return (StateTransition){MAT_FIRE, 300.0f, 0.03f};
        default:
            return (StateTransition){MAT_EMPTY, 9999.0f, 0.0f};
    }
}

/* Get death products for fire */
typedef struct {
    MaterialID ash;
    MaterialID smoke;
    float ash_chance;
    float smoke_chance;
} FireDeathProducts;

static inline FireDeathProducts bhv_get_fire_death(void) {
    return (FireDeathProducts){
        .ash = MAT_ASH,
        .smoke = MAT_SMOKE,
        .ash_chance = 0.3f,
        .smoke_chance = 0.5f
    };
}

/* =============================================================================
 * Reaction Rules
 *
 * Define interactions between different materials.
 * ============================================================================= */

typedef struct {
    MaterialID target;          /* Target material to react with */
    MaterialID result_self;     /* What source becomes (MAT_EMPTY = consumed) */
    MaterialID result_target;   /* What target becomes */
    float probability;          /* Probability per tick */
    MaterialID byproduct;       /* Optional byproduct (e.g., smoke) */
    float byproduct_chance;     /* Chance to spawn byproduct */
} ReactionRule;

/* Get corrosion reaction for acid + target */
static inline ReactionRule bhv_get_corrosion_reaction(MaterialID target) {
    if (!bhv_is_corrodible(target)) {
        return (ReactionRule){MAT_EMPTY, MAT_EMPTY, MAT_EMPTY, 0.0f, MAT_EMPTY, 0.0f};
    }

    return (ReactionRule){
        .target = target,
        .result_self = MAT_EMPTY,       /* Acid consumed (50% of time) */
        .result_target = MAT_EMPTY,     /* Target destroyed */
        .probability = 0.08f,
        .byproduct = MAT_SMOKE,
        .byproduct_chance = 0.5f
    };
}

/* Get fire spread reaction */
static inline ReactionRule bhv_get_fire_spread_reaction(MaterialID target) {
    if (!bhv_is_flammable(target)) {
        return (ReactionRule){MAT_EMPTY, MAT_EMPTY, MAT_EMPTY, 0.0f, MAT_EMPTY, 0.0f};
    }

    return (ReactionRule){
        .target = target,
        .result_self = MAT_FIRE,        /* Fire stays */
        .result_target = MAT_FIRE,      /* Target ignites */
        .probability = 0.03f,
        .byproduct = MAT_EMPTY,
        .byproduct_chance = 0.0f
    };
}

/* Get extinguish reaction (water/ice on fire) */
static inline ReactionRule bhv_get_extinguish_reaction(MaterialID source) {
    if (!bhv_extinguishes(source)) {
        return (ReactionRule){MAT_EMPTY, MAT_EMPTY, MAT_EMPTY, 0.0f, MAT_EMPTY, 0.0f};
    }

    return (ReactionRule){
        .target = MAT_FIRE,
        .result_self = (source == MAT_WATER) ? MAT_STEAM : source,
        .result_target = MAT_SMOKE,
        .probability = 0.5f,
        .byproduct = MAT_STEAM,
        .byproduct_chance = 0.3f
    };
}

/* =============================================================================
 * Movement Priority Tables
 *
 * Define movement attempt order for different material types.
 * ============================================================================= */

typedef struct {
    int dx, dy;
} MoveOffset;

/* Powder movement priorities (fall > diagonal > horizontal) */
static const MoveOffset POWDER_MOVE_PRIORITY[] = {
    { 0,  1},   /* Down */
    {-1,  1},   /* Down-left */
    { 1,  1},   /* Down-right */
};
#define POWDER_MOVE_COUNT 3

/* Fluid movement priorities (fall > horizontal > diagonal) */
static const MoveOffset FLUID_MOVE_PRIORITY[] = {
    { 0,  1},   /* Down */
    {-1,  0},   /* Left */
    { 1,  0},   /* Right */
    {-1,  1},   /* Down-left */
    { 1,  1},   /* Down-right */
};
#define FLUID_MOVE_COUNT 5

/* Gas movement priorities (rise > diagonal-up > horizontal) */
static const MoveOffset GAS_MOVE_PRIORITY[] = {
    { 0, -1},   /* Up */
    {-1, -1},   /* Up-left */
    { 1, -1},   /* Up-right */
    {-1,  0},   /* Left */
    { 1,  0},   /* Right */
};
#define GAS_MOVE_COUNT 5

#endif /* BEHAVIOR_H */
