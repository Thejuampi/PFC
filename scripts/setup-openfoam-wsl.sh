#!/usr/bin/env bash
# Install OpenFOAM (ESI package) inside Ubuntu WSL. Packages land in the
# distro VHDX (must be on G: — see docs/WSL_ON_G.md).
#
# Usage:
#   wsl -d Ubuntu-OF
#   bash /mnt/g/dev/repos/PFC/scripts/setup-openfoam-wsl.sh
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Run inside WSL Ubuntu, not Windows PowerShell."
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive

echo "==> Distro: $(. /etc/os-release; echo "$PRETTY_NAME")"

apt-get update -qq
apt-get install -y -qq ca-certificates curl wget gnupg

if [[ ! -f /etc/apt/sources.list.d/openfoam.list ]]; then
    echo "==> Adding dl.openfoam.com repo"
    curl -s https://dl.openfoam.com/add-debian-repo.sh | bash
else
    apt-get update -qq
fi

# Prefer stable latest; skip RC (e.g. openfoam2606 ~rc) unless only option.
CANDIDATES=(openfoam2512 openfoam2506 openfoam2412 openfoam2406 openfoam2312)
PKG=""
for c in "${CANDIDATES[@]}"; do
    if apt-cache policy "$c" 2>/dev/null | grep -q 'Candidate:'; then
        cand=$(apt-cache policy "$c" | awk '/Candidate:/ {print $2}')
        if [[ "$cand" != "(none)" && "$cand" != *rc* && "$cand" != *RC* ]]; then
            PKG="$c"
            break
        fi
    fi
done

if [[ -z "$PKG" ]]; then
    echo "No stable openfoam* package found. Candidates:"
    apt-cache search '^openfoam[0-9]' | head -30
    exit 1
fi

echo "==> Installing $PKG"
apt-get install -y "$PKG"

# Prefer openfoam-selector for login shells
if command -v openfoam-selector >/dev/null 2>&1; then
    openfoam-selector --set "$PKG" || true
fi

WRAPPER="/usr/bin/${PKG}"
if [[ -x "$WRAPPER" ]]; then
    echo "==> Smoke: $WRAPPER -c 'echo version=\$WM_PROJECT_VERSION; command -v laplacianFoam'"
    "$WRAPPER" -c 'echo version=$WM_PROJECT_VERSION; command -v blockMesh; command -v laplacianFoam'
else
    echo "Wrapper missing: $WRAPPER"
    exit 1
fi

echo ""
echo "Setup complete."
echo "  Run case:  $WRAPPER -c 'cd /mnt/g/dev/repos/PFC && bash scripts/run-laplace-cpu.sh'"
