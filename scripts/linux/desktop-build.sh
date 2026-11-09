#!/usr/bin/env bash
set -euo pipefail

# If launched from a Linux GUI file manager without a terminal, re-open in a terminal window
if [[ ! -t 0 && ! -t 1 && "${TERM:-dumb}" == "dumb" && -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" && -z "${GUIKIT_IN_TERM:-}" ]]; then
    export GUIKIT_IN_TERM=1
    for term in x-terminal-emulator gnome-terminal konsole xfce4-terminal mate-terminal kitty alacritty xterm; do
        if command -v "$term" >/dev/null 2>&1; then
            if [[ "$term" == "gnome-terminal" ]]; then
                exec "$term" -- bash -c '"$0" "$@"; echo ""; read -rp "Press Enter to close..."' "$0" "$@"
            else
                exec "$term" -e bash -c '"$0" "$@"; echo ""; read -rp "Press Enter to close..."' "$0" "$@"
            fi
        fi
    done
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
cd "$REPO_ROOT"

CONFIG="${1:-Release}"
if [[ "${CONFIG,,}" == "debug" ]]; then
    CONFIG="Debug"
else
    CONFIG="Release"
fi

echo "========================================================"
echo " GuiKit Desktop Build ($CONFIG)"
echo "========================================================"

if [[ ! -f "/usr/include/X11/Xlib.h" || ! -f "/usr/include/GL/gl.h" ]] || ! command -v pkg-config >/dev/null 2>&1; then
    echo "[INFO] Missing X11 / Wayland / OpenGL development headers for SDL3."
    "${SCRIPT_DIR}/install-deps.sh"
fi

if [[ ! -f "build/Makefile" && ! -f "build/build.ninja" ]]; then
    rm -f build/CMakeCache.txt
    echo "[INFO] Configuring CMake build directory ($CONFIG)..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE="$CONFIG"
fi

echo "[INFO] Building guikit_demo ($CONFIG)..."
cmake --build build --config "$CONFIG" --target guikit_demo --parallel

echo ""
echo "========================================================"
echo " Desktop build succeeded!"
echo " Binary: build/guikit_demo"
echo "========================================================"
