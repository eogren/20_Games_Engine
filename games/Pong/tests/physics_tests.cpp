#include "doctest.h"
#include "physics.h"
#include "pong_state.h"

using doctest::Approx;
using namespace pong;

// Returns a GameState with the ball placed at the given position/velocity and
// the paddle parked safely out of the way (far bottom) so it doesn't interfere.
static GameState ballOnly(glm::vec2 pos, glm::vec2 vel)
{
    GameState gs;
    gs.ball.pos = pos;
    gs.ball.posPrev = pos;
    gs.ball.vel = vel;
    // Park paddle below the screen so it never interferes with ball-only tests.
    gs.paddle.yPos = kGameH + 100.0f;
    gs.paddle.yPosPrev = kGameH + 100.0f;
    return gs;
}

TEST_CASE("ball integrates position")
{
    GameState gs = ballOnly({400.0f, 300.0f}, {100.0f, 50.0f});
    step_physics(gs, 1.0f);
    CHECK(gs.ball.pos.x == Approx(500.0f));
    CHECK(gs.ball.pos.y == Approx(350.0f));
    CHECK(gs.ball.posPrev.x == Approx(400.0f));
    CHECK(gs.ball.posPrev.y == Approx(300.0f));
}

TEST_CASE("ball bounces off top wall")
{
    // Ball 3 px above top wall (centre at y=3, radius=6 → left edge at -3).
    GameState gs = ballOnly({400.0f, 3.0f}, {0.0f, -100.0f});
    step_physics(gs, 0.0f);                 // dt=0: no movement, pure collision response
    CHECK(gs.ball.vel.y == Approx(100.0f)); // flipped positive
    CHECK(gs.ball.pos.y >= Approx(BallState::RADIUS));
}

TEST_CASE("ball bounces off bottom wall")
{
    const float bottomEdge = kGameH - BallState::RADIUS;
    GameState gs = ballOnly({400.0f, bottomEdge + 1.0f}, {0.0f, 100.0f});
    step_physics(gs, 0.0f);
    CHECK(gs.ball.vel.y == Approx(-100.0f)); // flipped negative
    CHECK(gs.ball.pos.y <= Approx(bottomEdge));
}

TEST_CASE("ball bounces off left paddle face")
{
    // Place ball just overlapping the paddle's right face.
    const float faceX = 50.0f + PaddleState::WIDTH;       // 66 px
    const float ballX = faceX + BallState::RADIUS - 2.0f; // 2 px into face
    const float paddleY = 200.0f;

    GameState gs = ballOnly({ballX, paddleY + PaddleState::HEIGHT * 0.5f}, {-200.0f, 0.0f});
    gs.paddle.xPos = 50.0f;
    gs.paddle.yPos = paddleY;
    gs.paddle.yPosPrev = paddleY;

    step_physics(gs, 0.0f);
    CHECK(gs.ball.vel.x == Approx(200.0f)); // reflected rightward
    CHECK(gs.ball.pos.x >= Approx(faceX + BallState::RADIUS));
}

TEST_CASE("ball misses paddle vertically — no bounce")
{
    // Ball is at paddle x but above the paddle entirely.
    const float faceX = 50.0f + PaddleState::WIDTH;
    const float ballX = faceX + BallState::RADIUS - 2.0f;

    GameState gs = ballOnly({ballX, -50.0f}, {-200.0f, 0.0f});
    gs.paddle.xPos = 50.0f;
    gs.paddle.yPos = 200.0f;
    gs.paddle.yPosPrev = 200.0f;

    step_physics(gs, 0.0f);
    CHECK(gs.ball.vel.x == Approx(-200.0f)); // unchanged
}

TEST_CASE("ball re-serves when it exits left")
{
    GameState gs = ballOnly({-BallState::RADIUS - 1.0f, 300.0f}, {-100.0f, 0.0f});
    gs.serveRight = true;
    step_physics(gs, 0.0f);
    CHECK(gs.serveRight == false);
    CHECK(gs.ball.pos.x == Approx(kGameW * 0.5f));
}

TEST_CASE("ball re-serves when it exits right")
{
    GameState gs = ballOnly({kGameW + BallState::RADIUS + 1.0f, 300.0f}, {100.0f, 0.0f});
    gs.serveRight = false;
    step_physics(gs, 0.0f);
    CHECK(gs.serveRight == true);
    CHECK(gs.ball.pos.x == Approx(kGameW * 0.5f));
}

TEST_CASE("paddle moves and clamps to bounds")
{
    GameState gs;
    gs.paddle.yPos = 0.0f;
    gs.paddle.yPosPrev = 0.0f;

    gs.paddle.vel = -PaddleState::VEL_MAX; // try to move above top
    step_physics(gs, 1.0f);
    CHECK(gs.paddle.yPos == Approx(0.0f)); // clamped at 0

    gs.paddle.vel = PaddleState::VEL_MAX * 10.0f; // try to move far below bottom
    step_physics(gs, 1.0f);
    CHECK(gs.paddle.yPos == Approx(kGameH - PaddleState::HEIGHT));
}
