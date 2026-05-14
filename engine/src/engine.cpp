#include "engine.h"

namespace engine
{

    const char* version()
    {
        return "0.0.1";
    }

    std::expected<VkInstance, VkResult> Engine::initRenderer(const std::string& appName,
                                                             std::span<const char* const> platformExtensions)
    {
        auto r = renderer::RendererInstance::create(appName, platformExtensions);
        if (!r) return std::unexpected(r.error());
        rendererInstance_.emplace(std::move(*r));
        return rendererInstance_->instance();
    }

    std::expected<void, VkResult> Engine::bindSurface(VkSurfaceKHR surface, VkExtent2D extent)
    {
        auto result = std::move(*rendererInstance_).bindSurface(surface, extent);
        // RendererInstance is consumed regardless. On success bindSurface
        // disarmed it; on failure its dtor still owns the VkInstance and
        // reset() destroys it correctly.
        rendererInstance_.reset();
        if (!result) return std::unexpected(result.error());
        renderer_.emplace(std::move(*result));
        return {};
    }

} // namespace engine
