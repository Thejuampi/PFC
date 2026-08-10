# laplaceOcl bench (Phase A3 + A4)

Generated: 2026-08-09  
Machine: Windows host + AMD OpenCL (RX 6800 XT / gfx1030)  
Steps: 10  

Default preconditioner: **poly2** (`M^{-1} ≈ 2 D^{-1} - D^{-1} A D^{-1}`).

## Results

| Mesh | Precond | PCG iters | solve ms | total ms | max \|ΔT\| | speedup vs CPU |
|------|---------|----------:|---------:|---------:|-----------:|---------------:|
| 100×100 | poly2 | 51 | 20.3 | 24.3 | 4.5e-13 | 0.20 |
| 500×500 | jacobi | 387 | 136 | 139 | 3.7e-12 | 14.7 |
| 500×500 | **poly2** | **202** | **73** | **77** | 1.8e-12 | **26.6** |
| 2000×2000 | poly2 | 799 | 2103 | 2130 | n/a (no CPU check) | n/a |

## Takeaways

- **poly2** ≈ **2× fewer PCG iterations** and ≈ **2× faster solve** than Jacobi on 500².
- Field **H2D = 0**; only residual reduction scalars + final `T` download cross the bus.
- **rbgs** is available for experiments but is **not SPD** → often **hurts** CG (more iters). Prefer poly2/jacobi with PCG.
- 2000² thesis-scale mesh runs in ~2.1 s solve on this GPU with poly2.

## Re-run

```powershell
.\scripts\build-laplace-ocl.ps1
.\scripts\test-laplace-ocl.ps1
.\scripts\bench-laplace-ocl.ps1
# manual compare:
.\apps\laplaceOcl\build\laplaceOcl.exe --nx 500 --ny 500 --precond jacobi --no-csv --quiet --kernels .\apps\laplaceOcl\kernels\laplace.cl
.\apps\laplaceOcl\build\laplaceOcl.exe --nx 500 --ny 500 --precond poly2  --no-csv --quiet --kernels .\apps\laplaceOcl\kernels\laplace.cl
```
