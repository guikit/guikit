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
BUILD_DIR="${REPO_ROOT}/build_wasm"
OUT_DIR="${REPO_ROOT}/dist/web"
BUILD_TYPE="Release"
INSTALL_EMSDK=1
CLEAN=0
SERVE=0
OPEN_BROWSER=0

for arg in "$@"; do
    case "$arg" in
        --debug)          BUILD_TYPE="Debug" ;;
        --install-emsdk)  INSTALL_EMSDK=1 ;;
        --no-install)     INSTALL_EMSDK=0 ;;
        --clean)          CLEAN=1 ;;
        --serve)          SERVE=1 ;;
        --open-browser)   OPEN_BROWSER=1 ;;
        *)
            echo "Usage: $0 [--debug] [--clean] [--serve] [--open-browser] [--no-install]"
            exit 1
            ;;
    esac
done

if [[ "$CLEAN" -eq 1 ]]; then
    rm -rf "$BUILD_DIR" "$OUT_DIR"
fi

if ! command -v emcmake >/dev/null 2>&1; then
    if [[ -f "${REPO_ROOT}/.emsdk/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "${REPO_ROOT}/.emsdk/emsdk_env.sh"
    elif [[ -f "${HOME}/emsdk/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "${HOME}/emsdk/emsdk_env.sh"
    fi
fi

if ! command -v emcmake >/dev/null 2>&1; then
    if [[ "$INSTALL_EMSDK" -eq 1 ]]; then
        if [[ ! -d "${REPO_ROOT}/.emsdk" ]]; then
            echo "[WASM] Cloning emsdk into ${REPO_ROOT}/.emsdk ..."
            git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${REPO_ROOT}/.emsdk"
        fi
        "${REPO_ROOT}/.emsdk/emsdk" install latest
        "${REPO_ROOT}/.emsdk/emsdk" activate latest
        # shellcheck disable=SC1091
        source "${REPO_ROOT}/.emsdk/emsdk_env.sh"
    else
        echo "Error: 'emcmake' not found in PATH. Activate emsdk_env.sh or omit --no-install." >&2
        exit 1
    fi
fi

if [[ ! -f "${BUILD_DIR}/Makefile" && ! -f "${BUILD_DIR}/build.ninja" ]]; then
    echo "[WASM] Configuring CMake (${BUILD_TYPE}) ..."
    emcmake cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DGUIKIT_BUILD_DEMO=ON \
        -DGUIKIT_BUILD_TESTS=OFF
fi

echo "[WASM] Building guikit_demo ..."
cmake --build "$BUILD_DIR" --target guikit_demo --parallel

mkdir -p "$OUT_DIR"
cp "${BUILD_DIR}/guikit_demo.html" "${OUT_DIR}/index.html"
cp "${BUILD_DIR}/guikit_demo.js"   "${OUT_DIR}/guikit_demo.js"
cp "${BUILD_DIR}/guikit_demo.wasm" "${OUT_DIR}/guikit_demo.wasm"
if [[ -f "${BUILD_DIR}/guikit_demo.data" ]]; then
    cp "${BUILD_DIR}/guikit_demo.data" "${OUT_DIR}/guikit_demo.data"
fi
touch "${OUT_DIR}/.nojekyll"

echo "[WASM] WebAssembly bundle ready in: ${OUT_DIR}"
ls -lh "$OUT_DIR"

if [[ "$SERVE" -eq 1 ]]; then
    URL="http://localhost:8080/index.html"
    echo "[WASM] Serving ${URL} (Ctrl+C to stop) ..."
    if [[ "$OPEN_BROWSER" -eq 1 ]] && command -v xdg-open >/dev/null 2>&1; then
        xdg-open "$URL" >/dev/null 2>&1 || true
    fi
    python3 -m http.server 8080 --directory "$OUT_DIR"
fi
