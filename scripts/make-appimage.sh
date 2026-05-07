#!/usr/bin/env bash
# make-appimage.sh — Package CerekaLauncher as an AppImage for Linux distribution.
#
# Usage:
#   ./scripts/make-appimage.sh [build-dir] [qmake-path]
#
# Produces:
#   CerekaLauncher-x86_64.AppImage (in the project root)
#
# Requires: wget (for downloading linuxdeploy tools if absent)
# Run on Linux x86_64 only — linuxdeploy does not support other arches via this script.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/build}"
QMAKE="${2:-$(which qmake6 2>/dev/null || which qmake 2>/dev/null || echo "qmake")}"

LINUXDEPLOY="$ROOT/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$ROOT/linuxdeploy-plugin-qt-x86_64.AppImage"

# ── Download linuxdeploy tools if not already present ────────────────────────
echo "[1/4] Checking linuxdeploy tools..."

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "  Downloading linuxdeploy..."
    wget -c "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
        -O "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "  Downloading linuxdeploy-plugin-qt..."
    wget -c "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
        -O "$LINUXDEPLOY_QT"
fi

chmod +x "$LINUXDEPLOY" "$LINUXDEPLOY_QT"

# ── Verify the launcher binary exists ────────────────────────────────────────
LAUNCHER_BIN="$BUILD_DIR/CerekaLauncher"
if [ ! -f "$LAUNCHER_BIN" ]; then
    echo "[ERROR] CerekaLauncher not found at: $LAUNCHER_BIN"
    echo "  Build first: cmake --build $BUILD_DIR"
    exit 1
fi

# ── Build AppDir structure ────────────────────────────────────────────────────
echo "[2/4] Creating AppDir..."

APPDIR="$ROOT/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$LAUNCHER_BIN"                          "$APPDIR/usr/bin/CerekaLauncher"
cp "$ROOT/launcher/cereka-launcher.desktop" "$APPDIR/usr/share/applications/"
cp "$ROOT/launcher/cereka-launcher.png"     "$APPDIR/usr/share/icons/hicolor/256x256/apps/"

echo "[3/4] Running linuxdeploy..."

cd "$ROOT"
QMAKE="$QMAKE" "$LINUXDEPLOY" \
    --appdir AppDir \
    --plugin qt \
    --output appimage

# ── Cleanup ───────────────────────────────────────────────────────────────────
echo "[4/4] Cleaning up AppDir..."
rm -rf "$APPDIR"

echo ""
echo "Done. AppImage: $(ls "$ROOT"/CerekaLauncher-*.AppImage 2>/dev/null || echo '(check project root)')"
