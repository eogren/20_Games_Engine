#include <metal_stdlib>
using namespace metal;

#include "EngineShaderTypes.h"

vertex MeshVertexOut mesh_vertex(MeshVertexIn v_in [[stage_in]],
                                 constant MeshGlobalUniform& global_uniform [[buffer(1)]],
                                 constant MeshModelUniform& model_uniform [[buffer(2)]]) {
    MeshVertexOut o;
    float4 worldPos = model_uniform.modelMatrix * float4(v_in.position, 1.0);
    o.position = global_uniform.viewProjectionMatrix * worldPos;
    o.worldPosition = worldPos.xyz;
    o.uv = v_in.uv;
    return o;
}
