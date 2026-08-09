# Step 0 — Traditional CPU baseline (no GPU)

**Status: DONE** on this machine (2026-08-09).

## Result

```
OpenFOAM  : v2512
App       : laplacianFoam (stock)
Solver    : DICPCG (PCG + DIC)
Mesh      : 100 x 100 x 1  (10k cells)
Run       : endTime 0.05, 10 steps, residuals drop each step
Wall      : ~0.08 s ExecutionTime
```

Command used:

```powershell
# From repo root, OpenFOAM env loaded:
bash scripts/run-laplace-cpu.sh
```

## Environment

| Asset | Notes |
|-------|--------|
| OpenFOAM | v2512-class (native Linux or WSL) |
| Case | `cases/laplaceCpu` (relative to clone root) |

Optional lab note (one developer machine kept distro off C:): see [`WSL_ON_G.md`](WSL_ON_G.md).

## What was modernized vs 2015

| Item | 2015 | Step 0 |
|------|------|--------|
| OpenFOAM | 2.4.0 | **v2512** |
| Case | `cpp/LaplaceQt` | `cases/laplaceCpu` |
| Binary | custom Laplace + Paralution | stock **laplacianFoam** |
| Linear solvers | paralution_* / PCG | **PCG+DIC** or GAMG (CPU) |
| blockMeshDict | `patches` + convertToMeters | `boundary` + `scale` in **system/** |
| Default mesh | 2000² | 100² smoke (`MESH=` override) |
| GPU libs | OpenCL / Paralution | **none** |

## Optional next commands

```powershell
# medium mesh + GAMG
MESH=500 SOLVER=GAMG bash scripts/run-laplace-cpu.sh

# thesis-sized mesh (slow on CPU; for timing only)
MESH=2000 SOLVER=GAMG bash scripts/run-laplace-cpu.sh

# custom timed pure-Laplace (after wmake inside OF env)
( cd apps/laplaceTimed && wmake )
```

## Success criteria checklist

- [x] `blockMesh` OK  
- [x] `laplacianFoam` finishes  
- [x] Residuals via **DICPCG** only (no GPU solvers)  
- [x] Field output at endTime  
- [x] Distro not on C: Packages  
