# Pixel-Cell Physics Simulator (C, from scratch)
**Design Doc v1.0 — Sand → Water → Solid → Fire + Gas, with fixed tick-rate**

> Objective: A 2D grid where **each cell is one pixel** and the simulation behaves physically plausible: gravity, pressure-driven water flow, solid collisions, heat transfer, combustion, smoke/steam as gas-like scalars.  
> Approach: Ship value in **incremental milestones**. Start with a robust cellular foundation (sand), then upgrade the engine to pressure-based fluids (water), lock in solid boundary physics, and finally layer thermal + combustion + gas transport.

---

## 1) Guiding Principles (non-negotiables)
1. **Fixed simulation tick-rate** (deterministic, stable). Rendering can be variable.
2. **Data-driven materials** (one table drives behavior; no spaghetti rules).
3. **Separable subsystems**: movement, pressure, thermal, reactions, rendering.
4. **Performance budget**: design for 60 FPS visuals; simulation tick-rate can be 60–240 Hz depending on grid size.
5. **Debuggability**: you will ship faster if you can visualize pressure, velocity, temperature, and update regions.

---

## 2) Simulation Clock: Tick-Rate Architecture (required)
You must decouple simulation time from rendering time.

### 2.1 Fixed timestep with accumulator
- Let `tick_hz` be the simulation update frequency (e.g., 120 Hz).
- `dt = 1.0 / tick_hz` (fixed).
- Rendering runs as fast as it can. Each frame:
  - accumulate real time
  - run `while (accumulator >= dt) step(dt); accumulator -= dt;`
  - render

### 2.2 Why this matters
- Cellular sims look “random” when dt fluctuates.
- Pressure solvers and thermal diffusion are sensitive to dt.
- Determinism enables reproducible debugging and consistent gameplay feel.

### 2.3 Practical defaults
- Small grids (≤ 512×512): `tick_hz = 120` feels premium.
- Medium grids (≈ 1024×768): start at `tick_hz = 60`.
- High velocities require **substeps** (see stability).

---

## 3) World Model: Grid, Layers, and Memory Layout
### 3.1 Grid
- 2D grid `W × H`.
- Each cell = one pixel = one simulation sample.

### 3.2 Layout strategy (performance)
Use **SoA (Structure of Arrays)**, not AoS. Example arrays:
- `mat[W*H]` (uint8)
- `flags[W*H]` (uint16)
- `temp[W*H]` (float)
- `pressure[W*H]` (float)
- `density[W*H]` (float or implicit via material)
- optional: `smoke[W*H]`, `steam[W*H]`, `fuel[W*H]`, `moisture[W*H]`

Double-buffer where it reduces complexity:
- For rule-based materials (sand): `mat_next`
- For scalar transport: `temp_next`, `smoke_next`

### 3.3 Velocity field for real fluid physics (MAC grid recommended)
For pressure-based water + gas transport, maintain a **staggered velocity field**:
- `u[(W+1)*H]` (x velocity on vertical faces)
- `v[W*(H+1)]` (y velocity on horizontal faces)

This is the cleanest foundation for:
- incompressible flow
- stable pressure projection
- solid boundary conditions
- buoyancy-driven gas movement

---

## 4) Material System (Data-Driven)
### 4.1 Material table fields (baseline)
Each material has a record:

**Identity**
- `id`, `name`
- `state`: `EMPTY | SOLID | POWDER | FLUID | GAS`

**Mechanical**
- `rho` (density)
- `friction` (solid/powder)
- `restitution` (bounce, mostly for future dynamic solids)
- `cohesion` (soil/mud feel)
- `viscosity_mu` (fluid)
- `permeability` (powder + water interaction later)

**Thermal**
- `conductivity_k`
- `heat_capacity_c`
- `ignition_temp`
- `burn_rate`
- `smoke_rate`
- `melting_temp`
- `boiling_temp`

### 4.2 Flags (per-cell)
Keep flags separate from `mat` so you can overlay states without material explosion:
- `SOLID`, `STATIC`, `BURNING`, `WET`, `HOT`, `UPDATED_THIS_TICK`, etc.

---

## 5) System Roadmap (Sand → Water → Solid → Fire + Gas)
You asked for a strict progression. This plan is designed to avoid rewrites while still reaching “pressure + collisions + heat”.

---

# Milestone A — Sand First (Powder Core)
**Goal:** A rock-solid cellular update loop, deterministic behavior, and high visual quality with minimal math.

### A1) Sand behavior (rule-based, high quality)
Per tick, update sand cells in a bias-resistant order:
- **Scan order randomization** or **checkerboard** to avoid directional artifacts.
- Use a per-tick RNG seed (deterministic if you want reproducibility).

