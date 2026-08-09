# Run full-device OpenCL Laplace on AMD GPU (Windows host)
param(
    [int]$Nx = 100,
    [int]$Ny = 100,
    [int]$Steps = 10,
    [double]$Dt = 0.005
)

$ErrorActionPreference = "Stop"
$exe = "G:\dev\repos\PFC\apps\laplaceOcl\build\laplaceOcl.exe"
$kernels = "G:\dev\repos\PFC\apps\laplaceOcl\kernels\laplace.cl"
$outDir = "G:\dev\repos\PFC\apps\laplaceOcl\build"
if (-not (Test-Path $exe)) {
    Write-Host "Building first..."
    & "$PSScriptRoot\build-laplace-ocl.ps1"
}

$out = Join-Path $outDir "T_gpu_${Nx}x${Ny}.csv"
& $exe --nx $Nx --ny $Ny --steps $Steps --dt $Dt --kernels $kernels --out $out
exit $LASTEXITCODE
