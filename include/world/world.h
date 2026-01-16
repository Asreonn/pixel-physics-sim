/*
 * world.h - Grid/World model with SoA (Structure of Arrays) layout
 */
#ifndef WORLD_H
#define WORLD_H

#include "core/types.h"
#include "materials/material.h"

/* =============================================================================
 * World State Structure (SoA layout for performance)
 * ============================================================================= */

typedef struct {
    /* Primary material grid (current frame) */
    MaterialID* mat;
    
    /* Double buffer for material (next frame) */
    MaterialID* mat_next;
    
    /* Per-cell flags */
    CellFlags* flags;
    
    /* Color seed per cell (for consistent visual variation) */
    uint32_t* color_seed;
    
    /* Temperature field (for future thermal simulation) */
    float* temp;
    float* temp_next;
    
    /* Pressure field (for future fluid simulation) */
    float* pressure;
    
    /* Density field (can be derived from material, but useful for fluids) */
    float* density;
    
    /* Per-cell velocity (fixed-point 8.8) */
    Fixed8* vel_x;
    Fixed8* vel_y;
    
    /* Particle lifetime (for fire animation, smoke fading) */
    uint8_t* lifetime;
    
    /* Chunk activation tracking */
    bool* chunk_active;
    bool* chunk_active_next;
    
    /* Grid dimensions (stored for convenience) */
    int width;
    int height;
    
    /* Statistics */
    uint32_t cells_updated;
    uint32_t active_chunks;
    
} World;

/* =============================================================================
 * World Functions
 * ============================================================================= */

/* Create and initialize a new world */
World* world_create(int width, int height);

/* Destroy and free world resources */
void world_destroy(World* world);

/* Clear the entire world to empty */
void world_clear(World* world);

/* Get material at position (returns MAT_EMPTY if out of bounds) */
MaterialID world_get_mat(const World* world, int x, int y);

/* Set material at position (no-op if out of bounds) */
void world_set_mat(World* world, int x, int y, MaterialID mat);

/* Set material in the next buffer (for double-buffered updates) */
void world_set_mat_next(World* world, int x, int y, MaterialID mat);

/* Get flags at position */
CellFlags world_get_flags(const World* world, int x, int y);

/* Set flags at position */
void world_set_flags(World* world, int x, int y, CellFlags flags);

/* Add flag to cell */
void world_add_flag(World* world, int x, int y, CellFlags flag);

/* Remove flag from cell */
void world_remove_flag(World* world, int x, int y, CellFlags flag);

/* Check if cell has flag */
bool world_has_flag(const World* world, int x, int y, CellFlags flag);

/* Check if position is empty */
bool world_is_empty(const World* world, int x, int y);

/* Check if position is solid (blocking) */
bool world_is_solid(const World* world, int x, int y);

/* Swap cell contents (for movement) */
void world_swap_cells(World* world, int x1, int y1, int x2, int y2);

/* Mark a chunk as active (needs processing) */
void world_activate_chunk(World* world, int chunk_x, int chunk_y);

/* Mark chunk containing cell as active */
void world_activate_chunk_at(World* world, int x, int y);

/* Check if chunk is active */
bool world_is_chunk_active(const World* world, int chunk_x, int chunk_y);

/* Swap buffers at end of tick */
void world_swap_buffers(World* world);

/* Clear per-tick flags (UPDATED, etc.) */
void world_clear_tick_flags(World* world);

/* Clear chunk activation for next tick */
void world_clear_chunk_activation(World* world);

/* Update chunk activation (swap active/next) */
void world_update_chunk_activation(World* world);

/* Paint a brush of material (circle) */
void world_paint_circle(World* world, int cx, int cy, int radius, MaterialID mat);

/* Paint a line of material */
void world_paint_line(World* world, int x0, int y0, int x1, int y1, int radius, MaterialID mat);

/* Get color for cell (using stored color seed) */
Color world_get_cell_color(const World* world, int x, int y);

#endif /* WORLD_H */
