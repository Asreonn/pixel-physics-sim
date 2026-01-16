/*
 * material.c - Data-driven material system implementation
 */
#include "materials/material.h"
#include "core/utils.h"
#include <string.h>

/* =============================================================================
 * Material Table (the heart of data-driven design)
 * ============================================================================= */

static MaterialProps g_materials[MAT_COUNT];

/* =============================================================================
 * Lookup Tables (LUTs) for fast material queries - avoid function call overhead
 * ============================================================================= */

static MaterialState g_material_state_lut[MAT_COUNT];
static bool g_material_is_powder_lut[MAT_COUNT];
static bool g_material_is_fluid_lut[MAT_COUNT];
static bool g_material_is_solid_lut[MAT_COUNT];
static bool g_material_is_empty_lut[MAT_COUNT];
static bool g_material_is_gas_lut[MAT_COUNT];

static void material_finalize_fixed(MaterialProps* mat) {
    mat->gravity_step_fixed = FIXED_FROM_FLOAT(GRAVITY_ACCEL * mat->gravity_scale);
    mat->drag_factor_fixed = FIXED_FROM_FLOAT(1.0f - mat->drag_coeff);
    mat->terminal_velocity_fixed = FIXED_FROM_FLOAT(mat->terminal_velocity);
}

void material_init(void) {
    memset(g_materials, 0, sizeof(g_materials));
    
    /* -------------------------------------------------------------------------
     * MAT_EMPTY - Air/void
     * ------------------------------------------------------------------------- */
    g_materials[MAT_EMPTY] = (MaterialProps){
        .id = MAT_EMPTY,
        .name = "Empty",
        .state = STATE_EMPTY,
        .base_color = {0, 0, 0, 255},
        .color_variation = 0,
        .density = 1.225f,
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.000018f,
        .gravity_scale = 0.0f,
        .drag_coeff = 1.0f,
        .terminal_velocity = 0.0f,
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.0f,
        .heat_capacity = 0.0f,
        .ignition_temp = 0.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 0.0f,
        .boiling_temp = 0.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_SAND - Powder material (falls and piles)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_SAND] = (MaterialProps){
        .id = MAT_SAND,
        .name = "Sand",
        .state = STATE_POWDER,
        .base_color = {220, 190, 130, 255},
        .color_variation = 25,
        .density = 1600.0f,           /* kg/m^3 - dry sand */
        .friction = 0.7f,
        .restitution = 0.0f,
        .cohesion = 0.15f,            /* Low cohesion - loose grains */
        .viscosity = 0.0f,
        .gravity_scale = 1.2f,        /* Falls a bit faster than water */
        .drag_coeff = 0.25f,          /* Some air resistance */
        .terminal_velocity = 3.5f,    /* cells/tick max speed */
        .flow_rate = 0.0f,            /* No horizontal flow */
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.25f,  /* Settles fairly quickly */
        .slide_bias = 0.5f,           /* No left/right bias */
        .conductivity = 0.3f,
        .heat_capacity = 0.8f,
        .ignition_temp = 9999.0f,     /* Can't burn */
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 1700.0f,      /* Silica melting point */
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_STONE - Solid material (immovable)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_STONE] = (MaterialProps){
        .id = MAT_STONE,
        .name = "Stone",
        .state = STATE_SOLID,
        .base_color = {80, 80, 90, 255},
        .color_variation = 20,
        .density = 2600.0f,
        .friction = 0.9f,
        .restitution = 0.1f,
        .cohesion = 1.0f,
        .viscosity = 0.0f,
        .gravity_scale = 0.0f,
        .drag_coeff = 1.0f,
        .terminal_velocity = 0.0f,
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 1.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.8f,
        .heat_capacity = 0.9f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 1200.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_WATER - Fluid material (Milestone B)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_WATER] = (MaterialProps){
        .id = MAT_WATER,
        .name = "Water",
        .state = STATE_FLUID,
        .base_color = {30, 100, 200, 200},
        .color_variation = 15,
        .density = 1000.0f,         /* kg/m^3 */
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.001f,         /* Pa*s */
        .gravity_scale = 1.0f,
        .drag_coeff = 0.1f,
        .terminal_velocity = 4.0f,
        .flow_rate = 0.6f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.6f,
        .heat_capacity = 4.2f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 0.0f,
        .boiling_temp = 100.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_WOOD - Flammable solid (Milestone D)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_WOOD] = (MaterialProps){
        .id = MAT_WOOD,
        .name = "Wood",
        .state = STATE_SOLID,
        .base_color = {139, 90, 43, 255},
        .color_variation = 25,
        .density = 600.0f,
        .friction = 0.8f,
        .restitution = 0.1f,
        .cohesion = 1.0f,
        .viscosity = 0.0f,
        .gravity_scale = 0.0f,
        .drag_coeff = 1.0f,
        .terminal_velocity = 0.0f,
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 1.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.15f,
        .heat_capacity = 1.7f,
        .ignition_temp = 300.0f,
        .burn_rate = 0.1f,
        .smoke_rate = 0.5f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_FIRE - Reaction state (Milestone D)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_FIRE] = (MaterialProps){
        .id = MAT_FIRE,
        .name = "Fire",
        .state = STATE_GAS,
        .base_color = {255, 100, 20, 255},
        .color_variation = 50,
        .density = 0.4f,
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.0f,
        .gravity_scale = -0.3f,
        .drag_coeff = 0.2f,
        .terminal_velocity = 2.0f,
        .flow_rate = 0.7f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.1f,
        .heat_capacity = 0.1f,
        .ignition_temp = 0.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 1.0f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_SMOKE - Gas material (Milestone D)
     * ------------------------------------------------------------------------- */
    g_materials[MAT_SMOKE] = (MaterialProps){
        .id = MAT_SMOKE,
        .name = "Smoke",
        .state = STATE_GAS,
        .base_color = {60, 60, 60, 150},
        .color_variation = 20,
        .density = 0.6f,
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.00002f,
        .gravity_scale = -0.1f,
        .drag_coeff = 0.8f,
        .terminal_velocity = 1.2f,
        .flow_rate = 0.5f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.02f,
        .heat_capacity = 0.1f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_SOIL - Heavier powder with more cohesion
     * ------------------------------------------------------------------------- */
    g_materials[MAT_SOIL] = (MaterialProps){
        .id = MAT_SOIL,
        .name = "Soil",
        .state = STATE_POWDER,
        .base_color = {100, 70, 40, 255},
        .color_variation = 20,
        .density = 1800.0f,
        .friction = 0.85f,
        .restitution = 0.0f,
        .cohesion = 0.4f,
        .viscosity = 0.0f,
        .gravity_scale = 1.1f,
        .drag_coeff = 0.3f,
        .terminal_velocity = 2.5f,
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.4f,
        .slide_bias = 0.5f,
        .conductivity = 0.5f,
        .heat_capacity = 1.0f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_ICE - Frozen water, melts at 0°C
     * ------------------------------------------------------------------------- */
    g_materials[MAT_ICE] = (MaterialProps){
        .id = MAT_ICE,
        .name = "Ice",
        .state = STATE_SOLID,
        .base_color = {180, 220, 255, 220},  /* Light blue, slightly transparent */
        .color_variation = 15,
        .density = 917.0f,           /* Ice is lighter than water */
        .friction = 0.1f,            /* Very slippery */
        .restitution = 0.2f,
        .cohesion = 1.0f,
        .viscosity = 0.0f,
        .gravity_scale = 0.0f,       /* Solid, doesn't fall */
        .drag_coeff = 1.0f,
        .terminal_velocity = 0.0f,
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 1.0f,
        .slide_bias = 0.5f,
        .conductivity = 2.2f,        /* Ice conducts heat well */
        .heat_capacity = 2.1f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 0.0f,        /* Melts at 0°C */
        .boiling_temp = 100.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_STEAM - Water vapor, rises fast, condenses when cool
     * ------------------------------------------------------------------------- */
    g_materials[MAT_STEAM] = (MaterialProps){
        .id = MAT_STEAM,
        .name = "Steam",
        .state = STATE_GAS,
        .base_color = {220, 220, 230, 80},   /* White-ish, very transparent */
        .color_variation = 10,
        .density = 0.6f,             /* Lighter than air when hot */
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.00001f,
        .gravity_scale = -0.5f,      /* Rises faster than smoke */
        .drag_coeff = 0.5f,
        .terminal_velocity = 2.5f,
        .flow_rate = 0.6f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.02f,
        .heat_capacity = 2.0f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 0.0f,
        .boiling_temp = 100.0f,      /* Condenses below 100°C */
    };
    
    /* -------------------------------------------------------------------------
     * MAT_ASH - Lightweight powder from burned wood
     * ------------------------------------------------------------------------- */
    g_materials[MAT_ASH] = (MaterialProps){
        .id = MAT_ASH,
        .name = "Ash",
        .state = STATE_POWDER,
        .base_color = {90, 90, 90, 255},     /* Gray */
        .color_variation = 15,
        .density = 500.0f,           /* Very light */
        .friction = 0.3f,
        .restitution = 0.0f,
        .cohesion = 0.05f,           /* Very loose */
        .viscosity = 0.0f,
        .gravity_scale = 0.3f,       /* Falls slowly */
        .drag_coeff = 0.7f,          /* High air resistance */
        .terminal_velocity = 1.0f,   /* Slow max speed */
        .flow_rate = 0.0f,
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.15f,
        .slide_bias = 0.5f,
        .conductivity = 0.1f,
        .heat_capacity = 0.8f,
        .ignition_temp = 9999.0f,    /* Already burned */
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
    };
    
    /* -------------------------------------------------------------------------
     * MAT_ACID - Corrosive fluid that dissolves materials
     * ------------------------------------------------------------------------- */
    g_materials[MAT_ACID] = (MaterialProps){
        .id = MAT_ACID,
        .name = "Acid",
        .state = STATE_FLUID,
        .base_color = {100, 255, 50, 200},   /* Bright toxic green */
        .color_variation = 20,
        .density = 1100.0f,          /* Slightly denser than water */
        .friction = 0.0f,
        .restitution = 0.0f,
        .cohesion = 0.0f,
        .viscosity = 0.002f,         /* Slightly thicker than water */
        .gravity_scale = 1.0f,
        .drag_coeff = 0.15f,
        .terminal_velocity = 3.5f,
        .flow_rate = 0.7f,           /* Flows well */
        .gravity_step_fixed = 0,
        .drag_factor_fixed = 0,
        .terminal_velocity_fixed = 0,
        .settle_probability = 0.0f,
        .slide_bias = 0.5f,
        .conductivity = 0.5f,
        .heat_capacity = 3.0f,
        .ignition_temp = 9999.0f,
        .burn_rate = 0.0f,
        .smoke_rate = 0.0f,
        .melting_temp = -20.0f,
        .boiling_temp = 120.0f,
    };

    /* Finalize fixed-point values and build LUTs */
    for (int i = 0; i < MAT_COUNT; i++) {
        material_finalize_fixed(&g_materials[i]);
        
        /* Build lookup tables */
        g_material_state_lut[i] = g_materials[i].state;
        g_material_is_powder_lut[i] = (g_materials[i].state == STATE_POWDER);
        g_material_is_fluid_lut[i] = (g_materials[i].state == STATE_FLUID);
        g_material_is_solid_lut[i] = (g_materials[i].state == STATE_SOLID);
        g_material_is_empty_lut[i] = (g_materials[i].state == STATE_EMPTY);
        g_material_is_gas_lut[i] = (g_materials[i].state == STATE_GAS);
    }
}

const MaterialProps* material_get(MaterialID id) {
    if (id >= MAT_COUNT) {
        return &g_materials[MAT_EMPTY];
    }
    return &g_materials[id];
}

/* Fast LUT-based material queries - inlined for hot paths */
MaterialState material_state(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_state_lut[id] : STATE_EMPTY;
}

bool material_is_powder(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_powder_lut[id] : false;
}

bool material_is_fluid(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_fluid_lut[id] : false;
}

bool material_is_solid(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_solid_lut[id] : false;
}

bool material_is_empty(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_empty_lut[id] : false;
}

bool material_is_gas(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_gas_lut[id] : false;
}

Color material_color(MaterialID id, uint32_t seed) {
    const MaterialProps* mat = material_get(id);
    Color c = mat->base_color;
    
    if (mat->color_variation > 0 && seed != 0) {
        uint32_t h = hash32(seed);
        int var = (int)(h % (mat->color_variation * 2 + 1)) - mat->color_variation;
        
        c.r = (uint8_t)CLAMP((int)c.r + var, 0, 255);
        c.g = (uint8_t)CLAMP((int)c.g + var, 0, 255);
        c.b = (uint8_t)CLAMP((int)c.b + var, 0, 255);
    }
    
    return c;
}
