# Thin wrapper around `make test-laplace` (+ optional VRAM stress)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
make test-laplace
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($env:LAPLACE_OCL_MEM_FRAC) {
    Write-Host "=== optional VRAM test (LAPLACE_OCL_MEM_FRAC=$env:LAPLACE_OCL_MEM_FRAC) ==="
    make test-vram
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
Write-Host "TEST PASS"
exit 0
