# Pixel-Cell Physics Simulator

A real-time 2D pixel sandbox in C/SDL2. Each cell is a single pixel with its own material, temperature, and state. The simulation uses a fixed timestep and data-driven material rules to model powder, fluid, gas, heat, and reactions with stable performance.

## Highlights
- Fixed-timestep simulation loop with deterministic updates
- Data-driven materials with separate per-cell flags and properties
- Powder dynamics with bias-resistant update order
- Fluid pressure equalization with gravity-driven flow
- Thermal diffusion, phase change, and heat-driven reactions
- Fire, smoke, steam, ash, and acid systems
- Temperature overlay and material-specific visuals (glow, smoke fade)
- Active-chunk processing to keep large grids fast

## Simulation Systems
- **Powder:** rule-based falling, diagonal settling, and momentum splash when impacting fluids
- **Fluid:** pressure equalization with density-aware movement and solid boundary checks
- **Thermal:** explicit diffusion with per-material conductivity and heat capacity
- **Phase Change:** ice melts, water boils to steam, steam condenses, water freezes
- **Combustion:** ignition, heat output, smoke generation, and ash on burnout
- **Gas Transport:** smoke/steam rise and dissipate with lifetime-based fading
- **Corrosion:** acid erodes stone, wood, sand, and soil over time

## Controls
- `Left Mouse`: Paint material
- `Right Mouse`: Erase (empty)
- `Tab`: Toggle temperature overlay

**Material Keys**
- `1` Sand
- `2` Stone
- `3` Water
- `4` Wood
- `5` Soil
- `6` Fire
- `7` Smoke
- `8` Empty
- `9` Ice
- `0` Steam
- `-` Ash
- `=` Acid

## Build & Run
**Requirements**
- `gcc`
- `make`
- `pkg-config`
- `SDL2` development headers

**Build**
```
make
```

**Run**
```
./pixelsim
```

## Project Structure
- `src/` core simulation and rendering systems
- `include/` public headers and material definitions
- `build/` object files (generated)

## Optimizations
- Structure-of-arrays memory layout for cache-friendly iteration
- Active chunk lists to avoid processing idle regions
- Lightweight per-cell flags to prevent double-updates

## Roadmap Ideas
- Pressure solver upgrade (RBGS/CG)
- PIC/FLIP-style velocity advection
- Water volume fractions for smoother surfaces
- Advanced rigid-body solids
