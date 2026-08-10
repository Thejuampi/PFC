# GPU path — full-device lifecycle (not “solver offload”)

## The real cut (architecture)

**Wrong cut (2015 hybrid, and the trap to avoid):**

```
every step/iteration:
  CPU: assemble A, b
  copy  → GPU
  GPU:  solve
  copy  ← host
```

That is slow because of **constant traffic**, not because the API is OpenCL.

**Right cut (what we want):**

```
startup:   load mesh / fields / constants → GPU   (once)
loop:      assemble + solve + update fields  entirely on GPU
shutdown:  unload results → host             (once)
```

One upload at the beginning of the run lifecycle, one download at the end (plus optional rare host I/O if we choose to write time steps — that is a policy, not the solver path).

OpenCL is a **perfectly valid** way to implement that full-device pipeline on AMD (RX 6800 XT). So is HIP. The choice of API is secondary to **where the data lives for the whole lifecycle**.

## What step 0 is (and is not)

| Step 0 (done) | Later |
|---------------|--------|
| Stock OpenFOAM on **CPU** | Same physics / case, **GPU-resident** |
| Reference residuals & fields | Bitwise-ish / residual-comparable checks |
| No GPU | OpenCL (or other) **integral** path |

Step 0 is a **correctness and timing reference**, not a statement that the product stays hybrid forever.

## OpenCL vs HIP (de-prioritized)

We do **not** block on “OpenCL is legacy, use HIP”.

- **OpenCL first is OK** if it gets a full-device loop running on this box sooner (existing PFC muscle memory, AMD ICD available).
- HIP later is optional if we want rocSPARSE/rocSOLVER ergonomics or ROCm-only tooling — **not** a prerequisite to invalidate OpenCL.
- Portability and available libraries matter only **after** the lifecycle design is solid.

## Implementation order (get it working, then improve)

1. **CPU reference** — `cases/laplaceCpu` / stock solvers (**done**).  
2. **GPU-resident skeleton (OpenCL)** — `apps/laplaceOcl` (**done**): DIA assemble + PCG on device, RX 6800 XT.  
3. **Single load / single unload** — matrix stays on device; one `T` download at end (**done**). Residual scalars only for PCG control.  
4. **Match CPU** — same scheme on host; smoke 100² max|Δ| ~ 1e-13; 500² ~ 1e-12 (**done**).  
5. **Phase A (hardening)** — metrics, bench, smoke test, **poly2 preconditioner** (**done**).  
6. **Primary product path v1** — `cases/windTunnel3D` on OpenFOAM CPU.  
   **Done:** RAS kEpsilon + forceCoeffs; showable U/p/k + Cd/Cl.  
7. **Primary v2 (new north star)** — **outer SIMPLE loop device-resident** on GPU  
   (assemble + solve + field update; one load / rare unload).  
   Ladder: DIA → CSR → OF matrix dump (**done**) → in-loop v2a → full loop v2b.  
   Goals: `docs/GOALS.md`, `docs/S5_DEVICE_SEGMENT.md`. PR #1.

## Explicit non-goals for the first GPU cut

- Not “PCG plugin that copies LDU→CSR every call”.  
- Not “prove HIP is better than OpenCL”.  
- Not rewriting all of OpenFOAM — a **segment dedicated** to generate matrices and solve in-place (same idea as the original insight).
