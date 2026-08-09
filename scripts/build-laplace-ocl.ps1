# Wrapper — preferred entrypoint is just: make
$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)
make build
exit $LASTEXITCODE
