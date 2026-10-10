#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/../.."

GUIKIT_BIN=""
for candidate in build/guikit_demo build/Release/guikit_demo build/Debug/guikit_demo; do
    if [ -f "$candidate" ]; then GUIKIT_BIN="$candidate"; break; fi
done

if [ -z "$GUIKIT_BIN" ]; then
    echo "[WARN] guikit_demo binary not found. Building target first..."
    "$(dirname "$0")/build.sh"
    for candidate in build/guikit_demo build/Release/guikit_demo build/Debug/guikit_demo; do
        if [ -f "$candidate" ]; then GUIKIT_BIN="$candidate"; break; fi
    done
fi

if [ -z "$GUIKIT_BIN" ]; then
    echo "[ERROR] Build failed, guikit_demo binary not found."
    exit 1
fi

echo "========================================================"
echo " Launching GuiKit Vector GUI Demo"
echo " Executable: $GUIKIT_BIN"
echo "========================================================"

exec "$GUIKIT_BIN" "$@"
