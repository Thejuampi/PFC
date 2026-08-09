# ofDumpCsr — OpenFOAM → device CSR bridge

Assembles a pressure Laplacian on the current case and writes Matrix Market files
for [`csrOcl`](../csrOcl).

**Full integration guide (humans + agents):**  
→ **[`docs/INTEGRATE_OPENFOAM.md`](../../docs/INTEGRATE_OPENFOAM.md)**

## Quick start

```bash
# From PFC clone root

# 1) Build dump utility (OpenFOAM environment active)
( cd apps/ofDumpCsr && wmake )

# 2) Run inside a case that has mesh + p,U fields
( cd /path/to/YOUR_CASE && ofDumpCsr )
# → matrix/of_p.mtx  matrix/of_p.rhs

# 3) Solve on GPU host (same clone root)
make csrOcl
./build/csrOcl/csrOcl \
  --mtx /path/to/YOUR_CASE/matrix/of_p.mtx \
  --rhs /path/to/YOUR_CASE/matrix/of_p.rhs \
  --kernels build/csrOcl/kernels/csr.cl
```

Success: log line `RESIDUAL check   : OK`.

## This repo’s reference case

From the **clone root**:

```bash
# OpenFOAM env active
( cd apps/ofDumpCsr && wmake )
( cd cases/windTunnel3D && ofDumpCsr )

# GPU host (same tree)
make csrOcl
./build/csrOcl/csrOcl \
  --mtx cases/windTunnel3D/matrix/of_p.mtx \
  --rhs cases/windTunnel3D/matrix/of_p.rhs \
  --kernels build/csrOcl/kernels/csr.cl
```

## Notes

- Prefers **latest time** directory when a finished run exists.
- Uses `fvm::laplacian(p)` + pressure reference (cell 0).
- Does **not** modify your solver; pure export utility (integration Mode A).
