# AVM Setup — Windows

## Requirements

- Windows 10 (2004) or Windows 11
- CPU with Intel VT-x or AMD-V (enabled in BIOS/UEFI)
- Windows Hypervisor Platform (WHPX) or AEHD enabled
- Vulkan-capable GPU (NVIDIA / AMD / Intel Arc with recent drivers)
- Visual Studio 2022 with C++ workload
- CMake 3.20+, Ninja

## Enable Windows Hypervisor Platform

1. Open **Turn Windows features on or off**
2. Enable **Windows Hypervisor Platform**
3. Reboot

Or via PowerShell (Admin):
```powershell
Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform -NoRestart
Restart-Computer
```

## Install AEHD (Preferred over WHPX)

Download and install the **Android Emulator Hypervisor Driver** from:
https://github.com/google/android-emulator-hypervisor-driver

This provides better performance than WHPX for Android emulation.

## Install Vulkan SDK

Download from: https://vulkan.lunarg.com/sdk/home

Install and ensure `VULKAN_SDK` environment variable is set.

## Build AVM

```powershell
git clone https://github.com/wisnuub/AVM.git
cd AVM
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DAVM_ENABLE_WHPX=ON `
    -DAVM_ENABLE_VULKAN=ON
cmake --build build --config Release
```

## Run AVM

```powershell
.\build\Release\avm.exe `
  --image C:\path\to\android.img `
  --memory 4096 `
  --cores 4 `
  --gpu vulkan
```