**Movement rules (priority order):**
1. If below is empty → fall down
2. Else if down-left is empty → fall down-left
3. Else if down-right is empty → fall down-right
4. Else stay

**Angle of repose (pile slope)**
To get realistic dunes:
- Allow lateral settling when blocked.
- Introduce `cohesion` and `friction` to tune how easily sand slides.

**Stop double-update**
- Set `UPDATED_THIS_TICK` when moved, clear at end of tick.

### A2) Sand “physics knobs” that matter
- `settle_probability`: reduces jitter when piles stabilize
- `slide_bias`: random left/right
- `cohesion`: soil-like clumps
- `granularity`: noise in motion rules

### A3) Deliverables for Milestone A
- Stable tick loop with fixed timestep
- Material table working
- Sand piles that look convincing
- Profiling counters: updated cells per tick, ms per tick

**Quality bar:** You can paint sand, it falls naturally, piles form, and performance is stable.

---

# Milestone B — Water Next (Pressure-Based Fluid)
**Goal:** Upgrade from “cell swapping water” to **pressure-projected incompressible flow**.

This is where “real physics” starts: pressure, divergence, boundaries.

### B1) Introduce the velocity field (MAC)
Add:
- `u`, `v` arrays
- sampling functions (bilinear)
- boundary handling hooks

### B2) External forces
- Gravity: `v += g * dt`
- Optional: user wind tool

### B3) Velocity advection (Semi-Lagrangian)
For each velocity face:
- backtrace position by `dt`
- bilinear sample from old velocities

This is stable, simple, and production-proven.

### B4) Pressure projection (incompressibility)
Compute:
- divergence of velocity
Solve Poisson:
- `∇²p = (rho/dt) * div`
Project:
- subtract pressure gradient from velocity

**Solver options (pick one now, upgrade later):**
- Jacobi (simplest; more iterations)
- Red-Black Gauss-Seidel (faster convergence)
- Conjugate Gradient (scales better)

Start with **Jacobi**. It’s predictable and easy to debug.

### B5) Rendering water in a cell world
Even with a velocity field, you still need a **fluid occupancy map**:
- simplest: treat `mat=water` cells as fluid region
- update occupancy via advection of a scalar “water amount” (advanced) later

**Pragmatic approach:**
- Keep water as a material in cells for painting and visibility.
- Drive movement via the velocity field + pressure.
- For early versions, “water region” = cells labeled water.

### B6) Deliverables for Milestone B
- Velocity field with advection
- Pressure solver working (visualize pressure!)
- Water responds to gravity and fills containers
- No obvious compressibility “breathing” artifacts

**Quality bar:** Water looks like it has volume, pushes outward, and doesn’t collapse into jitter.

---

# Milestone C — Solid After Water (Boundary Physics)
**Goal:** Correct collisions between fluids and solids; solid walls become first-class citizens.

### C1) Solid mask
Define `SOLID` cells:
- stone, metal, terrain
- these define “blocked volume”

### C2) No-penetration boundary conditions
For each velocity face on a solid boundary:
- set normal component to 0
- prevent flow entering solid cells

Example conceptually:
- if a `u` face separates solid from non-solid: `u=0`
- if a `v` face separates solid from non-solid: `v=0`

### C3) No-slip (optional, but improves realism)
Dampen tangential flow along walls:
- `tangent *= (1 - wall_friction)`
This gives water that “sticks” slightly instead of skating unrealistically.

### C4) Solid interactions for sand
Now sand must treat solids as immovable blockers (already, but ensure consistent flags).

### C5) Deliverables for Milestone C
- Solid drawing/painting tools
- Water respects containers (zero penetration)
- Sand piles on platforms correctly
- Debug overlays: solid mask, velocity vectors near walls

**Quality bar:** A “box” holds water. No leaks. No wall tunneling.

---

# Milestone D — Fire + Gas (Thermal + Reactions + Buoyancy)
**Goal:** A thermal system that drives combustion and gas transport (smoke/steam). This creates the “wow factor”.

### D1) Thermal field (temperature)
Maintain `T` per cell.

**Heat diffusion (simple, stable)**
- apply a discrete Laplacian diffusion step:
  - `T_new = T + alpha * dt * Laplacian(T)`
- alpha depends on conductivity and heat capacity:
  - `alpha ~ k / (rho * c)` (practical tuning is fine)

### D2) Fire model (cellular reaction)
Fire is a **state + reaction**, not a pure material.
Per cell with `BURNING`:
- consume `fuel`
- produce heat: `T += heat_output * dt`
- produce smoke: `smoke += smoke_rate * dt`
- extinguish if fuel depleted or temperature drops

