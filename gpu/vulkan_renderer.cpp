#include "avm/gpu/gpu_renderer.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <stdexcept>

#ifdef AVM_VULKAN_ENABLED
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#endif

namespace avm {

/**
 * VulkanRenderer — host-side Vulkan GPU backend.
 *
 * Pipeline overview:
 *
 *   gfxstream command buffer (from GfxstreamBridge)
 *       │
 *       ▼
 *   process_command_buffer()  — decodes GLES/VK commands
 *       │  ANGLE translates OpenGL ES → Vulkan draw calls
 *       ▼
 *   VkCommandBuffer recorded and submitted to graphics queue
 *       │
 *       ▼
 *   present_frame()  — blits decoded framebuffer → swapchain image
 *       │  vkAcquireNextImageKHR → blit → vkQueuePresentKHR
 *       ▼
 *   Host window (SDL2)
 */
class VulkanRenderer : public GpuRenderer {
public:

#ifdef AVM_VULKAN_ENABLED

    bool initialize(int width, int height, const std::string& title) override {
        width_  = width;
        height_ = height;

        // --- SDL2 Window ---
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::cerr << "[Vulkan] SDL_Init failed: " << SDL_GetError() << "\n";
            return false;
        }
        window_ = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
        );
        if (!window_) {
            std::cerr << "[Vulkan] SDL_CreateWindow failed: " << SDL_GetError() << "\n";
            return false;
        }

        if (!create_instance(title)) return false;
        if (!create_surface())        return false;
        if (!pick_physical_device())  return false;
        if (!create_logical_device()) return false;
        if (!create_swapchain())      return false;
        if (!create_render_pass())    return false;
        if (!create_framebuffers())   return false;
        if (!create_command_pool())   return false;
        if (!create_sync_objects())   return false;

