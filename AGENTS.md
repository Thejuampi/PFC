# Agent instructions — PFC

## Default user command for GPU stack

```text
make
```

Builds device apps, auto-downloads OpenCL headers into `deps/`, runs smokes.  
**Do not** tell users to install or vendor OpenCL headers manually.  
**Do not** require a separate `make deps` step.

## Integrating with OpenFOAM

**Canonical guide:** [`docs/INTEGRATE_OPENFOAM.md`](docs/INTEGRATE_OPENFOAM.md)

### What to do (Mode A — only supported full path today)

1. `make` on the GPU host.
2. In OpenFOAM env: `wmake` → `apps/ofDumpCsr`.
3. In the target case (mesh + fields): run `ofDumpCsr` → `matrix/of_p.mtx` + `matrix/of_p.rhs`.
4. `build/csrOcl/csrOcl --mtx … --rhs … --kernels build/csrOcl/kernels/csr.cl`.
5. Gate: log shows `RESIDUAL check   : OK` and process exit 0.

### What not to do

- Hybrid thrash: upload full \(A\) every PCG or every SIMPLE iteration.
- Treat `cpp/icoFoamOCL` / Paralution as current integration surface (archive only).
- Claim full OpenFOAM-on-GPU before v2b criteria in `docs/GOALS.md`.
- Commit `deps/`, `build/`, `polyMesh`, logs, VTK, animation frames.

### Product north star

| ID | Meaning | Status |
|----|---------|--------|
| Primary v1 | Showable 3D CFD CPU | done (`windTunnel3D`, `windTunnelCar`) |
| Primary v2 | SIMPLE outer loop device-resident | active |
| Mode A | Export matrix → `csrOcl` | **use this for integrations** |
| Mode B/C | In-loop / full loop | see `docs/S5_DEVICE_SEGMENT.md` |

## Repo map for agents

| Path | Use |
|------|-----|
| `Makefile` | build/test device apps |
| `apps/csrOcl` | device CSR PCG |
| `apps/ofDumpCsr` | OF LDU → Matrix Market |
| `apps/laplaceOcl` | structured DIA reference |
| `cases/windTunnelCar` | main visual demo |
| `cases/windTunnel3D` | smaller 3D + matrix export reference |
| `docs/INTEGRATE_OPENFOAM.md` | **how to integrate** |
| `docs/GOALS.md` | why / definition of done |

## When the user says “integrate with my OpenFOAM”

1. Open `docs/INTEGRATE_OPENFOAM.md`.
2. Execute Mode A on their case path.
3. Report `n`, `nnz`, `REL_RESIDUAL`, `TIMING_MS solve`, and exact commands.
4. Only then discuss Mode B (in-loop) if they need runtime coupling.
