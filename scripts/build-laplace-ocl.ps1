# Build apps/laplaceOcl on Windows (MinGW + system OpenCL.dll)
$ErrorActionPreference = "Stop"
$root = "G:\dev\repos\PFC"
$src = Join-Path $root "apps\laplaceOcl"
$build = Join-Path $src "build"

if (-not (Test-Path "$root\third_party\OpenCL-Headers\CL\cl.h")) {
    throw "Missing OpenCL headers under third_party/OpenCL-Headers"
}
if (-not (Test-Path "$root\third_party\opencl-lib\libOpenCL.a")) {
    throw "Missing third_party/opencl-lib/libOpenCL.a (gendef/dlltool import lib)"
}

New-Item -ItemType Directory -Force -Path $build | Out-Null
Push-Location $build
try {
    cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release $src
    cmake --build . -j
    Write-Host "OK: $build\laplaceOcl.exe"
} finally {
    Pop-Location
}
