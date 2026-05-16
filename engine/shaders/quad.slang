// Substrate quad shader.
//
// Vertex stage emits a unit-quad triangle list keyed off SV_VertexID alone —
// no vertex buffer, no descriptor set. Fragment stage emits a placeholder
// color. Push constants for per-draw rect + color land in the follow-up PR
// that actually wires the pipeline and the drawQuad call.
//
// Built by add_slang_shaders() (root CMakeLists.txt). slangc bakes both
// entry points into one SPIR-V module at ${CMAKE_BINARY_DIR}/shaders/quad.spv.

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
    // Half-size center-of-screen placement. The real push-constant-driven
    // rect arrives in the next PR.
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
