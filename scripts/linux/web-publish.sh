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
STAGE_DIR="${REPO_ROOT}/dist/gh-pages"
REMOTE_URL="git@github.com:guikit/demo.git"
SKIP_BUILD=0

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        --remote=*)   REMOTE_URL="${arg#*=}" ;;
        *)
            echo "Usage: $0 [--skip-build] [--remote=<git-url>]"
            exit 1
            ;;
    esac
done

echo "==> [1/4] Preparing WebAssembly build..."
if [[ "$SKIP_BUILD" -eq 0 ]]; then
    "${SCRIPT_DIR}/web-build.sh"
else
    echo "    Skipping build (--skip-build specified)."
fi

for f in "${BUILD_DIR}/guikit_demo.html" "${BUILD_DIR}/guikit_demo.js" "${BUILD_DIR}/guikit_demo.wasm"; do
    if [[ ! -f "$f" ]]; then
        echo "Error: Required WebAssembly artifact not found: $f" >&2
        exit 1
    fi
done

echo "==> [2/4] Staging static bundle in ${STAGE_DIR}..."
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

cp "${BUILD_DIR}/guikit_demo.html" "${STAGE_DIR}/index.html"
cp "${BUILD_DIR}/guikit_demo.html" "${STAGE_DIR}/guikit_demo.html"
cp "${BUILD_DIR}/guikit_demo.js"   "${STAGE_DIR}/guikit_demo.js"
cp "${BUILD_DIR}/guikit_demo.wasm" "${STAGE_DIR}/guikit_demo.wasm"
if [[ -f "${BUILD_DIR}/guikit_demo.data" ]]; then
    cp "${BUILD_DIR}/guikit_demo.data" "${STAGE_DIR}/guikit_demo.data"
fi
touch "${STAGE_DIR}/.nojekyll"

SOURCE_COMMIT="$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || echo "unknown")"
UTC_NOW="$(date -u +"%Y-%m-%d %H:%M:%S UTC")"

cat > "${STAGE_DIR}/README.md" <<EOF
# GuiKit — WebAssembly Interactive Demo

Live WebAssembly build of **GuiKit** (\`guikit_demo\`), compiled with Emscripten + SDL3 + ThorVG + WebGL2.

- **Live Demo (GitHub Pages)**: [https://guikit.github.io/demo/](https://guikit.github.io/demo/)
- **Source Repository**: \`guikit/guikit\` (\`@${SOURCE_COMMIT}\`)
- **Last Published**: ${UTC_NOW}
EOF

echo "==> [3/4] Creating deployment commit (source @${SOURCE_COMMIT})..."
pushd "$STAGE_DIR" >/dev/null
git init -b main >/dev/null
git add -A
git commit -m "deploy(wasm): publish guikit_demo @${SOURCE_COMMIT} (${UTC_NOW})" >/dev/null

echo "==> [4/4] Pushing to ${REMOTE_URL} (branches: main, gh-pages)..."
git remote add origin "$REMOTE_URL"
git push --force origin HEAD:main HEAD:gh-pages
popd >/dev/null

echo ""
echo "[OK] Published successfully to ${REMOTE_URL}!"
echo "     URL: https://guikit.github.io/demo/"
