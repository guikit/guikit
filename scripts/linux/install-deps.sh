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

echo "========================================================"
echo " Installing GuiKit Linux Build Dependencies"
echo "========================================================"

if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake pkg-config \
        libgl-dev libegl-dev \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
        libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y \
        gcc-c++ cmake pkgconf-pkg-config \
        mesa-libGL-devel mesa-libEGL-devel \
        libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXfixes-devel libXi-devel libXScrnSaver-devel \
        wayland-devel libxkbcommon-devel wayland-protocols-devel libdecor-devel
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --needed --noconfirm \
        base-devel cmake pkgconf \
        mesa libx11 libxext libxrandr libxcursor libxfixes libxi libxss \
        wayland libxkbcommon wayland-protocols libdecor
else
    echo "[ERROR] Unsupported package manager. Please install CMake, pkg-config, OpenGL/EGL, X11, and Wayland development headers manually." >&2
    exit 1
fi

echo ""
echo "========================================================"
echo " Dependencies installed successfully!"
echo "========================================================"
