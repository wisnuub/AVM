#include <iostream>
#include <fstream>

/**
 * hypervisor_detect.cpp
 *
 * Standalone hypervisor capability probe.
 * Can be used as a CLI tool to check what acceleration is available:
 *
 *   ./avm-hypervisor-check
 *
 * Output example:
 *   [KVM]  /dev/kvm found — hardware acceleration available
 *   [CPU]  vmx (Intel VT-x) flag present in /proc/cpuinfo
 */

int main() {
    std::cout << "=== AVM Hypervisor Check ===\n\n";

#ifdef __linux__
    // KVM
    std::ifstream kvm("/dev/kvm");
    if (kvm.good()) {
        std::cout << "[KVM]  /dev/kvm accessible — KVM available ✓\n";
    } else {
        std::cout << "[KVM]  /dev/kvm NOT accessible.\n"
                  << "       Try: sudo modprobe kvm_intel   (Intel)\n"
                  << "            sudo modprobe kvm_amd     (AMD)\n"
                  << "            sudo usermod -aG kvm $USER\n";
    }

    // CPU flags
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    bool has_vmx = false, has_svm = false;
    while (std::getline(cpuinfo, line)) {
        if (line.find("vmx") != std::string::npos) has_vmx = true;
        if (line.find("svm") != std::string::npos) has_svm = true;
    }
    if (has_vmx) std::cout << "[CPU]  Intel VT-x (vmx) detected ✓\n";
    if (has_svm) std::cout << "[CPU]  AMD-V (svm) detected ✓\n";
    if (!has_vmx && !has_svm)
        std::cout << "[CPU]  No hardware virtualization flags found.\n"
                  << "       Ensure VT-x/AMD-V is enabled in BIOS/UEFI.\n";

#elif _WIN32
    std::cout << "[WIN]  Run: Get-WmiObject -Class Win32_Processor | Select VirtualizationFirmwareEnabled\n";
    std::cout << "[WIN]  Check AEHD: https://github.com/google/android-emulator-hypervisor-driver\n";
    std::cout << "[WIN]  Check WHPX: Enable 'Windows Hypervisor Platform' in Windows Features\n";
#else
    std::cout << "[INFO] Platform not fully supported yet.\n";
#endif

    std::cout << "\n=== Done ===\n";
    return 0;
}
