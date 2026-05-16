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

// SDF disc: discard pixels outside the unit circle inscribed in [0,1]x[0,1].
// The caller sizes the bounding quad to exactly 2r×2r so the circle always
// inscribes at dot(d,d)==0.25 in UV space regardless of world-space radius.
[shader("fragment")]
float4 fragDisc(VSOut input) : SV_Target {
    float2 d = input.uv - float2(0.5, 0.5);
    if (dot(d, d) > 0.25) discard;
    return pc.color;
}
