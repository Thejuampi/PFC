# Mode B GPU worker (Windows host).
# Watches matrix/pfc_gpu.request, runs csrOcl.exe, writes pfc_gpu.x + pfc_gpu.done
#
# Usage (from PFC clone on Windows):
#   powershell -ExecutionPolicy Bypass -File scripts\pfc_gpu_worker.ps1 [-CaseDir path] [-Once]
#
# Default CaseDir: cases\windTunnel3D under PFC root.

param(
    [string]$CaseDir = "",
    [string]$PfcRoot = "",
    [switch]$Once,
    [double]$PollSec = 0.25
)

$ErrorActionPreference = "Stop"

if (-not $PfcRoot) {
    $PfcRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -ErrorAction SilentlyContinue
    if (-not $PfcRoot) { $PfcRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path }
    # script is scripts/ → parent is PFC root
    $PfcRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

if (-not $CaseDir) {
    $CaseDir = Join-Path $PfcRoot "cases\windTunnel3D"
}

$exe = Join-Path $PfcRoot "build\csrOcl\csrOcl.exe"
$kernels = Join-Path $PfcRoot "build\csrOcl\kernels\csr.cl"
$matrixDir = Join-Path $CaseDir "matrix"
$bin = Join-Path $matrixDir "pfc_gpu.bin"
$xout = Join-Path $matrixDir "pfc_gpu.x"
$req = Join-Path $matrixDir "pfc_gpu.request"
$done = Join-Path $matrixDir "pfc_gpu.done"
$fail = Join-Path $matrixDir "pfc_gpu.fail"
$log = Join-Path $matrixDir "pfc_gpu_worker.log"

if (-not (Test-Path $exe)) {
    Write-Error "csrOcl.exe not found: $exe  (run: make csrOcl)"
}
if (-not (Test-Path $kernels)) {
    Write-Error "kernels not found: $kernels"
}

New-Item -ItemType Directory -Force -Path $matrixDir | Out-Null

function Write-Log($msg) {
    $line = "{0} {1}" -f (Get-Date -Format "o"), $msg
    Add-Content -Path $log -Value $line
    Write-Host $line
}

Write-Log "worker start case=$CaseDir once=$Once"
Write-Log "exe=$exe"

while ($true) {
    if (Test-Path $req) {
        Remove-Item $req -Force -ErrorAction SilentlyContinue
        Remove-Item $done -Force -ErrorAction SilentlyContinue
        Remove-Item $fail -Force -ErrorAction SilentlyContinue
        if (-not (Test-Path $bin)) {
            Write-Log "ERROR: missing $bin"
            Set-Content -Path $fail -Value "missing bin"
            if ($Once) { exit 1 }
            continue
        }
        Write-Log "solve start"
        $args = @(
            "--pfc-bin", $bin,
            "--x-out", $xout,
            "--kernels", $kernels,
            "--no-cpu-check",
            "--tol", "1e-8",
            "--max-iters", "5000"
        )
        try {
            $p = Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru -NoNewWindow `
                -RedirectStandardOutput (Join-Path $matrixDir "pfc_gpu_csr.out") `
                -RedirectStandardError (Join-Path $matrixDir "pfc_gpu_csr.err")
            if ($p.ExitCode -ne 0 -and -not (Test-Path $xout)) {
                Write-Log "csrOcl exit $($p.ExitCode) and no x-out"
                Set-Content -Path $fail -Value "exit $($p.ExitCode)"
            } else {
                # Mode B: x-out may exist even if residual warn
                if (Test-Path $xout) {
                    Set-Content -Path $done -Value "ok exit=$($p.ExitCode)"
                    Write-Log "solve done exit=$($p.ExitCode)"
                } else {
                    Set-Content -Path $fail -Value "no x-out exit=$($p.ExitCode)"
                    Write-Log "solve failed exit=$($p.ExitCode)"
                }
            }
        } catch {
            Write-Log "EXCEPTION $_"
            Set-Content -Path $fail -Value "$_"
        }
        if ($Once) { exit 0 }
    }
    Start-Sleep -Seconds $PollSec
}
