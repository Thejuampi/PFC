# csrOcl — full-device CSR PCG (S5 ladder step 3)

OpenCL **CSR** SpMV + poly2-PCG. Same residency rule as `laplaceOcl`:

1. Host builds (or later: imports) CSR **once**  
2. Upload matrix + RHS **once**  
3. PCG on GPU  
4. One solution download  

Connectivity is still structured (5/7-point Poisson) but **stored as CSR** — the format OpenFOAM LDU will map into next.

## Build / run

```powershell
mkdir apps\csrOcl\build -Force
cd apps\csrOcl\build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
.\csrOcl.exe --nx 64 --ny 64 --kernels ..\kernels\csr.cl
.\csrOcl.exe --nx 24 --ny 24 --nz 24 --kernels ..\kernels\csr.cl
```

## Metrics

```text
REL_RESIDUAL ...
PCG_ITERS ...
TRAFFIC_BYTES h2d=... d2h_field=... d2h_scalar=...
MAX_ABS_ERR ...   # vs host CSR PCG
```

## Next

- Import CSR from OpenFOAM (dump / `lduMatrix` export).  
- Use on pressure systems from `cases/windTunnel3D` (S5 mvp).
