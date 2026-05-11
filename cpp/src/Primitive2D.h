#pragma once

// Per-instance primitive sent to the 2D renderer's instanced draw call.
// The CPU pushes Primitive2D values into a per-frame buffer; the vertex
// shader reads them back as instance data, and the fragment shader
// switches on the low byte of type_and_flags to render the shape.
//
// Layout is shared with Metal — the C++ and MSL definitions below must
// stay byte-compatible. Encoding:
//
//   offset  0: float2 center        — world-space center of the shape
//   offset  8: float2 half_extents  — distance from center to edge per
//                                     axis; for circles hx == hy == r
//   offset 16: uint   color_rgba    — 8888, low byte = R; matches
//                                     Metal's unpack_unorm4x8_to_float
//   offset 20: uint   type_and_flags— low byte = ShapeType, upper
//                                     bytes reserved for future flags
//
// sizeof = 24, alignof = 8.

#ifdef __METAL_VERSION__

#include <metal_stdlib>
using namespace metal;

struct Primitive2D
{
    float2 center;
    float2 half_extents;
    uint color_rgba;
    uint type_and_flags;
};

#else

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <simd/simd.h>

struct Primitive2D
{
    simd::float2 center;
    simd::float2 half_extents;
    uint32_t color_rgba;
    uint32_t type_and_flags;
};

// If these fail, the GPU and CPU disagree about instance layout
// and the symptom is corrupted shapes rather than a build error.
// Cheaper to catch at compile time.
static_assert(sizeof(Primitive2D) == 24, "Primitive2D layout is shared with Metal; size must be 24");
static_assert(alignof(Primitive2D) == 8, "Primitive2D must be 8-byte aligned");
static_assert(offsetof(Primitive2D, center) == 0, "Primitive2D::center must live at offset 0");
static_assert(offsetof(Primitive2D, half_extents) == 8, "Primitive2D::half_extents must live at offset 8");
static_assert(offsetof(Primitive2D, color_rgba) == 16, "Primitive2D::color_rgba must live at offset 16");
static_assert(offsetof(Primitive2D, type_and_flags) == 20, "Primitive2D::type_and_flags must live at offset 20");

// Pack RGBA in [0, 1] to the 32-bit format color_rgba expects.
// Out-of-range channels are clamped — values outside [0, 1] have
// no meaning in an unorm encoding. std::lround for correct
// round-to-nearest (the +0.5/cast trick mis-rounds near 0.5).
inline uint32_t pack_rgba(simd::float4 rgba)
{
    rgba = simd::clamp(rgba, simd::float4(0.0f), simd::float4(1.0f));
    const auto to_byte = [](float c) -> uint32_t { return static_cast<uint32_t>(std::lround(c * 255.0f)); };
    return to_byte(rgba.x) | (to_byte(rgba.y) << 8) | (to_byte(rgba.z) << 16) | (to_byte(rgba.w) << 24);
}

inline uint32_t pack_rgba(float r, float g, float b, float a = 1.0f)
{
    return pack_rgba(simd::float4{r, g, b, a});
}

#endif

// uint8_t is available in both <cstdint> and <metal_stdlib>, so the
// enum can live outside the language guard. Stored as one byte; the
// renderer casts to uint32_t when packing into type_and_flags.
enum class ShapeType : uint8_t
{
    Rect = 0,
    Circle = 1,
};

// Argument-table slots for the primitive2d pipeline. Pong.metal references
// these in [[buffer(N)]] on primitive2d_vertex; Renderer.cpp references
// them when binding the per-frame buffer. Single source of truth so the
// CPU and GPU sides can't drift. #define rather than constexpr because
// MSL requires program-scope variables to live in the `constant` address
// space, and constexpr alone doesn't qualify.
#define PRIMITIVE2D_BUFFER_SLOT 0
#define UNIFORMS_BUFFER_SLOT 1
