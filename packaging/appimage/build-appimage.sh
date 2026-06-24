#!/usr/bin/env bash
# Build an Owera MindFlow AppImage. Requires linuxdeploy + linuxdeploy-plugin-qt on PATH
# (https://github.com/linuxdeploy/linuxdeploy). Run from the repository root:
#   ./packaging/appimage/build-appimage.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build-appimage"
APPDIR="$BUILD/AppDir"

cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"
DESTDIR="$APPDIR" cmake --install "$BUILD" --prefix /usr

export QML_SOURCES_PATHS="$ROOT"
linuxdeploy --appdir "$APPDIR" \
  --desktop-file "$APPDIR/usr/share/applications/com.owera.MindFlow.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/com.owera.MindFlow.svg" \
  --plugin qt \
  --output appimage

echo "AppImage written to: $(ls -1 "$ROOT"/Owera*MindFlow*.AppImage 2>/dev/null || echo '<cwd>')"
