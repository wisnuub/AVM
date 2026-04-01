#pragma once
#include "config.h"
#include <memory>

namespace avm {

class HypervisorBackendBase;
class GpuRenderer;
class InputBridge;

/**
 * VmManager — top-level VM lifecycle controller.
 *
 * Responsibilities:
 *  - Detect and initialize the best available hypervisor backend
 *  - Launch and configure QEMU with the provided Config
 *  - Wire up GPU forwarding (gfxstream) and input bridge
 *  - Manage start / stop / snapshot / restore lifecycle
 */
class VmManager {
public:
    explicit VmManager(const Config& config);
    ~VmManager();

    /**
     * initialize() — detect hypervisor, set up GPU renderer and input bridge.
     * Must be called before run().
     * @return true on success
     */
    bool initialize();

    /**
     * run() — start the VM and block until the user closes it.
     * @return exit code
     */
    int run();

    /** Gracefully stop the VM. */
    void stop();

    /** Save a snapshot to disk. */
    bool snapshot(const std::string& name);

    /** Restore a previously saved snapshot. */
    bool restore_snapshot(const std::string& name);

private:
    bool detect_hypervisor();
    bool launch_qemu();

    Config config_;
    std::unique_ptr<HypervisorBackendBase> hypervisor_;
    std::unique_ptr<GpuRenderer>           gpu_renderer_;
    std::unique_ptr<InputBridge>           input_bridge_;
};

} // namespace avm
