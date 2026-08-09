#!/usr/bin/env python3
"""Build a graph-Laplacian CSR Matrix Market file from OpenFOAM polyMesh.

Uses owner/neighbour internal faces as undirected cell-cell edges.
Produces SPD -Laplace operator (degree on diagonal, -1 off-diagonal) and
a unit RHS for all cells (Dirichlet-free pure topology Poisson-like system
with a slight diagonal boost so it is invertible without BC).

Usage:
  python scripts/polyMesh_to_mtx.py cases/windTunnel3D/constant/polyMesh out.mtx
"""
from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path


def parse_label_list(path: Path) -> list[int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    # After the header, OpenFOAM has:  N  (  ...labels...  )
    m = re.search(r"\n\s*(\d+)\s*\n\s*\(", text)
    if not m:
        raise RuntimeError(f"cannot find labelList size in {path}")
    n = int(m.group(1))
    start = m.end()
    end = text.find(")", start)
    if end < 0:
        raise RuntimeError(f"unterminated list in {path}")
    body = text[start:end]
    vals = [int(x) for x in body.split()]
    if len(vals) != n:
        raise RuntimeError(f"{path}: expected {n} labels, got {len(vals)}")
    return vals


def n_cells_from_note(path: Path) -> int | None:
    text = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"nCells:(\d+)", text)
    return int(m.group(1)) if m else None


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    mesh = Path(sys.argv[1])
    out = Path(sys.argv[2])
    owner_p = mesh / "owner"
    neigh_p = mesh / "neighbour"
    if not owner_p.is_file() or not neigh_p.is_file():
        print(f"missing owner/neighbour under {mesh}", file=sys.stderr)
        return 1

    owner = parse_label_list(owner_p)
    neigh = parse_label_list(neigh_p)
    n_int = len(neigh)
    # owner has all faces; first n_int are internal
    if len(owner) < n_int:
        print("owner shorter than neighbour", file=sys.stderr)
        return 1

    n_cells = n_cells_from_note(owner_p)
    if n_cells is None:
        n_cells = max(max(owner), max(neigh) if neigh else 0) + 1

    # adjacency undirected unique
    adj: dict[int, set[int]] = defaultdict(set)
    for f in range(n_int):
        a, b = owner[f], neigh[f]
        if a == b:
            continue
        adj[a].add(b)
        adj[b].add(a)

    # Matrix Market coordinate (symmetric stored as full for simple loaders)
    # A = (1+eps)*I + graph Laplacian  so SPD and invertible without BCs
    eps = 1e-2
    triples: list[tuple[int, int, float]] = []
    for i in range(n_cells):
        deg = len(adj[i])
        diag = deg + eps
        triples.append((i + 1, i + 1, diag))  # 1-based MM
        for j in sorted(adj[i]):
            triples.append((i + 1, j + 1, -1.0))

    nnz = len(triples)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8") as fh:
        fh.write("%%MatrixMarket matrix coordinate real general\n")
        fh.write(f"% polyMesh graph-Laplace+eps from {mesh.as_posix()}\n")
        fh.write(f"% nCells={n_cells} nInternalFaces={n_int} eps={eps}\n")
        fh.write(f"{n_cells} {n_cells} {nnz}\n")
        for r, c, v in triples:
            fh.write(f"{r} {c} {v}\n")

    # companion RHS (unit vector) as simple text
    rhs = out.with_suffix(".rhs")
    with rhs.open("w", encoding="utf-8") as fh:
        fh.write(f"{n_cells}\n")
        for _ in range(n_cells):
            fh.write("1\n")

    print(f"Wrote {out}  n={n_cells} nnz={nnz}")
    print(f"Wrote {rhs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
