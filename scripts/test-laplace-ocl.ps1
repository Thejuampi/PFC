# Wrapper — preferred entrypoint is just: make   (or make test)
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)
make test
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($env:LAPLACE_OCL_MEM_FRAC) {
    make test-vram
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
