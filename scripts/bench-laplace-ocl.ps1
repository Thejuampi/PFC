# Phase A3: bench matrix for laplaceOcl (writes docs/BENCH_LAPLACE_OCL.md)
param(
    [int[]]$Sizes = @(100, 500, 2000),
    [int]$Steps = 10
)

$ErrorActionPreference = "Stop"
$root = "G:\dev\repos\PFC"
$exe = Join-Path $root "apps\laplaceOcl\build\laplaceOcl.exe"
$kernels = Join-Path $root "apps\laplaceOcl\kernels\laplace.cl"
$doc = Join-Path $root "docs\BENCH_LAPLACE_OCL.md"

if (-not (Test-Path $exe)) {
    & (Join-Path $root "scripts\build-laplace-ocl.ps1")
}
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) { $env:PATH = (Split-Path $gpp.Source) + ";" + $env:PATH }

$stamp = Get-Date -Format "yyyy-MM-dd HH:mm"
$rows = @()

foreach ($n in $Sizes) {
    Write-Host "=== bench ${n}x${n} steps=$Steps ==="
    $cpuFlag = @()
    # CPU reference is O(n^2 * iters); skip CPU check on very large meshes
    if ($n -ge 1500) { $cpuFlag = @("--no-cpu-check") }

    $raw = & $exe --nx $n --ny $n --steps $Steps --kernels $kernels --no-csv --quiet @cpuFlag 2>&1 | Out-String
    Write-Host $raw

    $timing = if ($raw -match 'TIMING_MS setup=([0-9.]+)\s+solve=([0-9.]+)\s+download=([0-9.]+)\s+total=([0-9.]+)') {
        @{ setup = $Matches[1]; solve = $Matches[2]; download = $Matches[3]; total = $Matches[4] }
    } else { @{ setup = "?"; solve = "?"; download = "?"; total = "?" } }

    $traffic = if ($raw -match 'TRAFFIC_BYTES h2d=(\d+)\s+d2h_field=(\d+)\s+d2h_scalar=(\d+)\s+scalar_reads=(\d+)') {
        @{ h2d = $Matches[1]; field = $Matches[2]; scalar = $Matches[3]; reads = $Matches[4] }
    } else { @{ h2d = "?"; field = "?"; scalar = "?"; reads = "?" } }

    $pcg = if ($raw -match 'PCG_ITERS total=(\d+)') { $Matches[1] } else { "?" }
    $maxAbs = if ($raw -match 'MAX_ABS_ERR\s+([0-9.eE+-]+)') { $Matches[1] } else { "n/a" }
    $cpuMs = if ($raw -match 'CPU_MS\s+([0-9.]+)') { $Matches[1] } else { "n/a" }
    $speed = if ($raw -match 'SPEEDUP_vs_cpu\s+([0-9.eE+-]+)') { $Matches[1] } else { "n/a" }
    $hybrid = if ($raw -match 'HYBRID_EST_BYTES_per_run≈(\d+)') { $Matches[1] } else { "?" }

    $rows += [pscustomobject]@{
        Mesh = "${n}x${n}"
        Cells = $n * $n
        SetupMs = $timing.setup
        SolveMs = $timing.solve
        DownloadMs = $timing.download
        TotalMs = $timing.total
        CpuMs = $cpuMs
        Speedup = $speed
        PcgIters = $pcg
        MaxAbsErr = $maxAbs
        H2D = $traffic.h2d
        D2HField = $traffic.field
        D2HScalar = $traffic.scalar
        ScalarReads = $traffic.reads
        HybridEstBytes = $hybrid
    }
}

$md = @()
$md += "# laplaceOcl bench (Phase A3)"
$md += ""
$md += "Generated: $stamp"
$md += "Machine: Windows host + AMD OpenCL (RX 6800 XT / gfx1030)"
$md += "Steps: $Steps | App: apps/laplaceOcl"
$md += ""
$md += "## Timing and traffic"
$md += ""
$md += "| Mesh | Cells | setup ms | solve ms | download ms | total ms | CPU ref ms | speedup | PCG iters | max\|ΔT\| | H2D | D2H field | D2H scalar | scalar reads | hybrid-est bytes |"
$md += "|------|------:|---------:|---------:|------------:|---------:|-----------:|--------:|----------:|----------:|----:|----------:|-----------:|-------------:|-----------------:|"
foreach ($r in $rows) {
    $md += "| $($r.Mesh) | $($r.Cells) | $($r.SetupMs) | $($r.SolveMs) | $($r.DownloadMs) | $($r.TotalMs) | $($r.CpuMs) | $($r.Speedup) | $($r.PcgIters) | $($r.MaxAbsErr) | $($r.H2D) | $($r.D2HField) | $($r.D2HScalar) | $($r.ScalarReads) | $($r.HybridEstBytes) |"
}
$md += ""
$md += "## Notes"
$md += ""
$md += "- **H2D field uploads = 0**: mesh/init/assemble run on device."
$md += "- **D2H field**: single final ``T`` download (``n * 8`` bytes)."
$md += "- **D2H scalar**: residual reduction partials only (not the matrix)."
$md += "- **hybrid-est**: rough bytes if A (5 diagonals) + x + b + sol were recopied every time step (2015-style)."
$md += "- 2000² CPU check skipped (too slow for the paired host PCG); use smaller meshes for correctness."
$md += ""
$md += "Re-run:"
$md += '```powershell'
$md += '.\scripts\bench-laplace-ocl.ps1'
$md += '```'

$md -join "`n" | Set-Content -Encoding utf8 $doc
Write-Host "Wrote $doc"
$rows | Format-Table -AutoSize
