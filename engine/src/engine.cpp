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
        // right choice for a frame timer.
        using Clock = std::chrono::steady_clock;
        auto prev = Clock::now();
        float accumulator = 0.0f;
        // 60 Hz fixed step matches Bevy's FixedUpdate default cadence.
        static constexpr float kFixedDt = 1.0f / 60.0f;

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
            // this tick — skip simulation and endFrame and re-enter the loop
            // so the platform can drain whatever events drove the resize.
            // Keeping time measurement inside this block means resize frames
            // don't accumulate phantom time that would cause a burst of fixed
            // steps on the next real frame.
            if (r.beginFrame())
            {
                const auto now = Clock::now();
                const float elapsed = std::chrono::duration<float>(now - prev).count();
                prev = now;
                // Cap prevents a spiral of death after a long stall (e.g. a
                // debugger break or OS suspend) from triggering a burst of
                // catch-up steps that locks the frame for another long stall.
                accumulator += std::min(elapsed, kFixedDt * 4.0f);

                while (accumulator >= kFixedDt)
                {
                    FixedUpdateContext fctx{.keyboard = input_state_.keyboard};
                    game.fixedUpdate(fctx, kFixedDt);
                    accumulator -= kFixedDt;
                }

                const float alpha = accumulator / kFixedDt;
                GameContext ctx{.keyboard = input_state_.keyboard, .renderer = r, .alpha = alpha};
                game.update(ctx, elapsed);
                r.endFrame();
            }
        }
    }

} // namespace engine
