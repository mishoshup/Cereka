#!/usr/bin/env bash
# scripts/verify-windows.sh — Local Windows cross-compile verification.
#
# Requires: llvm-mingw, wine
#
# This script performs a cross-compile for Windows using the project's
# toolchain and executes the tests using Wine.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "─── Starting Windows Verification (Wine) ───"

# Check for llvm-mingw (just check if clang is available via the toolchain logic)
# Actually, let's just try to configure.

echo '─── Configuring CMake ───'
cmake -B build-verify-win -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/ucrt64.cmake

echo '─── Building ───'
cmake --build build-verify-win

echo '─── Running Tests (Wine) ───'
export WINEDEBUG=-all
wine build-verify-win/tests/cereka_test.exe

echo "─── Verification Successful ───"
