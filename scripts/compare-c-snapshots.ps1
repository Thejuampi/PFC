<#
.SYNOPSIS
  Compare two C: snapshots and flag growth on C: (should stay near-zero for our WSL-on-G plan).

.EXAMPLE
  .\scripts\compare-c-snapshots.ps1
  # uses docs/snapshots/C-before-wsl-LATEST vs a new after snapshot

  .\scripts\compare-c-snapshots.ps1 -BeforeDir ... -AfterDir ...
#>
param(
    [string]$BeforeDir = "G:\dev\repos\PFC\docs\snapshots\C-before-wsl-LATEST",
    [string]$AfterDir = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BeforeDir)) {
    throw "Before snapshot not found: $BeforeDir"
}

if (-not $AfterDir) {
    Write-Host "No -AfterDir given; taking a fresh snapshot labeled after-wsl ..."
    $AfterDir = & "$PSScriptRoot\snapshot-c-drive.ps1" -Label "after-wsl"
}

$before = Get-Content (Join-Path $BeforeDir "00-baseline.json") -Raw | ConvertFrom-Json
$after = Get-Content (Join-Path $AfterDir "00-baseline.json") -Raw | ConvertFrom-Json

$dCFree = [math]::Round($after.C_FreeGB - $before.C_FreeGB, 3)
$dCUsed = [math]::Round($after.C_UsedGB - $before.C_UsedGB, 3)
$dGFree = [math]::Round($after.G_FreeGB - $before.G_FreeGB, 3)
$dGUsed = [math]::Round($after.G_UsedGB - $before.G_UsedGB, 3)

Write-Host ""
Write-Host "=== Disk delta (After - Before) ==="
Write-Host ("C: Free {0:+0.###} GB   Used {1:+0.###} GB" -f $dCFree, $dCUsed)
Write-Host ("G: Free {0:+0.###} GB   Used {1:+0.###} GB" -f $dGFree, $dGUsed)
Write-Host ""

# Compare hot dir CSVs
function Read-HotMap([string]$csvPath) {
    $map = @{}
    if (-not (Test-Path $csvPath)) { return $map }
    Import-Csv $csvPath | ForEach-Object {
        $map[$_.path] = $_
    }
    return $map
}

$bHot = Read-HotMap (Join-Path $BeforeDir "04-hot-dir-sizes.csv")
$aHot = Read-HotMap (Join-Path $AfterDir "04-hot-dir-sizes.csv")
$allPaths = @($bHot.Keys + $aHot.Keys) | Select-Object -Unique

Write-Host "=== Hot directory size deltas (GB) ==="
$warnC = @()
foreach ($p in ($allPaths | Sort-Object)) {
    $bs = if ($bHot.ContainsKey($p) -and $bHot[$p].size_GB -ne "") { [double]$bHot[$p].size_GB } else { 0 }
    $as = if ($aHot.ContainsKey($p) -and $aHot[$p].size_GB -ne "") { [double]$aHot[$p].size_GB } else { 0 }
    $d = [math]::Round($as - $bs, 3)
    $flag = ""
    if ($p -like "C:\*" -and [math]::Abs($d) -ge 0.05) {
        $flag = "  <== C: GROWTH"
        $warnC += [pscustomobject]@{ Path = $p; DeltaGB = $d }
    }
    Write-Host ("{0,8:+0.###}  {1}{2}" -f $d, $p, $flag)
}

Write-Host ""
Write-Host "=== WSL-related packages on C: (after) ==="
$pkgAfter = Join-Path $AfterDir "05-wsl-related-packages.csv"
if (Test-Path $pkgAfter) { Get-Content $pkgAfter }

Write-Host ""
if ($dCUsed -gt 0.5 -or $warnC.Count -gt 0) {
    Write-Host "ALERT: C: grew more than expected or hot C: dirs changed >= 50 MB."
    Write-Host "Expected for G-only plan: distro VHDX under G:\wsl\..., C: delta near 0 (except maybe tiny caches)."
    Write-Host "Already-on-C baseline that is OK: C:\Program Files\WSL (~0.8 GB platform bits)."
    if ($warnC.Count -gt 0) {
        $warnC | Format-Table -AutoSize
    }
    exit 2
} else {
    Write-Host "OK: C: usage delta looks within tolerance (< 0.5 GB used, no hot-dir spike >= 50 MB)."
    exit 0
}
