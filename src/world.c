/*
 * world.c - Grid/World model implementation
 */
#include "world.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Simple RNG for color seeds */
static uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

World* world_create(int width, int height) {
    World* world = calloc(1, sizeof(World));
    if (!world) return NULL;
    
    world->width = width;
    world->height = height;
    
    size_t grid_size = (size_t)width * height;
    size_t chunk_count = (size_t)CHUNKS_X * CHUNKS_Y;
    
    /* Allocate all arrays */
    world->mat = calloc(grid_size, sizeof(MaterialID));
    world->mat_next = calloc(grid_size, sizeof(MaterialID));
    world->flags = calloc(grid_size, sizeof(CellFlags));
    world->color_seed = calloc(grid_size, sizeof(uint32_t));
    world->temp = calloc(grid_size, sizeof(float));
    world->temp_next = calloc(grid_size, sizeof(float));
    world->pressure = calloc(grid_size, sizeof(float));
    world->density = calloc(grid_size, sizeof(float));
    world->vel_x = calloc(grid_size, sizeof(Fixed8));
    world->vel_y = calloc(grid_size, sizeof(Fixed8));
    world->lifetime = calloc(grid_size, sizeof(uint8_t));
    world->chunk_active = calloc(chunk_count, sizeof(bool));
    world->chunk_active_next = calloc(chunk_count, sizeof(bool));
    
    /* Check allocations */
    if (!world->mat || !world->mat_next || !world->flags || 
        !world->color_seed || !world->temp || !world->temp_next ||
        !world->pressure || !world->density || !world->vel_x || !world->vel_y ||
        !world->lifetime || !world->chunk_active || !world->chunk_active_next) {
        world_destroy(world);
        return NULL;
    }
    
    /* Initialize color seeds with random values */
    uint32_t seed = 12345;
    for (size_t i = 0; i < grid_size; i++) {
        world->color_seed[i] = xorshift32(&seed);
    }
    
    /* Initialize temperature to ambient */
    for (size_t i = 0; i < grid_size; i++) {
        world->temp[i] = 20.0f;  /* Room temperature */
        world->temp_next[i] = 20.0f;
    }
    
    return world;
}

void world_destroy(World* world) {
    if (!world) return;
    
    free(world->mat);
    free(world->mat_next);
    free(world->flags);
    free(world->color_seed);
    free(world->temp);
    free(world->temp_next);
    free(world->pressure);
    free(world->density);
    free(world->vel_x);
    free(world->vel_y);
    free(world->lifetime);
    free(world->chunk_active);
    free(world->chunk_active_next);
    free(world);
}

void world_clear(World* world) {
    size_t grid_size = (size_t)world->width * world->height;
    memset(world->mat, MAT_EMPTY, grid_size * sizeof(MaterialID));
    memset(world->mat_next, MAT_EMPTY, grid_size * sizeof(MaterialID));
    memset(world->flags, 0, grid_size * sizeof(CellFlags));
    memset(world->vel_x, 0, grid_size * sizeof(Fixed8));
    memset(world->vel_y, 0, grid_size * sizeof(Fixed8));
    memset(world->lifetime, 0, grid_size * sizeof(uint8_t));
}

MaterialID world_get_mat(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return MAT_EMPTY;
    return world->mat[IDX(x, y)];
}

void world_set_mat(World* world, int x, int y, MaterialID mat) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    world->mat[idx] = mat;
    world->vel_x[idx] = 0;
    world->vel_y[idx] = 0;
    world_activate_chunk_at(world, x, y);
}

void world_set_mat_next(World* world, int x, int y, MaterialID mat) {
    if (!IN_BOUNDS(x, y)) return;
    world->mat_next[IDX(x, y)] = mat;
}

CellFlags world_get_flags(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return FLAG_NONE;
    return world->flags[IDX(x, y)];
}

void world_set_flags(World* world, int x, int y, CellFlags flags) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] = flags;
}

void world_add_flag(World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] |= flag;
}

void world_remove_flag(World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] &= ~flag;
}

bool world_has_flag(const World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return false;
    return (world->flags[IDX(x, y)] & flag) != 0;
}

bool world_is_empty(const World* world, int x, int y) {
    return material_is_empty(world_get_mat(world, x, y));
}

bool world_is_solid(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return true;  /* Out of bounds treated as solid */
    return material_is_solid(world_get_mat(world, x, y));
}

