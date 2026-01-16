# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
make           # Build optimized release binary
make debug     # Clean + build with debug flags (-g -O0 -DDEBUG)
make run       # Build and run
make clean     # Remove build artifacts
./pixelsim     # Run the simulator
```

**Requirements:** gcc, make, pkg-config, SDL2 development headers

## Project Structure

```
include/
├── core/           # Core types and utilities
│   ├── types.h     # Fundamental types, macros, constants
│   └── utils.h     # RNG, hash functions
├── world/          # World and cell operations
│   ├── world.h     # World struct and basic operations
│   ├── cell_ops.h  # Cell movement, state checks, neighbors
│   └── grid_iter.h # Grid traversal patterns
├── physics/        # Physics calculations
│   ├── physics.h   # Gravity, velocity, collision
│   └── thermal.h   # Heat diffusion, phase changes
├── materials/      # Material system
│   ├── material.h  # Material properties
│   └── behavior.h  # Behavior flags, transitions, reactions
├── subsystems/     # Simulation subsystems
│   ├── powder.h    # Sand, soil, ash
│   ├── fluid.h     # Water, acid
│   ├── fire.h      # Fire, smoke, steam
│   └── acid.h      # Corrosion
└── engine/         # Engine components
    ├── simulation.h # Main simulation loop
    ├── render.h     # SDL2 rendering
    └── input.h      # Input handling

src/
├── main.c          # Entry point
├── world/          # World implementation
├── physics/        # Physics implementation
├── materials/      # Material implementation
├── subsystems/     # Subsystem implementations
└── engine/         # Engine implementations
```

## Architecture Overview

This is a 2D pixel physics sandbox where each cell is one pixel. The simulation uses a **fixed timestep** (120 Hz default) with an accumulator pattern, decoupled from rendering.

### Include Path Convention

All includes use paths relative to `include/`:
```c
#include "core/types.h"
#include "world/world.h"
#include "materials/material.h"
#include "physics/physics.h"
#include "subsystems/powder.h"
#include "engine/simulation.h"
```

### Memory Layout (SoA)

The world uses **Structure of Arrays** for cache-friendly iteration:
- `mat[]` / `mat_next[]` - Double-buffered material IDs
- `flags[]` - Per-cell flags (updated, burning, wet)
- `temp[]` / `temp_next[]` - Temperature field
- `vel_x[]` / `vel_y[]` - Per-cell velocity (8.8 fixed-point)
- `lifetime[]` - Particle effects (fire, smoke)
- `chunk_active[]` - Dirty region tracking (32x32 chunks)

### Simulation Pipeline

The `simulation_tick()` runs these subsystems in order:
1. **Powder** - Sand/soil falling with angle of repose
2. **Fluid** - Water with pressure equalization
3. **Fire** - Combustion, smoke production
4. **Gas** - Smoke/steam rising
5. **Acid** - Corrosion
6. **Thermal** - Heat diffusion, phase changes

### Cell Operations (`world/cell_ops.h`)

```c
// Cell type classification
CellType cell_get_type(world, x, y);  // CELL_EMPTY, CELL_SOLID, CELL_POWDER, CELL_FLUID, CELL_GAS
bool cell_is_empty/solid/powder/fluid/gas(world, x, y);

// Movement validation
bool cell_powder_can_enter(world, x, y);  // Empty, fluid, or gas
bool cell_fluid_can_enter(world, x, y);   // Empty or gas
bool cell_gas_can_enter(world, x, y);     // Empty only

// Movement execution
bool cell_move(world, from_x, from_y, to_x, to_y);  // Swap + mark updated
```

### Physics Module (`physics/physics.h`)

```c
phys_apply_gravity_fixed(world, x, y, props);
MovementSteps phys_calc_fall_steps(world, x, y, max_steps);
phys_collision_vertical(world, x, y, COLLISION_STOP, restitution);
bool phys_can_displace(source_mat, target_mat);
int phys_column_height(world, x, y, mat);
```

### Behavior System (`materials/behavior.h`)

```c
// Movement behaviors
bhv_falls(mat)           // Affected by gravity
bhv_rises(mat)           // Negative gravity (smoke, fire)
bhv_flows(mat)           // Horizontal spreading

// Interaction behaviors
bhv_is_flammable(mat)    // Can catch fire
bhv_is_corrodible(mat)   // Can be dissolved by acid

// Phase changes
bhv_can_melt/freeze/boil/condense(mat)

// State transitions
StateTransition bhv_get_melt_transition(mat);    // Ice -> Water
StateTransition bhv_get_freeze_transition(mat);  // Water -> Ice
```

### Grid Iteration (`world/grid_iter.h`)

```c
grid_iterate_falling(sim, world, callback, userdata);  // Bottom-up
grid_iterate_rising(sim, world, callback, userdata);   // Top-down
grid_iterate(sim, world, ITER_BOTTOM_UP, ITER_RANDOM, callback, userdata);
```

Callback: `bool callback(Simulation* sim, World* world, int x, int y, void* userdata)`

### Adding New Materials

1. Define `MAT_*` constant in `core/types.h`, increment `MAT_COUNT`
2. Add properties in `material_init()` in `materials/material.c`
3. Add behavior flags in `materials/behavior.h` `BEHAVIOR_TABLE`
4. Implement behavior in appropriate subsystem

### Neighbor Arrays (`world/cell_ops.h`)

```c
// 4-directional (cardinal)
NEIGHBOR4_DX[4], NEIGHBOR4_DY[4]

// 8-directional (including diagonal)
NEIGHBOR8_DX[8], NEIGHBOR8_DY[8]
```

### Key Macros (`core/types.h`)

```c
IDX(x, y)           // 2D -> 1D index
IN_BOUNDS(x, y)     // Bounds check
CHUNK_IDX(x, y)     // Get chunk index

// Fixed-point (8.8)
FIXED_FROM_FLOAT(x)
FIXED_TO_FLOAT(x)
FIXED_MUL(a, b)
```
