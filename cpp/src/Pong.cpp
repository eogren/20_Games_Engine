#include "Pong.h"

constexpr double SIM_TICK = 1.0 / 60.0;
// Cap a single frame's dt so a stall (pause/resume, debugger, GPU hitch) can't
// queue an unbounded burst of catch-up steps and spiral.
constexpr double MAX_FRAME_DT = 0.25;

// Pong's world coordinate system: 320x240 with origin bottom-left, matching
// the offscreen render-target dimensions (1 world unit = 1 offscreen pixel).
// The renderer doesn't dictate units — this is a Pong-side choice.
constexpr float WORLD_WIDTH = 320.0f;
constexpr float WORLD_HEIGHT = 240.0f;

Pong::Pong(MTL::Device* device) : renderer_(device), time_(0.0), start_time_(0.0), accumulator_(0.0), seeded_(false) {}

namespace
{
    void drawCenterLine(Renderer& renderer)
    {
        constexpr float midX = WORLD_WIDTH / 2.0f;
        constexpr float dashHalfW = 3.0f;
        constexpr float dashHalfH = 5.0f;
        constexpr float gap = 6.0f;
        constexpr float stride = 2 * dashHalfH + gap;

        // Fit as many dashes as will leave at least `gap` of padding on each
        // end, then center the resulting block so top/bottom gaps are equal.
        constexpr int count = static_cast<int>((WORLD_HEIGHT - gap) / stride);
        constexpr float blockHeight = count * (2 * dashHalfH) + (count - 1) * gap;
        constexpr float firstCenterY = (WORLD_HEIGHT - blockHeight) / 2.0f + dashHalfH;

        for (int i = 0; i < count; ++i)
        {
            const float y = firstCenterY + i * stride;
            renderer.drawRect({midX, y}, {dashHalfW, dashHalfH}, {1, 1, 1, 1});
        }
    }
} // namespace

void Pong::tick(CA::MetalDrawable* drawable, double targetTimestamp)
{
    if (!seeded_)
    {
        time_ = targetTimestamp;
        start_time_ = targetTimestamp;
        seeded_ = true;
    }

    auto dt = targetTimestamp - time_;
    if (dt > MAX_FRAME_DT) dt = MAX_FRAME_DT;
    accumulator_ += dt;
    time_ = targetTimestamp;

    while (accumulator_ >= SIM_TICK)
    {
        do_step(SIM_TICK);
        accumulator_ -= SIM_TICK;
    }

    OrthoView2D view{.left = 0.0f, .right = WORLD_WIDTH, .bottom = 0.0f, .top = WORLD_HEIGHT};
    auto time = static_cast<float>(time_ - start_time_);

    renderer_.beginFrame(drawable, view, time);
    drawCenterLine(renderer_);
    renderer_.endFrame();
}

void Pong::do_step(double dt)
{
    // nothing to do yet
}
