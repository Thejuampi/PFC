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

Side ¾ + pressure + midplane speed:

![CarConcept side flow](../../docs/images/carconcept_side_flow.png)

Front stagnation (pressure on surface):

![CarConcept front pressure](../../docs/images/carconcept_front_pressure.png)

Geometry only:

| Side | Front |
|------|-------|
| ![body side](../../docs/images/carconcept_body.png) | ![body front](../../docs/images/carconcept_front_body.png) |

**Airflow animation (30 s @ 30 fps, fixed camera, inlet → outlet):**  
[`images/windTunnelCar_airflow_30s_30fps.mp4`](images/windTunnelCar_airflow_30s_30fps.mp4)

## Layout

```
0/                  # U, p, k, epsilon, nut
constant/
  triSurface/
    CarConcept.glb  # source mesh (Khronos sample asset)
    car.stl         # placed & scaled for the tunnel
  transportProperties
  turbulenceProperties
system/             # blockMesh, snappy, fv*, controlDict + forceCoeffs
scripts/
  render_animation.py   # pyvista offscreen → frames → ffmpeg
_prep_car.py            # glb → oriented car.stl
Allrun / Allclean
```

## Domain & BCs (summary)

- Box **18 × 4 × 4 m** (X streamwise, Y up, Z span)
- Car: nose ≈ x = 3.8 m, length 4.2 m, on the ground, centered in Z
- Inlet: fixed U = (10, 0, 0); outlet: fixed p; ground + vehicle: walls
- `forceCoeffs` on patch `vehicle` (`lRef=4.2`, `Aref≈1.9`)

## Run (WSL + OpenFOAM v2512)

```bash
# from Windows
wsl -d Ubuntu-OF -- openfoam2512 -c \
  "cd /mnt/g/dev/repos/PFC/cases/windTunnelCar && sed -i 's/\r$//' Allrun Allclean && bash Allrun"
```

Approx. wall time on this machine: snappy + 150 SIMPLE ≈ **40–50 min**.

Export + animate (Windows Python + ffmpeg):

```powershell
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC/cases/windTunnelCar && foamToVTK -latestTime"
python cases\windTunnelCar\scripts\render_animation.py
```

Tune perceived air speed with `N_CYCLES` in `scripts/render_animation.py` (default **8**).

## Regenerating `car.stl`

```powershell
python cases\windTunnelCar\_prep_car.py
```

Requires `trimesh`. Source: `constant/triSurface/CarConcept.glb`  
([Khronos glTF Sample Assets — CarConcept](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/CarConcept)).

## Credits

- Car mesh: © 2024 Darmstadt Graphics Group GmbH / Eric Chadwick — **CC BY 4.0** (Khronos CarConcept sample).
- Solver stack: OpenFOAM® v2512, pyvista, ffmpeg.
