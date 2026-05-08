#include <metal_stdlib>
using namespace metal;

// Mirrors of `MeshVertexOut` and `MeshGlobalUniform` in
// `EngineShaderTypes.h`. Inlined so this test shader doesn't pull in a
// header from the engine bundle — matches the convention in
// `MeshSolid.metal` / `MeshWorldPosition.metal`. Keep both struct
// layouts in sync with the engine; in particular, `MeshGlobalUniform`'s
// field order is the cross-library buffer binding contract for slot 1.
struct MeshVertexOut {
    float4 position [[position]];
    float2 uv;
    float3 worldPosition;
};

struct MeshGlobalUniform {
    float4x4 viewProjectionMatrix;
    float time;
};

// Test-only fragment paired with the engine's `mesh_vertex`. Reads the
// per-frame `MeshGlobalUniform` at buffer 1 and emits `time` directly
// into the red channel, clamped to [0, 1]. A readback proves the engine
// supplied time is reaching the mesh fragment stage — distinct from the
// view-projection field, which the vertex shader consumes but the
// fragment never sees.
fragment float4 mesh_time_red(MeshVertexOut in [[stage_in]],
                              constant MeshGlobalUniform& g [[buffer(1)]]) {
    (void)in;
    float t = clamp(g.time, 0.0, 1.0);
    return float4(t, 0.0, 0.0, 1.0);
}