        ready_ = true;
        std::cout << "[Vulkan] Renderer initialized ("
                  << width << "x" << height << ").\n";
        return true;
    }

    void shutdown() override {
        if (device_ != VK_NULL_HANDLE) vkDeviceWaitIdle(device_);

        for (auto& s : image_available_semaphores_) vkDestroySemaphore(device_, s, nullptr);
        for (auto& s : render_finished_semaphores_) vkDestroySemaphore(device_, s, nullptr);
        for (auto& f : in_flight_fences_)           vkDestroyFence(device_, f, nullptr);

        if (command_pool_ != VK_NULL_HANDLE)
            vkDestroyCommandPool(device_, command_pool_, nullptr);

        for (auto fb : swapchain_framebuffers_)
            vkDestroyFramebuffer(device_, fb, nullptr);

        if (render_pass_ != VK_NULL_HANDLE)
            vkDestroyRenderPass(device_, render_pass_, nullptr);

        for (auto iv : swapchain_image_views_)
            vkDestroyImageView(device_, iv, nullptr);

        if (swapchain_ != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        if (surface_ != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(instance_, surface_, nullptr);

        if (device_   != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);

        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        ready_ = false;
        std::cout << "[Vulkan] Renderer shut down.\n";
    }

    void process_command_buffer(const uint8_t* cmdbuf, size_t size) override {
        // TODO: integrate AOSP gfxstream host decoder here.
        // The decoder will walk the gfxstream wire format and call into
        // ANGLE (OpenGL ES → Vulkan) or the Vulkan passthrough path.
        //
        // Pseudocode:
        //   gfxstream::FrameDecoder decoder(cmdbuf, size);
        //   while (decoder.has_next()) {
        //       auto cmd = decoder.next();
        //       dispatch_gles_or_vk_command(cmd);   // → ANGLE or VK passthrough
        //   }
        (void)cmdbuf; (void)size;
    }

    void present_frame(const FrameBuffer& fb) override {
        if (!ready_) return;

        // Acquire next swapchain image
        uint32_t image_index;
        VkResult result = vkAcquireNextImageKHR(
            device_, swapchain_, UINT64_MAX,
            image_available_semaphores_[current_frame_],
            VK_NULL_HANDLE, &image_index);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            handle_resize(width_, height_);
            return;
        }

        // Upload framebuffer pixels to a staging VkBuffer, then
        // copy to the swapchain image via a transfer command.
        // For now we record a command buffer that clears to black
        // as a placeholder until gfxstream decode is wired up.
        VkCommandBuffer cmd = command_buffers_[image_index];

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        vkBeginCommandBuffer(cmd, &begin_info);

        VkRenderPassBeginInfo rp_info{};
        rp_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_info.renderPass        = render_pass_;
        rp_info.framebuffer       = swapchain_framebuffers_[image_index];
        rp_info.renderArea.offset = {0, 0};
        rp_info.renderArea.extent = swapchain_extent_;

        VkClearValue clear_color = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
        rp_info.clearValueCount = 1;
        rp_info.pClearValues    = &clear_color;

        vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
        // TODO: blit fb.data RGBA8 texture here once staging buffer is set up
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        // Submit
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkPipelineStageFlags wait_stage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit_info.waitSemaphoreCount   = 1;
        submit_info.pWaitSemaphores      = &image_available_semaphores_[current_frame_];
        submit_info.pWaitDstStageMask    = &wait_stage;
        submit_info.commandBufferCount   = 1;
        submit_info.pCommandBuffers      = &cmd;
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores    = &render_finished_semaphores_[current_frame_];

        vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
        vkQueueSubmit(graphics_queue_, 1, &submit_info,
                      in_flight_fences_[current_frame_]);

        // Present
        VkPresentInfoKHR present_info{};
        present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores    = &render_finished_semaphores_[current_frame_];
        present_info.swapchainCount     = 1;
        present_info.pSwapchains        = &swapchain_;
        present_info.pImageIndices      = &image_index;

        vkQueuePresentKHR(present_queue_, &present_info);
        current_frame_ = (current_frame_ + 1) % kMaxFramesInFlight;
    }

    void handle_resize(int new_width, int new_height) override {
        width_  = new_width;
        height_ = new_height;
        vkDeviceWaitIdle(device_);
        // Rebuild swap chain and framebuffers
        for (auto fb : swapchain_framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
        for (auto iv : swapchain_image_views_)  vkDestroyImageView(device_, iv, nullptr);
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        create_swapchain();
        create_framebuffers();
        std::cout << "[Vulkan] Swapchain recreated (" << new_width << "x" << new_height << ").\n";
    }

    bool is_ready() const override { return ready_; }
    const char* backend_name() const override { return "Vulkan"; }

private:
    // --------------------------------------------------------
    // Vulkan init helpers
    // --------------------------------------------------------

    bool create_instance(const std::string& app_name) {
        // Gather required extensions from SDL2
        uint32_t ext_count = 0;
        SDL_Vulkan_GetInstanceExtensions(window_, &ext_count, nullptr);
        std::vector<const char*> extensions(ext_count);
        SDL_Vulkan_GetInstanceExtensions(window_, &ext_count, extensions.data());

        VkApplicationInfo app_info{};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName   = app_name.c_str();
        app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        app_info.pEngineName        = "AVM";
        app_info.engineVersion      = VK_MAKE_VERSION(0, 1, 0);
        app_info.apiVersion         = VK_API_VERSION_1_1;

        VkInstanceCreateInfo create_info{};
        create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo        = &app_info;
        create_info.enabledExtensionCount   = ext_count;
        create_info.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
            std::cerr << "[Vulkan] vkCreateInstance failed.\n";
            return false;
        }
        return true;
    }

    bool create_surface() {
        if (!SDL_Vulkan_CreateSurface(window_, instance_, &surface_)) {
            std::cerr << "[Vulkan] SDL_Vulkan_CreateSurface failed.\n";
            return false;
        }
        return true;
    }

    bool pick_physical_device() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (count == 0) {
            std::cerr << "[Vulkan] No physical devices found.\n";
            return false;
        }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        // Prefer discrete GPU
        for (auto& d : devices) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(d, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device_ = d;
                std::cout << "[Vulkan] GPU: " << props.deviceName << "\n";
                break;
            }
        }
        // Fallback to first device
        if (physical_device_ == VK_NULL_HANDLE) {
            physical_device_ = devices[0];
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(physical_device_, &props);
            std::cout << "[Vulkan] GPU (fallback): " << props.deviceName << "\n";
        }

        // Find graphics + present queue families
        uint32_t qfam_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qfam_count, nullptr);
        std::vector<VkQueueFamilyProperties> qfams(qfam_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &qfam_count, qfams.data());

        for (uint32_t i = 0; i < qfam_count; i++) {
            if (qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphics_family_ = i;

            VkBool32 present_support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device_, i, surface_, &present_support);
            if (present_support) present_family_ = i;

            if (graphics_family_ >= 0 && present_family_ >= 0) break;
        }
        return (graphics_family_ >= 0 && present_family_ >= 0);
    }

    bool create_logical_device() {
        float queue_priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queue_infos;

        for (int family : {graphics_family_, present_family_}) {
            bool already_added = false;
            for (auto& q : queue_infos)
                if ((int)q.queueFamilyIndex == family) { already_added = true; break; }
            if (already_added) continue;

            VkDeviceQueueCreateInfo qi{};
            qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = family;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &queue_priority;
            queue_infos.push_back(qi);
        }

        const char* device_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo create_info{};
        create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount    = (uint32_t)queue_infos.size();
        create_info.pQueueCreateInfos       = queue_infos.data();
        create_info.enabledExtensionCount   = 1;
        create_info.ppEnabledExtensionNames = device_exts;

        if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
            std::cerr << "[Vulkan] vkCreateDevice failed.\n";
            return false;
        }
        vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
        vkGetDeviceQueue(device_, present_family_,  0, &present_queue_);
        return true;
    }

    bool create_swapchain() {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);

        // Prefer mailbox (triple buffering) for lowest latency gaming
        uint32_t fmt_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmt_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, formats.data());

        VkSurfaceFormatKHR chosen_fmt = formats[0];
        for (auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                chosen_fmt = f;

        uint32_t pm_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(pm_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &pm_count, present_modes.data());

        // Prefer VK_PRESENT_MODE_MAILBOX_KHR (low-latency triple buffer)
        // Fall back to FIFO (guaranteed to exist) if mailbox unavailable
        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
        for (auto& pm : present_modes)
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { present_mode = pm; break; }

        swapchain_extent_ = caps.currentExtent;
        if (swapchain_extent_.width == UINT32_MAX) {
            swapchain_extent_ = { (uint32_t)width_, (uint32_t)height_ };
        }

        uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
            image_count = caps.maxImageCount;

        VkSwapchainCreateInfoKHR sc_info{};
        sc_info.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sc_info.surface          = surface_;
        sc_info.minImageCount    = image_count;
        sc_info.imageFormat      = chosen_fmt.format;
        sc_info.imageColorSpace  = chosen_fmt.colorSpace;
        sc_info.imageExtent      = swapchain_extent_;
        sc_info.imageArrayLayers = 1;
        sc_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        uint32_t qfam_indices[] = {
            (uint32_t)graphics_family_, (uint32_t)present_family_
        };
        if (graphics_family_ != present_family_) {
            sc_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            sc_info.queueFamilyIndexCount = 2;
            sc_info.pQueueFamilyIndices   = qfam_indices;
        } else {
            sc_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        sc_info.preTransform   = caps.currentTransform;
        sc_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sc_info.presentMode    = present_mode;
        sc_info.clipped        = VK_TRUE;

        swapchain_format_ = chosen_fmt.format;

        if (vkCreateSwapchainKHR(device_, &sc_info, nullptr, &swapchain_) != VK_SUCCESS) {
            std::cerr << "[Vulkan] vkCreateSwapchainKHR failed.\n";
            return false;
        }

        // Retrieve swapchain images and create image views
        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
        swapchain_images_.resize(image_count);
        vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

        swapchain_image_views_.resize(image_count);
        for (uint32_t i = 0; i < image_count; i++) {
            VkImageViewCreateInfo iv_info{};
            iv_info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            iv_info.image    = swapchain_images_[i];
            iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            iv_info.format   = swapchain_format_;
            iv_info.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY };
            iv_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            iv_info.subresourceRange.levelCount     = 1;
            iv_info.subresourceRange.layerCount     = 1;
            vkCreateImageView(device_, &iv_info, nullptr, &swapchain_image_views_[i]);
        }

        std::cout << "[Vulkan] Swapchain created " << image_count << " images, "
                  << (present_mode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO")
                  << " present mode.\n";
        return true;
    }

    bool create_render_pass() {
        VkAttachmentDescription color_attachment{};
        color_attachment.format         = swapchain_format_;
        color_attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        color_attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &color_ref;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo rp_info{};
        rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rp_info.attachmentCount = 1;
        rp_info.pAttachments    = &color_attachment;
        rp_info.subpassCount    = 1;
        rp_info.pSubpasses      = &subpass;
        rp_info.dependencyCount = 1;
        rp_info.pDependencies   = &dep;

        return vkCreateRenderPass(device_, &rp_info, nullptr, &render_pass_) == VK_SUCCESS;
    }

    bool create_framebuffers() {
        swapchain_framebuffers_.resize(swapchain_image_views_.size());
        for (size_t i = 0; i < swapchain_image_views_.size(); i++) {
            VkFramebufferCreateInfo fb_info{};
            fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fb_info.renderPass      = render_pass_;
            fb_info.attachmentCount = 1;
            fb_info.pAttachments    = &swapchain_image_views_[i];
            fb_info.width           = swapchain_extent_.width;
            fb_info.height          = swapchain_extent_.height;
            fb_info.layers          = 1;
            if (vkCreateFramebuffer(device_, &fb_info, nullptr,
                &swapchain_framebuffers_[i]) != VK_SUCCESS)
                return false;
        }
        return true;
    }

    bool create_command_pool() {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = graphics_family_;
        pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS)
            return false;

        command_buffers_.resize(swapchain_images_.size());
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool        = command_pool_;
        alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = (uint32_t)command_buffers_.size();
        return vkAllocateCommandBuffers(device_, &alloc_info,
                                        command_buffers_.data()) == VK_SUCCESS;
    }

    bool create_sync_objects() {
        image_available_semaphores_.resize(kMaxFramesInFlight);
        render_finished_semaphores_.resize(kMaxFramesInFlight);
        in_flight_fences_.resize(kMaxFramesInFlight);

        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < kMaxFramesInFlight; i++) {
            if (vkCreateSemaphore(device_, &sem_info, nullptr,
                                  &image_available_semaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device_, &sem_info, nullptr,
                                  &render_finished_semaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(device_, &fence_info, nullptr,
                              &in_flight_fences_[i]) != VK_SUCCESS)
                return false;
        }
        return true;
    }

    // --- State ---
    int width_ = 0, height_ = 0;
    bool ready_ = false;

    SDL_Window*    window_   = nullptr;
    VkInstance     instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR   surface_  = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice         device_          = VK_NULL_HANDLE;
    VkQueue          graphics_queue_  = VK_NULL_HANDLE;
    VkQueue          present_queue_   = VK_NULL_HANDLE;
    int              graphics_family_ = -1;
    int              present_family_  = -1;

    VkSwapchainKHR             swapchain_       = VK_NULL_HANDLE;
    VkFormat                   swapchain_format_;
    VkExtent2D                 swapchain_extent_;
    std::vector<VkImage>       swapchain_images_;
    std::vector<VkImageView>   swapchain_image_views_;
    std::vector<VkFramebuffer> swapchain_framebuffers_;

    VkRenderPass                  render_pass_   = VK_NULL_HANDLE;
    VkCommandPool                 command_pool_  = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer>  command_buffers_;

    static constexpr int kMaxFramesInFlight = 2; // double buffering
    int current_frame_ = 0;
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence>     in_flight_fences_;

#else // !AVM_VULKAN_ENABLED

    bool initialize(int, int, const std::string&) override {
        std::cerr << "[Vulkan] AVM was built without Vulkan support.\n";
        return false;
    }
    void shutdown() override {}
    void process_command_buffer(const uint8_t*, size_t) override {}
    void present_frame(const FrameBuffer&) override {}
    void handle_resize(int, int) override {}
    bool is_ready() const override { return false; }
    const char* backend_name() const override { return "Vulkan (disabled)"; }

#endif
};

// Factory function used by VmManager
std::unique_ptr<GpuRenderer> create_vulkan_renderer() {
    return std::make_unique<VulkanRenderer>();
}

} // namespace avm
