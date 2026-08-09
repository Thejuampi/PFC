# Phase A5: smoke test GPU vs CPU (same scheme), max abs err < 1e-9
$ErrorActionPreference = "Stop"
$root = "G:\dev\repos\PFC"
$exe = Join-Path $root "apps\laplaceOcl\build\laplaceOcl.exe"
$kernels = Join-Path $root "apps\laplaceOcl\kernels\laplace.cl"

if (-not (Test-Path $exe)) {
    Write-Host "Building laplaceOcl..."
    & (Join-Path $root "scripts\build-laplace-ocl.ps1")
}

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
    $env:PATH = (Split-Path $gpp.Source) + ";" + $env:PATH
}

Write-Host "=== smoke 64x64 ==="
$out = & $exe --nx 64 --ny 64 --steps 5 --kernels $kernels --no-csv --quiet 2>&1 | Out-String
Write-Host $out

if ($out -notmatch 'MAX_ABS_ERR\s+([0-9.eE+-]+)') {
    throw "MAX_ABS_ERR not found in output"
}
$err = [double]$Matches[1]
Write-Host "Parsed MAX_ABS_ERR=$err"
if ($err -ge 1e-9) {
    throw "MAX_ABS_ERR $err >= 1e-9"
}

if ($out -notmatch 'TRAFFIC_BYTES h2d=0') {
    Write-Warning "Expected intentional h2d=0 (geometry on device)"
}
if ($out -notmatch 'd2h_field=') {
    throw "missing d2h_field traffic line"
}

Write-Host "TEST PASS"
exit 0
