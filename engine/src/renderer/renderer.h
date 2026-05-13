#pragma once

#include "volk.h"

#include <expected>
#include <span>
#include <string>

namespace renderer
{
    class Renderer;

    // Phase 1: Vulkan instance only.
    //
    // Created before platform produces a VkSurfaceKHR. Caller (platform)
    // reads instance() to feed vkCreateWin32SurfaceKHR, then hands the
    // resulting surface back via bindSurface() to advance to phase 2.
    class RendererInstance
    {
    public:
        static std::expected<RendererInstance, VkResult> create(const std::string& appName,
                                                                std::span<const char* const> platformExtensions);

        RendererInstance(const RendererInstance&) = delete;
        RendererInstance& operator=(const RendererInstance&) = delete;
        RendererInstance(RendererInstance&&) noexcept;
        RendererInstance& operator=(RendererInstance&&) noexcept;
        ~RendererInstance();

        [[nodiscard]] VkInstance instance() const noexcept
        {
            return instance_;
        }

        // Phase-1 -> phase-2 transition. Rvalue-qualified so the call site
        // has to std::move into it — signals that *this is consumed.
        //
        // Takes ownership of `surface`. On success, ownership of the
        // VkInstance moves into the new Renderer and *this is disarmed
        // (caller should discard). On failure, the surface is destroyed
        // here, and *this still owns the instance — let its dtor run and
        // the whole renderer state tears down (the "lose both phases on
        // failure" policy — caller retries from create()).
        std::expected<Renderer, VkResult> bindSurface(VkSurfaceKHR surface, VkExtent2D extent) &&;

    private:
        RendererInstance(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger) noexcept
            : instance_(instance), debugMessenger_(debugMessenger)
        {
        }
        VkInstance instance_ = VK_NULL_HANDLE;
        // Routes Vulkan validation/perf messages to spdlog. VK_NULL_HANDLE in
        // release builds (extension not requested). Must be destroyed before
        // instance_.
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    };

    // Phase 2: bound to a surface, ready to draw.
    //
    // Only constructible by RendererInstance::bindSurface. Owns the
    // instance, surface, logical device, and (TODO) all remaining
    // device-side resources.
    class Renderer
    {
        friend class RendererInstance;

    public:
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) noexcept;
        Renderer& operator=(Renderer&&) noexcept;
        ~Renderer();

        // draw API lands here.

    private:
        Renderer(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, VkSurfaceKHR surface,
                 VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueIdx,
                 VkExtent2D extent) noexcept
            : instance_(instance), debugMessenger_(debugMessenger), surface_(surface), physicalDevice_(physicalDevice),
              device_(device), graphicsQueue_(graphicsQueue), graphicsQueueIdx_(graphicsQueueIdx), extent_(extent)
        {
        }

        // Destroys owned handles in dependency order and nulls them out.
        // Shared by dtor and move-assign so the order can't drift.
        void destroy_() noexcept;

        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE; // VK_NULL_HANDLE in release
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE; // not owned (lifetime tied to instance)
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE; // not owned (lifetime tied to device)
        uint32_t graphicsQueueIdx_ = 0;
        VkExtent2D extent_{};
        // TODO: VkSwapchainKHR, per-frame sync, command pools, etc.
        // Add as init lands.
    };
} // namespace renderer
