#include <metal_stdlib>
using namespace metal;

// Mirrors `MeshVertexOut` in `EngineShaderTypes.h`. Inlined so this
// test shader doesn't need to include a header from the engine bundle —
// matches the pattern in `UVGradient.metal`. Keep in sync.
struct MeshVertexOut {
    float4 position [[position]];
    float2 uv;
};

// Test-only fragment paired with the engine's `mesh_vertex`. Emits an
// opaque red so a readback can prove the mesh actually rasterized to
// the framebuffer (not silently skipped due to a residency fault, a
// vertex layout mismatch, or a busted index buffer). Used by
// `RendererSmokeTests`.
fragment float4 mesh_solid_red(MeshVertexOut in [[stage_in]]) {
    (void)in;
    return float4(1.0, 0.0, 0.0, 1.0);
}

// Sibling of `mesh_solid_red` for tests that need to tell two overlapping
// draws apart in the readback (e.g. depth-occlusion tests where the
// near draw is red and the far draw is blue).
fragment float4 mesh_solid_blue(MeshVertexOut in [[stage_in]]) {
    (void)in;
    return float4(0.0, 0.0, 1.0, 1.0);
}
