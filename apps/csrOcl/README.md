# csrOcl — full-device CSR PCG

**OpenFOAM integration:** use with `ofDumpCsr` dumps —  
→ **[`docs/INTEGRATE_OPENFOAM.md`](../../docs/INTEGRATE_OPENFOAM.md)**

OpenCL **CSR** SpMV + poly2-PCG. Same residency rule as `laplaceOcl`:

1. Host builds (or imports) CSR **once**
2. Upload matrix + RHS **once**
3. PCG on GPU
4. One solution download

## Build / test / run

From the **repo root**:

```powershell
make              # auto-deps + build + test
make csrOcl
make run-csr
```

Binary: `build/csrOcl/csrOcl.exe`

```powershell
.\build\csrOcl\csrOcl.exe --nx 64 --ny 64 --kernels build\csrOcl\kernels\csr.cl
.\build\csrOcl\csrOcl.exe --nx 24 --ny 24 --nz 24 --kernels build\csrOcl\kernels\csr.cl
```

## Import real mesh topology (windTunnel3D)

```powershell
python scripts/polyMesh_to_mtx.py cases/windTunnel3D/constant/polyMesh cases/windTunnel3D/matrix/windTunnel3D_graphL.mtx

.\build\csrOcl\csrOcl.exe `
  --mtx cases/windTunnel3D/matrix/windTunnel3D_graphL.mtx `
  --rhs cases/windTunnel3D/matrix/windTunnel3D_graphL.rhs `
  --kernels build/csrOcl/kernels/csr.cl
```

Validated on RX 6800 XT: **n=76764, nnz=536124**, residual ~1e-8.
