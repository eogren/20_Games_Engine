#include <metal_stdlib>
using namespace metal;

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
