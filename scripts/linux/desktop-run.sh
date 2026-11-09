#!/usr/bin/env bash
set -euo pipefail

# If launched from a Linux GUI file manager without a terminal, re-open in a terminal window
if [[ ! -t 0 && ! -t 1 && "${TERM:-dumb}" == "dumb" && -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" && -z "${GUIKIT_IN_TERM:-}" ]]; then
    export GUIKIT_IN_TERM=1
    for term in x-terminal-emulator gnome-terminal konsole xfce4-terminal mate-terminal kitty alacritty xterm; do
        if command -v "$term" >/dev/null 2>&1; then
            if [[ "$term" == "gnome-terminal" ]]; then
                exec "$term" -- bash -c '"$0" "$@"; status=$?; if [ $status -ne 0 ]; then echo ""; read -rp "Press Enter to close..."; fi; exit $status' "$0" "$@"
            else
                exec "$term" -e bash -c '"$0" "$@"; status=$?; if [ $status -ne 0 ]; then echo ""; read -rp "Press Enter to close..."; fi; exit $status' "$0" "$@"
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
    shift || true
elif [[ "${CONFIG,,}" == "release" ]]; then
    CONFIG="Release"
    shift || true
else
    CONFIG="Release"
fi

GUIKIT_BIN=""
for candidate in "build/guikit_demo" "build/${CONFIG}/guikit_demo" "build/Release/guikit_demo" "build/Debug/guikit_demo"; do
    if [[ -f "$candidate" ]]; then
        GUIKIT_BIN="$candidate"
        break
    fi
done

if [[ -z "$GUIKIT_BIN" ]]; then
    echo "[guikit] guikit_demo binary not found. Building ${CONFIG} first..."
    "${SCRIPT_DIR}/desktop-build.sh" "$CONFIG"
    for candidate in "build/guikit_demo" "build/${CONFIG}/guikit_demo"; do
        if [[ -f "$candidate" ]]; then
            GUIKIT_BIN="$candidate"
            break
        fi
    done
fi

if [[ -z "$GUIKIT_BIN" ]]; then
    echo "[ERROR] Build failed, guikit_demo binary not found." >&2
    exit 1
fi

echo "[guikit] Launching Desktop Demo: $GUIKIT_BIN"
exec "$GUIKIT_BIN" "$@"
