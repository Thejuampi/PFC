#!/usr/bin/env bash
# Step-0 CPU baseline: stock laplacianFoam, no GPU.
#
# Preferred invocation (from Windows or WSL):
#   From repo root (OpenFOAM env loaded):
#   bash scripts/run-laplace-cpu.sh
#
# Or inside an already-loaded OpenFOAM shell:
#   bash scripts/run-laplace-cpu.sh
#
# Env:
#   MESH=100|500|2000   SOLVER=PCG|GAMG   APP=laplacianFoam|laplaceTimed
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CASE="${ROOT}/cases/laplaceCpu"
MESH="${MESH:-100}"
SOLVER="${SOLVER:-PCG}"
APP="${APP:-laplacianFoam}"

if [[ -z "${WM_PROJECT_DIR:-}" ]]; then
    if [[ -x /usr/bin/openfoam2512 ]]; then
        echo "OpenFOAM env not loaded; re-exec via openfoam2512"
        exec /usr/bin/openfoam2512 -c "cd '$ROOT' && MESH='$MESH' SOLVER='$SOLVER' APP='$APP' bash scripts/run-laplace-cpu.sh"
    fi
    for b in /usr/lib/openfoam/openfoam*/etc/bashrc; do
        if [[ -f "$b" ]]; then
            # shellcheck disable=SC1090
            set +u
            source "$b"
            set -u
            break
        fi
    done
fi

if [[ -z "${WM_PROJECT_DIR:-}" ]]; then
    echo "ERROR: OpenFOAM environment not loaded."
    echo "  bash scripts/setup-openfoam-wsl.sh   # from repo root, inside OF-capable env"
    exit 1
fi

echo "OpenFOAM  : ${WM_PROJECT_VERSION:-unknown}"
echo "Case      : ${CASE}"
echo "Mesh      : ${MESH} x ${MESH} x 1"
echo "fvSolution: ${SOLVER}"
echo "App       : ${APP}"

cd "$CASE"

BMD="system/blockMeshDict"
if [[ ! -f "$BMD" ]]; then
    BMD="constant/polyMesh/blockMeshDict"
fi

if [[ -f "$BMD" ]]; then
    sed -i -E "s/hex \(0 1 2 3 4 5 6 7\) \([0-9]+ [0-9]+ 1\)/hex (0 1 2 3 4 5 6 7) (${MESH} ${MESH} 1)/" "$BMD"
fi

if [[ "$SOLVER" == "GAMG" ]]; then
    cp system/fvSolution.GAMG system/fvSolution
elif grep -q 'solver[[:space:]]*GAMG' system/fvSolution 2>/dev/null; then
    cat > system/fvSolution <<'EOF'
/*--------------------------------*- C++ -*----------------------------------*\
FoamFile
{
    version     2.0;
    format      ascii;
    class       dictionary;
    object      fvSolution;
}
solvers
{
    T
    {
        solver          PCG;
        preconditioner  DIC;
        tolerance       1e-08;
        relTol          0.01;
    }
    TFinal { $T; relTol 0; }
}
SIMPLE { nNonOrthogonalCorrectors 0; }
EOF
fi

chmod +x Allrun Allclean 2>/dev/null || true
./Allclean >/dev/null 2>&1 || true

echo "==> blockMesh"
blockMesh 2>&1 | tee log.blockMesh

if [[ "$APP" == "laplacianFoam" ]]; then
    echo "==> laplacianFoam"
    laplacianFoam 2>&1 | tee log.laplacianFoam
else
    if ! command -v "$APP" >/dev/null 2>&1; then
        echo "ERROR: $APP not in PATH. Build apps/laplaceTimed with wmake first."
        exit 1
    fi
    echo "==> $APP"
    "$APP" 2>&1 | tee "log.${APP}"
fi

echo ""
echo "OK — step-0 CPU run finished."
