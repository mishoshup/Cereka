#!/usr/bin/env bash
# release.sh — Build both platforms and create GitHub release archives.
#
# Usage:
#   ./release.sh [version]          # e.g.  ./release.sh v0.0.2
#
# Prerequisites (Linux):
#   - cmake, ninja, zip, tar, gh (GitHub CLI)
#   - llvm-mingw UCRT64 toolchain for Windows cross-compile
#
# What it produces (in the repo root):
#   cereka-<version>-linux.tar.gz   — Linux launcher + linux + windows runtimes
#   cereka-<version>-windows.zip    — Windows launcher + windows runtime

set -euo pipefail

VERSION="${1:-v0.0.2}"
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_LINUX="$REPO_ROOT/build"
BUILD_WIN="$REPO_ROOT/build-win"

# ── Colors ──────────────────────────────────────────────────────────────────
GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
info()  { echo -e "${GREEN}[+]${NC} $*"; }
warn()  { echo -e "${YELLOW}[!]${NC} $*"; }
error() { echo -e "${RED}[x]${NC} $*" >&2; }

# ── 1. Linux build ──────────────────────────────────────────────────────────
info "Building Linux (native)..."
cmake -B "$BUILD_LINUX" -G Ninja -DCMAKE_BUILD_TYPE=Release -S "$REPO_ROOT" -Wno-dev 2>&1 | grep -E "^(CMake Warning|CMake Error|--)" | tail -5 || true
cmake --build "$BUILD_LINUX" --config Release
info "Linux build done."

# ── 2. Windows cross-compile ────────────────────────────────────────────────
info "Building Windows (cross-compile)..."
cmake -B "$BUILD_WIN" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/toolchains/ucrt64.cmake" \
    -S "$REPO_ROOT" -Wno-dev 2>&1 | grep -E "^(CMake Warning|CMake Error|--)" | tail -5 || true
cmake --build "$BUILD_WIN" --config Release
info "Windows build done."

# ── 3. Validate outputs ─────────────────────────────────────────────────────
missing=0
for f in \
    "$BUILD_LINUX/CerekaLauncher" \
    "$BUILD_LINUX/runtimes/linux/CerekaGame" \
    "$BUILD_WIN/CerekaLauncher.exe" \
    "$BUILD_WIN/runtimes/windows/CerekaGame.exe"
do
    if [[ ! -f "$f" ]]; then
        error "Missing: $f"
        missing=$((missing+1))
    fi
done
(( missing == 0 )) || { error "Build validation failed."; exit 1; }

# Copy the Windows runtime alongside the Linux build so the Linux launcher
# can package Windows games out of the box.
info "Syncing Windows runtime → $BUILD_LINUX/runtimes/windows/ ..."
mkdir -p "$BUILD_LINUX/runtimes/windows"
cp -u "$BUILD_WIN/runtimes/windows/"* "$BUILD_LINUX/runtimes/windows/"

# ── 4. Stage and archive ─────────────────────────────────────────────────────
DIST="$REPO_ROOT/dist-$VERSION"
rm -rf "$DIST"
mkdir -p "$DIST"

# ── 4a. Linux archive ────────────────────────────────────────────────────────
LINUX_STAGE="$DIST/cereka-linux"
mkdir -p "$LINUX_STAGE/runtimes/linux"
mkdir -p "$LINUX_STAGE/runtimes/windows"

cp "$BUILD_LINUX/CerekaLauncher"                    "$LINUX_STAGE/"
cp "$BUILD_LINUX/runtimes/linux/CerekaGame"         "$LINUX_STAGE/runtimes/linux/"
cp "$BUILD_LINUX/runtimes/windows/CerekaGame.exe"   "$LINUX_STAGE/runtimes/windows/"
cp "$BUILD_LINUX/runtimes/windows/"*.dll             "$LINUX_STAGE/runtimes/windows/" 2>/dev/null || true
chmod +x "$LINUX_STAGE/CerekaLauncher" \
         "$LINUX_STAGE/runtimes/linux/CerekaGame"

LINUX_ARCHIVE="$REPO_ROOT/cereka-${VERSION}-linux.tar.gz"
tar -czf "$LINUX_ARCHIVE" -C "$DIST" cereka-linux
info "Created: cereka-${VERSION}-linux.tar.gz"

# ── 4b. Windows archive ──────────────────────────────────────────────────────
WIN_STAGE="$DIST/cereka-windows"
mkdir -p "$WIN_STAGE/runtimes/windows"

cp "$BUILD_WIN/CerekaLauncher.exe"                  "$WIN_STAGE/"
# SDL3.dll is needed alongside CerekaLauncher.exe (launcher links SDL3)
cp "$BUILD_WIN/runtimes/windows/SDL3.dll"           "$WIN_STAGE/"
cp "$BUILD_WIN/runtimes/windows/CerekaGame.exe"     "$WIN_STAGE/runtimes/windows/"
cp "$BUILD_WIN/runtimes/windows/"*.dll               "$WIN_STAGE/runtimes/windows/" 2>/dev/null || true

WIN_ARCHIVE="$REPO_ROOT/cereka-${VERSION}-windows.zip"
(cd "$DIST" && zip -r "$WIN_ARCHIVE" cereka-windows)
info "Created: cereka-${VERSION}-windows.zip"

# ── 5. Cleanup ──────────────────────────────────────────────────────────────
rm -rf "$DIST"

# ── 6. Sizes ─────────────────────────────────────────────────────────────────
echo ""
info "Archives ready:"
ls -lh "$REPO_ROOT/cereka-${VERSION}-linux.tar.gz" "$REPO_ROOT/cereka-${VERSION}-windows.zip"

# ── 7. GitHub release (optional) ─────────────────────────────────────────────
echo ""
read -rp "Create GitHub release $VERSION? [y/N] " yn
if [[ "$yn" =~ ^[Yy]$ ]]; then
    cd "$REPO_ROOT"
    git tag -a "$VERSION" -m "Release $VERSION" 2>/dev/null || warn "Tag $VERSION already exists, skipping."
    git push origin "$VERSION"
    gh release create "$VERSION" \
        "$REPO_ROOT/cereka-${VERSION}-linux.tar.gz" \
        "$REPO_ROOT/cereka-${VERSION}-windows.zip" \
        --title "Cereka $VERSION" \
        --notes "$(cat <<'NOTES'
## Cereka $VERSION

A Ren'Py-rival visual novel engine. Authors write only `.crka` scripts — no C++ needed.

### Downloads

| File | Platform |
|------|----------|
| `cereka-*-linux.tar.gz` | Linux launcher + both runtimes (package for Linux **and** Windows from one machine) |
| `cereka-*-windows.zip` | Windows launcher + Windows runtime |

### Getting started

**Linux:**
```bash
tar xzf cereka-*-linux.tar.gz
cd cereka-linux
./CerekaLauncher
```

**Windows:**
Unzip `cereka-*-windows.zip`, run `CerekaLauncher.exe`.

### What's new in $VERSION
- Multi-platform `runtimes/` layout: one launcher install holds both Linux and Windows game runners
- Package button produces `.tar.gz` (Linux) and `.zip` (Windows) archives from the same machine
- Tutorial script shows all character positions and folder structure as in-game narration
- Cross-platform Windows build via llvm-mingw UCRT64 toolchain
NOTES
)"
    info "Release $VERSION published."
fi
