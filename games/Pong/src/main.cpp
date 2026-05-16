// Pong — entry point.

#include "engine.h"
#include "game.h"
#include "log/log.h"
#include "platform/platform.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace
{
    // Paddles, ball, scoring will land here as the engine substrate (drawing
    // API, audio) comes online. Today the game just owns its clear color and
    // brightens/dims it with W/S — an interactive demo that exercises input
    // and renderer access through GameContext end-to-end.
    class Pong : public engine::Game
    {
    public:
        void update(engine::GameContext& ctx, float dt) override
        {
            constexpr float kValueRate = 0.5f; // HSV value units per second held
            if (ctx.keyboard.pressed(platform::KeyCode::KeyW))
            {
                clear_ = clear_.with_value(std::min(clear_.value() + kValueRate * dt, 1.0f));
            }
            if (ctx.keyboard.pressed(platform::KeyCode::KeyS))
            {
                clear_ = clear_.with_value(std::max(clear_.value() - kValueRate * dt, 0.0f));
            }
            ctx.renderer.setClearColor(clear_);

            constexpr engine::Color kWhite = engine::Color::rgb(1.0f, 1.0f, 1.0f);

            // Placeholder paddle near the top-left.
            ctx.renderer.drawQuad(50.0f, 100.0f, 20.0f, 120.0f, kWhite);

            // Dashed center line — n dashes centered vertically so margins are equal.
            constexpr float kDashW = 4.0f;
            constexpr float kDashH = 20.0f;
            constexpr float kGap = 15.0f;
            constexpr float kPeriod = kDashH + kGap;
            const VkExtent2D vp = ctx.renderer.viewportExtent();
            const float vpW = static_cast<float>(vp.width);
            const float vpH = static_cast<float>(vp.height);
            const float cx = std::floor(vpW * 0.5f - kDashW * 0.5f);
            const int n = static_cast<int>((vpH + kGap) / kPeriod);
            const float startY = std::floor((vpH - (n * kPeriod - kGap)) * 0.5f);
            for (int i = 0; i < n; ++i)
            {
                ctx.renderer.drawQuad(cx, startY + static_cast<float>(i) * kPeriod, kDashW, kDashH, kWhite);
            }
        }

    private:
        // Dark navy starting state. The very first beginFrame uses the
        // renderer's default (black) since update runs after begin; from
        // frame 2 onward our setClearColor below is in effect.
        engine::Color clear_ = engine::Color::rgb(0.05f, 0.08f, 0.18f);
    };
} // namespace

int main()
{
    engine::log::init({.file_path = "pong.log"});
    spdlog::info("Pong - engine {}, platform {}", engine::version(), platform::version());

    platform::Platform platform{"Pong"};
    engine::Engine eng{platform};
    if (auto r = eng.initRenderer("Pong"); !r)
    {
        spdlog::error("Renderer init failed: {}", static_cast<int>(r.error()));
        return 1;
    }

    Pong pong;
    eng.run(pong);
    return 0;
}
