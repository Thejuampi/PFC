param(
    [int]$Nx = 100,
    [int]$Ny = 100,
    [int]$Steps = 10,
    [double]$Dt = 0.005
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
make laplaceOcl
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$exe = Join-Path $root "build\laplaceOcl\laplaceOcl.exe"
$kernels = Join-Path $root "build\laplaceOcl\kernels\laplace.cl"
$out = Join-Path $root "build\laplaceOcl\T_gpu_${Nx}x${Ny}.csv"
& $exe --nx $Nx --ny $Ny --steps $Steps --dt $Dt --kernels $kernels --out $out
exit $LASTEXITCODE
