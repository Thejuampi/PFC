# simpleOcl — device-resident SIMPLE outer loop (primary v2)

**Status:** skeleton toward **v2a/v2b**. Not a drop-in `simpleFoam` yet.

## Goal

Run a **SIMPLE-class outer loop** with fields and linear solves **resident on the GPU** for the whole run (one upload at start, one download at end).  
After this path is solid → **PIMPLE on GPU** ([`docs/SIMPLE_GPU.md`](../../docs/SIMPLE_GPU.md)).

## Lifecycle

```text
startup:  mesh + U,p (+ later turb) → GPU
loop:     momentum → pressure Poisson/PCG → correct   (all on device)
shutdown: download fields
```

## Build / run

```bash
# from repo root
make simpleOcl
./build/simpleOcl/simpleOcl --nx 64 --ny 64 --outers 20 --tol 1e-8
# Windows: build\simpleOcl\simpleOcl.exe ...
```

## Relation to other apps

| App | Role |
|-----|------|
| `csrOcl` | One CSR system (Mode A / pressure brick) |
| `laplaceOcl` | Structured DIA Poisson brick |
| **`simpleOcl`** | **Outer SIMPLE loop** using those bricks |
| `ofDumpCsr` | OF → Matrix Market (Mode A bridge) |

## What this is not

- Not full OpenFOAM FV schemes on day one  
- Not PIMPLE (v3)  
- Not “hybrid copy A every outer” as the end state  
