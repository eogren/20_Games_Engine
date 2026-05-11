#pragma once

// Per-frame uniforms shared with Metal. The renderer fills this once
// per frame and binds it to a fixed buffer slot; the vertex shader
// reads view_proj, the fragment shader (or any later vertex shader)
// can read time for animated effects (pulse, scroll, dither, etc).
//
// Layout:
//   offset  0: float4x4 view_proj  (64 B)
//   offset 64: float    time       (4 B, plus 12 B trailing pad to 80)
//
// sizeof = 80, alignof = 16.

#ifdef __METAL_VERSION__

#include <metal_stdlib>
using namespace metal;

struct Uniforms
{
    float4x4 view_proj;
    float time;
};

#else

#include <cstddef>
#include <simd/simd.h>

struct Uniforms
{
    simd::float4x4 view_proj;
    float time = 0.0f; // seconds — semantics (since app, since
                       // game-start, etc) decided by the renderer
};

// If these fail, the GPU and CPU disagree about uniform layout
// and the symptom is a malformed view_proj or garbage time, not
// a build error. Cheaper to catch up front.
static_assert(sizeof(Uniforms) == 80, "Uniforms layout is shared with Metal; size must be 80");
static_assert(alignof(Uniforms) == 16, "Uniforms must be 16-byte aligned");
static_assert(offsetof(Uniforms, view_proj) == 0, "Uniforms::view_proj must live at offset 0");
static_assert(offsetof(Uniforms, time) == 64, "Uniforms::time must live at offset 64");

#endif
