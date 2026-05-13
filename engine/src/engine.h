#pragma once

#include "renderer/renderer.h"

#include <expected>
#include <optional>
#include <span>
#include <string>

namespace engine
{

    // Returns a static string with the engine version. Placeholder API while
    // the engine is being scaffolded; will get replaced by real surface area.
    const char* version();

    // Two-phase Vulkan init mirrors the type-state in renderer/:
    //
    //   1. initRenderer(appName, extensions)  -> VkInstance handle
    //   2. platform creates VkSurfaceKHR using that handle
    //   3. bindSurface(surface, extent)       -> device + swapchain + ...
    //
    // initRenderer hands back the VkInstance directly (rather than a getter)
    // so the "valid only between init and bindSurface" window doesn't need a
    // sentinel — the caller has the handle exactly when it can use it.
    //
    // See CLAUDE.md > "Renderer design" / "Layer split".
    class Engine
    {
    public:
        Engine() = default;

        // Phase 1: brings up the loader + VkInstance. The returned VkInstance
        // is owned by the engine; the caller is expected to use it to create
        // a VkSurfaceKHR and pass that to bindSurface(). Don't store it past
        // the bindSurface() call — after that point ownership moves on.
        std::expected<VkInstance, VkResult> initRenderer(const std::string& appName,
                                                         std::span<const char* const> platformExtensions);

        // Phase 2: consume the RendererInstance, build a Renderer bound to `surface`.
        // On failure both phases tear down — caller retries from initRenderer().
        std::expected<void, VkResult> bindSurface(VkSurfaceKHR surface, VkExtent2D extent);

        // Valid only after bindSurface() succeeds.
        renderer::Renderer& renderer() { return *renderer_; }
        const renderer::Renderer& renderer() const { return *renderer_; }

    private:
        // Exactly one of these is engaged at a time after initRenderer():
        //   - between initRenderer() and bindSurface(): rendererInstance_.
        //   - after a successful bindSurface(): renderer_.
        std::optional<renderer::RendererInstance> rendererInstance_;
        std::optional<renderer::Renderer> renderer_;
    };

} // namespace engine
