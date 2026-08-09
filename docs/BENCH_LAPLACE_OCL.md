# laplaceOcl bench (Phase A3)

Generated: 2026-08-09 13:28
Machine: Windows host + AMD OpenCL (RX 6800 XT / gfx1030)
Steps: 10 | App: apps/laplaceOcl

## Timing and traffic

| Mesh | Cells | setup ms | solve ms | download ms | total ms | CPU ref ms | speedup | PCG iters | max\|ΔT\| | H2D | D2H field | D2H scalar | scalar reads | hybrid-est bytes |
|------|------:|---------:|---------:|------------:|---------:|-----------:|--------:|----------:|----------:|----:|----------:|-----------:|-------------:|-----------------:|
| 100x100 | 10000 | 1.4528 | 30.7562 | 0.3809 | 32.5899 | 5.1599 | 0.158328 | 86 | 5.11591e-13 | 0 | 80000 | 137216 | 268 | ? |
| 500x500 | 250000 | 3.0388 | 149.326 | 1.384 | 153.749 | 3090.96 | 20.104 | 387 | 3.69482e-12 | 0 | 2000000 | 599552 | 1171 | ? |
| 2000x2000 | 4000000 | 14.2404 | 2948.09 | 15.9822 | 2978.31 | n/a | n/a | 1610 | n/a | 0 | 32000000 | 2478080 | 4840 | ? |

## Notes

- **H2D field uploads = 0**: mesh/init/assemble run on device.
- **D2H field**: single final `T` download (`n * 8` bytes).
- **D2H scalar**: residual reduction partials only (not the matrix).
- **hybrid-est**: rough bytes if A (5 diagonals) + x + b + sol were recopied every time step (2015-style).
- 2000² CPU check skipped (too slow for the paired host PCG); use smaller meshes for correctness.

Re-run:
```powershell
.\scripts\bench-laplace-ocl.ps1
```