**Ignition**
- if `T > ignition_temp` and cell has fuel → set `BURNING`

### D3) Gas transport (smoke/steam as scalars)
Smoke/steam are best treated as **advected scalars** using the same velocity field as water:
- `smoke_new(x) = smoke_old(backtraced_position)`
- apply mild diffusion for visual smoothness

### D4) Buoyancy (why smoke rises)
Add buoyancy force to velocity:
- `v += buoyancy * (T - ambient_T) * dt`
or for smoke-based buoyancy:
- `v += buoyancy * smoke * dt`

This is cheap and looks real.

### D5) Steam and phase change (optional but strong)
When `T` exceeds boiling threshold for water:
- reduce water amount / water cells
- increase `steam` scalar
When steam cools:
- condense back to water

### D6) Deliverables for Milestone D
- Temperature diffusion
- Fire that spreads realistically in fuel materials
- Smoke rising and pooling under ceilings
- Steam from heated water

**Quality bar:** You can build a scene: wood burns, smoke accumulates, heat affects surroundings.

---

## 6) Stability Rules (don’t ignore)
### 6.1 CFL condition (velocity stability)
If velocities get large, Semi-Lagrangian advection stays stable but visuals degrade.
Keep:
- `dt * max_speed < 1 cell` (target)
If violated:
- run **substeps**: split tick into `n` smaller steps.

### 6.2 Pressure solver iterations
- Fewer iterations = spongy water.
- More iterations = crisp incompressibility, more CPU.

Make iterations a runtime quality setting:
- 20 (fast), 60 (quality), 120 (ultra)

### 6.3 Thermal diffusion stability
If using explicit diffusion, keep alpha small:
- tune diffusion coefficient + dt
If you push too hard, temperature will blow up or smear too fast.

---

## 7) Performance Strategy (scales without pain)
### 7.1 Dirty regions / chunks
Most frames only a portion of the world changes.
Partition into chunks (e.g., 32×32) and maintain “active chunk list”.
Only process active chunks + neighbors.

### 7.2 Sparse updates for sand
Sand logic can be limited to cells in active chunks.
Keep a “moved last tick” mask to drive activation.

### 7.3 Solver cost control
Pressure solver is the main CPU sink.
Options:
- solve only in fluid regions (mask)
- reduce iterations when scene is calm
- multi-resolution later (advanced)

---

## 8) Debug Overlays (mandatory for progress)
Build toggles:
- Pressure heatmap
- Divergence heatmap
- Velocity vectors (downsampled)
- Temperature heatmap
- Smoke density heatmap
- Solid mask
- Active chunk visualization
- Tick time / iteration counters on screen

Without this, you will waste days chasing invisible bugs.

---

## 9) Minimal Tick Pipeline (final target order)
At each fixed tick `dt`:

1. Input paint (materials/tools)
2. **Powder step** (sand/soil) — rule-based
3. Apply forces to velocity (gravity, buoyancy)
4. Advect velocity (u,v)
5. Viscosity diffusion (optional, fluid quality)
6. Enforce solid boundaries
7. Pressure projection (solve p, project u,v)
8. Enforce solid boundaries again
9. Advect scalars (temperature, smoke, steam)
10. Thermal diffusion
11. Reactions (fire spread, phase change)
12. Clear per-tick flags

You can reorder powder vs fluid steps depending on how you couple them later. Early on, keep them separate.

---

## 10) Acceptance Criteria per Milestone (clear “done” definition)
### Sand done when:
- piles form naturally
- no directional bias
- no double-update glitches
- stable at fixed tick

### Water done when:
- water fills containers smoothly
- no obvious compressibility pumping
- pressure/velocity debug looks coherent

### Solid done when:
- zero penetration
- believable wall interaction
- no leak-through at corners

### Fire + Gas done when:
- ignition/propagation is controllable
- smoke rises + accumulates
- thermal diffusion impacts behavior (not just visuals)

---

## 11) Upgrade Path (future-proof)
Once the above works, the next “serious realism” upgrades are:
- Better pressure solver (RBGS or CG)
- FLIP/PIC hybrid to reduce Semi-Lagrangian diffusion
- Two-way coupling: sand affected by fluid velocity (erosion)
- Water volume fraction per cell (more continuous surfaces)
- Dynamic rigid bodies (big scope)

---

## 12) Next step (to keep momentum)
If you want, I’ll produce the next doc as:
- **Milestone A implementation spec**: exact cell flags, update ordering, chunk activation rules, and parameter defaults for sand
- plus a **Milestone B technical spec**: MAC indexing, divergence formulas, boundary rules, and Jacobi pressure solver structure (still in English MD)

Just say: “write Milestone A spec” or “write Milestone B spec”.
