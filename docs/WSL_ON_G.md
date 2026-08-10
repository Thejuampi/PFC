# WSL + OpenFOAM only on G: (policy)

## Constraint

**Nothing project-related may grow on C:.**  
Repo, distro disk, apt packages, OpenFOAM → **G:** only.

## Snapshot (taken 2026-08-09 before any WSL distro install)

Location:

- `docs/snapshots/C-before-wsl-20260809-130159/`
- pointer: `docs/snapshots/C-before-wsl-LATEST/`

| Disk | Free GB | Used GB |
|------|---------|---------|
| **C:** | **83.89** | **153.63** |
| **G:** | **1366.53** | **496.47** |

### Relevant C: state *before*

| Path | Size / note |
|------|-------------|
| `C:\Program Files\WSL` | **0.822 GB** — WSL platform already installed |
| `C:\Users\Juan\AppData\Local\Packages` | 1.339 GB total; WSL Store package entry ~0 |
| `C:\Users\Juan\AppData\Local\wsl` | empty (0) |
| `C:\Users\Juan\.wslconfig` | missing |
| Distros | **none** |
| Docker on C: | not present |

### What we *cannot* fully eliminate on C:

- **`C:\Program Files\WSL`** — Windows component (kernel/tools). Already there.
- Optional feature metadata under Windows (tiny).
- Transient download cache if a tool ignores our paths (we avoid that).

That platform is **not** the Ubuntu rootfs and **not** OpenFOAM.

### What must live on G:

| Asset | Path |
|-------|------|
| Distro VHDX | `G:\wsl\ubuntu-24.04\` |
| Rootfs download cache | `G:\wsl\cache\` |
| Project / cases | `G:\dev\repos\PFC\` |
| OpenFOAM (apt inside distro) | inside the VHDX on G: |

## Forbidden

```text
wsl --install -d Ubuntu          # Store → C:\Users\...\Packages\...
Ubuntu from Microsoft Store UI   # same
Docker Desktop default           # often C: unless relocated
```

## Allowed install path

Elevated PowerShell:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
G:\dev\repos\PFC\scripts\install-wsl-ubuntu-on-G.ps1
```

That script:

1. Snapshots C:  
2. `wsl --install --no-distribution` (platform only)  
3. Downloads Ubuntu rootfs → `G:\wsl\cache`  
4. `wsl --import ... G:\wsl\ubuntu-24.04`  
5. Snapshots C: again and diffs  

Manual compare anytime:

```powershell
G:\dev\repos\PFC\scripts\compare-c-snapshots.ps1
```

## Pass / fail after install

- **Pass:** C: used delta ≲ 0.5 GB; no new fat folders under `AppData\Local\Packages\*Ubuntu*`; `G:\wsl\ubuntu-24.04\ext4.vhdx` exists and grows with apt.  
- **Fail:** Ubuntu under `C:\Users\Juan\AppData\Local\Packages\Canonical...` → unregister and re-import on G:.

```powershell
wsl --unregister Ubuntu-OF   # or whatever name landed on C:
# then re-run install-wsl-ubuntu-on-G.ps1
```
