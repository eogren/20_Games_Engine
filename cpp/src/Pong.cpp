#include "Pong.h"

constexpr double SIM_TICK = 1.0 / 60.0;
// Cap a single frame's dt so a stall (pause/resume, debugger, GPU hitch) can't
// queue an unbounded burst of catch-up steps and spiral.
constexpr double MAX_FRAME_DT = 0.25;

Pong::Pong(MTL::Device* device) : renderer_(device), time_(0.0), accumulator_(0.0), seeded_(false) {}

void Pong::tick(CA::MetalDrawable* drawable, double targetTimestamp)
{
    if (!seeded_)
    {
        time_ = targetTimestamp;
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

    renderer_.beginFrame(drawable);
    // Game-side draws will go here once Pong has anything to draw.
    renderer_.endFrame();
}

void Pong::do_step(double dt)
{
    // nothing to do yet
}
