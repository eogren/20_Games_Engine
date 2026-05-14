// Renderer class lifetime + per-frame surface.
//
// One-time setup (RendererInstance, bindSurface, swapchain recreate) lives
// in renderer_init.cpp; this file owns the Renderer's move/dtor/destroy_
// plus the (forthcoming) beginFrame/endFrame/draw API.

#include "renderer/renderer.h"

#include <utility>

#include "volk.h"

namespace renderer
{
    void Renderer::destroy_() noexcept
    {
        // Dependency order: image views -> swapchain -> device -> surface ->
        // debug messenger -> instance. Views reference swapchain images; the
        // swapchain owns those images; both live on the device. Messenger and
        // surface both depend only on the instance; messenger is destroyed
        // last (just before the instance) so validation messages emitted
        // during device/surface teardown still flow through our callback.
        // (Queue/physical-device handles aren't destroyed — their lifetimes
        // are tied to device and instance respectively. Swapchain images
        // aren't destroyed directly either; vkDestroySwapchainKHR releases
        // them. Command buffers likewise — vkDestroyCommandPool frees them
        // when the pool lands.)
        for (VkImageView v : swapchainImageViews_)
        {
            // vkDestroyImageView accepts VK_NULL_HANDLE as a no-op, so partial
            // image-view creation failures don't need a special path here.
            if (v != VK_NULL_HANDLE) vkDestroyImageView(device_, v, nullptr);
        }
        swapchainImageViews_.clear();
        swapchainImages_.clear();
        if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        for (VkSemaphore s : renderCompleteSemaphores)
        {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(device_, s, nullptr);
        }
        renderCompleteSemaphores.clear();
        for (VkSemaphore s : imageAcquiredSemaphores)
        {
            if (s != VK_NULL_HANDLE) vkDestroySemaphore(device_, s, nullptr);
        }
        for (VkFence f : fences)
        {
            if (f != VK_NULL_HANDLE) vkDestroyFence(device_, f, nullptr);
        }
        // Pool destruction frees its allocated command buffers; no separate
        // vkFreeCommandBuffers needed.
        if (commandPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, commandPool_, nullptr);
        if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
        if (surface_ != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (debugMessenger_ != VK_NULL_HANDLE) vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
        debugMessenger_ = VK_NULL_HANDLE;
        instance_ = VK_NULL_HANDLE;
        physicalDevice_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.fill(VK_NULL_HANDLE);
        fences.fill(VK_NULL_HANDLE);
        imageAcquiredSemaphores.fill(VK_NULL_HANDLE);
    }

    Renderer::Renderer(Renderer&& other) noexcept
        : instance_(other.instance_), debugMessenger_(other.debugMessenger_), surface_(other.surface_),
          physicalDevice_(other.physicalDevice_), device_(other.device_), graphicsQueue_(other.graphicsQueue_),
          graphicsQueueIdx_(other.graphicsQueueIdx_), extent_(other.extent_), swapchain_(other.swapchain_),
          swapchainFormat_(other.swapchainFormat_), swapchainImages_(std::move(other.swapchainImages_)),
          swapchainImageViews_(std::move(other.swapchainImageViews_)), commandPool_(other.commandPool_),
          commandBuffers_(other.commandBuffers_), fences(other.fences),
          imageAcquiredSemaphores(other.imageAcquiredSemaphores),
          renderCompleteSemaphores(std::move(other.renderCompleteSemaphores))
    {
        other.instance_ = VK_NULL_HANDLE;
        other.debugMessenger_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.graphicsQueue_ = VK_NULL_HANDLE;
        other.swapchain_ = VK_NULL_HANDLE;
        other.commandPool_ = VK_NULL_HANDLE;
        other.commandBuffers_.fill(VK_NULL_HANDLE);
        other.fences.fill(VK_NULL_HANDLE);
        other.imageAcquiredSemaphores.fill(VK_NULL_HANDLE);
        // swapchainImages_ / swapchainImageViews_ / renderCompleteSemaphores: vector move leaves source empty.
    }

    Renderer& Renderer::operator=(Renderer&& other) noexcept
    {
        if (this != &other)
        {
            destroy_();
            instance_ = other.instance_;
            debugMessenger_ = other.debugMessenger_;
            surface_ = other.surface_;
            physicalDevice_ = other.physicalDevice_;
            device_ = other.device_;
            graphicsQueue_ = other.graphicsQueue_;
            graphicsQueueIdx_ = other.graphicsQueueIdx_;
            extent_ = other.extent_;
            swapchain_ = other.swapchain_;
            swapchainFormat_ = other.swapchainFormat_;
            swapchainImages_ = std::move(other.swapchainImages_);
            swapchainImageViews_ = std::move(other.swapchainImageViews_);
            commandPool_ = other.commandPool_;
            commandBuffers_ = other.commandBuffers_;
            fences = other.fences;
            imageAcquiredSemaphores = other.imageAcquiredSemaphores;
            renderCompleteSemaphores = std::move(other.renderCompleteSemaphores);
            other.instance_ = VK_NULL_HANDLE;
            other.debugMessenger_ = VK_NULL_HANDLE;
            other.surface_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.graphicsQueue_ = VK_NULL_HANDLE;
            other.swapchain_ = VK_NULL_HANDLE;
            other.commandPool_ = VK_NULL_HANDLE;
            other.commandBuffers_.fill(VK_NULL_HANDLE);
            other.fences.fill(VK_NULL_HANDLE);
            other.imageAcquiredSemaphores.fill(VK_NULL_HANDLE);
        }
        return *this;
    }

    Renderer::~Renderer()
    {
        destroy_();
    }
} // namespace renderer
