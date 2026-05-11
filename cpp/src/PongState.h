#pragma once

#include <simd/simd.h>

/**
 * Sim state for Pong.
 */
struct PongState
{
    simd::float2 left_paddle_coord_;
    simd::float2 right_paddle_coord_;
    simd::float2 ball_coord_;
    simd::float2 ball_velocity_;
};

constexpr float WIDTH_GAME_UNITS = 12.0;
constexpr float HEIGHT_GAME_UNITS = WIDTH_GAME_UNITS / (3.0 / 4.0);
