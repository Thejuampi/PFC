# pfcSimpleFoam — Mode B (SIMPLE + GPU pressure)

Stock **SIMPLE** outer loop (momentum + turbulence on CPU OpenFOAM).  
Optional: **pressure linear solve on GPU** via `csrOcl` (Mode B / primary v2a).

## Enable GPU pressure

```bash
# option A — env
export PFC_GPU_PRESSURE=1
export PFC_GPU_SCRIPT=/path/to/PFC/scripts/pfc_gpu_pcg.sh

# option B — case dict
# system/pfcGpuDict:
#   enabled  true;
```

Also set on the **GPU host** (Windows in this lab):

```bash
# from PFC clone root
make csrOcl
```

## Run (from case, OpenFOAM env)

```bash
# build once
( cd apps/pfcSimpleFoam && wmake )

cd cases/windTunnel3D   # or windTunnelCar
export PFC_GPU_PRESSURE=1
export PFC_GPU_SCRIPT=$PFC_ROOT/scripts/pfc_gpu_pcg.sh   # absolute path
pfcSimpleFoam
```

Without GPU flags, behaviour matches `simpleFoam` (CPU `pEqn.solve()`).

## Bridge

Each pressure corrector:

1. Host assembles `laplacian(rAtU,p) == div(phiHbyA)`
2. Writes `matrix/pfc_gpu.bin` (PFC1 CSR + b)
3. `scripts/pfc_gpu_pcg.sh` → `csrOcl --pfc-bin … --x-out …`
4. Host loads `matrix/pfc_gpu.x` into `p`, continues SIMPLE

**Not yet v2b:** matrix values still leave the host every outer (topology rebuild each time). Next step: topology once, coeffs update only, or assemble on device.

## Roadmap

See [`docs/SIMPLE_GPU.md`](../../docs/SIMPLE_GPU.md): SIMPLE GPU → then PIMPLE GPU.
