# ofDumpCsr — OpenFOAM → device CSR bridge

Assembles a pressure Laplacian on the current case and writes Matrix Market files
for [`csrOcl`](../csrOcl).

**Full integration guide (humans + agents):**  
→ **[`docs/INTEGRATE_OPENFOAM.md`](../../docs/INTEGRATE_OPENFOAM.md)**

## Quick start

```bash
# 1) Build (OpenFOAM environment)
cd apps/ofDumpCsr && wmake

# 2) Run inside a case that has mesh + p,U fields
cd /path/to/YOUR_CASE
ofDumpCsr
# → matrix/of_p.mtx  matrix/of_p.rhs
```

```powershell
# 3) Solve on GPU host (repo root)
make csrOcl
.\build\csrOcl\csrOcl.exe `
  --mtx \path\to\YOUR_CASE\matrix\of_p.mtx `
  --rhs \path\to\YOUR_CASE\matrix\of_p.rhs `
  --kernels build\csrOcl\kernels\csr.cl
```

Success: log line `RESIDUAL check   : OK`.

## This repo’s reference case

```bash
openfoam2512 -c "cd /mnt/g/dev/repos/PFC/apps/ofDumpCsr && wmake"
openfoam2512 -c "cd /mnt/g/dev/repos/PFC/cases/windTunnel3D && ofDumpCsr"
```

```powershell
make csrOcl
.\build\csrOcl\csrOcl.exe `
  --mtx cases\windTunnel3D\matrix\of_p.mtx `
  --rhs cases\windTunnel3D\matrix\of_p.rhs `
  --kernels build\csrOcl\kernels\csr.cl
```

## Notes

- Prefers **latest time** directory when a finished run exists.
- Uses `fvm::laplacian(p)` + pressure reference (cell 0).
- Does **not** modify your solver; pure export utility (integration Mode A).
