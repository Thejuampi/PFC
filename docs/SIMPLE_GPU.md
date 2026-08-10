# SIMPLE on GPU → then PIMPLE on GPU

**Product decision:** ship a working **device-resident SIMPLE** loop first; only then build **PIMPLE** on the same device machinery.

```text
Mode A (done)     export A,b → csrOcl once
       ↓
v2a (now)         SIMPLE outer loop: pressure (at least) resident on GPU
       ↓
v2b               full SIMPLE: U + p + (k,ε) assemble+solve+update on GPU
       ↓
v3                PIMPLE on GPU (time + PISO/PIMPLE correctors) — reuse v2
```

Canonical goals: [`GOALS.md`](GOALS.md) · ladder: [`S5_DEVICE_SEGMENT.md`](S5_DEVICE_SEGMENT.md).

---

## Why SIMPLE first

| | SIMPLE | PIMPLE |
|--|--------|--------|
| Time | Steady outer iters | Transient + outer correctors |
| Loop shape | One outer pattern | Time × outer × (optional PISO) |
| Demo fit | `windTunnelCar` / `windTunnel3D` already | Needs `pimpleFoam` case + Δt |
| Risk | Lower | Higher (stability, flux correctors) |

PIMPLE **reuses** momentum / pressure / corrector kernels from SIMPLE. Starting PIMPLE first would rewrite the same segments under more pressure.

---

## Lifecycle (both solvers)

```text
startup:  mesh topology + fields + constants → GPU   (once)
loop:     assemble + linear solve + field update    on GPU
shutdown: download fields / forces                   (once / rare)
```

**Forbidden:** re-upload full \(A\) every PCG iteration (2015 trap).  
**Allowed in v2a mvp:** host-driven outer loop with **device-resident** pressure solve (topology on GPU; coeffs/RHS updated without thrashing mid-PCG).  
**v2b target:** assemble on GPU too.

---

## Implementation slices

### Slice 0 — Mode A regression (keep green)

`ofDumpCsr` + `csrOcl` on windTunnel matrices. Gate for every PR.

### Slice 1 — `apps/simpleOcl` (device SIMPLE skeleton) **← active**

Structured (then CSR) **mini-SIMPLE** full-device:

1. Fields `U`, `p` (and later `k,ε`) live on GPU for the whole run.
2. Outer loop `nOuter` times on device:
   - momentum segment (start simple; grow to FV-quality)
   - pressure Poisson + device PCG (poly2)
   - velocity / flux corrector
3. Metrics: `OUTER_ITERS`, `REL_RESIDUAL`, `TRAFFIC` (h2d after startup ≈ 0 for fields/matrix).
4. Optional host reference on small grids.

This proves residency **without** requiring OpenCL inside WSL OpenFOAM on day one.

### Slice 2 — Mode B: OF SIMPLE + GPU pressure (v2a) **landed**

App: **`apps/pfcSimpleFoam`** (stock SIMPLE; optional GPU pressure).

Each pressure corrector:

1. Host assembles `laplacian(rAtU,p) == div(phiHbyA)` (+ boundary fold-in)
2. Writes `matrix/pfc_gpu.bin` (PFC1 CSR + b)
3. GPU: `csrOcl --pfc-bin … --x-out …` via **Windows file-watch worker**
4. Host loads `matrix/pfc_gpu.x` into `p`, continues SIMPLE (U / turb on CPU)

```bash
# Terminal A — Windows GPU host
make csrOcl
powershell -ExecutionPolicy Bypass -File scripts/pfc_gpu_worker.ps1 `
  -CaseDir cases\windTunnel3D -PfcRoot .

# Terminal B — OpenFOAM (WSL)
export PFC_GPU_PRESSURE=1
( cd apps/pfcSimpleFoam && wmake )
cd cases/windTunnel3D && pfcSimpleFoam
```

**Smoke (windTunnel3D, gfx1030):** n≈77k, REL_RESIDUAL ~1e‑8, GPU solve ~50–70 ms, SIMPLE continued (Cd≈1.25).

Still **not** full residency (A re-exported each corrector). Next: topology once / assemble on device (v2b).

### Slice 3 — v2b full SIMPLE on device

U, p, RAS set assemble+solve+update resident. Cd/Cl band vs CPU `simpleFoam`.

### Slice 4 — v3 PIMPLE on GPU

- New case class (`pimpleFoam` / transient tunnel or cavity).
- Reuse device momentum + pressure + correctors.
- Add time loop + PIMPLE corrector counts.
- Do **not** start until v2b is accepted.

---

## Definition of done (reminders)

| Claim | Gate |
|-------|------|
| “Pressure in SIMPLE on GPU” (v2a) | Device-resident PCG inside outer iter; residual band vs OF; no mid-PCG A thrash |
| “SIMPLE on GPU” (v2b) | Full outer equations on device; one load / rare unload; Cd/Cl band |
| “PIMPLE on GPU” (v3) | Same residency for transient loop; only after v2b |

---

## Non-goals (until later)

- Bit-identical float vs OpenFOAM  
- Full scheme zoo (every div/grad limiter)  
- MPI multi-GPU  
- Starting PIMPLE before SIMPLE device loop is green  

---

## Commands (as slices land)

```bash
# Slice 0 (today)
make
make bench-speedup   # idle machine; linear-solve claim only

# Slice 1 (simpleOcl)
make simpleOcl
make test-simple
```
