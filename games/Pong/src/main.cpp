// Pong — entry point.

#include "engine.h"
#include "game.h"
#include "log/log.h"
#include "platform/platform.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace
{
    constexpr float kGameW = 800.0f;
    constexpr float kGameH = 600.0f;

    struct PaddleState
    {
        static constexpr float HEIGHT = kGameH * 0.20f;
        static constexpr float WIDTH = kGameW * 0.02f;
        static constexpr float VEL_MAX = kGameH * 0.20f;

        float yPos;
    };

    class Pong : public engine::Game
    {
    public:
        Pong()
        {
            paddle_ = {.yPos = PaddleState::HEIGHT / 2 + 20};
        }

        void fixedUpdate(engine::FixedUpdateContext& ctx, float fixedDt) override
        {
            float vel = 0.0f;
            if (ctx.keyboard.pressed(platform::KeyCode::KeyW))
            {
                vel = -PaddleState::VEL_MAX;
            }
            if (ctx.keyboard.pressed(platform::KeyCode::KeyS))
            {
                vel += PaddleState::VEL_MAX;
            }
            paddle_.yPos = std::clamp(paddle_.yPos + vel * fixedDt, 0.0f, kGameH - PaddleState::HEIGHT);
        }

        void update(engine::GameContext& ctx, float /*dt*/) override
        {
            ctx.renderer.setProjectionExtent(kGameW, kGameH);
            ctx.renderer.setClearColor(clear_);

            constexpr engine::Color kWhite = engine::Color::rgb(1.0f, 1.0f, 1.0f);

            ctx.renderer.drawQuad(50.0f, paddle_.yPos, PaddleState::WIDTH, PaddleState::HEIGHT, kWhite);

            // Dashed center line — n dashes centered vertically so margins are equal.
            constexpr float kDashW = 4.0f;
            constexpr float kDashH = 20.0f;
            constexpr float kGap = 15.0f;
            constexpr float kPeriod = kDashH + kGap;
            const float cx = std::floor(kGameW * 0.5f - kDashW * 0.5f);
            const int n = static_cast<int>((kGameH + kGap) / kPeriod);
            const float startY = std::floor((kGameH - (n * kPeriod - kGap)) * 0.5f);
            for (int i = 0; i < n; ++i)
            {
                ctx.renderer.drawQuad(cx, startY + static_cast<float>(i) * kPeriod, kDashW, kDashH, kWhite);
            }
        }

    private:
        engine::Color clear_ = engine::Color::rgb(0.05f, 0.08f, 0.18f);
        PaddleState paddle_{};
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
