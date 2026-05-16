// Pong — entry point.

#include "engine.h"
#include "game.h"
#include "log/log.h"
#include "platform/platform.h"

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>
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
        float yPosPrev;
    };

    struct BallState
    {
        static constexpr float RADIUS = 6.0f;
        static constexpr float SPEED = 340.0f; // px/s, total magnitude

        glm::vec2 pos;     // centre
        glm::vec2 posPrev; // centre previous fixed tick
        glm::vec2 vel;

        // Launch at 30° from horizontal so bounces stay interesting.
        static constexpr float kCos30 = 0.866025f;
        static constexpr float kSin30 = 0.500000f;

        static BallState serve(bool toRight)
        {
            const glm::vec2 center{kGameW * 0.5f, kGameH * 0.5f};
            const float sx = SPEED * kCos30 * (toRight ? 1.0f : -1.0f);
            const float sy = SPEED * kSin30;
            return {.pos = center, .posPrev = center, .vel = {sx, sy}};
        }
    };

    class Pong : public engine::Game
    {
    public:
        Pong()
        {
            const float startY = PaddleState::HEIGHT / 2 + 20;
            paddle_ = {.yPos = startY, .yPosPrev = startY};
            ball_ = BallState::serve(true);
        }

        void fixedUpdate(engine::FixedUpdateContext& ctx, float fixedDt) override
        {
            // --- paddle ---
            float vel = 0.0f;
            if (ctx.keyboard.pressed(platform::KeyCode::KeyW)) vel = -PaddleState::VEL_MAX;
            if (ctx.keyboard.pressed(platform::KeyCode::KeyS)) vel += PaddleState::VEL_MAX;
            paddle_.yPosPrev = paddle_.yPos;
            paddle_.yPos = std::clamp(paddle_.yPos + vel * fixedDt, 0.0f, kGameH - PaddleState::HEIGHT);

            // --- ball ---
            ball_.posPrev = ball_.pos;
            ball_.pos += ball_.vel * fixedDt;

            // top wall
            if (ball_.pos.y - BallState::RADIUS < 0.0f)
            {
                ball_.pos.y = BallState::RADIUS + (BallState::RADIUS - ball_.pos.y);
                ball_.vel.y = std::abs(ball_.vel.y);
            }
            // bottom wall
            if (ball_.pos.y + BallState::RADIUS > kGameH)
            {
                ball_.pos.y = (kGameH - BallState::RADIUS) - (ball_.pos.y + BallState::RADIUS - kGameH);
                ball_.vel.y = -std::abs(ball_.vel.y);
            }

            // out of bounds → re-serve toward the side that just scored
            if (ball_.pos.x + BallState::RADIUS < 0.0f || ball_.pos.x - BallState::RADIUS > kGameW)
            {
                serveRight_ = !serveRight_;
                ball_ = BallState::serve(serveRight_);
            }
        }

        void update(engine::GameContext& ctx, float /*dt*/) override
        {
            ctx.renderer.setProjectionExtent(kGameW, kGameH);
            ctx.renderer.setClearColor(clear_);

            constexpr engine::Color kWhite = engine::Color::rgb(1.0f, 1.0f, 1.0f);

            // left paddle
            const float yRender = std::lerp(paddle_.yPosPrev, paddle_.yPos, ctx.alpha);
            ctx.renderer.drawQuad(50.0f, yRender, PaddleState::WIDTH, PaddleState::HEIGHT, kWhite);

            // ball
            const glm::vec2 bRender = glm::mix(ball_.posPrev, ball_.pos, ctx.alpha);
            ctx.renderer.drawDisc(bRender.x, bRender.y, BallState::RADIUS, kWhite);

            // dashed center line — n dashes centered vertically so margins are equal
            constexpr float kDashW = 4.0f;
            constexpr float kDashH = 20.0f;
            constexpr float kGap = 15.0f;
            constexpr float kPeriod = kDashH + kGap;
            const float lcx = std::floor(kGameW * 0.5f - kDashW * 0.5f);
            const int n = static_cast<int>((kGameH + kGap) / kPeriod);
            const float lineY0 = std::floor((kGameH - (n * kPeriod - kGap)) * 0.5f);
            for (int i = 0; i < n; ++i)
                ctx.renderer.drawQuad(lcx, lineY0 + static_cast<float>(i) * kPeriod, kDashW, kDashH, kWhite);
        }

    private:
        engine::Color clear_ = engine::Color::rgb(0.05f, 0.08f, 0.18f);
        PaddleState paddle_{};
        BallState ball_{};
        bool serveRight_ = true;
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
