# windTunnelCar — concept-car wind tunnel (OpenFOAM)

Steady external aerodynamics around a **real concept-car body** (Khronos *CarConcept*, CC BY 4.0), meshed with `snappyHexMesh` and solved with `simpleFoam` (RAS k-ε).

## Results (this demo)

| Quantity | Value |
|----------|--------|
| Body | CarConcept, length **4.2 m** |
| Free-stream \|U\| | **10 m/s** |
| Mesh (order of) | **~1.2 M** cells after snappy |
| Iterations | 150 SIMPLE |
| **Cd** | **≈ 0.87** |
| **Cl** | see `postProcessing/forces/` after a run |

## Gallery

Stills live in [`docs/images/`](../../docs/images/) (shared with the root README).

| Side flow | Front pressure |
|:--:|:--:|
| ![side](../../docs/images/carconcept_side_flow.png) | ![front](../../docs/images/carconcept_front_pressure.png) |

**Airflow animation (30 s @ 30 fps, fixed camera, inlet → outlet):**  
[`images/windTunnelCar_airflow_30s_30fps.mp4`](images/windTunnelCar_airflow_30s_30fps.mp4)

## Layout

```
0/                  # U, p, k, epsilon, nut
constant/
  triSurface/
    car.stl         # placed & scaled for the tunnel (checked in)
  transportProperties
  turbulenceProperties
system/             # blockMesh, snappy, fv*, controlDict + forceCoeffs
scripts/
  render_animation.py   # pyvista offscreen → frames → ffmpeg
_prep_car.py            # optional: download glb → regenerate car.stl
Allrun / Allclean
```

## Domain & BCs (summary)

- Box **18 × 4 × 4 m** (X streamwise, Y up, Z span)
- Car: nose ≈ x = 3.8 m, length 4.2 m, on the ground, centered in Z
- Inlet: fixed U = (10, 0, 0); outlet: fixed p; ground + vehicle: walls
- `forceCoeffs` on patch `vehicle` (`lRef=4.2`, `Aref≈1.9`)

## Run (OpenFOAM v2512-class)

From the **repo root**, with OpenFOAM environment loaded:

```bash
cd cases/windTunnelCar
sed -i 's/\r$//' Allrun Allclean 2>/dev/null || true
bash Allrun
```

Approx. wall time: snappy + 150 SIMPLE ≈ **40–50 min** (depends on machine).

Export + animate (from repo root; needs Python `pyvista` + `ffmpeg`):

```bash
( cd cases/windTunnelCar && foamToVTK -latestTime )
python cases/windTunnelCar/scripts/render_animation.py
```

Tune perceived air speed with `N_CYCLES` in `scripts/render_animation.py` (default **8**).

## Regenerating `car.stl` (optional)

`car.stl` is already committed. To rebuild from the Khronos GLB:

```bash
python cases/windTunnelCar/_prep_car.py
```

Requires `trimesh`. Downloads CarConcept.glb if missing  
([Khronos glTF Sample Assets — CarConcept](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept)).
## Credits

- Car mesh: © 2024 Darmstadt Graphics Group GmbH / Eric Chadwick — **CC BY 4.0** (Khronos CarConcept sample).
- Solver stack: OpenFOAM® v2512, pyvista, ffmpeg.
