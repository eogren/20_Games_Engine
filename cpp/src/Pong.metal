#include <metal_stdlib>
using namespace metal;

struct FullscreenVOut {
    float4 position [[position]];
    float2 uv;
};

// Single-triangle fullscreen pass. Caller draws 3 verts, no vertex buffer
// bound. The triangle is oversized — 3/4 of it lies outside [-1,1]^2 and
// gets clipped, leaving the screen covered exactly once.
//
//   vid 0 -> NDC (-1,-1), uv (0, 1)
//   vid 1 -> NDC ( 3,-1), uv (2, 1)   (clipped)
//   vid 2 -> NDC (-1, 3), uv (0,-1)   (clipped)
//
// Preferred over a 2-tri quad: GPUs shade in 2x2 quads, and along the seam
// of a quad both triangles touch the same quads and double-shade helper
// lanes. One triangle has no seam.
//
// uv has top-left origin (Metal texture convention) — V is flipped from
// pos.y so a render-target's contents sample back unmirrored.
vertex FullscreenVOut fullscreen_vertex(uint vid [[vertex_id]]) {
    float2 pos = float2(float(vid & 1u), float(vid >> 1)) * 4.0 - 1.0;
    FullscreenVOut o;
    o.position = float4(pos, 0.0, 1.0);
    o.uv = float2(pos.x, -pos.y) * 0.5 + 0.5;
    return o;
}

fragment float4 blit_fragment(FullscreenVOut in [[stage_in]],
                              texture2d<float> src [[texture(0)]],
                              sampler          smp [[sampler(0)]]) {
    return src.sample(smp, in.uv);
}
