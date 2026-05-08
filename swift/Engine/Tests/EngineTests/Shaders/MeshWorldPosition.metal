#include <metal_stdlib>
using namespace metal;

// Mirrors `MeshVertexOut` in `EngineShaderTypes.h`. Inlined so this test
// shader doesn't need to include a header from the engine bundle —
// matches the pattern in `MeshSolid.metal` / `UVGradient.metal`. Keep
// the field order in sync: Metal links cross-library stage_in by
// auto-numbered location, so reordering shifts the contract.
struct MeshVertexOut {
    float4 position [[position]];
    float2 uv;
    float3 worldPosition;
};

// Test-only fragment paired with the engine's `mesh_vertex`. Encodes the
// interpolated world-space position into the color channels as
// `worldPos * 0.5 + 0.5`, so a readback of a known pixel proves the
// vertex shader applied the model matrix and the rasterizer interpolated
// the new varying through to the fragment. The 0.5+0.5 mapping keeps
// negative coordinates representable; the test geometry stays in
// [-1, 1] per axis.
fragment float4 mesh_world_position(MeshVertexOut in [[stage_in]]) {
    float3 encoded = in.worldPosition * 0.5 + 0.5;
    return float4(encoded, 1.0);
}
