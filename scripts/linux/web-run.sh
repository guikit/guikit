#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "[guikit] Building WebAssembly bundle and launching in browser..."
exec "${SCRIPT_DIR}/web-build.sh" --serve --open-browser "$@"
