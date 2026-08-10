# Plan — Full SIMPLE on GPU (primary v2b)

**Status:** planning (post–PR #1 merge)  
**Depends on:** Mode A + Mode B shipped on `master`  
**After this:** PIMPLE on GPU (v3) — only when v2b is green  

Canonical: [`GOALS.md`](GOALS.md) · [`SIMPLE_GPU.md`](SIMPLE_GPU.md) · [`S5_DEVICE_SEGMENT.md`](S5_DEVICE_SEGMENT.md)

---

## Goal (one sentence)

Run the **steady incompressible SIMPLE outer loop** for a wind-tunnel-class case with **fields + assemble + sparse solves + updates resident on the GPU** for the whole solve phase — one upload at start, rare download for I/O.

```text
startup:  mesh topology + fields + constants → GPU
loop:     U → p → correct → turb     entirely on device
shutdown: fields / forces → host
```

**Not** the end state: re-export \(A\) every outer (current Mode B bridge).

---

## Baseline (what we already have)

| Asset | Role in full GPU |
|-------|------------------|
| `csrOcl` | Device CSR poly2-PCG brick |
| `simpleOcl` | Outer-loop residency skeleton (structured) |
| `pfcSimpleFoam` Mode B | Real OF SIMPLE + GPU **p** only (file worker) |
| `ofDumpCsr` / PFC1 | Matrix export / debug |
| `windTunnel3D` / `Car` | CPU reference + showable demos |
| ~17× Mode A speedup | Linear-solve claim only |

---

## Work packages (ordered)

### WP0 — Freeze regression gates (1–2 days)

Keep green forever while building v2b:

- [ ] `make` + `make test` (laplaceOcl, csrOcl, simpleOcl)
- [ ] Mode A: `ofDumpCsr` + `csrOcl` residual OK on box and car matrices
- [ ] Mode B smoke: `pfcSimpleFoam` + worker, 1–2 outers, residual OK
- [ ] Document commands in `FULL_GPU_PLAN` / README “gates”

**Exit:** CI-or-script checklist that every full-GPU PR must pass.

---

### WP1 — Kill file thrash (topology once) (3–7 days)

Mode B+ without full FV rewrite:

- [ ] Once per run: map OF `lduAddressing` → device CSR **structure** (rowPtr, colInd)
- [ ] Each pressure corrector: upload **vals + b only** (or shared-memory / socket pack)
- [ ] Long-lived GPU process (replace per-call `csrOcl.exe` spawn)
  - Preferred: socket or named-pipe protocol on Windows host
  - Or: OpenCL ICD working **inside WSL** (investigate `/dev/dxg` + vendor)
- [ ] Metrics: no mid-PCG H2D of \(A\); report bytes/outer for coeffs only

**Exit:** same residual band as Mode B; wall time dominated by GPU solve, not process spawn + full CSR rewrite.

**Risk:** WSL↔Windows IPC; mitigate with single long-lived worker protocol.

---

### WP2 — Device field state for \(U,p,\phi\) (1–2 weeks)

- [ ] Keep \(U,p,\phi\) (and later \(k,\varepsilon\)) as device buffers for the whole run
- [ ] Host only seeds at start and reads for writeTime / forces
- [ ] Pressure solve writes \(p\) in-place on device; host optional mirror for debug

**Exit:** Mode B pressure path no longer needs full \(p\) D2H every outer except when writing time dirs.

---

### WP3 — Momentum on GPU (2–4 weeks)

Hardest linear-algebra segment after pressure:

- [ ] Assemble (or update) momentum systems on device **or** host-assemble + device-solve (intermediate)
- [ ] Prefer same CSR PCG brick; diagonal/under-relaxation matching `UEqn.relax()` band
- [ ] Validate \(U\) residual vs stock `simpleFoam` on `windTunnel3D` (small endTime)

**Exit:** one outer: GPU \(U\) + GPU \(p\) + host turb still OK for Cd band ± stated %.

---

### WP4 — Correctors on GPU (1–2 weeks)

- [ ] \(rAU\), \(HbyA\), \(\phi_{HbyA}\), flux correction, \(U\) corrector on device
- [ ] Continuity error metric on device (scalar D2H only)

**Exit:** full pressure–velocity SIMPLE corrector without host FV for that segment.

---

### WP5 — Turbulence RAS \(k,\varepsilon\) on GPU (1–3 weeks)

- [ ] \(k,\varepsilon\) equations assemble+solve (or device-solve of host-built \(A\))
- [ ] Boundedness / production terms — start with schemes matching case `fvSchemes`
- [ ] Validate vs CPU \(k\) field band on box tunnel

**Exit:** **v2b claim:** full SIMPLE outer (U, p, k, ε) device-driven for box case.

---

### WP6 — Validation + product packaging (1–2 weeks)

- [ ] `windTunnel3D`: Cd/Cl within agreed band vs CPU reference run
- [ ] Optional: car case reduced endTime smoke
- [ ] Bench table: **outer-loop wall** CPU `simpleFoam` vs device path (not Mode A only)
- [ ] Traffic report: h2d field/matrix after startup ≈ 0
- [ ] README + GOALS checkboxes; demote Mode B file bridge to “legacy fallback”
- [ ] **Do not start PIMPLE (v3) until this gate is signed**

---

## Non-goals for v2b

| Out of scope | Why |
|--------------|-----|
| Bit-identical float vs OpenFOAM | Residual/field band is enough |
| All OF schemes / multi-region / MPI | Scope explosion |
| PIMPLE / transient | v3 after v2b |
| OEM aero certification | Demo + scientific honesty |

---

## Architecture target

```text
┌─────────────────────────────────────────────────────────┐
│  GPU (OpenCL)                                           │
│   topology CSR (once)                                   │
│   fields: U, p, φ, k, ε                                 │
│   loop: assemble/update coeffs → PCG → field update     │
│   scalars only: residual, Cd/Cl partials (optional)     │
└─────────────────────────────────────────────────────────┘
         ▲ once                         │ rare D2H
         │ H2D mesh/IC                  ▼
┌─────────────────────────────────────────────────────────┐
│  Host                                                   │
│   mesh gen, case I/O, ParaView, forceCoeffs write       │
│   optional: OF as mesh/BC authority at t=0 only         │
└─────────────────────────────────────────────────────────┘
```

Two viable product shapes (pick in WP1 design spike):

| Shape | Pros | Cons |
|-------|------|------|
| **A. OF host + device segment** | Reuse mesh, BCs, I/O | Bridge complexity |
| **B. Standalone `simpleOcl` FV** | Clean residency | Reimplement more FV |

**Recommendation:** Shape A for validation parity; grow `simpleOcl` as pure-device lab for kernels (already started).

---

## Suggested PR stack (after this plan)

| PR | Scope | Gate |
|----|-------|------|
| PR-GPU-1 | WP0 gates + long-lived worker / topology-once pressure | Mode B residual + faster outer |
| PR-GPU-2 | WP2–3 momentum + field residency | U+p vs CPU band |
| PR-GPU-3 | WP4 correctors | continuity + Cd stable |
| PR-GPU-4 | WP5 turb + WP6 packaging | **v2b done** |
| PR-GPU-5 | PIMPLE v3 (new plan) | only after v2b |

---

## First concrete actions (next session)

1. Open branch `feature/full-gpu-wp1-topology-once` from `master`.  
2. Spike: long-lived `csrOcl` server (`--serve` or separate `csrOclServe`) reading vals+b, keeping topology.  
3. Point `pfcSimpleFoam` at server; delete per-outer full PFC1 rebuild of indices.  
4. Measure: ms/outer pressure vs Mode B file spawn.  
5. Only then schedule WP3 momentum design.

---

## Success definition (v2b)

- [ ] Wind-tunnel-class SIMPLE run: **solve phase** does not thrash matrix/fields over PCIe every outer.  
- [ ] Equations: **U, p, k, ε** (RAS) on device path.  
- [ ] Cd/Cl (or agreed force metric) within band vs CPU `simpleFoam` on `windTunnel3D`.  
- [ ] Documented run + bench table.  
- [ ] PIMPLE explicitly **not** required to claim v2b.
