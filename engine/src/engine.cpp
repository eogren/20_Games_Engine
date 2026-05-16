#include "engine.h"

#include "platform/platform.h"
#include "platform/surface.h"

#include <chrono>

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

    void Engine::run(Game& game)
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

        // steady_clock is monotonic, immune to wall-clock adjustments — the
        // right choice for a frame timer. First-tick dt is 0.0 by convention
        // (no "previous frame" yet to measure against).
        using Clock = std::chrono::steady_clock;
        auto prev = Clock::now();
        float dt = 0.0f;

        while (!platform_.shouldClose())
        {
            platform_.pollEvents();
            // Fold a fresh snapshot of currently-held keys into the engine's
            // keyboard state. Runs after pollEvents so the underlying input
            // source (GameInput on Win32) sees any state transitions the OS
            // just delivered, and before game.update so the game sees this
            // frame's edges.
            input_state_.keyboard.update(platform_.pollPressedKeys());

            // beginFrame returns false when it had to recreate the swapchain
            // this tick — skip game.update and endFrame and re-enter the loop
            // so the platform can drain whatever events drove the resize.
            // Skipping the game tick on a swapchain rebuild keeps dt's
            // backing measurement tied to ticks the game actually saw.
            if (r.beginFrame())
            {
                GameContext ctx{.keyboard = input_state_.keyboard, .renderer = r};
                game.update(ctx, dt);
                r.endFrame();

                const auto now = Clock::now();
                dt = std::chrono::duration<float>(now - prev).count();
                prev = now;
            }
        }
    }

} // namespace engine
