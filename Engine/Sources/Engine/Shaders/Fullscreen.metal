#include <metal_stdlib>
using namespace metal;

// Substrate vertex shader for screen-space passes. Synthesizes the four
// corners of NDC from [[vertex_id]] — caller draws 4 verts as triangle
// strip with no vertex buffer bound.
//
// Fragment shaders pair with this by declaring a matching stage_in
// struct. The user-attribute number is part of the cross-library link
// contract — keep `uv` at locn0 here AND in the fragment shader:
//
//     struct FullscreenVOut {
//         float4 position [[position]];
//         float2 uv [[user(locn0)]];
//     };
//
// `uv` is in [0,1] with origin at the bottom-left (Metal texture
// convention). Flip in the fragment shader if you need top-left origin.

struct FullscreenVOut {
    float4 position [[position]];
    float2 uv [[user(locn0)]];
};

vertex FullscreenVOut fullscreen_vertex(uint vid [[vertex_id]]) {
    // Bit 0 picks x ∈ {0,1}, bit 1 picks y ∈ {0,1}; remap to NDC ∈ {-1,+1}.
    float2 p = float2(float(vid & 1), float((vid >> 1) & 1)) * 2.0 - 1.0;
    FullscreenVOut o;
    o.position = float4(p, 0, 1);
    o.uv = p * 0.5 + 0.5;
    return o;
}
