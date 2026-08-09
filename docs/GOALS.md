# Project goals

## Primary goal (north star)

**Deliver a 3D discrete CFD simulation whose results can be inspected and shown** — same spirit as a vehicle in a wind tunnel: mesh → solve → fields (U, p, …) → visualization.

Not “only a linear solver on a cube of heat.” The solver work exists to enable and accelerate that simulation path (especially device-resident linear algebra later).

Concrete milestone for the revival:

1. **3D wind-tunnel-style case** in this repo (body in a free-stream / channel).  
2. **Runs on modern OpenFOAM** (CPU reference path).  
3. **Post-processable results** (ParaView / VTK): velocity, pressure, streamlines / slices.  
4. Documented “how to reproduce the show.”

A production-accurate full car (detailed CAD, DES/LES, force coefficients to industry standards) is **aspirational**; the primary goal is a **honest 3D external-flow demonstration** that matches the original project intent, then grow fidelity.

## Secondary goals

| ID | Goal | Why |
|----|------|-----|
| S1 | **Full-device OpenCL segment** (assemble + solve in GPU memory; one load / one unload) | Original PFC insight; avoid hybrid CPU assemble / GPU solve thrash |
| S2 | **CPU OpenFOAM baseline** on modern OF | Correctness and fair timing reference |
| S3 | **Measurable residency & residuals** | Prove correctness and no matrix PCIe thrash |
| S4 | **Scale** (large meshes / meaningful VRAM use) | Stress path toward real case sizes |
| S5 | **Couple device path into the 3D simulation loop** | GPU accelerates the real goal, not a toy Laplace forever |
| S6 | **Env on G:** (WSL/OF/distro not filling C:) | Lab machine constraint |
| S7 | **Track work on PR** | Visible progress |

## Non-goals (for now)

- Full OEM vehicle aero validation suite  
- MPI multi-GPU cluster product  
- Rewriting all of OpenFOAM on GPU  
- 2D-only demos as the end state (2D may appear only as tiny unit tests)

## Current direction of work

```text
Primary:  cases/windTunnel3D  →  run  →  visualize
Secondary: laplaceOcl / device segment mature in parallel, then plug into 3D solves
```

## Definition of “primary goal achieved” (v1)

- [x] `cases/windTunnel3D` runs end-to-end on OpenFOAM v2512+ (`simpleFoam`)  
- [x] 3D mesh with a **bluff body** (box “vehicle”) in a tunnel domain (`snappyHexMesh`)  
- [x] Fields written (`U`, `p`, `k`, …)  
- [x] **RAS kEpsilon** + wall functions on vehicle/ground  
- [x] **forceCoeffs** on patch `vehicle` (`postProcessing/forces/…/coefficient.dat`)  
- [x] Short doc: `cases/windTunnel3D/README.md` (ParaView + Cd/Cl recipe)  
- [x] Linked from root README as the main demo  

**Latest successful RAS run (demo numbers, not validation):**  
converged ~222 SIMPLE iters; \(C_d \approx 1.25\), \(C_l \approx 0.63\) (box bluff body, \(\nu=0.01\)).

**Still open for fidelity (not v1 blockers):** Ahmed/CAD body, air-like \(\nu\) + finer mesh, GPU on this case (S5).
