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
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC && bash scripts/run-laplace-cpu.sh"
```

## Environment (G: only for distro)

| Asset | Location |
|-------|----------|
| WSL distro | `Ubuntu-OF` |
| VHDX | `G:\wsl\ubuntu-24.04\ext4.vhdx` |
| Rootfs cache | `G:\wsl\cache\ubuntu-24.04.rootfs.tar.gz` |
| OpenFOAM | inside VHDX: `/usr/lib/openfoam/openfoam2512` |
| Case | `G:\dev\repos\PFC\cases\laplaceCpu` |

C: delta after WSL import: **+0.03 GB used** (pass). OpenFOAM apt install grows **G: VHDX only**.

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
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC && MESH=500 SOLVER=GAMG bash scripts/run-laplace-cpu.sh"

# thesis-sized mesh (slow on CPU; for timing only)
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC && MESH=2000 SOLVER=GAMG bash scripts/run-laplace-cpu.sh"

# custom timed pure-Laplace (after wmake inside OF env)
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC/apps/laplaceTimed && wmake"
```

## Success criteria checklist

- [x] `blockMesh` OK  
- [x] `laplacianFoam` finishes  
- [x] Residuals via **DICPCG** only (no GPU solvers)  
- [x] Field output at endTime  
- [x] Distro not on C: Packages  
