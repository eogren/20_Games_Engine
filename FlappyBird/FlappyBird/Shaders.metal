#include <metal_stdlib>
using namespace metal;

#include "EngineShaderTypes.h"

// Mirror of the engine's `FullscreenVOut`. The `[[user(locn0)]]`
// annotation is part of the cross-library link contract — must match
// the engine's vertex shader output declaration exactly.
struct FullscreenVOut {
    float4 position [[position]];
    float2 uv [[user(locn0)]];
};

struct BackgroundUniforms {
    float time;
};

fragment float4 background(FullscreenVOut in [[stage_in]],
                           constant BackgroundUniforms& u [[buffer(0)]]) {
    // uv.x in red, uv.y in green, sin(time)-driven blue. Animates a
    // pastel diagonal gradient that pulses in blue.
    float blue = 0.5 + 0.5 * sin(u.time);
    return float4(in.uv.x, in.uv.y, blue, 1.0);
}

// Cube fragment paired with the engine's `mesh_vertex`. Emits the
// per-vertex UV directly into red/green so each face shows a different
// gradient as the cube spins, making rotation visually obvious. Blue
// pulses off the engine-supplied `MeshGlobalUniform.time` at twice the
// background's rate so the cube and background pulses are distinguishable
// — and the cube proves engine globals are reaching the mesh fragment.
fragment float4 cube_uv(MeshVertexOut in [[stage_in]],
                        constant MeshGlobalUniform& g [[buffer(1)]]) {
    float blue = 0.5 + 0.3 * sin(g.time * 6.0);
    return float4(in.uv.x, in.uv.y, blue, 1.0);
}
