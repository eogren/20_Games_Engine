// Substrate quad shader.
//
// Six vertices expanded from SV_VertexID — no vertex buffer. The push-constant
// `rect` places the unit square in pixel space; `viewProj` maps pixel space to
// Vulkan NDC.

struct PushConstants {
    float4x4 viewProj;
    float4 rect;   // (x, y, w, h) in pixel-space, top-left origin
    float4 color;  // RGBA, linear; swapchain encodes to sRGB on present
};

[[vk::push_constant]] ConstantBuffer<PushConstants> pc;

struct VSOut {
    float4 pos : SV_Position;
    float2 uv : UV;
};

[shader("vertex")]
VSOut vertMain(uint vid : SV_VertexID) {
    const float2 corners[6] = {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };
    float2 c = corners[vid];
    float2 pixelPos = pc.rect.xy + c * pc.rect.zw;
    VSOut o;
    o.pos = mul(pc.viewProj, float4(pixelPos, 0.0, 1.0));
    o.uv = c;
    return o;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target {
    return pc.color;
}
