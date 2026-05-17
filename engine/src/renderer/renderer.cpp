// Renderer class lifetime + per-frame surface.
//
// One-time setup (RendererInstance, bindSurface, swapchain recreate) lives
// in renderer_init.cpp; this file owns the Renderer's move/dtor/destroy_
// plus the beginFrame/endFrame API (and forthcoming draw API).

#include "renderer/renderer.h"

#include "profiling.h"
#include "renderer/quad_push_constants.h"

#include <cstdint>
#include <utility>

#include <glm/ext/matrix_clip_space.hpp>
#include <spdlog/spdlog.h>

#include "volk.h"
#include "vk_check.h"

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
        if (discPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, discPipeline_, nullptr);
        if (quadPipeline_ != VK_NULL_HANDLE) vkDestroyPipeline(device_, quadPipeline_, nullptr);
        if (quadPipelineLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, quadPipelineLayout_, nullptr);
        if (quadShaderModule_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, quadShaderModule_, nullptr);
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
#ifdef ENGINE_TRACY_ENABLED
        if (tracyVkCtx_)
        {
            TracyVkDestroy(tracyVkCtx_);
            tracyVkCtx_ = nullptr;
        }
#endif
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
        quadShaderModule_ = VK_NULL_HANDLE;
        quadPipelineLayout_ = VK_NULL_HANDLE;
        quadPipeline_ = VK_NULL_HANDLE;
        discPipeline_ = VK_NULL_HANDLE;
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
          renderCompleteSemaphores(std::move(other.renderCompleteSemaphores)), clearColor_(other.clearColor_),
          quadShaderModule_(other.quadShaderModule_), quadPipelineLayout_(other.quadPipelineLayout_),
          quadPipeline_(other.quadPipeline_), discPipeline_(other.discPipeline_), viewProj_(other.viewProj_),
          activePipeline_(other.activePipeline_), frameIndex_(other.frameIndex_), imageIndex_(other.imageIndex_)
#ifdef ENGINE_TRACY_ENABLED
          ,
          tracyVkCtx_(other.tracyVkCtx_)
