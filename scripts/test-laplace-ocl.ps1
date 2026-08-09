# GPU stress + correctness for laplaceOcl
# 1) Small mesh with CPU reference (correctness)
# 2) Fat mesh targeting >= 50% of device VRAM (stress; no host PCG)
$ErrorActionPreference = "Stop"
$root = "G:\dev\repos\PFC"
$exe = Join-Path $root "apps\laplaceOcl\build\laplaceOcl.exe"
$kernels = Join-Path $root "apps\laplaceOcl\kernels\laplace.cl"
$memFrac = if ($env:LAPLACE_OCL_MEM_FRAC) { [double]$env:LAPLACE_OCL_MEM_FRAC } else { 0.5 }

if (-not (Test-Path $exe)) {
    Write-Host "Building laplaceOcl..."
    & (Join-Path $root "scripts\build-laplace-ocl.ps1")
}

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
    $env:PATH = (Split-Path $gpp.Source) + ";" + $env:PATH
}

function Assert-Match([string]$text, [string]$pattern, [string]$label) {
    if ($text -notmatch $pattern) { throw "Missing $label (pattern: $pattern)" }
    return $Matches
}

# --- 1) Correctness (host PCG feasible) ---
Write-Host "=== correctness 256x256 (CPU check) ==="
$outSmall = & $exe --nx 256 --ny 256 --steps 5 --kernels $kernels --no-csv --quiet 2>&1 | Out-String
Write-Host $outSmall
$m = Assert-Match $outSmall 'MAX_ABS_ERR\s+([0-9.eE+-]+)' 'MAX_ABS_ERR'
$err = [double]$m[1]
if ($err -ge 1e-9) { throw "MAX_ABS_ERR $err >= 1e-9" }
Write-Host "Correctness OK (MAX_ABS_ERR=$err)"

# --- 2) VRAM stress: auto mesh for mem-frac of GPU global memory ---
Write-Host "=== VRAM stress --mem-frac $memFrac (no CPU check) ==="
# Occupy VRAM; fixed PCG iters so wall time is predictable on ~0.5*VRAM meshes
$outFat = & $exe `
    --mem-frac $memFrac `
    --steps 1 `
    --fixed-iters 5 `
    --precond poly2 `
    --kernels $kernels `
    --no-csv --quiet --no-cpu-check `
    2>&1 | Out-String
Write-Host $outFat

Assert-Match $outFat 'TRAFFIC_BYTES h2d=0' 'h2d=0'
Assert-Match $outFat 'd2h_field=' 'd2h_field'
$mf = Assert-Match $outFat 'MEM_FRAC_USED\s+([0-9.eE+-]+)' 'MEM_FRAC_USED'
$used = [double]$mf[1]
Write-Host "Parsed MEM_FRAC_USED=$used (require >= $memFrac * 0.95 for slack)"
# allow 5% slack from sqrt rounding / max-alloc caps
if ($used + 1e-9 -lt ($memFrac * 0.95)) {
    throw "MEM_FRAC_USED $used is below target floor $($memFrac * 0.95)"
}

# Sanity on field (downloaded once)
if ($outFat -notmatch 'T range\s*:\s*\[([0-9.eE+-]+),\s*([0-9.eE+-]+)\]') {
    throw "T range not found"
}
$tmin = [double]$Matches[1]
$tmax = [double]$Matches[2]
if ($tmin -lt 270 -or $tmax -gt 575 -or $tmax -lt $tmin) {
    throw "T range looks wrong: [$tmin, $tmax]"
}

Write-Host "VRAM stress OK (MEM_FRAC_USED=$used, T in [$tmin, $tmax])"
Write-Host "TEST PASS"
exit 0
