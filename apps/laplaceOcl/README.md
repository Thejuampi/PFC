# laplaceOcl — full-device OpenCL diffusion

Structured 2D implicit-Euler diffusion + Jacobi-PCG, **entirely on the GPU**.

## Lifecycle

1. Create device buffers  
2. `mark_interior`, `init_temperature`, `assemble_A_dia` on GPU (**once**)  
3. Each step: `build_rhs` → PCG (`spmv`, axpy, Jacobi) on GPU  
4. **One** `clEnqueueReadBuffer` of `T` at the end  

Host only pulls a few scalar reductions per PCG iteration for residual checks (not the matrix). Use `--fixed-iters N` to skip residual host reads.

## BCs (PFC laplace case)

| Edge | T |
|------|---|
| top `j=ny-1` (hPatch) | 573 |
| left `i=0` (cPatch) | 373 |
| bottom / right (fixedWalls) | 273 |
| interior init | 273 |

## Build / run (Windows, MinGW)

```powershell
G:\dev\repos\PFC\scripts\build-laplace-ocl.ps1
G:\dev\repos\PFC\scripts\run-laplace-ocl.ps1 -Nx 100 -Ny 100 -Steps 10
```

Needs:

- `third_party/OpenCL-Headers`
- `third_party/opencl-lib/libOpenCL.a` (from system `OpenCL.dll`)
- AMD GPU OpenCL driver (verified: gfx1030 / RX 6800 XT)

## Validation

Same matrix/RHS scheme is re-run on CPU; expect max |ΔT| ~ 1e-12 … 1e-15 on smoke meshes.

```powershell
G:\dev\repos\PFC\scripts\test-laplace-ocl.ps1   # 256² CPU check + --mem-frac 0.5 VRAM stress
G:\dev\repos\PFC\scripts\bench-laplace-ocl.ps1  # 100 / 500 / 2000 → docs/BENCH_LAPLACE_OCL.md

# Fill ~50% of GPU VRAM (auto nx=ny from device global mem):
.\apps\laplaceOcl\build\laplaceOcl.exe --mem-frac 0.5 --steps 2 --no-cpu-check --no-csv --kernels .\apps\laplaceOcl\kernels\laplace.cl
```

## Metrics (parseable)

Each run prints:

```text
TIMING_MS setup=... solve=... download=... total=...
TRAFFIC_BYTES h2d=0 d2h_field=... d2h_scalar=... scalar_reads=...
PCG_ITERS total=...
MAX_ABS_ERR ...          # if CPU check on
HYBRID_EST_BYTES_per_run≈...
```

### `--precond jacobi|poly2|rbgs`

| Precond | Notes |
|---------|--------|
| **poly2** (default) | `M^{-1} ≈ 2 D^{-1} - D^{-1} A D^{-1}` — SPD-friendly, fewer CG iters than Jacobi |
| **jacobi** | `M^{-1} = D^{-1}` — baseline |
| **rbgs** | Red-black GS (experimental). Not SPD → often **hurts** CG; kept for research |

On 500² / 10 steps (this machine): poly2 ~202 PCG iters / ~77 ms solve vs jacobi ~387 iters / ~147 ms.

### `--fixed-iters N`

Runs exactly N PCG iterations per step **without** residual norm host checks (only the dots needed for PCG itself still read small reduction buffers). Use when measuring pure device throughput; correctness mode keeps default residual-based exit.

### `--no-csv` / `--quiet`

Skip field CSV and per-step logs (useful for benches/tests).
