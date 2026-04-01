#include <iostream>
#include <string>
#include <memory>
#include "avm/core/vm_manager.h"
#include "avm/core/config.h"
#include "avm/core/platform.h"

static void print_banner() {
    std::cout << R"(
    ___   _   ____  ___
   /   | | | / /  |/  /
  / /| | | |/ / /|_/ /
 / ___ |_|___/_/  /_/
/_/  |_(_)____/_/

Android Virtual Machine v0.1.0
)" <<
#if AVM_OS_MACOS && AVM_ARCH_ARM64
    "Platform: macOS Apple Silicon (ARM64) — use ARM64 Android image\n"
#elif AVM_OS_MACOS
    "Platform: macOS Intel (x86_64)\n"
#elif AVM_OS_WINDOWS
    "Platform: Windows\n"
#else
    "Platform: Linux\n"
#endif
    << std::endl;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "  --image <path>     Path to Android system image\n"
#if AVM_OS_MACOS && AVM_ARCH_ARM64
              << "                     NOTE: Must be an ARM64 image on Apple Silicon!\n"
              << "                     x86_64 images will NOT work on M1/M2/M3.\n"
#endif
              << "  --memory <mb>      RAM in MB (default: 4096)\n"
              << "  --cores <n>        vCPU count (default: 4)\n"
              << "  --gpu vulkan|gl    Host GPU renderer (default: vulkan)\n"
              << "  --no-accel         Disable HW virtualization (slow)\n"
              << "  --help             Show this message\n";
}

int main(int argc, char* argv[]) {
    print_banner();

    avm::Config config;
    config.memory_mb      = 4096;
    config.vcpu_cores     = 4;
    config.gpu_backend    = avm::GpuBackend::Vulkan;
    config.hardware_accel = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--image" && i + 1 < argc) {
            config.system_image_path = argv[++i];
        } else if (arg == "--memory" && i + 1 < argc) {
            config.memory_mb = std::stoi(argv[++i]);
        } else if (arg == "--cores" && i + 1 < argc) {
            config.vcpu_cores = std::stoi(argv[++i]);
        } else if (arg == "--gpu" && i + 1 < argc) {
            std::string b = argv[++i];
            config.gpu_backend = (b == "gl")
                ? avm::GpuBackend::OpenGL
                : avm::GpuBackend::Vulkan;
        } else if (arg == "--no-accel") {
            config.hardware_accel = false;
        } else {
            std::cerr << "[AVM] Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (config.system_image_path.empty()) {
        std::cerr << "[AVM] Error: no --image specified.\n";
        print_usage(argv[0]);
        return 1;
    }

    std::cout
        << "[AVM] Config:\n"
        << "  Image:    " << config.system_image_path << "\n"
        << "  Memory:   " << config.memory_mb << " MB\n"
        << "  vCPUs:    " << config.vcpu_cores << "\n"
        << "  GPU:      "
        << (config.gpu_backend == avm::GpuBackend::Vulkan ? "Vulkan" : "OpenGL")
        << "\n"
        << "  HW Accel: " << (config.hardware_accel ? "Yes" : "No") << "\n\n";

    auto vm = std::make_unique<avm::VmManager>(config);
    if (!vm->initialize()) {
        std::cerr << "[AVM] VM initialization failed.\n";
        return 1;
    }
    return vm->run();
}
