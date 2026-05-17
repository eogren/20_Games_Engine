#pragma once

#include <glm/vec2.hpp>

namespace pong
{
    constexpr float kGameW = 800.0f;
    constexpr float kGameH = 600.0f;

    struct PaddleState
    {
        static constexpr float HEIGHT  = kGameH * 0.20f; // 120 px
        static constexpr float WIDTH   = kGameW * 0.02f; // 16 px
        static constexpr float VEL_MAX = kGameH * 0.20f; // 120 px/s

        float xPos    = 50.0f;
        float yPos    = 0.0f;
        float yPosPrev = 0.0f;
        float vel     = 0.0f; // set from input each tick, consumed by step_physics
    };

    struct BallState
    {
        static constexpr float RADIUS = 6.0f;
        static constexpr float SPEED  = 340.0f; // px/s, total magnitude

        glm::vec2 pos     = {};
        glm::vec2 posPrev = {};
        glm::vec2 vel     = {};

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

    struct GameState
    {
        PaddleState paddle{};
        BallState   ball{};
        bool        serveRight = true;
    };
} // namespace pong
