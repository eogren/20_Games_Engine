#pragma once

#include "renderer/renderer.h"

#include <expected>
#include <optional>
#include <string>

namespace platform
{
    class Platform;
}

namespace engine
{

    // Returns a static string with the engine version. Placeholder API while
    // the engine is being scaffolded; will get replaced by real surface area.
    const char* version();

    // Vulkan init is one phase from the caller's perspective: hand Engine a
    // Platform at construction, then call initRenderer(). Internally that
    // creates the VkInstance, asks platform for a surface against it, asks
    // platform for the framebuffer extent, and consumes both into a Renderer.
    //
    // The renderer/ tree still has a RendererInstance -> Renderer type-state
    // (queue-family picking needs the surface), but that split is internal
    // to engine.cpp and doesn't leak through this header anymore.
    //
    // See CLAUDE.md > "Renderer design" / "Layer split".
    class Engine
    {
    public:
        explicit Engine(platform::Platform& platform) noexcept : platform_(platform) {}

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        std::expected<void, VkResult> initRenderer(const std::string& appName);

        // Valid only after initRenderer() succeeds.
        renderer::Renderer& renderer()
        {
            return *renderer_;
        }
        const renderer::Renderer& renderer() const
        {
            return *renderer_;
        }

    private:
        platform::Platform& platform_;
        std::optional<renderer::Renderer> renderer_;
    };

} // namespace engine
