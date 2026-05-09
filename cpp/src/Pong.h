#pragma once
#include "Renderer.h"

class Pong
{
public:
    explicit Pong(MTL::Device* device);

    void tick(CA::MetalDrawable* drawable, double targetTimestamp);

private:
    void do_step(double dt);

    Renderer renderer_;

    /**
     * Simulation clock. Monotonic seconds-since-boot, same base as
     * CAMetalDisplayLinkUpdate.targetPresentationTimestamp.
     */
    double time_;

    /**
     * Accumulated time
     */
    double accumulator_;

    /**
     * False until the first display-link callback seeds time_ from the
     * presentation timestamp. Avoids a large initial dt spike from
     * (constructor wall time) -> (first frame).
     */
    bool seeded_;
};
