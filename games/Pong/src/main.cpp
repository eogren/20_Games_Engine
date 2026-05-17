// Pong — entry point.

#include "engine.h"
#include "game.h"
#include "log/log.h"
#include "physics.h"
#include "platform/platform.h"
#include "pong_state.h"

#include <cmath>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

namespace
{
    class Pong : public engine::Game
    {
    public:
        Pong()
        {
            const float startY = pong::PaddleState::HEIGHT / 2 + 20;
            gs_.paddle = {.yPos = startY, .yPosPrev = startY};
            gs_.ball = pong::BallState::serve(true);
        }

        void fixedUpdate(engine::FixedUpdateContext& ctx, float fixedDt) override
        {
            gs_.paddle.vel = 0.0f;
            if (ctx.keyboard.pressed(platform::KeyCode::KeyW)) gs_.paddle.vel = -pong::PaddleState::VEL_MAX;
            if (ctx.keyboard.pressed(platform::KeyCode::KeyS)) gs_.paddle.vel += pong::PaddleState::VEL_MAX;

            pong::step_physics(gs_, fixedDt);
        }

        void update(engine::GameContext& ctx, float /*dt*/) override
        {
            ctx.renderer.setProjectionExtent(pong::kGameW, pong::kGameH);
            ctx.renderer.setClearColor(clear_);

            constexpr engine::Color kWhite = engine::Color::rgb(1.0f, 1.0f, 1.0f);

            // left paddle
            const float yRender = std::lerp(gs_.paddle.yPosPrev, gs_.paddle.yPos, ctx.alpha);
            ctx.renderer.drawQuad(gs_.paddle.xPos, yRender, pong::PaddleState::WIDTH, pong::PaddleState::HEIGHT,
                                  kWhite);

            // ball
            const glm::vec2 bRender = glm::mix(gs_.ball.posPrev, gs_.ball.pos, ctx.alpha);
            ctx.renderer.drawDisc(bRender.x, bRender.y, pong::BallState::RADIUS, kWhite);

            // dashed center line — n dashes centered vertically so margins are equal
            constexpr float kDashW = 4.0f;
            constexpr float kDashH = 20.0f;
            constexpr float kGap = 15.0f;
            constexpr float kPeriod = kDashH + kGap;
            const float lcx = std::floor(pong::kGameW * 0.5f - kDashW * 0.5f);
            const int n = static_cast<int>((pong::kGameH + kGap) / kPeriod);
            const float lineY0 = std::floor((pong::kGameH - (n * kPeriod - kGap)) * 0.5f);
            for (int i = 0; i < n; ++i)
                ctx.renderer.drawQuad(lcx, lineY0 + static_cast<float>(i) * kPeriod, kDashW, kDashH, kWhite);
        }

    private:
        engine::Color clear_ = engine::Color::rgb(0.05f, 0.08f, 0.18f);
        pong::GameState gs_{};
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
