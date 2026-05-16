// Substrate quad shader.
//
// Geometry comes from SV_VertexID alone — no vertex buffer, no descriptor
// set. Lets the slangc -> .spv path be exercised before any buffer-allocation
// machinery exists.

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
    float2 ndc = (c * 2.0 - 1.0) * 0.5;
    VSOut o;
    o.pos = float4(ndc, 0.0, 1.0);
    o.uv = c;
    return o;
}

[shader("fragment")]
float4 fragMain(VSOut input) : SV_Target {
    // Hot pink: obvious "placeholder, replace me" color if it ever lands on screen.
    return float4(1.0, 0.0, 1.0, 1.0);
}
