#include "engine.h"

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

} // namespace engine
