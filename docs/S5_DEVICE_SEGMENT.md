# S5 — couple full-device path into 3D flow

Primary demo (`cases/windTunnel3D`) already produces showable 3D CFD on **CPU OpenFOAM**.  
S5 is when the **device-resident** linear algebra path accelerates the *same class* of solves.

## Target lifecycle (unchanged)

```
startup:  mesh topology + fields + constants → GPU   (once)
loop:     assemble operators + PCG/precond + field update  on GPU
shutdown: download fields / forces for I/O               (once / rare)
```

Not: assemble on CPU → copy A → solve → copy x every iteration.

## Ladder toward S5

| Step | Deliverable | Status |
|------|-------------|--------|
| 1 | 2D structured DIA Laplace, full-device PCG + poly2 | **done** (`laplaceOcl`) |
| 2 | 3D structured DIA Poisson (7-point), same lifecycle | **done** (`--nz N`, MAX_ABS_ERR ~ 1e-13 on 32³) |
| 3 | CSR SpMV + PCG full-device (unstructured-ready format) | **done** (`apps/csrOcl`, host CSR assemble + one upload) |
| 3b | Import mesh topology CSR from polyMesh (graph Laplace) | **done** (`polyMesh_to_mtx.py` + `csrOcl --mtx`) — windTunnel 76k cells |
| 3c | Import **coefficient** matrix from OF pressure system | next |
| 4 | Replace *one* simpleFoam linear solve (e.g. pressure) with device segment | S5 mvp |
| 5 | Keep residual parity vs stock OF on windTunnel3D | gate |

## Wind-tunnel coupling sketch (mvp)

1. Run stock `simpleFoam` mesh + fields as today.  
2. Export or map the pressure matrix once per outer iteration **on device** if reassembled, or freeze topology and update coefficients in-place.  
3. Device PCG returns `p` correction; host continues SIMPLE outer loop until mvp proves residency.  
4. Later: U/turbulence segments, then multi-equation full-device outer loop.

## Non-goals for first S5 cut

- Bit-identical float vs OpenFOAM (residual band is enough).  
- Full turbulence models on GPU.  
- MPI multi-GPU.

## Validation

- Same mesh as `cases/windTunnel3D` (or a smaller clone).  
- Report: `REL_RESIDUAL`, Cd/Cl vs CPU reference within a stated band.  
- Traffic: `h2d` field ≈ 0 after startup; matrix not re-downloaded.
