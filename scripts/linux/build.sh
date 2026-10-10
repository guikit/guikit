#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/../.."

CONFIG="${1:-Release}"

echo "========================================================"
echo " Building GuiKit Library & Demo ($CONFIG)"
echo "========================================================"

if [ ! -d "build" ]; then
    echo "[INFO] Generating CMake build configuration..."
    cmake -B build -S .
fi

echo "[INFO] Compiling target: guikit_demo ($CONFIG)..."
cmake --build build --config "$CONFIG" --target guikit_demo

echo ""
echo "========================================================"
echo " Build succeeded!"
echo "========================================================"