#endif
    {
        other.instance_ = VK_NULL_HANDLE;
        other.debugMessenger_ = VK_NULL_HANDLE;
        other.surface_ = VK_NULL_HANDLE;
        other.physicalDevice_ = VK_NULL_HANDLE;
        other.device_ = VK_NULL_HANDLE;
        other.graphicsQueue_ = VK_NULL_HANDLE;
        other.swapchain_ = VK_NULL_HANDLE;
        other.commandPool_ = VK_NULL_HANDLE;
        other.quadShaderModule_ = VK_NULL_HANDLE;
        other.quadPipelineLayout_ = VK_NULL_HANDLE;
        other.quadPipeline_ = VK_NULL_HANDLE;
        other.discPipeline_ = VK_NULL_HANDLE;
        other.commandBuffers_.fill(VK_NULL_HANDLE);
        other.fences.fill(VK_NULL_HANDLE);
        other.imageAcquiredSemaphores.fill(VK_NULL_HANDLE);
        // swapchainImages_ / swapchainImageViews_ / renderCompleteSemaphores: vector move leaves source empty.
#ifdef ENGINE_TRACY_ENABLED
        other.tracyVkCtx_ = nullptr;
#endif
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
            clearColor_ = other.clearColor_;
            quadShaderModule_ = other.quadShaderModule_;
            quadPipelineLayout_ = other.quadPipelineLayout_;
            quadPipeline_ = other.quadPipeline_;
            discPipeline_ = other.discPipeline_;
            viewProj_ = other.viewProj_;
            activePipeline_ = other.activePipeline_;
            frameIndex_ = other.frameIndex_;
            imageIndex_ = other.imageIndex_;
#ifdef ENGINE_TRACY_ENABLED
            tracyVkCtx_ = other.tracyVkCtx_;
            other.tracyVkCtx_ = nullptr;
#endif
            other.instance_ = VK_NULL_HANDLE;
            other.debugMessenger_ = VK_NULL_HANDLE;
            other.surface_ = VK_NULL_HANDLE;
            other.physicalDevice_ = VK_NULL_HANDLE;
            other.device_ = VK_NULL_HANDLE;
            other.graphicsQueue_ = VK_NULL_HANDLE;
            other.swapchain_ = VK_NULL_HANDLE;
            other.commandPool_ = VK_NULL_HANDLE;
            other.quadShaderModule_ = VK_NULL_HANDLE;
            other.quadPipelineLayout_ = VK_NULL_HANDLE;
            other.quadPipeline_ = VK_NULL_HANDLE;
            other.discPipeline_ = VK_NULL_HANDLE;
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

    void Renderer::setClearColor(engine::Color color) noexcept
    {
        const auto rgba = color.floats();
        clearColor_.float32[0] = rgba[0];
        clearColor_.float32[1] = rgba[1];
        clearColor_.float32[2] = rgba[2];
        clearColor_.float32[3] = rgba[3];
    }

    void Renderer::setProjectionExtent(float w, float h) noexcept
    {
        viewProj_ = glm::orthoRH_ZO(0.0f, w, 0.0f, h, 0.0f, 1.0f);
    }

    bool Renderer::beginFrame()
    {
        ZoneScoped;
        // pixel (0, 0) -> NDC (-1, -1) (top-left in Vulkan's Y-down NDC);
        // pixel (w, h) -> NDC (1, 1) (bottom-right).
        viewProj_ = glm::orthoRH_ZO(0.0f, static_cast<float>(extent_.width), 0.0f, static_cast<float>(extent_.height),
                                    0.0f, 1.0f);
        activePipeline_ = VK_NULL_HANDLE;

        // Wait for the previous submit using this slot's resources to retire.
        // The fence was created SIGNALED in createFrameSync, so frame 0
        // returns immediately on the first call.
        VK_CHECK(vkWaitForFences(device_, 1, &fences[frameIndex_], VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        const VkResult acqRes = vkAcquireNextImageKHR(
            device_, swapchain_, UINT64_MAX, imageAcquiredSemaphores[frameIndex_], VK_NULL_HANDLE, &imageIndex);
        if (acqRes == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // Don't reset the fence — nothing was submitted that would re-signal
            // it, so leaving it signaled keeps next frame's wait honest.
            if (auto r = recreateSwapchain_(extent_); !r)
            {
                spdlog::error("[vulkan] swapchain recreate after OUT_OF_DATE on acquire failed: {}",
                              vkResultString(r.error()));
            }
            return false;
        }
        // SUBOPTIMAL_KHR is a success — the semaphore got signaled and we can
        // still render this frame. Let the present-side check trigger recreate.
        if (acqRes != VK_SUCCESS && acqRes != VK_SUBOPTIMAL_KHR) VK_CHECK(acqRes);
        imageIndex_ = imageIndex;

        // Only reset the fence after acquire commits — otherwise an early
        // OUT_OF_DATE bail would leave us with an unsignaled fence and no
        // submission coming to signal it, deadlocking the next wait.
        VK_CHECK(vkResetFences(device_, 1, &fences[frameIndex_]));

        const VkCommandBuffer cb = commandBuffers_[frameIndex_];
        VK_CHECK(vkResetCommandBuffer(cb, 0));
        const VkCommandBufferBeginInfo bi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        VK_CHECK(vkBeginCommandBuffer(cb, &bi));
#ifdef ENGINE_TRACY_ENABLED
        TracyVkCollect(tracyVkCtx_, cb);
#endif

        // UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL. UNDEFINED on the old layout is
        // fine because LOAD_OP_CLEAR discards prior contents — we don't need to
        // preserve whatever the swapchain image held last time around.
        const VkImageMemoryBarrier2 toColor{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages_[imageIndex_],
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        const VkDependencyInfo depBegin{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toColor,
        };
        vkCmdPipelineBarrier2(cb, &depBegin);

        const VkRenderingAttachmentInfo colorAttachment{
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = swapchainImageViews_[imageIndex_],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_NONE,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = clearColor_},
        };
        const VkRenderingInfo ri{
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{0, 0}, extent_},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
        };
        vkCmdBeginRendering(cb, &ri);
        return true;
    }

    void Renderer::bindPipeline_(VkPipeline pipeline)
    {
        if (activePipeline_ == pipeline) return;
        const VkCommandBuffer cb = commandBuffers_[frameIndex_];
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        // Viewport and scissor are dynamic state — set them on every pipeline
        // switch so each pipeline's first draw sees valid values.
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(extent_.width),
            .height = static_cast<float>(extent_.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cb, 0, 1, &viewport);
        const VkRect2D scissor{{0, 0}, extent_};
        vkCmdSetScissor(cb, 0, 1, &scissor);
        activePipeline_ = pipeline;
    }

    void Renderer::drawQuad(float x, float y, float w, float h, engine::Color color)
    {
        const VkCommandBuffer cb = commandBuffers_[frameIndex_];
#ifdef ENGINE_TRACY_ENABLED
        TracyVkZone(tracyVkCtx_, cb, "drawQuad");
#endif
        bindPipeline_(quadPipeline_);
        const auto rgba = color.floats();
        const QuadPushConstants pc{
            .viewProj = viewProj_,
            .rect = {x, y, w, h},
            .color = {rgba[0], rgba[1], rgba[2], rgba[3]},
        };
        vkCmdPushConstants(cb, quadPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(QuadPushConstants), &pc);
        vkCmdDraw(cb, 6, 1, 0, 0);
    }

    void Renderer::drawDisc(float cx, float cy, float radius, engine::Color color)
    {
        const VkCommandBuffer cb = commandBuffers_[frameIndex_];
#ifdef ENGINE_TRACY_ENABLED
        TracyVkZone(tracyVkCtx_, cb, "drawDisc");
#endif
        bindPipeline_(discPipeline_);
        const auto rgba = color.floats();
        const QuadPushConstants pc{
            .viewProj = viewProj_,
            .rect = {cx - radius, cy - radius, radius * 2.0f, radius * 2.0f},
            .color = {rgba[0], rgba[1], rgba[2], rgba[3]},
        };
        vkCmdPushConstants(cb, quadPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(QuadPushConstants), &pc);
        vkCmdDraw(cb, 6, 1, 0, 0);
    }

    void Renderer::endFrame()
    {
        ZoneScoped;
        const VkCommandBuffer cb = commandBuffers_[frameIndex_];

        vkCmdEndRendering(cb);

        // COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR. dst stage NONE (paired
        // with a semaphore signal anyway) is the sync2-idiomatic way to say
        // "the present engine takes it from here."
        const VkImageMemoryBarrier2 toPresent{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages_[imageIndex_],
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        const VkDependencyInfo depEnd{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &toPresent,
        };
        vkCmdPipelineBarrier2(cb, &depEnd);

        VK_CHECK(vkEndCommandBuffer(cb));

        const VkSemaphoreSubmitInfo waitSem{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = imageAcquiredSemaphores[frameIndex_],
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        const VkSemaphoreSubmitInfo signalSem{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = renderCompleteSemaphores[imageIndex_],
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        };
        const VkCommandBufferSubmitInfo cbi{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cb,
        };
        const VkSubmitInfo2 si{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSem,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cbi,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSem,
        };
        VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &si, fences[frameIndex_]));

        const VkPresentInfoKHR pi{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderCompleteSemaphores[imageIndex_],
            .swapchainCount = 1,
            .pSwapchains = &swapchain_,
            .pImageIndices = &imageIndex_,
        };
        const VkResult presRes = vkQueuePresentKHR(graphicsQueue_, &pi);
        if (presRes == VK_ERROR_OUT_OF_DATE_KHR || presRes == VK_SUBOPTIMAL_KHR)
        {
            if (auto r = recreateSwapchain_(extent_); !r)
            {
                spdlog::error("[vulkan] swapchain recreate after {} on present failed: {}", vkResultString(presRes),
                              vkResultString(r.error()));
            }
        }
        else if (presRes != VK_SUCCESS)
        {
            VK_CHECK(presRes);
        }

        frameIndex_ = (frameIndex_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }
} // namespace renderer
