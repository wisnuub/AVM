#include <iostream>
#include <string>
#include <memory>
#include "avm/core/vm_manager.h"
#include "avm/core/config.h"

static void print_banner() {
    std::cout << R"(
    ___   _   ____  ___
   /   | | | / /  |/  /
  / /| | | |/ / /|_/ /
 / ___ |_|___/_/  /_/
/_/  |_(_)____/_/

Android Virtual Machine — v0.1.0
Gaming-focused Android emulator
)" << std::endl;
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options]\n"
              << "  --image <path>     Path to Android x86 system image\n"
              << "  --memory <mb>      RAM allocation in MB (default: 4096)\n"
              << "  --cores <n>        vCPU cores (default: 4)\n"
              << "  --gpu vulkan|gl    Host GPU renderer backend (default: vulkan)\n"
              << "  --no-accel         Disable hardware virtualization (slow, testing only)\n"
              << "  --help             Show this message\n";
}

int main(int argc, char* argv[]) {
    print_banner();

    avm::Config config;
    config.memory_mb = 4096;
    config.vcpu_cores = 4;
    config.gpu_backend = avm::GpuBackend::Vulkan;
    config.hardware_accel = true;

    // Parse CLI args
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
            std::string backend = argv[++i];
            config.gpu_backend = (backend == "gl")
                ? avm::GpuBackend::OpenGL
                : avm::GpuBackend::Vulkan;
        } else if (arg == "--no-accel") {
            config.hardware_accel = false;
        }
    }

    if (config.system_image_path.empty()) {
        std::cerr << "[AVM] Error: No system image specified. Use --image <path>\n";
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "[AVM] Starting VM...\n"
              << "  Image:  " << config.system_image_path << "\n"
              << "  Memory: " << config.memory_mb << " MB\n"
              << "  vCPUs:  " << config.vcpu_cores << "\n"
              << "  GPU:    " << (config.gpu_backend == avm::GpuBackend::Vulkan ? "Vulkan" : "OpenGL") << "\n"
              << "  HW Accel: " << (config.hardware_accel ? "Enabled" : "Disabled") << "\n\n";

    auto vm = std::make_unique<avm::VmManager>(config);

    if (!vm->initialize()) {
        std::cerr << "[AVM] Failed to initialize VM. Check hypervisor availability.\n";
        return 1;
    }

    return vm->run();
}
