# windTunnel3D — primary-goal demo (3D)

**3D** external-flow case: free stream past a bluff body (“vehicle”) in a rectangular tunnel.

Part of the **primary project goal** (`docs/GOALS.md`): show discrete CFD results (velocity, pressure, drag/lift), not only a linear solver.

## Geometry (meters)

| Region | Extent |
|--------|--------|
| Tunnel | \(x \in [0,12]\), \(y \in [0,3]\), \(z \in [0,4]\) |
| Vehicle (box) | \(x \in [3,5]\), \(y \in [0,1]\), \(z \in [1.2,2.8]\) |
| Inlet | \(U = (10,0,0)\) m/s |
| Physics | `simpleFoam`, **RAS kEpsilon**, wall functions on `vehicle` + `ground` |
| Viscosity | \(\nu = 0.01\) m²/s (Re ~ \(2\times 10^3\) on body length — demo-stable) |

Later: air-like \(\nu\), finer mesh, STL body (Ahmed / CAD).

## Outputs you can show

| What | Where |
|------|--------|
| Velocity / pressure fields | time directories → ParaView |
| Turbulent kinetic energy `k` | time directories |
| Drag / lift coeffs \(C_d\), \(C_l\) | `postProcessing/forces/.../coefficient.dat` |
| Residual history | `log.simpleFoam` |

Reference areas for forceCoeffs: `Aref = 1.6` m² (frontal), `lRef = 2` m (length).

## Run

From the **repo root**, with OpenFOAM environment loaded:

```bash
cd cases/windTunnel3D
sed -i 's/\r$//' Allrun Allclean 2>/dev/null || true
bash Allrun
```

Typical wall time on a modest mesh: a few minutes.

## Visualize (ParaView)

1. Install ParaView.  
2. Open `cases/windTunnel3D/case.foam` (create with `touch case.foam` if needed)  
   or use the OpenFOAM reader on the case directory.  
3. Suggested views:
   - Slice at `z = 2` → color by `|U|` or `p`
   - Streamlines from inlet plane  
   - Surface of patch `vehicle` → color by `p`
   - Contours of `k` in the wake
4. Plot force history: load `postProcessing/forces/*/coefficient.dat` (Cd, Cl columns).

## Relation to GPU work

- This case is the **product demo** path (CPU OpenFOAM first).  
- `apps/laplaceOcl` is the **device-resident linear algebra** path that should eventually serve cases like this (secondary goals S1/S5).
