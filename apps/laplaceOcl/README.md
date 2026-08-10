# laplaceOcl — full-device OpenCL diffusion

Structured 2D/3D DIA Laplace: assemble + poly2-PCG on the GPU, one final download of `T`.

## Build / test / run

From the **repo root** (no setup step for headers):

```powershell
make              # builds this app + csrOcl, runs smokes
make laplaceOcl   # this binary only
make run-laplace
```

Binary: `build/laplaceOcl/laplaceOcl.exe`

Requires a working OpenCL ICD (e.g. AMD GPU driver). Headers and the MinGW
import lib are pulled into `deps/` on first compile.

## Example

```powershell
.\build\laplaceOcl\laplaceOcl.exe --nx 256 --ny 256 --steps 10 --kernels build\laplaceOcl\kernels\laplace.cl
.\build\laplaceOcl\laplaceOcl.exe --nx 32 --ny 32 --nz 32 --steps 5 --kernels build\laplaceOcl\kernels\laplace.cl
```

## Metrics printed

```text
REL_RESIDUAL ...
MAX_ABS_ERR ...      # vs host reference (unless --no-cpu-check)
TRAFFIC_BYTES h2d=0 ...
MEM_FRAC_USED ...
```
