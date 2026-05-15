#include "engine.h"

#include "platform/platform.h"
#include "platform/surface.h"

namespace engine
{

    const char* version()
    {
        return "0.0.1";
    }

    std::expected<void, VkResult> Engine::initRenderer(const std::string& appName)
    {
        // Each intermediate is a local: RAII unwinds cleanly on early return.
        // RendererInstance's dtor destroys the VkInstance; RendererInstance::
        // bindSurface either consumes the surface into the Renderer or
        // destroys it on failure, so we never have to vkDestroySurfaceKHR
        // by hand here.
        auto rInst = renderer::RendererInstance::create(appName, platform::requiredInstanceExtensions());
        if (!rInst) return std::unexpected(rInst.error());

        auto surface = platform::createSurface(rInst->instance(), platform_);
        if (!surface) return std::unexpected(surface.error());

        const VkExtent2D extent = platform::framebufferExtent(platform_);

        auto rend = std::move(*rInst).bindSurface(*surface, extent);
        if (!rend) return std::unexpected(rend.error());

        renderer_.emplace(std::move(*rend));
        return {};
    }

    void Engine::run()
    {
        // Precondition: initRenderer() succeeded. The explicit has_value
        // check pins optional access into a single proven-engaged site so
        // the loop body operates on a plain reference.
        if (!renderer_.has_value()) return;
        auto& r = *renderer_;
        // Has to land after initRenderer (gated above) and before the loop's
        // first beginFrame so the user's first sight of the client area is the
        // cleared first frame, not the empty pre-init fill.
        platform_.show();
        while (!platform_.shouldClose())
        {
            platform_.pollEvents();
            // game_.update(ctx, dt) and input_.endFrame() land between begin
            // and end as those layers come online (CLAUDE.md > "Frame ordering").
            // beginFrame returns false when it had to recreate the swapchain
            // this tick — skip endFrame and re-enter the loop so the platform
            // can drain whatever events drove the resize before we try again.
            if (r.beginFrame()) r.endFrame();
        }
    }

} // namespace engine
