#!/usr/bin/env bash
# build_all.sh — Build Linux + Windows runtimes and collect them under one tree.
#
# Usage:
#   ./scripts/build_all.sh
#
# Produces:
#   build/CerekaLauncher
#   build/runtimes/linux/CerekaGame
#   build/runtimes/windows/CerekaGame.exe + *.dll
#
# For packaging / releasing use release.sh instead.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ── Linux native ─────────────────────────────────────────────────────────────
echo "[1/3] Linux native build"
cmake -B "$ROOT/build" -S "$ROOT" -G Ninja -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build "$ROOT/build"

# ── Windows cross-compile ────────────────────────────────────────────────────
echo "[2/3] Windows UCRT cross-compile"
cmake -B "$ROOT/build-win" -S "$ROOT" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/ucrt64.cmake" -Wno-dev
cmake --build "$ROOT/build-win"

# ── Collect runtimes ──────────────────────────────────────────────────────────
echo "[3/3] Syncing Windows runtime → build/runtimes/windows/"
mkdir -p "$ROOT/build/runtimes/windows"
cp -u "$ROOT/build-win/runtimes/windows/"* "$ROOT/build/runtimes/windows/"

echo ""
echo "Done."
echo "  Linux launcher : build/CerekaLauncher"
echo "  Linux runtime  : build/runtimes/linux/CerekaGame"
echo "  Windows runtime: build/runtimes/windows/"
