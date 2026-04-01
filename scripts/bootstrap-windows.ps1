# bootstrap-windows.ps1 - AVM setup for Windows
# Run in PowerShell as Administrator

Write-Host "[AVM] Windows bootstrap" -ForegroundColor Cyan

# Check for Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Warning "Visual Studio 2022 not found. Download from https://visualstudio.microsoft.com/"
    Write-Warning "Ensure the 'Desktop development with C++' workload is installed."
}

# Enable Windows Hypervisor Platform
Write-Host "[AVM] Enabling Windows Hypervisor Platform..." -ForegroundColor Green
$hvp = Get-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform
if ($hvp.State -ne 'Enabled') {
    Enable-WindowsOptionalFeature -Online -FeatureName HypervisorPlatform -NoRestart
    Write-Warning "HypervisorPlatform enabled. A reboot is required before AVM will work."
} else {
    Write-Host "  HypervisorPlatform already enabled." -ForegroundColor Green
}

# Install vcpkg if not present
$vcpkgDir = "C:\vcpkg"
if (-not (Test-Path $vcpkgDir)) {
    Write-Host "[AVM] Cloning vcpkg..." -ForegroundColor Green
    git clone https://github.com/microsoft/vcpkg $vcpkgDir
    & "$vcpkgDir\bootstrap-vcpkg.bat"
}

# Install SDL2
Write-Host "[AVM] Installing SDL2 via vcpkg..." -ForegroundColor Green
& "$vcpkgDir\vcpkg" install sdl2:x64-windows sdl2-ttf:x64-windows

# CMake configure
Write-Host "[AVM] Configuring CMake..." -ForegroundColor Green
$toolchain = "$vcpkgDir\scripts\buildsystems\vcpkg.cmake"
cmake -B build -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE=$toolchain `
    -DAVM_ENABLE_WHPX=ON `
    -DAVM_ENABLE_VULKAN=ON

Write-Host "[AVM] Building..." -ForegroundColor Green
cmake --build build --config RelWithDebInfo

Write-Host ""
Write-Host "[AVM] Build complete! Binary: build\RelWithDebInfo\avm.exe" -ForegroundColor Green
Write-Host "Run: .\build\RelWithDebInfo\avm.exe --image C:\path\to\android.img"
