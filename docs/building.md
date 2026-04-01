# Building AVM

## Prerequisites by Platform

### macOS (Apple Silicon M1/M2/M3 — Recommended for development)

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew dependencies
brew install cmake ninja sdl2 sdl2_ttf molten-vk

# Optional: LunarG Vulkan SDK (includes validation layers)
# Download from https://vulkan.lunarg.com/sdk/home#mac
# Then: source $VULKAN_SDK/setup-env.sh
```

**Important — Hypervisor entitlement:**
Apple requires a special entitlement to use `Hypervisor.framework`. For local development builds:

```bash
# Create entitlements file
cat > avm.entitlements << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>com.apple.security.hypervisor</key><true/>
  <key>com.apple.security.virtualization</key><true/>
</dict></plist>
EOF

# After building, sign the binary
codesign --entitlements avm.entitlements --force -s - ./build/avm
```

**Android image for Apple Silicon:**
You need an **ARM64 Android image** (not x86_64) because Hypervisor.framework on M1
runs ARM64 guests natively. Use AOSP arm64 builds or:
- [Android-x86 arm64 builds](https://www.android-x86.org)
- Custom AOSP `aosp_arm64-userdebug` build target

```bash
# Build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAVM_ENABLE_HVF=ON \
  -DAVM_ENABLE_MOLTENVK=ON
cmake --build build -j$(sysctl -n hw.logicalcpu)

# Sign & run
codesign --entitlements avm.entitlements --force -s - ./build/avm
./build/avm
```

---

### Windows (x86_64)

```powershell
# Install Visual Studio 2022 (with C++ workload) + CMake
# Install vcpkg
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install sdl2:x64-windows sdl2-ttf:x64-windows

# Install Vulkan SDK: https://vulkan.lunarg.com/sdk/home#windows
# Set VULKAN_SDK environment variable (installer does this automatically)
```

**Enable Windows Hypervisor Platform (WHPX):**
```powershell
# Run as Administrator
Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform
# Reboot required
```

```powershell
# Build
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DAVM_ENABLE_WHPX=ON `
  -DAVM_ENABLE_VULKAN=ON
cmake --build build --config RelWithDebInfo
```

---

### Linux (x86_64)

```bash
# Ubuntu/Debian
sudo apt install cmake ninja-build libsdl2-dev libsdl2-ttf-dev \
                 libvulkan-dev vulkan-validationlayers

# Verify KVM access
ls -la /dev/kvm
sudo usermod -aG kvm $USER  # add yourself to kvm group

# Build
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DAVM_ENABLE_KVM=ON \
  -DAVM_ENABLE_VULKAN=ON
cmake --build build -j$(nproc)
./build/avm
```

---

## Platform Feature Matrix

| Feature | Linux x86 | macOS Intel | macOS M1/M2/M3 | Windows x86 |
|---|---|---|---|---|
| Hypervisor | KVM | Hypervisor.framework | **Hypervisor.framework** | WHPX |
| Guest arch | x86_64 | x86_64 | **ARM64** | x86_64 |
| GPU renderer | Vulkan / OpenGL | MoltenVK (Vulkan→Metal) | **MoltenVK (Vulkan→Metal)** | Vulkan / OpenGL |
| virtio-input | ✅ | ✅ | ✅ | ⚠️ (ADB only for now) |
| Performance | ~95-100% native | ~85-95% native | **~90-98% native** | ~85-95% native |

---

## Apple Silicon Performance Notes

M1/M2/M3 is actually **excellent** for Android emulation because:
1. The host CPU is ARM64, same as the Android guest — no instruction translation needed
2. `Hypervisor.framework` provides direct vCPU access with ~1-3% overhead
3. MoltenVK translates Vulkan calls to Metal with typically <10% overhead
4. Android phones are also ARM64 — so this is as close to 1:1 as possible

The main caveat: you **must** use an ARM64 Android system image.
x86_64 images will not run under Hypervisor.framework on Apple Silicon.

---

## Troubleshooting

| Error | Fix |
|---|---|
| `hv_vm_create failed: 0xfae94005` | Binary not signed with Hypervisor entitlement — run `codesign` step |
| `Vulkan: No devices found` | MoltenVK not installed — `brew install molten-vk` |
| `SDL_Vulkan_CreateSurface failed` | VULKAN_SDK not set — `source $VULKAN_SDK/setup-env.sh` |
| WHPX init fails on Windows | Hypervisor Platform feature not enabled — see Windows steps above |
| `hv_vm_map failed` | Insufficient entitlements or macOS version < 11.0 |
