# DEPRECATED — do not use.
# Plain Store/install puts the distro on C:.
# Use instead (elevated):
#   G:\dev\repos\PFC\scripts\install-wsl-ubuntu-on-G.ps1
#
# See docs/WSL_ON_G.md

Write-Error @"
This script is DISABLED on purpose.

It would install Ubuntu via the default WSL/Store path on C:.
Project policy: distro + OpenFOAM must live on G: only.

Run (elevated PowerShell):
  Set-ExecutionPolicy -Scope Process Bypass -Force
  G:\dev\repos\PFC\scripts\install-wsl-ubuntu-on-G.ps1

Docs:
  G:\dev\repos\PFC\docs\WSL_ON_G.md
"@
exit 1
