#!/usr/bin/env bash
set -e

echo "=== AVM Bootstrap ==="
echo "Fetching git submodules..."
git submodule update --init --recursive

# Detect OS
if [[ "$(uname)" == "Linux" ]]; then
    echo "Detected: Linux"
    echo "Installing dependencies via apt..."
    sudo apt-get update -qq
    sudo apt-get install -y \
        cmake ninja-build \
        qemu-system-x86 \
        libvulkan-dev vulkan-tools \
        libsdl2-dev \
        python3 python3-pip \
        adb
elif [[ "$(uname)" == "Darwin" ]]; then
    echo "Detected: macOS"
    echo "Installing dependencies via brew..."
    brew install cmake ninja qemu sdl2
else
    echo "Windows detected — please install dependencies manually."
    echo "Required: CMake, Ninja, Vulkan SDK, QEMU for Windows"
    echo "See docs/setup-windows.md"
fi

echo ""
echo "=== Bootstrap complete ==="
echo "Run ./scripts/build.sh to compile AVM."
