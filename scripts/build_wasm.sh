#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build_wasm"
OUT_DIR="${REPO_ROOT}/dist/web"
BUILD_TYPE="Release"
INSTALL_EMSDK=0
CLEAN=0
SERVE=0

for arg in "$@"; do
    case "$arg" in
        --debug)         BUILD_TYPE="Debug" ;;
        --install-emsdk) INSTALL_EMSDK=1 ;;
        --clean)         CLEAN=1 ;;
        --serve)         SERVE=1 ;;
        *)
            echo "Usage: $0 [--debug] [--install-emsdk] [--clean] [--serve]"
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
    elif [[ "$INSTALL_EMSDK" -eq 1 ]]; then
        git clone --depth 1 https://github.com/emscripten-core/emsdk.git "${REPO_ROOT}/.emsdk"
        "${REPO_ROOT}/.emsdk/emsdk" install latest
        "${REPO_ROOT}/.emsdk/emsdk" activate latest
        # shellcheck disable=SC1091
        source "${REPO_ROOT}/.emsdk/emsdk_env.sh"
    else
        echo "Error: 'emcmake' not found in PATH. Pass --install-emsdk or source emsdk_env.sh first." >&2
        exit 1
    fi
fi

echo "[WASM] Configuring CMake (${BUILD_TYPE}) ..."
emcmake cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DGUIKIT_BUILD_DEMO=ON \
    -DGUIKIT_BUILD_TESTS=OFF

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

echo "[WASM] GitHub Pages bundle ready in: ${OUT_DIR}"
ls -lh "$OUT_DIR"

if [[ "$SERVE" -eq 1 ]]; then
    echo "[WASM] Serving http://localhost:8080 ..."
    python3 -m http.server 8080 --directory "$OUT_DIR"
fi
