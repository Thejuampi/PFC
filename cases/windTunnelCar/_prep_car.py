#!/usr/bin/env python3
"""
Prep a real concept-car body (Khronos CarConcept.glb, CC BY 4.0)
into constant/triSurface/car.stl for snappyHexMesh.

Tunnel coords: X=streamwise, Y=up, Z=spanwise.
Domain: X[0,18] Y[0,4] Z[0,4]
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
import trimesh

SRC = Path(__file__).resolve().parent / "constant" / "triSurface" / "CarConcept.glb"
OUT = Path(__file__).resolve().parent / "constant" / "triSurface" / "car.stl"
BACKUP = Path(__file__).resolve().parent / "constant" / "triSurface" / "car_ahmed_backup.stl"


def load_with_transforms(path: Path) -> trimesh.Trimesh:
    scene = trimesh.load(str(path), force="scene")
    if not isinstance(scene, trimesh.Scene):
        return scene
    parts = []
    for node_name in scene.graph.nodes_geometry:
        transform, geom_name = scene.graph[node_name]
        geom = scene.geometry[geom_name]
        if not isinstance(geom, trimesh.Trimesh):
            continue
        g = geom.copy()
        g.apply_transform(transform)
        parts.append(g)
    if not parts:
        parts = [g.copy() for g in scene.geometry.values() if isinstance(g, trimesh.Trimesh)]
    return trimesh.util.concatenate(parts)


def main() -> int:
    if not SRC.is_file():
        raise SystemExit(f"missing {SRC}")

    if OUT.is_file() and not BACKUP.is_file():
        try:
            prev = trimesh.load(str(OUT), force="mesh")
            if len(prev.faces) < 40000:
                OUT.replace(BACKUP)
                print(f"backed up Ahmed → {BACKUP.name}")
        except Exception:
            pass

    print(f"loading {SRC.name}...")
    mesh = load_with_transforms(SRC)
    print(f"  raw verts={len(mesh.vertices)} faces={len(mesh.faces)}")
    print(f"  raw bounds=\n{mesh.bounds}")
    print(f"  raw extents={mesh.extents}")

    # After scene transforms (observed): X=width, Y=height, Z=length
    # Tunnel: X=length (stream), Y=height (up), Z=width
    # (mx, my, mz) -> (mz, my, mx)
    T = np.array(
        [
            [0, 0, 1, 0],
            [0, 1, 0, 0],
            [1, 0, 0, 0],
            [0, 0, 0, 1],
        ],
        dtype=float,
    )
    mesh.apply_transform(T)
    print(f"  after axes extents={mesh.extents}  (want L,H,W)")

    # Nose toward inlet (-X): rotate 180 about Y
    R = trimesh.transformations.rotation_matrix(np.pi, [0, 1, 0])
    mesh.apply_transform(R)

    # Scale by length (X)
    target_len = 4.2
    scale = target_len / float(mesh.extents[0])
    mesh.apply_scale(scale)
    print(f"  scale={scale:.4f} extents={mesh.extents}")

    # Place: ground y=0, centered z=2.0, nose ~ x=3.8
    b = mesh.bounds
    tx = 3.8 - b[0, 0]
    ty = 0.0 - b[0, 1]
    tz = 2.0 - 0.5 * (b[0, 2] + b[1, 2])
    mesh.apply_translation([tx, ty, tz])

    mesh.merge_vertices()
    mask = mesh.nondegenerate_faces()
    mesh.update_faces(mask)
    mesh.remove_unreferenced_vertices()
    try:
        trimesh.repair.fix_normals(mesh)
    except Exception as e:
        print(f"  fix_normals skipped: {e}")

    print(f"  final bounds=\n{mesh.bounds}")
    print(f"  final extents={mesh.extents}")
    print(f"  watertight={mesh.is_watertight}")
    print(f"  faces={len(mesh.faces)}")

    b = mesh.bounds
    assert b[0, 0] > 0.5 and b[1, 0] < 17.0, f"X out of domain {b[:, 0]}"
    assert b[0, 1] >= -0.05 and b[1, 1] < 3.5, f"Y out of domain {b[:, 1]}"
    assert b[0, 2] > 0.2 and b[1, 2] < 3.8, f"Z out of domain {b[:, 2]}"

    mesh.export(str(OUT))
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")

    lref = float(mesh.extents[0])
    aref = float(mesh.extents[1] * mesh.extents[2] * 0.70)
    cof = mesh.bounds.mean(axis=0)
    print(f"  suggest lRef={lref:.3f} Aref~{aref:.3f} CofR=({cof[0]:.2f} {cof[1]:.2f} {cof[2]:.2f})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
