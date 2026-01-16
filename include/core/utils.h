/*
 * utils.h - Core Utility Functions
 *
 * Provides fundamental utilities used across all modules.
 * For specialized functionality, see:
 *   - cell_ops.h  : Cell operations and movement
 *   - physics.h   : Physics calculations
 *   - behavior.h  : Material behaviors and reactions
 *   - grid_iter.h : Grid iteration patterns
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/* =============================================================================
 * Random Number Generation
 * ============================================================================= */

/* Fast XORShift32 PRNG - inline for performance */
static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Hash function for deterministic variation */
static inline uint32_t hash32(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

/* =============================================================================
 * Neighbor Offset Arrays (Legacy - prefer cell_ops.h)
 * ============================================================================= */

#define NEIGHBOR_4_DX  { -1,  1,  0,  0 }
#define NEIGHBOR_4_DY  {  0,  0, -1,  1 }
#define NEIGHBOR_4_COUNT 4

#define NEIGHBOR_8_DX  { -1,  0,  1, -1, 1, -1,  0,  1 }
#define NEIGHBOR_8_DY  { -1, -1, -1,  0, 0,  1,  1,  1 }
#define NEIGHBOR_8_COUNT 8

#endif /* UTILS_H */
