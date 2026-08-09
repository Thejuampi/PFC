# PFC — 3D CFD you can show + device-resident GPU solvers

Revival of the 2015 thesis project  
***Análisis e Implementación de Resolutores Lineales en GPGPU aplicados a OpenFOAM®***  
Repo: [Thejuampi/PFC](https://github.com/Thejuampi/PFC)

---

## What this is

Two tracks, one repo:

| Track | Goal | Status |
|-------|------|--------|
| **Primary v1** | Showable **3D wind-tunnel CFD** on stock OpenFOAM | **done** |
| **Primary v2** | That same solve loop **device-resident on GPU** (assemble + sparse solves + field updates) | **active** |
| Secondary | Full-device OpenCL Laplace / CSR PCG ladder toward v2 | **done** (building blocks) |

Not a hybrid “offload one kernel per iteration” design — the 2015 lesson was that **CPU↔GPU thrash** kills you. See [`docs/GOALS.md`](docs/GOALS.md) and [`docs/S5_DEVICE_SEGMENT.md`](docs/S5_DEVICE_SEGMENT.md).

---

## Showcase — concept car in a wind tunnel

**Case:** [`cases/windTunnelCar`](cases/windTunnelCar)  
Real concept-car body (**Khronos CarConcept**), `snappyHexMesh` + `simpleFoam` RAS k-ε, force coefficients.

| | |
|:--:|:--:|
| ![Side ¾ — pressure + streamlines](docs/images/carconcept_side_flow.png) | ![Front — stagnation pressure](docs/images/carconcept_front_pressure.png) |
| Side ¾ · surface *p* · midplane \|U\| · streamlines | Front · high pressure on nose |

| Geometry (side) | Geometry (front) |
|:--:|:--:|
| ![body](docs/images/carconcept_body.png) | ![front body](docs/images/carconcept_front_body.png) |

**Demo numbers (this machine):**

| Item | Value |
|------|--------|
| Length | 4.2 m |
| Free stream | 10 m/s |
| Mesh | ~1.2 M cells |
| **Cd** | **≈ 0.87** |
| Animation | 30 s @ 30 fps, fixed camera, air from the inlet |

**Video:** [`cases/windTunnelCar/images/windTunnelCar_airflow_30s_30fps.mp4`](cases/windTunnelCar/images/windTunnelCar_airflow_30s_30fps.mp4)

```bash
# From repo root, with OpenFOAM env loaded (Linux or WSL)
cd cases/windTunnelCar
sed -i 's/\r$//' Allrun Allclean 2>/dev/null || true   # if checkout had CRLF
bash Allrun
foamToVTK -latestTime

# Optional animation (host Python + ffmpeg + pyvista)
# from repo root:
python cases/windTunnelCar/scripts/render_animation.py
```

Details: [`cases/windTunnelCar/README.md`](cases/windTunnelCar/README.md)

---

## Also: bluff-box tunnel (first 3D milestone)

**Case:** [`cases/windTunnel3D`](cases/windTunnel3D) — simple boxed vehicle, same RAS + forceCoeffs stack.

| Pressure on vehicle | Wake slice | Midplane speed |
|:--:|:--:|:--:|
| ![p](docs/images/box_vehicle_pressure.png) | ![wake](docs/images/box_vehicle_wake.png) | ![U](docs/images/box_slice_umag.png) |

Converges ~222 iters · **Cd ≈ 1.25**, **Cl ≈ 0.63** (bluff demo body, ν = 0.01).

```bash
# From repo root, OpenFOAM env loaded
cd cases/windTunnel3D && sed -i 's/\r$//' Allrun Allclean 2>/dev/null || true && bash Allrun
```

---

## GPU path (full-device OpenCL)

Windows host + **AMD Radeon RX 6800 XT**. One upload → assemble + PCG on device → one download of the solution.

### Architecture

```mermaid
flowchart LR
  subgraph OF["OpenFOAM host (WSL/Linux)"]
    Case["Your case<br/>mesh + fields"]
    Dump["ofDumpCsr"]
    Case --> Dump
    Dump --> MTX["matrix/of_p.mtx<br/>matrix/of_p.rhs"]
  end

  subgraph GPU["GPU host (OpenCL)"]
    Make["make<br/>auto-deps + build"]
    CSR["csrOcl<br/>CSR SpMV + poly2-PCG"]
    Dev["Device memory<br/>A, b, x resident"]
    Make --> CSR
    CSR -->|"H2D once"| Dev
    Dev -->|"PCG loop on GPU"| Dev
    Dev -->|"D2H once"| X["solution x<br/>residual OK"]
  end

  MTX --> CSR

  style Dev fill:#1a3a2a,stroke:#3d8,color:#fff
  style CSR fill:#1a2a3a,stroke:#48f,color:#fff
```

**Rule:** matrix/fields live on the GPU for the whole solve — not copy-per-iteration (that was the 2015 trap).

| App | Role |
|-----|------|
| [`apps/laplaceOcl`](apps/laplaceOcl) | Structured 2D/3D DIA Laplace, poly2, residual gate, VRAM stress |
| [`apps/csrOcl`](apps/csrOcl) | Unstructured-ready **CSR** SpMV + PCG |
| [`apps/ofDumpCsr`](apps/ofDumpCsr) | Dump real OpenFOAM `laplacian(p)` → Matrix Market for the GPU |
| [`scripts/polyMesh_to_mtx.py`](scripts/polyMesh_to_mtx.py) | polyMesh topology → CSR |

```bash
# From repo root (GPU host: g++, curl, OpenCL driver)
make            # build + test (OpenCL headers auto-downloaded on first run)
make run        # quick demo of both apps
```

### Integrate with *your* OpenFOAM (people + agents)

**→ Full guide: [`docs/INTEGRATE_OPENFOAM.md`](docs/INTEGRATE_OPENFOAM.md)**  
**→ Agent checklist: [`AGENTS.md`](AGENTS.md)**

Supported **today** = **Mode A (export)**: dump the pressure system from any case, solve on the GPU. Not a silent drop-in `lduMatrix` replacement yet (that is primary v2 / Mode B–C).

```bash
# All paths relative to the clone root
make                                          # 1) device solver

# 2) OF bridge + dump (OpenFOAM environment must be active)
( cd apps/ofDumpCsr && wmake )
( cd cases/windTunnel3D && ofDumpCsr )        # or: cd /path/to/YOUR_CASE && ofDumpCsr

# 3) GPU solve
./build/csrOcl/csrOcl \
  --mtx cases/windTunnel3D/matrix/of_p.mtx \
  --rhs cases/windTunnel3D/matrix/of_p.rhs \
  --kernels build/csrOcl/kernels/csr.cl
# Windows: build\csrOcl\csrOcl.exe  (same relative args)
```

Demonstrated (box tunnel matrix class): residual OK; car tunnel pressure on GPU is ~**1–2 s**/solve class (re-bench when idle).

Same three steps work for **any** case with mesh + `p`/`U` fields — see the integration doc.

### GPU vs CPU speedup (the scientific question)

**Question:** on the **car wind-tunnel pressure system** (same \(A,b\)), how much faster is device CSR poly2-PCG than a **strong multi-thread CPU** of the *same* algorithm?

| Arm | Time (pressure solve, n≈1.24e6) |
|-----|----------------------------------|
| CPU OpenMP poly2-PCG (20 threads) | **23.7 s** |
| GPU `csrOcl` poly2-PCG (gfx1030) | **1.34 s** |
| **SPEEDUP** | **~17.7×** → **WIN** |

- Same 1006 PCG iters, residual ~1e‑8, max |Δx| ~1e‑12.  
- **Not** the metric: full `simpleFoam` wall (~42 min) vs one GPU solve — simpleFoam is **context only**.  
- Protocol: [`docs/CPU_GPU_SPEEDUP.md`](docs/CPU_GPU_SPEEDUP.md) · re-run: `make bench-speedup`
Roadmap: [`docs/AMD_GPU_ROADMAP.md`](docs/AMD_GPU_ROADMAP.md) · device segment: [`docs/S5_DEVICE_SEGMENT.md`](docs/S5_DEVICE_SEGMENT.md) · goals: [`docs/GOALS.md`](docs/GOALS.md)

---

## Requirements (generic)

| Need | Notes |
|------|--------|
| OpenFOAM | v2412/v2512-class; env loaded (`openfoam2512`, `source …/bashrc`, etc.) |
| Device apps | `g++`, `curl`, OpenCL ICD; then `make` from repo root |
| Optional viz | ParaView; animation: Python `pyvista` + `ffmpeg` |

CPU baseline smoke (from **repo root**, OpenFOAM env active):

```bash
bash scripts/run-laplace-cpu.sh
```

Machine-specific WSL-on-another-drive notes (optional): [`docs/WSL_ON_G.md`](docs/WSL_ON_G.md).

---

## Layout

```
cases/windTunnelCar/   # ★ main visual demo — concept car + airflow video
cases/windTunnel3D/    # bluff-box 3D milestone
cases/laplaceCpu/      # stock laplacianFoam reference
apps/laplaceOcl/       # full-device OpenCL DIA Laplace
apps/csrOcl/           # full-device CSR SpMV+PCG
apps/ofDumpCsr/        # OF → Matrix Market for GPU
docs/                  # goals, S5 segment, WSL-on-G
docs/images/           # README gallery stills
scripts/               # build/run/bench helpers
cpp/                   # LEGACY 2015 (archive — do not expect to build)
latex/                 # original thesis sources
deps/                  # auto cache (gitignored) — you never touch this
cmake/                 # auto-fetch helpers if you use CMake
```

---

## Design note

CPU OpenFOAM is the **reference** (fields, residuals, Cd/Cl, pictures).  
The product bet is a **full-device lifecycle** for the outer SIMPLE-class loop — not shipping mesh generation or ParaView to the GPU.

Legacy stack (OpenFOAM 2.4 / Paralution / SpeedIT under `cpp/`) stays as historical archive.

---

## License / credits

- Code: see repo `LICENSE`
- CarConcept mesh: © 2024 Darmstadt Graphics Group GmbH — **CC BY 4.0**  
  ([Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept))
- OpenFOAM® is a registered trademark of OpenCFD Ltd
