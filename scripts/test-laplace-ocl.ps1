# Correctness tests for laplaceOcl
# 1) Small mesh: GPU vs CPU field error
# 2) ~50% VRAM mesh: true residual ||b-Ax||/||b|| after convergent PCG
$ErrorActionPreference = "Stop"
$root = "G:\dev\repos\PFC"
$exe = Join-Path $root "apps\laplaceOcl\build\laplaceOcl.exe"
$kernels = Join-Path $root "apps\laplaceOcl\kernels\laplace.cl"
$memFrac = if ($env:LAPLACE_OCL_MEM_FRAC) { [double]$env:LAPLACE_OCL_MEM_FRAC } else { 0.5 }
$tol = 1e-8

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

# --- 1) Field correctness vs CPU ---
Write-Host "=== correctness 256x256 (CPU check) ==="
$outSmall = & $exe --nx 256 --ny 256 --steps 5 --tol $tol --kernels $kernels --no-csv --quiet 2>&1 | Out-String
Write-Host $outSmall
if ($LASTEXITCODE -ne 0) { throw "small run exit $LASTEXITCODE" }
$m = Assert-Match $outSmall 'MAX_ABS_ERR\s+([0-9.eE+-]+)' 'MAX_ABS_ERR'
$err = [double]$m[1]
if ($err -ge 1e-9) { throw "MAX_ABS_ERR $err >= 1e-9" }
$r = Assert-Match $outSmall 'REL_RESIDUAL\s+([0-9.eE+-]+)' 'REL_RESIDUAL'
if ([double]$r[1] -gt $tol * 10) { throw "small REL_RESIDUAL too large: $($r[1])" }
Write-Host "Correctness OK (MAX_ABS_ERR=$err, REL_RESIDUAL=$($r[1]))"

# --- 2) Large mesh: residual validation (no host PCG) ---
Write-Host "=== VRAM validate --mem-frac $memFrac (convergent PCG + residual) ==="
$outFat = & $exe `
    --mem-frac $memFrac `
    --steps 1 `
    --tol $tol `
    --max-iters 5000 `
    --precond poly2 `
    --kernels $kernels `
    --no-csv --quiet --no-cpu-check `
    2>&1 | Out-String
Write-Host $outFat
if ($LASTEXITCODE -ne 0) { throw "fat run exit $LASTEXITCODE" }

Assert-Match $outFat 'TRAFFIC_BYTES h2d=0' 'h2d=0'
$mf = Assert-Match $outFat 'MEM_FRAC_USED\s+([0-9.eE+-]+)' 'MEM_FRAC_USED'
$used = [double]$mf[1]
if ($used + 1e-9 -lt ($memFrac * 0.95)) {
    throw "MEM_FRAC_USED $used below floor $($memFrac * 0.95)"
}

$rr = Assert-Match $outFat 'REL_RESIDUAL\s+([0-9.eE+-]+)' 'REL_RESIDUAL'
$rel = [double]$rr[1]
# Must meet solver tol (with small slack for FP)
if ($rel -gt $tol * 10) {
    throw "REL_RESIDUAL $rel not converged (tol=$tol)"
}
if ($outFat -notmatch 'RESIDUAL check\s*:\s*OK') {
    throw "RESIDUAL check not OK"
}

Write-Host "VRAM validate OK (MEM_FRAC_USED=$used, REL_RESIDUAL=$rel)"
Write-Host "TEST PASS"
exit 0
