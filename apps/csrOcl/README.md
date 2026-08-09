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

## Import real mesh topology (windTunnel3D)

```powershell
# needs existing polyMesh (run Allrun once)
python scripts/polyMesh_to_mtx.py cases/windTunnel3D/constant/polyMesh cases/windTunnel3D/matrix/windTunnel3D_graphL.mtx

.\apps\csrOcl\build\csrOcl.exe `
  --mtx cases/windTunnel3D/matrix/windTunnel3D_graphL.mtx `
  --rhs cases/windTunnel3D/matrix/windTunnel3D_graphL.rhs `
  --kernels apps/csrOcl/kernels/csr.cl
```

Validated on RX 6800 XT: **n=76764, nnz=536124**, poly2-PCG ~125 iters, REL_RESIDUAL ~1e-8, solve ~60 ms.

## Next

- Import **coefficient** CSR from OpenFOAM pressure matrix (not just graph topology).  
- S5 mvp: replace one SIMPLE pressure solve with device CSR PCG.