void world_swap_cells(World* world, int x1, int y1, int x2, int y2) {
    if (!IN_BOUNDS(x1, y1) || !IN_BOUNDS(x2, y2)) return;
    
    int idx1 = IDX(x1, y1);
    int idx2 = IDX(x2, y2);
    
    /* Swap material */
    MaterialID tmp_mat = world->mat[idx1];
    world->mat[idx1] = world->mat[idx2];
    world->mat[idx2] = tmp_mat;
    
    /* Swap color seed (so colors follow the material) */
    uint32_t tmp_seed = world->color_seed[idx1];
    world->color_seed[idx1] = world->color_seed[idx2];
    world->color_seed[idx2] = tmp_seed;
    
    /* Swap velocity */
    Fixed8 tmp_vx = world->vel_x[idx1];
    Fixed8 tmp_vy = world->vel_y[idx1];
    world->vel_x[idx1] = world->vel_x[idx2];
    world->vel_y[idx1] = world->vel_y[idx2];
    world->vel_x[idx2] = tmp_vx;
    world->vel_y[idx2] = tmp_vy;
    
    /* Swap lifetime */
    uint8_t tmp_life = world->lifetime[idx1];
    world->lifetime[idx1] = world->lifetime[idx2];
    world->lifetime[idx2] = tmp_life;
    
    /* Activate both chunks */
    world_activate_chunk_at(world, x1, y1);
    world_activate_chunk_at(world, x2, y2);
}

void world_activate_chunk(World* world, int chunk_x, int chunk_y) {
    if (chunk_x < 0 || chunk_x >= CHUNKS_X || chunk_y < 0 || chunk_y >= CHUNKS_Y) return;
    int idx = chunk_y * CHUNKS_X + chunk_x;
    world->chunk_active_next[idx] = true;
}

void world_activate_chunk_at(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;
    world_activate_chunk(world, chunk_x, chunk_y);
    
    /* Also activate neighbor chunks (for particles that might move across boundaries) */
    world_activate_chunk(world, chunk_x - 1, chunk_y);
    world_activate_chunk(world, chunk_x + 1, chunk_y);
    world_activate_chunk(world, chunk_x, chunk_y - 1);
    world_activate_chunk(world, chunk_x, chunk_y + 1);
    world_activate_chunk(world, chunk_x - 1, chunk_y + 1);
    world_activate_chunk(world, chunk_x + 1, chunk_y + 1);
}

bool world_is_chunk_active(const World* world, int chunk_x, int chunk_y) {
    if (chunk_x < 0 || chunk_x >= CHUNKS_X || chunk_y < 0 || chunk_y >= CHUNKS_Y) return false;
    int idx = chunk_y * CHUNKS_X + chunk_x;
    return world->chunk_active[idx];
}

void world_swap_buffers(World* world) {
    /* For materials, we typically don't double-buffer in cellular automata
     * Instead, we use in-place updates with careful ordering
     * This function is here for future fluid simulation needs */
    (void)world;
}

void world_clear_tick_flags(World* world) {
    size_t grid_size = (size_t)world->width * world->height;
    for (size_t i = 0; i < grid_size; i++) {
        world->flags[i] &= ~FLAG_UPDATED;
    }
}

void world_clear_chunk_activation(World* world) {
    memset(world->chunk_active_next, 0, CHUNK_COUNT * sizeof(bool));
}

void world_update_chunk_activation(World* world) {
    /* Swap active buffers */
    bool* tmp = world->chunk_active;
    world->chunk_active = world->chunk_active_next;
    world->chunk_active_next = tmp;
    
    /* Count active chunks */
    world->active_chunks = 0;
    for (int i = 0; i < CHUNK_COUNT; i++) {
        if (world->chunk_active[i]) {
            world->active_chunks++;
        }
    }
}

void world_paint_circle(World* world, int cx, int cy, int radius, MaterialID mat) {
    int r2 = radius * radius;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int x = cx + dx;
                int y = cy + dy;
                if (IN_BOUNDS(x, y)) {
                    world_set_mat(world, x, y, mat);
                }
            }
        }
    }
}

void world_paint_line(World* world, int x0, int y0, int x1, int y1, int radius, MaterialID mat) {
    /* Bresenham's line algorithm with thickness */
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (1) {
        world_paint_circle(world, x0, y0, radius, mat);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

Color world_get_cell_color(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) {
        return (Color){0, 0, 0, 255};
    }
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    uint32_t seed = world->color_seed[idx];
    
    return material_color(mat, seed);
}
