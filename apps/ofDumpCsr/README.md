# ofDumpCsr — dump OpenFOAM pressure matrix for device CSR

OpenFOAM utility (WSL): assemble `fvm::laplacian(1, p) == fvc::div(phi)` on the
current case mesh/fields and write:

- `matrix/of_p.mtx` — Matrix Market  
- `matrix/of_p.rhs` — RHS  

Then solve on the GPU with `apps/csrOcl` (no OF linkage on Windows host).

## Build (WSL + OpenFOAM on G:)

```bash
openfoam2512 -c "cd /mnt/g/dev/repos/PFC/apps/ofDumpCsr && wmake"
```

## Run on windTunnel3D

```bash
# mesh + 0/ fields must exist (Allrun at least through snappy)
openfoam2512 -c "cd /mnt/g/dev/repos/PFC/cases/windTunnel3D && ofDumpCsr"
```

## Device solve

```powershell
.\apps\csrOcl\build\csrOcl.exe `
  --mtx cases\windTunnel3D\matrix\of_p.mtx `
  --rhs cases\windTunnel3D\matrix\of_p.rhs `
  --kernels apps\csrOcl\kernels\csr.cl
```
