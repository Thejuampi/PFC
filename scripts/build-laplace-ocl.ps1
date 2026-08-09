# Thin wrapper: Makefile is the source of truth
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
make laplaceOcl
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "OK: $root\build\laplaceOcl\laplaceOcl.exe"
