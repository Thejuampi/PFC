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

### Slice 2 — Mode B: OF SIMPLE + GPU pressure (v2a product)

Couple stock `simpleFoam`-class outer loop to the device pressure segment:

- Once: LDU topology → device CSR structure.
- Each outer: pack coeffs + `b` → device (or assemble coeffs on device).
- Device PCG → write `p` back.
- Host continues U / turb until those move in v2b.

Bridge must respect the **Windows GPU host + WSL OpenFOAM** split (shared filesystem or OpenCL-in-WSL if ICD works). Prefer a stable C API from `simpleOcl`/`csrOcl`, not Matrix Market every iter.

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
