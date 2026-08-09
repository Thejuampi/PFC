<#
.SYNOPSIS
  Snapshot of C: free space and WSL-related paths (stored on G:).

.EXAMPLE
  .\scripts\snapshot-c-drive.ps1
  .\scripts\snapshot-c-drive.ps1 -Label after-wsl
#>
param(
    [string]$Label = "manual"
)

$ErrorActionPreference = "Continue"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$snapRoot = "G:\dev\repos\PFC\docs\snapshots"
$snapDir = Join-Path $snapRoot ("C-{0}-{1}" -f $Label, $stamp)
New-Item -ItemType Directory -Force -Path $snapDir | Out-Null

function Get-DirSizeGB([string]$Path) {
    if (-not (Test-Path $Path)) { return $null }
    try {
        $sum = (Get-ChildItem -LiteralPath $Path -Force -Recurse -File -ErrorAction SilentlyContinue |
            Measure-Object -Property Length -Sum -ErrorAction SilentlyContinue).Sum
        if ($null -eq $sum) { $sum = 0 }
        return [math]::Round($sum / 1GB, 3)
    } catch {
        return -1
    }
}

# Disks
$disks = Get-CimInstance Win32_LogicalDisk | Select-Object DeviceID, VolumeName, FileSystem,
    @{N = 'SizeGB'; E = { [math]::Round($_.Size / 1GB, 2) } },
    @{N = 'FreeGB'; E = { [math]::Round($_.FreeSpace / 1GB, 2) } },
    @{N = 'UsedGB'; E = { [math]::Round(($_.Size - $_.FreeSpace) / 1GB, 2) } }
$disks | Format-Table -AutoSize | Out-String | Set-Content (Join-Path $snapDir "01-disks.txt") -Encoding utf8

# WSL (force UTF8-safe capture)
$wslStatus = & cmd /c "wsl --status 2>&1"
$wslList = & cmd /c "wsl -l -v 2>&1"
@"
=== wsl --status ===
$($wslStatus -join "`n")

=== wsl -l -v ===
$($wslList -join "`n")
"@ | Set-Content (Join-Path $snapDir "02-wsl-state.txt") -Encoding utf8

$paths = @(
    "C:\Users\Juan\AppData\Local\Packages",
    "C:\Users\Juan\AppData\Local\Docker",
    "C:\Users\Juan\AppData\Local\wsl",
    "C:\Users\Juan\AppData\Local\lxss",
    "C:\Users\Juan\.wslconfig",
    "C:\Program Files\WSL",
    "C:\Program Files\Docker",
    "C:\ProgramData\Microsoft\Windows\WSL",
    "C:\Windows\System32\lxss",
    "C:\Windows\System32\wsl.exe",
    "C:\WSL",
    "C:\Ubuntu",
    "G:\WSL",
    "G:\wsl",
    "G:\dev\wsl",
    "G:\dev\repos\PFC"
)

$existLines = foreach ($p in $paths) {
    if (Test-Path $p) {
        $item = Get-Item $p -Force -ErrorAction SilentlyContinue
        if ($item.PSIsContainer) { "EXISTS DIR  $p" } else { "EXISTS FILE $p  size=$($item.Length)" }
    } else {
        "MISSING     $p"
    }
}
$existLines | Set-Content (Join-Path $snapDir "03-key-paths-exist.txt") -Encoding utf8

$hot = @(
    "C:\Users\Juan\AppData\Local\Packages",
    "C:\Users\Juan\AppData\Local\Docker",
    "C:\Users\Juan\AppData\Local\wsl",
    "C:\Users\Juan\AppData\Local\Temp",
    "C:\Program Files\WSL",
    "C:\ProgramData\Microsoft\Windows\WSL",
    "C:\Program Files\Docker",
    "G:\wsl",
    "G:\dev\wsl"
)

$hotLines = @("path,size_GB,exists")
foreach ($h in $hot) {
    if (Test-Path $h) {
        Write-Host "Sizing $h ..."
        $sz = Get-DirSizeGB $h
        $hotLines += "$h,$sz,true"
    } else {
        $hotLines += "$h,,false"
    }
}
$hotLines | Set-Content (Join-Path $snapDir "04-hot-dir-sizes.csv") -Encoding utf8

$pkgRoot = "C:\Users\Juan\AppData\Local\Packages"
$pkgLines = @("name,size_GB")
if (Test-Path $pkgRoot) {
    Get-ChildItem $pkgRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match 'Canonical|Ubuntu|Linux|WSL|SUSE|Debian|Docker|openfoam|Lxss' } |
        ForEach-Object {
            Write-Host "Sizing package $($_.Name) ..."
            $sz = Get-DirSizeGB $_.FullName
            $pkgLines += "$($_.Name),$sz"
        }
}
$pkgLines | Set-Content (Join-Path $snapDir "05-wsl-related-packages.csv") -Encoding utf8

$c = $disks | Where-Object DeviceID -eq 'C:'
$g = $disks | Where-Object DeviceID -eq 'G:'
$baseline = [ordered]@{
    timestamp   = $stamp
    label       = $Label
    snapDir     = $snapDir
    C_FreeGB    = $c.FreeGB
    C_UsedGB    = $c.UsedGB
    G_FreeGB    = $g.FreeGB
    G_UsedGB    = $g.UsedGB
    wsl_list    = ($wslList -join " | ")
}
$baseline | ConvertTo-Json | Set-Content (Join-Path $snapDir "00-baseline.json") -Encoding utf8

# Stable pointer for this label
$latest = Join-Path $snapRoot ("C-{0}-LATEST" -f $Label)
if (Test-Path $latest) { Remove-Item $latest -Recurse -Force }
Copy-Item $snapDir $latest -Recurse

Write-Host ""
Write-Host "SNAPSHOT OK: $snapDir"
Write-Host ("C free={0} GB used={1} GB | G free={2} GB used={3} GB" -f $c.FreeGB, $c.UsedGB, $g.FreeGB, $g.UsedGB)
return $snapDir
