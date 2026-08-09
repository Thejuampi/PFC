# PFC — Linear solvers / GPGPU ideas for OpenFOAM (revival)

Original project (2015): *Análisis e Implementación de Resolutores Lineales en GPGPU aplicados a OpenFOAM®*  
GitHub: [Thejuampi/PFC](https://github.com/Thejuampi/PFC)

This checkout modernizes a **step-0 traditional CPU baseline** on current OpenFOAM, without GPU plugins.

## Step 0 status (this machine)

| Item | Value |
|------|--------|
| Distro | WSL2 **Ubuntu-OF** (Ubuntu 24.04 rootfs) |
| Distro disk | **`G:\wsl\ubuntu-24.04\ext4.vhdx`** (not on C:) |
| OpenFOAM | **v2512** (`openfoam2512` package) |
| Baseline case | `cases/laplaceCpu` |
| Solver | stock **`laplacianFoam`** + **PCG+DIC** (CPU) |
| Smoke mesh | 100×100×1 — OK (DICPCG converges) |

### Run again

```powershell
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC && bash scripts/run-laplace-cpu.sh"
```

Larger mesh / GAMG:

```powershell
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC && MESH=500 SOLVER=GAMG bash scripts/run-laplace-cpu.sh"
```

### C: disk policy

**Project rule:** distro + apt + OpenFOAM stay on **G:**.  
See `docs/WSL_ON_G.md`. Snapshots: `docs/snapshots/`.

Compare anytime:

```powershell
G:\dev\repos\PFC\scripts\compare-c-snapshots.ps1 -BeforeDir G:\dev\repos\PFC\docs\snapshots\C-before-wsl-LATEST
```

## Layout

```
cases/laplaceCpu/     # modern CPU case (stock laplacianFoam) — reference
apps/laplaceOcl/      # full-device OpenCL Laplace (assemble+PCG on GPU)
apps/laplaceTimed/    # optional timed pure-Laplace OF app (wmake)
scripts/              # WSL-on-G, OpenFOAM, laplaceOcl build/run, C: snapshots
docs/                 # STEP0, WSL_ON_G, AMD_GPU_ROADMAP, snapshots
cpp/                  # LEGACY 2015 code (SpeedIT, OpenCL, Paralution) — archive
latex/                # original thesis sources
third_party/          # OpenCL-Headers + MinGW import lib (on G:)
```

## GPU path (OpenCL, full-device)

Windows host + AMD OpenCL (RX 6800 XT). One assemble on device, time loop on device, one download of `T`.

```powershell
G:\dev\repos\PFC\scripts\build-laplace-ocl.ps1
G:\dev\repos\PFC\scripts\run-laplace-ocl.ps1 -Nx 100 -Ny 100 -Steps 10
# larger:
G:\dev\repos\PFC\apps\laplaceOcl\build\laplaceOcl.exe --nx 500 --ny 500 --steps 10 --kernels G:\dev\repos\PFC\apps\laplaceOcl\kernels\laplace.cl
```
## Why step 0 is CPU-only

CPU stock OpenFOAM is only the **reference** (fields, residuals, wall time).

The product direction is **not** hybrid offload. It is a **full-device lifecycle**: one load to GPU at start, assemble+solve in place for the whole run, one unload at end. OpenCL is a valid implementation of that (see `docs/AMD_GPU_ROADMAP.md`). The 2015 problem was per-iteration CPU↔GPU copies, not “OpenCL bad”.

## Legacy (do not expect to build as-is)

- OpenFOAM **2.4.0**, Ubuntu 14.04  
- Paralution / OpenCL / SpeedIT plugins under `cpp/`  
- Custom `Laplace` binary linked against GPU libs  

## Hardware note

GPU: **AMD Radeon RX 6800 XT**. Full-device OpenCL is on the table; API choice is secondary to memory residency.
