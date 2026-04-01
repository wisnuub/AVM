#!/usr/bin/env bash
set -e

BUILD_DIR="build"

echo "=== AVM Build ==="
echo "Build dir: $BUILD_DIR"

cmake -B "$BUILD_DIR" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAVM_ENABLE_KVM=ON \
    -DAVM_ENABLE_VULKAN=ON \
    -DAVM_ENABLE_OVERLAY=ON

cmake --build "$BUILD_DIR" --parallel

echo ""
echo "=== Build complete ==="
echo "Binary: $BUILD_DIR/avm"
echo ""
echo "Usage:"
echo "  ./build/avm --image /path/to/android.img --memory 4096 --cores 4 --gpu vulkan"
