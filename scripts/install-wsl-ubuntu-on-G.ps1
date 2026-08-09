#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Install Ubuntu WSL2 with the distro filesystem ON G: (not C:).

.DESCRIPTION
  NEVER use plain `wsl --install -d Ubuntu` — that drops the distro under
  C:\Users\...\AppData\Local\Packages (Store layout).

  This script:
    1. Snapshots C: before (on G:\dev\repos\PFC\docs\snapshots)
    2. Ensures WSL platform is available (platform bits may already live on C: —
       that is Windows itself; we cannot relocate Program Files\WSL)
    3. Downloads Ubuntu rootfs to G:\wsl\cache
    4. Imports distro to G:\wsl\ubuntu-24.04  (ext4.vhdx lives here)
    5. Snapshots C: after and prints the diff

  OpenFOAM packages installed later with apt land INSIDE the VHDX on G:.

.EXAMPLE
  # Elevated PowerShell:
  Set-ExecutionPolicy -Scope Process Bypass -Force
  G:\dev\repos\PFC\scripts\install-wsl-ubuntu-on-G.ps1
#>
param(
    [string]$DistroName = "Ubuntu-OF",
    [string]$InstallDir = "G:\wsl\ubuntu-24.04",
    [string]$CacheDir = "G:\wsl\cache",
    # Cloud image rootfs (Ubuntu 24.04 LTS). Override if URL breaks.
    [string]$RootfsUrl = "https://cloud-images.ubuntu.com/wsl/releases/24.04/current/ubuntu-noble-wsl-amd64-24.04lts.rootfs.tar.gz"
)

$ErrorActionPreference = "Stop"
$RepoScripts = "G:\dev\repos\PFC\scripts"

Write-Host "=== 0) Snapshot C: BEFORE ==="
& "$RepoScripts\snapshot-c-drive.ps1" -Label "before-wsl"

New-Item -ItemType Directory -Force -Path $InstallDir, $CacheDir | Out-Null

Write-Host "=== 1) Ensure WSL is installed (platform; may touch C:\Program Files\WSL) ==="
# --no-distribution avoids Store Ubuntu on C:
wsl --install --no-distribution 2>&1 | Write-Host

Write-Host "=== 2) Download rootfs to G: cache ==="
$rootfs = Join-Path $CacheDir "ubuntu-24.04.rootfs.tar.gz"
if (-not (Test-Path $rootfs)) {
    Write-Host "Downloading $RootfsUrl"
    Write-Host " -> $rootfs"
    # BITS/curl prefer G: temp
    $ProgressPreference = "SilentlyContinue"
    Invoke-WebRequest -Uri $RootfsUrl -OutFile $rootfs -UseBasicParsing
} else {
    Write-Host "Rootfs already cached: $rootfs"
}

Write-Host "=== 3) Import distro onto G: ==="
# If name exists, abort rather than clobber
$existing = & cmd /c "wsl -l -q 2>&1"
if ($existing -match [regex]::Escape($DistroName)) {
    throw "Distro '$DistroName' already registered. Unregister first if intentional: wsl --unregister $DistroName"
}

wsl --import $DistroName $InstallDir $rootfs --version 2
Write-Host "Imported $DistroName -> $InstallDir"

Write-Host "=== 4) Set default distro + smoke ==="
wsl --set-default $DistroName
wsl -d $DistroName -- uname -a
wsl -d $DistroName -- bash -lc "echo HOME=\$HOME; df -h / | tail -1"

Write-Host "=== 5) Snapshot C: AFTER + compare ==="
& "$RepoScripts\compare-c-snapshots.ps1" `
    -BeforeDir "G:\dev\repos\PFC\docs\snapshots\C-before-wsl-LATEST"

Write-Host ""
Write-Host "Next (inside WSL, project on G: mount):"
Write-Host "  wsl -d $DistroName"
Write-Host "  cd /mnt/g/dev/repos/PFC"
Write-Host "  bash scripts/setup-openfoam-wsl.sh"
Write-Host "  bash scripts/run-laplace-cpu.sh"
Write-Host ""
Write-Host "NOTE: C:\Program Files\WSL is the Windows WSL platform (already present ~0.8 GB)."
Write-Host "      Distro + apt + OpenFOAM must live under $InstallDir only."
