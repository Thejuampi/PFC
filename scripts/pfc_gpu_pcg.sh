#!/usr/bin/env bash
# Mode B bridge: PFC1 binary → csrOcl GPU PCG → solution .x
#
# Usage: pfc_gpu_pcg.sh <pfc_gpu.bin> <pfc_gpu.x>
#
# Env:
#   PFC_ROOT     — PFC clone root (auto-detected from script location if unset)
#   PFC_CSROCL   — path to csrOcl or csrOcl.exe
#   PFC_KERNELS  — path to csr.cl
#   PFC_GPU_TOL / PFC_GPU_MAX_ITERS

set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <pfc_gpu.bin> <pfc_gpu.x>" >&2
  exit 2
fi

BIN="$1"
XOUT="$2"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PFC_ROOT="${PFC_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"

TOL="${PFC_GPU_TOL:-1e-8}"
MAXIT="${PFC_GPU_MAX_ITERS:-5000}"

# Prefer explicit binary, then Windows exe on shared drive, then Linux build
if [[ -n "${PFC_CSROCL:-}" ]]; then
  CSROCL="$PFC_CSROCL"
elif [[ -x "$PFC_ROOT/build/csrOcl/csrOcl.exe" ]]; then
  CSROCL="$PFC_ROOT/build/csrOcl/csrOcl.exe"
elif [[ -x "$PFC_ROOT/build/csrOcl/csrOcl" ]]; then
  CSROCL="$PFC_ROOT/build/csrOcl/csrOcl"
else
  echo "ERROR: csrOcl not found. Run: make csrOcl  (from PFC root=$PFC_ROOT)" >&2
  exit 1
fi

if [[ -n "${PFC_KERNELS:-}" ]]; then
  KERNELS="$PFC_KERNELS"
elif [[ -f "$PFC_ROOT/build/csrOcl/kernels/csr.cl" ]]; then
  KERNELS="$PFC_ROOT/build/csrOcl/kernels/csr.cl"
else
  KERNELS="$PFC_ROOT/apps/csrOcl/kernels/csr.cl"
fi

# Windows .exe needs Windows paths when launched from WSL
is_win_exe=0
case "$CSROCL" in
  *.exe|*.EXE) is_win_exe=1 ;;
esac

if [[ "$is_win_exe" -eq 1 ]] && command -v wslpath >/dev/null 2>&1; then
  # Running under WSL: convert paths for the Windows process
  BIN_A="$(wslpath -w "$BIN" 2>/dev/null || echo "$BIN")"
  XOUT_A="$(wslpath -w "$XOUT" 2>/dev/null || echo "$XOUT")"
  KER_A="$(wslpath -w "$KERNELS" 2>/dev/null || echo "$KERNELS")"
  # csrOcl.exe may need to be invoked with its directory on PATH / as-is
  "$CSROCL" \
    --pfc-bin "$BIN_A" \
    --x-out "$XOUT_A" \
    --kernels "$KER_A" \
    --no-cpu-check \
    --tol "$TOL" \
    --max-iters "$MAXIT"
else
  "$CSROCL" \
    --pfc-bin "$BIN" \
    --x-out "$XOUT" \
    --kernels "$KERNELS" \
    --no-cpu-check \
    --tol "$TOL" \
    --max-iters "$MAXIT"
fi
