#pragma once

#include "pong_state.h"

namespace pong
{
    // Advance one fixed timestep: apply paddle.vel, move ball, resolve wall and
    // paddle collisions, re-serve if out of bounds.
    void step_physics(GameState& gs, float dt);
} // namespace pong
