# AVM Setup — Linux

## Requirements

- Linux kernel 4.4+ with KVM support
- CPU with Intel VT-x or AMD-V (check: `egrep '(vmx|svm)' /proc/cpuinfo`)
- `kvm` module loaded: `lsmod | grep kvm`
- GPU with Vulkan support (check: `vulkaninfo | grep deviceName`)

## Check KVM Availability

```bash
ls -la /dev/kvm
# Should show: crw-rw---- 1 root kvm ...

# Add yourself to the kvm group if needed:
sudo usermod -aG kvm $USER
newgrp kvm
```

## Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build \
    qemu-system-x86 \
    libvulkan-dev vulkan-tools vulkan-validationlayers \
    libsdl2-dev \
    adb android-tools-adb \
    python3 python3-pip
```

## Build AVM

```bash
git clone https://github.com/wisnuub/AVM.git
cd AVM
chmod +x scripts/bootstrap.sh scripts/build.sh
./scripts/bootstrap.sh
./scripts/build.sh
```

## Download Android x86 System Image

AVM requires a pre-built Android x86 AOSP system image. You can use:
- AOSP emulator images from Android SDK (`sdkmanager "system-images;android-33;google_apis;x86_64"`)
- [Android-x86 project](https://www.android-x86.org/) images

```bash
# Using Android SDK manager (requires Android Studio or cmdline-tools)
sdkmanager "system-images;android-33;google_apis;x86_64"
```

## Run AVM

```bash
./build/avm \
  --image ~/.android/avd/system.img \
  --memory 4096 \
  --cores 4 \
  --gpu vulkan
```
