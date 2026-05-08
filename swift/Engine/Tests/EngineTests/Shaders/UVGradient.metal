#include <metal_stdlib>
using namespace metal;

// Test-only fragment paired with the engine's `fullscreen_vertex`. Emits
// the interpolated uv directly into the red/green channels so a readback
// of the four corners can prove the rasterizer covered the whole target
// (and that uv increases left-to-right + bottom-to-top, i.e. viewport
// orientation is correct). Used by `RendererSmokeTests`.
//
// The `[[user(locn0)]]` attribute number is part of the cross-library
// link contract with `Fullscreen.metal` — keep in sync.
struct FullscreenVOut {
    float4 position [[position]];
    float2 uv [[user(locn0)]];
};

// `drawFullscreenQuad` always binds a uniform buffer at fragment buffer 0
// from its generic upload path. The test passes a one-float dummy so the
// binding contract is real (rather than relying on the shader not reading
// an unbound slot).
struct UVGradientUniforms {
    float dummy;
};

fragment float4 uv_gradient(FullscreenVOut in [[stage_in]],
                            constant UVGradientUniforms& u [[buffer(0)]]) {
    (void)u;
    return float4(in.uv.x, in.uv.y, 0.0, 1.0);
}
