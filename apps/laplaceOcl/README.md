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
