# GPU vs CPU speedup — what we measure (and what we do not)

## The one-sentence question

> **On the real car wind-tunnel pressure system, how much faster is our
> device-resident CSR poly2-PCG on GPU than a strong multi-thread CPU
> implementation of the *same* algorithm on the *same* \(A,b\)?**

That is the **primary speedup number** this project reports.

---

## Why not “simpleFoam wall time vs csrOcl”?

| | simpleFoam (CPU) | csrOcl (GPU) |
|--|------------------|--------------|
| Scope | Full SIMPLE: mesh ops, **U + p + k + ε**, assembly every outer iter, BCs, forces | **One** linear system \(A x = b\) (pressure Laplacian snapshot) |
| Work per “step” | Outer iteration (many kernels + several linear solves) | One PCG to tolerance |
| Fair? | **No** as head-to-head speedup | — |

`simpleFoam` is still useful as **context / reference**:

- How expensive is the *whole* real CFD case on CPU?
- windTunnelCar: ~**2540 s** wall for 150 outer iters (~**17 s**/outer), **Cd ≈ 0.87**, **n ≈ 1.24×10⁶** cells.

It is **not** the efficient CPU baseline for the linear-algebra claim.

---

## Fair comparison protocol (primary)

### Case
`cases/windTunnelCar` — concept car in the tunnel (video scenario).

### System under test
Pressure Laplacian at a frozen field state (typically latest time after a CFD run):

```bash
# OpenFOAM env, from clone root (once)
( cd cases/windTunnelCar && ofDumpCsr )
# → cases/windTunnelCar/matrix/of_p.mtx
# → cases/windTunnelCar/matrix/of_p.rhs
```

Same files for CPU and GPU. Same \(n\), \(nnz\), tolerance, max PCG iters, poly2 precond.

### Methods

| Arm | Method | Why |
|-----|--------|-----|
| **GPU (ours)** | `csrOcl` full-device CSR poly2-PCG, one H2D of \(A,b\), solve on device, one D2H of \(x\) | Product claim |
| **CPU (fair baseline)** | **Same** poly2-PCG on host CSR, **OpenMP** multi-thread SpMV/axpy (`-fopenmp`, all cores) | Algorithm-matched, efficient CPU — not a weak 1-thread strawman |
| **CPU (context only)** | stock `simpleFoam` wall / outer-iter cost | “How big is the real CFD job?” — **not** the speedup denominator |

Optional future “even stronger CPU” (not required for the primary claim):

- OpenFOAM **GAMG** / DICPCG on an equivalent pressure solve  
- Vendor libs (MKL PARDISO, hypre BoomerAMG, PETSc)  

Those answer “fastest CPU solver in the world?” — a different paper. Our primary claim is: **device-resident same algorithm beats strong multi-thread same algorithm**.

### Metric

```text
SPEEDUP = CPU_MS / GPU_SOLVE_MS
```

- `GPU_SOLVE_MS` — device PCG only (not MTX parse, not dump)  
- `CPU_MS` — host OpenMP PCG only (same \(A,b\))  
- Also report: `n`, `nnz`, `PCG_ITERS`, `REL_RESIDUAL`, `CPU_THREADS`, `MAX_ABS_ERR`

### Verdict

| Condition | Verdict |
|-----------|---------|
| `SPEEDUP > 1` and residual OK and `MAX_ABS_ERR` small | **WIN** on linear-solve claim |
| `SPEEDUP ≤ 1` | **LOSE** on linear-solve claim |
| Full SIMPLE on GPU not implemented | **N/A** for “whole CFD on GPU” (primary v2 — separate question) |

---

## How to run (idle machine only)

Do **not** measure while training models or under heavy load.

```bash
# From clone root
make csrOcl          # OpenMP + OpenCL build

# Matrix must exist (ofDumpCsr once after a CFD run)
make bench-speedup
# or:
./build/csrOcl/csrOcl \
  --mtx cases/windTunnelCar/matrix/of_p.mtx \
  --rhs cases/windTunnelCar/matrix/of_p.rhs \
  --kernels build/csrOcl/kernels/csr.cl \
  --force-cpu-check --tol 1e-8 --max-iters 5000
```

Look for lines:

```text
CPU_THREADS ...
CPU_MS ...
GPU_SOLVE_MS ...
SPEEDUP_vs_cpu ...
VERDICT_LINEAR_SOLVE WIN|LOSE
```

Control threads: `OMP_NUM_THREADS=8 make bench-speedup` (use physical cores; try a small sweep 1/2/4/8/16).

---

## Pipeline with clocks (conceptual)

```text
[CFD context — CPU only today]
  simpleFoam  ~2540 s / 150 outer   (reference, not speedup denom)
        │
        ▼
  ofDumpCsr   ~tens of s            (export once; not in SPEEDUP)
        │
        ├──────────────────┬────────────────────┐
        ▼                  ▼                    │
  host OpenMP        GPU csrOcl                 │
  poly2-PCG          poly2-PCG                  │
  CPU_MS             GPU_SOLVE_MS (~1.8 s class)│
        │                  │                    │
        └──────── SPEEDUP ─┘                    │
                                                │
  post (ParaView / video) ──────────────────────┘  (never in SPEEDUP)
```

---

## Status of numbers

| Item | Status |
|------|--------|
| Matrix dump windTunnelCar | **done** (`n=1 244 327`, `nnz=8 608 083`) |
| GPU pressure solve | **done** |
| OpenMP CPU vs GPU speedup | **done** (idle machine) |
| Full CFD on GPU speedup | **not claimed** until primary v2 |

### Result — clean bench (primary claim)

| Quantity | Value |
|----------|-------|
| Case | `cases/windTunnelCar` pressure Laplacian snapshot |
| `n` / `nnz` | **1 244 327** / **8 608 083** |
| Algorithm | CSR poly2-PCG, tol `1e-8`, same on CPU and GPU |
| `PCG_ITERS` | **1006** (both arms) |
| `CPU_THREADS` | **20** (OpenMP) |
| `CPU_MS` | **23 717** (~23.7 s) |
| `GPU_SOLVE_MS` | **1 339** (~1.34 s) |
| `SPEEDUP_vs_cpu` | **17.7×** |
| `MAX_ABS_ERR` | **1.7×10⁻¹²** |
| `REL_RESIDUAL` | **9.7×10⁻⁹** |
| `VERDICT_LINEAR_SOLVE` | **WIN** |
| GPU | AMD gfx1030 (RX 6800 XT class) |
| Date | 2026-08-09 (idle host; no concurrent AI training) |

**Headline:** on the real car wind-tunnel pressure system, device-resident poly2-PCG is **~18× faster** than the same algorithm multi-threaded on CPU.

### Context only (not the speedup denominator)

| Quantity | Value |
|----------|-------|
| `simpleFoam` full case wall | ~**2542 s** / 150 outer iters (~17 s/outer) |
| ofDumpCsr export | ~**38 s** (one-shot; not in SPEEDUP) |

---

## Bottom line for readers and agents

1. **Primary answer:** speedup of **GPU poly2-PCG vs OpenMP CPU poly2-PCG** on **windTunnelCar pressure \(A,b\)**.  
2. **simpleFoam:** context only.  
3. **Video/images:** never part of the timer.  
4. **Full outer-loop GPU:** future work (v2), not this number.
