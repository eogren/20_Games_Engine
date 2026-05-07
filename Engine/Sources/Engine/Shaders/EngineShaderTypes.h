#pragma once
#include <metal_stdlib>
using namespace metal;

/// Vertex structure used by the engine for loaded assets
struct MeshVertexIn {
    float3 position [[attribute(0)]];
    float2 uv [[attribute(1)]];
};

struct MeshVertexOut {
    float4 position [[position]];
    float2 uv;
    float3 worldPosition;
};

/// Uniforms related to the current model
struct MeshModelUniform {
    float4x4 modelMatrix;
};

/// Uniforms that are true for the entire frame for all objects
struct MeshGlobalUniform {
    float4x4 viewProjectionMatrix;
};

