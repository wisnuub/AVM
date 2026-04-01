#include "avm/gpu/gpu_renderer.h"
#include <iostream>

/**
 * OpenGLRenderer — host-side OpenGL fallback backend.
 *
 * Used on systems where Vulkan is unavailable.
 * Receives decoded OpenGL ES commands from gfxstream and
 * translates them to desktop OpenGL 4.x calls.
 *
 * TODO:
 *  - Initialize OpenGL context (via SDL2 or GLFW)
 *  - Implement framebuffer texture + fullscreen quad for presentation
 *  - Map gfxstream GLES commands → desktop GL calls
 */

namespace avm {

class OpenGLRenderer : public GpuRenderer {
public:
    bool initialize() override {
        std::cout << "[OpenGLRenderer] Initializing OpenGL fallback... (stub)\n";
        return true;
    }

    void shutdown() override {
        std::cout << "[OpenGLRenderer] Shutdown.\n";
    }

    void present_frame(const uint8_t* framebuffer, int width, int height) override {
        (void)framebuffer; (void)width; (void)height;
        // TODO: glTexSubImage2D + draw fullscreen quad
    }

    void process_command_buffer(const uint8_t* cmdbuf, size_t size) override {
        (void)cmdbuf; (void)size;
        // TODO: decode and dispatch GL commands
    }

    const char* backend_name() const override { return "OpenGL"; }
};

} // namespace avm
