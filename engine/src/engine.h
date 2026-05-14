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

    class Engine
    {
    public:
        explicit Engine(platform::Platform& platform) noexcept : platform_(platform) {}

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&) = delete;
        Engine& operator=(Engine&&) = delete;

        std::expected<void, VkResult> initRenderer(const std::string& appName);

        // Per-frame loop. Returns when the platform reports it's time to exit.
        // Frame ordering (see CLAUDE.md > "Frame ordering"): pollEvents at the
        // top; renderer begin/end and game/input land as those layers do.
        void run();

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
