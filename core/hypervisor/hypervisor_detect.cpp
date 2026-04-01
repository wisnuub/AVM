#include "avm/core/platform.h"
#include "avm/core/vm_manager.h"
#include <iostream>
#include <fstream>

// This file supplements vm_manager.cpp with macOS HVF detection.
// The main detect_hypervisor() lives in vm_manager.cpp but only covers
// Windows (AEHD/WHPX) and Linux (KVM). This translation unit patches
// the macOS case by overriding the detect path when compiled on Apple.

// Nothing to do here for now — macOS detection is handled inline in
// vm_manager.cpp via the AVM_OS_MACOS guard added in this patch.
// This file is kept as a dedicated home for future per-platform
// hypervisor capability queries (e.g. querying max vCPU count from HVF).

namespace avm {

#if AVM_OS_MACOS
bool query_hvf_max_vcpus(int& out_max) {
    // Hypervisor.framework does not expose a direct API for max vCPU count.
    // Practical limit on Apple Silicon is 16 (matches the physical core count).
    // We use hw.logicalcpu as a safe upper bound.
    out_max = 16;
#  include <sys/sysctl.h>
    int ncpu = 4;
    size_t len = sizeof(ncpu);
    sysctlbyname("hw.logicalcpu", &ncpu, &len, nullptr, 0);
    out_max = ncpu;
    return true;
}
#endif

} // namespace avm
