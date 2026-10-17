#!/usr/bin/env bash
# Builds Snapline-<version>-<arch>.AppImage with linuxdeploy and its Qt plugin.
# Usage: packaging/appimage/build-appimage.sh [output-dir]
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
out="${1:-$root/dist}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
arch="$(uname -m)"

mkdir -p "$out"
cmake -S "$root" -B "$work/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF
cmake --build "$work/build"
DESTDIR="$work/AppDir" cmake --install "$work/build"

tools="$root/packaging/appimage/.tools"
mkdir -p "$tools"
for tool in linuxdeploy linuxdeploy-plugin-qt; do
  if [[ ! -x "$tools/$tool-$arch.AppImage" ]]; then
    curl -fsSL -o "$tools/$tool-$arch.AppImage" \
      "https://github.com/linuxdeploy/$tool/releases/download/continuous/$tool-$arch.AppImage"
    chmod +x "$tools/$tool-$arch.AppImage"
  fi
done

export PATH="$tools:$PATH"
export QMAKE="${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake)}"
export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client;wayland-graphics-integration-client"
# Qt < 6.9 ships libqwayland-generic/-egl, newer Qt a single libqwayland.so.
plugin_dir="$("$QMAKE" -query QT_INSTALL_PLUGINS)/platforms"
EXTRA_PLATFORM_PLUGINS="$(cd "$plugin_dir" && ls libqwayland*.so 2>/dev/null | paste -sd';')"
export EXTRA_PLATFORM_PLUGINS
export APPIMAGE_EXTRACT_AND_RUN=1
export LINUXDEPLOY_OUTPUT_VERSION
LINUXDEPLOY_OUTPUT_VERSION="$(grep -oP 'project\(snapline VERSION \K[0-9.]+' "$root/CMakeLists.txt")"

cd "$out"
"$tools/linuxdeploy-$arch.AppImage" \
  --appdir "$work/AppDir" \
  --desktop-file "$work/AppDir/usr/share/applications/io.github.snapline.Snapline.desktop" \
  --icon-file "$work/AppDir/usr/share/icons/hicolor/scalable/apps/io.github.snapline.Snapline.svg" \
  --plugin qt \
  --output appimage

ls -1 "$out"/*.AppImage
