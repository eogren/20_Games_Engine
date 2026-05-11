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
    renderer_.drawRect({160, 120}, {40, 20}, {1, 1, 1, 1});
    renderer_.endFrame();
}

void Pong::do_step(double dt)
{
    // nothing to do yet
}
