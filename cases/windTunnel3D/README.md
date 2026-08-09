# windTunnel3D — primary-goal demo (3D)

**3D** external-flow style case: free stream past a bluff body (“vehicle”) in a rectangular tunnel.

Part of the **primary project goal** (`docs/GOALS.md`): show discrete CFD results, not only a linear solver.

## Geometry (meters)

| Region | Extent |
|--------|--------|
| Tunnel | \(x \in [0,12]\), \(y \in [0,3]\), \(z \in [0,4]\) |
| Vehicle (box) | \(x \in [3,5]\), \(y \in [0,1]\), \(z \in [1.2,2.8]\) |
| Inlet | \(U = (10,0,0)\) m/s |
| Physics v1 | `simpleFoam`, **laminar**, \(\nu = 0.01\) (Re ~ \(10^3\) on body length) |

Later: RAS, finer mesh, STL body (Ahmed / CAD).

## Run (WSL + OpenFOAM on G:)

```powershell
wsl -d Ubuntu-OF -- openfoam2512 -c "cd /mnt/g/dev/repos/PFC/cases/windTunnel3D && sed -i 's/\r$//' Allrun Allclean && bash Allrun"
```

## Visualize (ParaView on Windows)

1. Install ParaView (Windows build).  
2. Open: `G:\dev\repos\PFC\cases\windTunnel3D\case.foam`  
   (or **OpenFOAM** reader on the case directory).  
3. Suggested views:
   - Slice at `z = 2` → color by `|U|` or `p`
   - Streamlines from inlet plane  
   - Surface of patch `vehicle` → color by `p`

## Relation to GPU work

- This case is the **product demo** path (CPU OpenFOAM first).  
- `apps/laplaceOcl` is the **device-resident linear algebra** path that should eventually serve cases like this (secondary goals S1/S5).
