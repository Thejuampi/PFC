# Project goals

## Primary goal v1 — **achieved**

**Showable 3D discrete CFD** (wind-tunnel style on stock OpenFOAM CPU).

| Check | Status |
|-------|--------|
| `cases/windTunnel3D` end-to-end (`simpleFoam`, OF v2512) | done |
| 3D bluff body + tunnel (`snappyHexMesh`) | done |
| Fields + RAS kEpsilon + forceCoeffs (Cd/Cl) | done |
| Documented ParaView recipe | done |

Demo numbers (not industrial validation): \(C_d \approx 1.25\), \(C_l \approx 0.63\).  
**v1 is closed.** Fidelity extras (Ahmed/CAD, air \(\nu\), finer mesh) remain optional polish, not the north star.

---

## Primary goal v2 (new north star)

**Run the OpenFOAM *outer solve loop* for the 3D case as a full-device lifecycle on the GPU** — the place where the bottleneck hypothesis lives.

### Bottleneck hypothesis (why this goal)

In steady incompressible RANS (`simpleFoam`-class), wall time is dominated by the **repeated outer iterations**:

```text
each SIMPLE iteration:
  assemble momentum / pressure / turbulence operators
  solve large sparse linear systems (PCG/GAMG/…)
  update fields, fluxes, residuals
```

Not by: mesh generation once, writing VTK once, or starting the binary.

The **2015/PFC trap** was hybrid thrash:

```text
every linear solve:  CPU assemble A → copy → GPU solve → copy x → CPU
```

That can make GPU *slower* than CPU even when the kernel is fast.  
The **right cut** is residency:

```text
startup:   mesh topology + fields + constants → GPU   (once)
outer loop: assemble + solve + update fields        entirely on GPU
shutdown:  download fields / forces for I/O         (once / rare)
```

So: **yes — the new primary is “the loop on GPU”**, meaning the **CFD time-step / SIMPLE outer loop**, not “reimplement every OpenFOAM utility on GPU.”

### Scope: what “whole loop on GPU” means here

| In scope (must end on device) | Out of scope (stay on host / offline) |
|-------------------------------|----------------------------------------|
| Field state `U, p, φ, k, ε, …` resident on GPU across outer iters | `blockMesh` / `snappyHexMesh` / case setup |
| Operator assembly for the equations we solve | ParaView, plots, force post I/O policy |
| Linear solves (PCG + precond) for those systems | Rewriting all of OpenFOAM |
| Residual norms / outer convergence control (scalars only) | MPI multi-GPU product |
| Same *class* of physics as `windTunnel3D` (simpleFoam + RAS) | OEM validation suite |

**Product statement:**  
*A 3D wind-tunnel-style RANS run whose **iterative solve phase** does not thrash matrix/fields over PCIe; showable fields and Cd/Cl still match a CPU OpenFOAM reference within a stated band.*

### Definition of done (primary v2)

**MVP (v2a) — one equation in the loop**

- [ ] At least the **pressure** (or dominant) linear solve of SIMPLE runs **device-resident** inside an outer iteration driven from the wind-tunnel case.  
- [ ] Matrix/fields for that segment not re-uploaded every PCG iteration.  
- [ ] Residual gate + traffic metrics (`h2d` after startup ≈ 0 for fields/matrix).  
- [ ] Compare to stock OF residual / field band on `windTunnel3D` (or clone).

**Full outer loop (v2b) — the actual goal**

- [ ] Full SIMPLE outer iteration on device for the case equations: **U, p, (k, ε)** (or equivalent RAS set).  
- [ ] Assemble **and** solve on GPU for those systems (not “GPU solve of host-built A only” as the end state).  
- [ ] One load at start of solve phase, one unload of results for post.  
- [ ] Cd/Cl (or forceCoeffs-equivalent) within agreed band vs CPU reference.  
- [ ] Documented run path + bench table (CPU OF wall time vs device loop).

**Aspirational (v2c, not required to claim v2)**

- Finer mesh / air-like \(\nu\) / body CAD while keeping the device loop.  
- More of the finite-volume machinery on device (grad/div schemes, limiters).

### Relation to old secondary IDs

| Old ID | Role under v2 |
|--------|----------------|
| S1–S4 | **Building blocks** (mostly done): full-device segment, metrics, scale |
| S5 | **Becomes the spine of primary v2** (couple → then own the outer loop) |
| S2 | CPU OpenFOAM remains the **correctness & timing reference** |
| S6–S7 | Lab constraints / PR tracking — still apply |

Ladder detail: `docs/S5_DEVICE_SEGMENT.md`.  
Architecture: `docs/AMD_GPU_ROADMAP.md`.  
**Integration (your OpenFOAM / agents):** `docs/INTEGRATE_OPENFOAM.md` · `AGENTS.md`.

---

## Secondary goals (supporting)

| ID | Goal | Status / note |
|----|------|----------------|
| S1 | Full-device OpenCL segment | largely done (`laplaceOcl`, `csrOcl`) |
| S2 | CPU OF baseline | done (`windTunnel3D`, laplaceCpu) |
| S3 | Residuals + traffic metrics | done on device apps; keep for v2 |
| S4 | Scale / VRAM | done smoke; re-check on full loop |
| S5 | Device path in 3D loop | **active — core of primary v2** |
| S6 | Env on G: | policy still on |
| S7 | Track on PR | PR #1 |

---

## Non-goals (unchanged spirit)

- Full OEM aero validation suite  
- MPI multi-GPU cluster product  
- **Rewriting all of OpenFOAM on GPU** (mesh tools, GUI, every model)  
- Hybrid “plugin that copies A every call” as the architecture  
- 2D-only as the end state  

---

## Current direction of work

```text
Primary v1:  cases/windTunnel3D on CPU     →  ACHIEVED (showable 3D CFD)
Primary v2:  same class of run, outer loop device-resident on GPU
             v2a: one system in-loop  →  v2b: full SIMPLE (U,p,turb) on device
Reference:   stock simpleFoam on CPU for residuals, Cd/Cl, wall time
```
