/*
 * input.h - Input handling for painting materials
 */
#ifndef INPUT_H
#define INPUT_H

#include "core/types.h"
#include "world/world.h"
#include "engine/simulation.h"
#include "engine/render.h"
#include <SDL2/SDL.h>

/* =============================================================================
 * Input State
 * ============================================================================= */

typedef struct {
    /* Mouse state */
    int mouse_x, mouse_y;
    int prev_mouse_x, prev_mouse_y;
    bool mouse_left;
    bool mouse_right;
    bool mouse_middle;
    
    /* Brush settings */
    MaterialID current_material;
    int brush_size;
    int min_brush_size;
    int max_brush_size;
    
    /* Keyboard state */
    bool key_space;      /* Pause toggle */
    bool key_escape;     /* Quit */
    bool key_c;          /* Clear world */
    bool key_tab;        /* Cycle overlay */
    bool key_f;          /* Toggle FPS */
    bool key_s;          /* Toggle stats */
    bool key_period;     /* Step once */
    
    /* Number keys for material selection */
    bool key_1, key_2, key_3, key_4, key_5, key_6, key_7, key_8, key_9, key_0;
    bool key_minus, key_equals;  /* For additional materials */
    
    /* Application state */
    bool quit_requested;
    
} Input;

/* =============================================================================
 * Input Functions
 * ============================================================================= */

/* Create and initialize input state */
Input* input_create(void);

/* Destroy input state */
void input_destroy(Input* input);

/* Process SDL events and update input state */
void input_update(Input* input);

/* Apply input to world (painting, etc.) */
void input_apply(Input* input, World* world, Simulation* sim, Renderer* renderer);

/* Get name of current material */
const char* input_get_material_name(const Input* input);

/* Cycle to next/previous material */
void input_next_material(Input* input);
void input_prev_material(Input* input);

/* Increase/decrease brush size */
void input_increase_brush(Input* input);
void input_decrease_brush(Input* input);

#endif /* INPUT_H */
