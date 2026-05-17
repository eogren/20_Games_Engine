#include "physics.h"

#include <algorithm>
#include <cmath>

namespace pong
{
    void step_physics(GameState& gs, float dt)
    {
        // --- paddle ---
        gs.paddle.yPosPrev = gs.paddle.yPos;
        gs.paddle.yPos = std::clamp(gs.paddle.yPos + gs.paddle.vel * dt,
                                    0.0f, kGameH - PaddleState::HEIGHT);

        // --- ball: integrate ---
        gs.ball.posPrev = gs.ball.pos;
        gs.ball.pos += gs.ball.vel * dt;

        // top wall
        if (gs.ball.pos.y - BallState::RADIUS < 0.0f)
        {
            gs.ball.pos.y = BallState::RADIUS + (BallState::RADIUS - gs.ball.pos.y);
            gs.ball.vel.y = std::abs(gs.ball.vel.y);
        }
        // bottom wall
        if (gs.ball.pos.y + BallState::RADIUS > kGameH)
        {
            gs.ball.pos.y = (kGameH - BallState::RADIUS)
                            - (gs.ball.pos.y + BallState::RADIUS - kGameH);
            gs.ball.vel.y = -std::abs(gs.ball.vel.y);
        }

        // left paddle — ball hits the right face
        {
            const float faceX = gs.paddle.xPos + PaddleState::WIDTH;
            if (gs.ball.vel.x < 0.0f
                && gs.ball.pos.x - BallState::RADIUS <= faceX
                && gs.ball.pos.x >= gs.paddle.xPos
                && gs.ball.pos.y + BallState::RADIUS >= gs.paddle.yPos
                && gs.ball.pos.y - BallState::RADIUS <= gs.paddle.yPos + PaddleState::HEIGHT)
            {
                gs.ball.pos.x = (faceX + BallState::RADIUS)
                                + ((faceX + BallState::RADIUS) - gs.ball.pos.x);
                gs.ball.vel.x = std::abs(gs.ball.vel.x);
            }
        }

        // out of bounds → re-serve toward the side that just scored
        if (gs.ball.pos.x + BallState::RADIUS < 0.0f
            || gs.ball.pos.x - BallState::RADIUS > kGameW)
        {
            gs.serveRight = !gs.serveRight;
            gs.ball = BallState::serve(gs.serveRight);
        }
    }
} // namespace pong
