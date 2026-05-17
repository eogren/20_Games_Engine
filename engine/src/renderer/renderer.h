#pragma once

#include "math/color.h"
#include "volk.h"

#ifdef ENGINE_TRACY_ENABLED
#include <tracy/TracyVulkan.hpp>
#endif

#include <glm/mat4x4.hpp>

#include <array>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace renderer
{
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT{2};

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

        // Stateful clear color: the next beginFrame() and every one after will
        // clear the swapchain image to this color until set again. Placeholder
        // for the proper Camera/viewport API later (CLAUDE.md > "Renderer
        // design": workflow extracted on top of substrate after 2–3 games).
        void setClearColor(engine::Color color) noexcept;

        // Override the coordinate space for drawQuad. Call once per update()
        // before any drawQuad calls; stays in effect until the next beginFrame
        // resets it to pixel space. Typical callers pass fixed game dimensions
        // (e.g. 800 x 600) so coordinates are independent of window size.
        void setProjectionExtent(float w, float h) noexcept;

        // Begin a frame: wait on the frame-in-flight fence, acquire the next
        // swapchain image, open the command buffer, transition the image to
        // COLOR_ATTACHMENT_OPTIMAL, and start dynamic rendering with a clear
        // to the stored color.
        //
        // Returns true if rendering proceeds this frame. Returns false when
        // the swapchain is stale (VK_ERROR_OUT_OF_DATE_KHR on acquire) and
        // was recreated transparently — caller must skip endFrame() this tick.
        // Unrecoverable errors (fence wait / command buffer reset failures)
        // abort via VK_CHECK.
        bool beginFrame();

        // End a frame: close dynamic rendering, transition the image to
        // PRESENT_SRC_KHR, submit, present. Must only be called after a
        // beginFrame() that returned true. Recreates the swapchain on
        // VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR from present.
        void endFrame();

        // Draw a solid-color axis-aligned rectangle in the coordinate space set
        // by setProjectionExtent (or pixel coordinates if it was never called),
        // with (0, 0) at the top-left. Must be called between a beginFrame that
        // returned true and the matching endFrame.
        void drawQuad(float x, float y, float w, float h, engine::Color color);

        // Draw a solid-color filled disc. (cx, cy) is the centre; radius is in
        // the same coordinate space as drawQuad. May be freely interleaved with
        // drawQuad calls in the same frame.
        void drawDisc(float cx, float cy, float radius, engine::Color color);

        [[nodiscard]] VkExtent2D viewportExtent() const noexcept
        {
            return extent_;
        }

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

        // (Re)builds the swapchain and its per-image views against the current
        // surface. Called once from bindSurface for the initial build; called
        // again from the resize / VK_ERROR_OUT_OF_DATE_KHR path later. The
        // `windowExtent` argument is the platform's current client-area size;
        // it's clamped against surfaceCaps.{min,max}ImageExtent, or ignored
        // entirely when surfaceCaps.currentExtent reports a definite size
        // (the usual Win32 case — surface dictates extent).
        std::expected<void, VkResult> recreateSwapchain_(VkExtent2D windowExtent);

        // Called once from bindSurface after the swapchain exists — the color
        // format is baked into the pipeline via dynamic rendering, so the
        // pipeline would need a rebuild if the format ever changed across a
        // recreate (not handled today; format is stable on typical Win32).
        std::expected<void, VkResult> buildQuadPipeline_();

        // Builds the disc pipeline. Reuses quadShaderModule_ and
        // quadPipelineLayout_; must be called after buildQuadPipeline_().
        std::expected<void, VkResult> buildDiscPipeline_();

        // Bind pipeline if not already active; set viewport+scissor dynamic state.
        void bindPipeline_(VkPipeline pipeline);

        VkInstance instance_ = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE; // VK_NULL_HANDLE in release
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE; // not owned (lifetime tied to instance)
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE; // not owned (lifetime tied to device)
        uint32_t graphicsQueueIdx_ = 0;
        VkExtent2D extent_{};
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
        std::vector<VkImage> swapchainImages_;         // not owned (lifetime tied to swapchain)
        std::vector<VkImageView> swapchainImageViews_; // owned, one per image
        // Single pool flagged RESET_COMMAND_BUFFER so each frame's buffer can be
        // reset independently when its fence signals. Pool-per-frame (with
        // vkResetCommandPool) is the textbook alternative — revisit if recording
        // cost ever shows up in profiles.
        VkCommandPool commandPool_ = VK_NULL_HANDLE;
        std::array<VkCommandBuffer, MAX_FRAMES_IN_FLIGHT> commandBuffers_{}; // not owned (freed with command pool)
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> fences{};
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAcquiredSemaphores{};
        // Indexed by swapchain image (not frame-in-flight): vkQueuePresentKHR
        // waits on the semaphore that the submit for *this* image signaled, and
        // frame-in-flight count doesn't match image count in general.
        std::vector<VkSemaphore> renderCompleteSemaphores;
        // Stateful clear color (see setClearColor). Black opaque is a safe
        // default — first frame with no game-side call still produces a valid
        // image rather than reading from an uninitialized union member.
        VkClearColorValue clearColor_{.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};
        // One shader module covers both vertMain and fragMain entry points;
        // the layout's single push-constant range is shared by both stages.
        VkShaderModule quadShaderModule_ = VK_NULL_HANDLE;
        VkPipelineLayout quadPipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline quadPipeline_ = VK_NULL_HANDLE;
        VkPipeline discPipeline_ = VK_NULL_HANDLE;
        // Coordinate-space -> Vulkan NDC projection. beginFrame resets it to
        // pixel space; setProjectionExtent overrides it before any drawQuad.
        glm::mat4 viewProj_{1.0f};
        // Tracks which pipeline is currently bound so drawQuad/drawDisc can
        // skip redundant vkCmdBindPipeline calls while handling interleaving.
        VkPipeline activePipeline_ = VK_NULL_HANDLE;
        // Which MAX_FRAMES_IN_FLIGHT slot is active this frame; rotates after
        // every successful present.
        uint32_t frameIndex_ = 0;
        // Swapchain image picked by the current beginFrame; only meaningful
        // between beginFrame and endFrame.
        uint32_t imageIndex_ = 0;
#ifdef ENGINE_TRACY_ENABLED
        TracyVkCtx tracyVkCtx_ = nullptr;
#endif
    };
} // namespace renderer
