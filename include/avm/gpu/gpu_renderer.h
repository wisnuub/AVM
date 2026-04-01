#pragma once
#include <cstdint>

namespace avm {

/**
 * GpuRenderer — abstract interface for host-side GPU rendering.
 *
 * The guest Android VM sends serialized OpenGL ES / Vulkan commands
 * via gfxstream. The GpuRenderer receives decoded commands and
 * dispatches them to the actual host GPU.
 *
 * Concrete implementations:
 *  - VulkanRenderer   (preferred, best performance)
 *  - OpenGLRenderer   (fallback for older hardware)
 *  - SoftwareRenderer (slow, testing only)
 */
class GpuRenderer {
public:
    virtual ~GpuRenderer() = default;

    virtual bool initialize() = 0;
    virtual void shutdown()   = 0;

    /** Called each frame — present the decoded framebuffer to the host window. */
    virtual void present_frame(const uint8_t* framebuffer, int width, int height) = 0;

    /** Process a serialized gfxstream command buffer from the guest. */
    virtual void process_command_buffer(const uint8_t* cmdbuf, size_t size) = 0;

    virtual const char* backend_name() const = 0;
};

} // namespace avm
