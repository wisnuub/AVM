#!/usr/bin/env bash
# bootstrap-macos.sh — One-shot setup for AVM on macOS (Apple Silicon or Intel)
set -euo pipefail

RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; NC='\033[0m'
log()  { echo -e "${GRN}[AVM]${NC} $*"; }
warn() { echo -e "${YLW}[WARN]${NC} $*"; }
err()  { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ── Detect architecture
ARCH=$(uname -m)
log "Architecture: $ARCH"

# ── Xcode CLI tools
if ! xcode-select -p &>/dev/null; then
    log "Installing Xcode Command Line Tools..."
    xcode-select --install
    read -p "Press Enter after the installer finishes..."
fi

# ── Homebrew
if ! command -v brew &>/dev/null; then
    log "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# ── Dependencies
log "Installing dependencies via Homebrew..."
brew install cmake ninja sdl2 sdl2_ttf molten-vk

# QEMU: pick the right binary for the host arch
if [ "$ARCH" = "arm64" ]; then
    brew install qemu   # includes qemu-system-aarch64
    QEMU_BIN="qemu-system-aarch64"
    warn "Apple Silicon detected. You MUST use an ARM64 Android system image."
    warn "x86_64 images will not boot under Hypervisor.framework on M1/M2/M3."
else
    brew install qemu
    QEMU_BIN="qemu-system-x86_64"
fi
log "QEMU binary: $(command -v $QEMU_BIN)"

# ── Configure & build
BUILD_DIR="$(pwd)/build"
log "Configuring CMake (build dir: $BUILD_DIR)..."
cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DAVM_ENABLE_HVF=ON \
    -DAVM_ENABLE_MOLTENVK=ON

log "Building..."
NCPU=$(sysctl -n hw.logicalcpu)
cmake --build "$BUILD_DIR" -j"$NCPU"

# ── Code-sign with Hypervisor entitlement
ENTITLEMENTS="$(pwd)/avm.entitlements"
BINARY="$BUILD_DIR/avm"
if [ ! -f "$BINARY" ]; then
    err "Build succeeded but binary not found at $BINARY"
fi
log "Signing binary with Hypervisor.framework entitlement..."
codesign --entitlements "$ENTITLEMENTS" --force -s - "$BINARY"
log "Signed OK."

# ── Done
echo ""
log "Build complete! Binary: $BINARY"
echo ""
echo "  Run:"
echo "    $BINARY --image /path/to/android.img --memory 4096 --cores 4"
if [ "$ARCH" = "arm64" ]; then
    echo ""
    warn "Remember: --image must point to an ARM64 (aarch64) Android image!"
fi
