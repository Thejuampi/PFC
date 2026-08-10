# Agent instructions — PFC

## Default user command for GPU stack

```text
make
```

Builds device apps, auto-downloads OpenCL headers into `deps/`, runs smokes.  
**Do not** tell users to install or vendor OpenCL headers manually.  
**Do not** require a separate `make deps` step.

## GPU vs CPU speedup (scientific claim)

Canonical protocol: [`docs/CPU_GPU_SPEEDUP.md`](docs/CPU_GPU_SPEEDUP.md).

- **Compare:** same windTunnelCar pressure \(A,b\); GPU `csrOcl` vs **OpenMP** host poly2-PCG.  
- **Do not** use full `simpleFoam` wall as the speedup denominator (context only).  
- **Do not** bench under AI training / heavy load.  
- Command: `make bench-speedup` → report `SPEEDUP_vs_cpu` + `VERDICT_LINEAR_SOLVE`.

## Integrating with OpenFOAM

**Canonical guide:** [`docs/INTEGRATE_OPENFOAM.md`](docs/INTEGRATE_OPENFOAM.md)

### What to do (Mode A — only supported full path today)

All paths relative to the **PFC clone root** (never hardcode a developer’s `G:\…` or `/mnt/g/…`).

1. `make` on the GPU host.
2. OpenFOAM env active: `( cd apps/ofDumpCsr && wmake )`.
3. Target case: `( cd cases/windTunnel3D && ofDumpCsr )` or any case with mesh + fields.
4. `./build/csrOcl/csrOcl --mtx <case>/matrix/of_p.mtx --rhs <case>/matrix/of_p.rhs --kernels build/csrOcl/kernels/csr.cl`.
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
| Primary v2 | **SIMPLE** outer loop device-resident | active (`simpleOcl` + Mode B `pfcSimpleFoam`) |
| Primary v3 | **PIMPLE** on GPU | **after** v2b green — see `docs/SIMPLE_GPU.md` |
| Mode A | Export matrix → `csrOcl` | still the offline integration path |
| Mode B | In-loop GPU pressure | `pfcSimpleFoam` + `scripts/pfc_gpu_worker.ps1` |
| Mode C | Full SIMPLE on GPU | not yet |

## Repo map for agents

| Path | Use |
|------|-----|
| `Makefile` | build/test device apps |
| `apps/csrOcl` | device CSR PCG |
| `apps/simpleOcl` | device-resident SIMPLE outer loop skeleton |
| `apps/pfcSimpleFoam` | simpleFoam + Mode B GPU pressure |
| `scripts/pfc_gpu_worker.ps1` | Windows GPU worker for Mode B |
| `docs/SIMPLE_GPU.md` | SIMPLE GPU then PIMPLE GPU roadmap |
| `docs/FULL_GPU_PLAN.md` | **v2b plan** — work packages after PR #1 |
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
