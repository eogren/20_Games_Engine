#include <metal_stdlib>
using namespace metal;

#include "Primitive2D.h"
#include "Uniforms.h"

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
                              texture2d<float> src [[texture(0)]]) {
    // Inline sampler — settings are fixed for the substrate's nearest-neighbor
    // upscale and don't need a runtime-mutable MTLSamplerState. Filter mode is
    // baked into the shader binary; switching to linear is a code edit + recompile.
    constexpr sampler smp(filter::nearest, address::clamp_to_edge);
    return src.sample(smp, in.uv);
}

// =====================================================================
// 2D primitive pipeline.
//
// One PSO renders every shape type via instanced unit quads; the
// fragment shader switches on type_and_flags to pick the look. The
// quad is 6 verts laid out as two triangles in [-1, 1]² local space,
// which the vertex shader scales by half_extents and translates to
// world. half_extents.x == half_extents.y gives a round circle;
// unequal gives an axis-aligned ellipse for free.
//
// Caller's draw call shape:
//   primitive type   = MTL::PrimitiveTypeTriangle
//   vertex count     = 6
//   instance count   = primitives.size()
//   buffer(PRIMITIVE2D_BUFFER_SLOT) = array<Primitive2D> instance data
//   buffer(UNIFORMS_BUFFER_SLOT)    = Uniforms (view_proj + time)
//
// PSO needs alpha blending enabled for the circle's AA edge.
// =====================================================================

struct Primitive2DVOut {
    float4 position [[position]];
    float2 local_uv;
    float4 color;
    uint   type     [[flat]];
};

vertex Primitive2DVOut primitive2d_vertex(uint vid [[vertex_id]],
                                          uint iid [[instance_id]],
                                          constant Primitive2D* prims    [[buffer(PRIMITIVE2D_BUFFER_SLOT)]],
                                          constant Uniforms&    uniforms [[buffer(UNIFORMS_BUFFER_SLOT)]]) {
    // Two triangles forming a unit quad. Wound CCW.
    //   t0: (-1,-1) -> ( 1,-1) -> (-1, 1)
    //   t1: ( 1,-1) -> ( 1, 1) -> (-1, 1)
    constexpr float2 quad_corners[6] = {
        float2(-1, -1), float2( 1, -1), float2(-1,  1),
        float2( 1, -1), float2( 1,  1), float2(-1,  1),
    };

    Primitive2D p     = prims[iid];
    float2      local = quad_corners[vid];
    float2      world = local * p.half_extents + p.center;

    Primitive2DVOut o;
    o.position = uniforms.view_proj * float4(world, 0.0, 1.0);
    o.local_uv = local;
    o.color    = unpack_unorm4x8_to_float(p.color_rgba);
    o.type     = p.type_and_flags & 0xFFu;
    return o;
}

fragment float4 primitive2d_fragment(Primitive2DVOut in [[stage_in]]) {
    switch (in.type) {
    case 0u:  // ShapeType::Rect — solid fill
        return in.color;
    case 1u: { // ShapeType::Circle — inscribed in the unit quad, AA edge
        float r  = length(in.local_uv);
        float aa = fwidth(r);
        float a  = 1.0 - smoothstep(1.0 - aa, 1.0, r);
        if (a <= 0.0) {
            discard_fragment();
        }
        return float4(in.color.rgb, in.color.a * a);
    }
    }
    // Unknown ShapeType — return magenta so it's obvious onscreen.
    return float4(1.0, 0.0, 1.0, 1.0);
}
