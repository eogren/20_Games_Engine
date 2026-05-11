#pragma once
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <simd/simd.h>

#include <array>

#include "Primitive2D.h"
#include "Semaphore.h"
#include "Uniforms.h"

// Pixel format the CAMetalLayer / drawable presents at. Source of truth —
// PongApplication's layer setup and the Renderer's blit pipeline both read
// this. Numeric value matches the ObjC MTLPixelFormatBGRA8Unorm, so
// PongApplication can cast:
//     _metalLayer.pixelFormat = (MTLPixelFormat)DRAWABLE_PIXEL_FORMAT;
inline constexpr MTL::PixelFormat DRAWABLE_PIXEL_FORMAT = MTL::PixelFormatBGRA8Unorm;

// Axis-aligned 2D view rectangle in the game's world coords, passed into
// beginFrame. The renderer builds the projection matrix from this — RH-vs-LH,
// Metal's [0,1] clip-space z range, and the z-range hack that keeps the 2D
// shader's z=0 geometry on-screen are all renderer-internal details that the
// game stays out of.
struct OrthoView2D
{
    float left;
    float right;
    float bottom;
    float top;
};

// Fixed-function 2D renderer for the Pong-style retro path: clears a small
// offscreen color target, then upscales (nearest) to the drawable. Substrate
// is the begin/end frame bracket — game-side draw calls land on Renderer as
// they're added.
class Renderer
{
public:
    explicit Renderer(MTL::Device* device);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Opens the offscreen render pass for the frame. Game draws issued
    // between beginFrame and endFrame land in the offscreen buffer, which
    // endFrame upscales to the drawable. `view` defines the 2D world rect
    // visible this frame; `time` is in seconds (the game decides the epoch).
    // Both are packed into per-frame uniforms and bound for every draw.
    void beginFrame(CA::MetalDrawable* drawable, const OrthoView2D& view, float time);

    // Game-side 2D draw API. Calls between beginFrame and endFrame
    // accumulate Primitive2D instances; endFrame uploads + draws them
    // in one instanced pass. center / half_extents are world-space
    // (game units), color is unpremultiplied RGBA in [0, 1].
    void drawRect(simd::float2 center, simd::float2 half_extents, simd::float4 color);
    void drawCircle(simd::float2 center, float radius, simd::float4 color);

    // Ends the offscreen pass, blits the offscreen color target to the
    // drawable, commits, and presents.
    void endFrame();

private:
    static constexpr long FRAMES_IN_FLIGHT = 2;

    // Resources owned per in-flight frame. Indexed by frame_ % FRAMES_IN_FLIGHT;
    // the allocator semaphore gates reuse so the GPU is done with slot N before
    // the CPU touches it again.
    struct PerFrame
    {
    public:
        // Exists only for std::array to construct
        PerFrame() = default;

        explicit PerFrame(MTL::Device* device, ssize_t num2dPrimitives);

        MTL4::CommandAllocator* command_allocator() const
        {
            return command_allocator_.get();
        };

        ssize_t num_primitives_used() const
        {
            return num2dPrimitives_used_;
        }

        void addToResidencySet(MTL::ResidencySet* residency_set) const;

        // Resets the per-frame counter, writes `uniforms` into the slot's
        // buffer, and points `argument_table`'s buffer slots at this slot's
        // GPU addresses. Called once per frame, before any draw in the slot.
        void beginFrame(const Uniforms& uniforms, MTL4::ArgumentTable* argument_table);

        void addPrimitive(const Primitive2D& primitive);

    private:
        NS::SharedPtr<MTL4::CommandAllocator> command_allocator_;
        NS::SharedPtr<MTL::Buffer> buffer_;

        ssize_t num2dPrimitives_used_ = 0;
        ssize_t num2dPrimitives_capacity_ = 0;
    };

    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL4::CommandQueue> command_queue_;
    NS::SharedPtr<MTL4::CommandBuffer> command_buffer_;
    std::array<PerFrame, FRAMES_IN_FLIGHT> per_frame_;
    NS::SharedPtr<MTL::Texture> render_texture_;
    NS::SharedPtr<MTL::ResidencySet> residency_set_;
    NS::SharedPtr<MTL4::RenderPassDescriptor> render_pass_descriptor_;
    NS::SharedPtr<MTL::Library> default_library_;
    NS::SharedPtr<MTL4::Compiler> compiler_;
    NS::SharedPtr<MTL::SamplerState> blit_sampler_;
    NS::SharedPtr<MTL::RenderPipelineState> blit_pipeline_;
    NS::SharedPtr<MTL4::ArgumentTable> blit_argument_table_;
    NS::SharedPtr<MTL::RenderPipelineState> primitive2d_pipeline_;
    NS::SharedPtr<MTL4::ArgumentTable> primitive2d_argument_table_;

    // Frame boundary for Xcode's GPU capture. Xcode's automatic boundary
    // heuristic doesn't fire under MTL4 + CAMetalDisplayLink (the classic
    // nextDrawable/presentDrawable pair it looks for is gone) — without an
    // explicit scope, capture reports "no capture boundary detected".
    NS::SharedPtr<MTL::CaptureScope> capture_scope_;

    Semaphore allocator_semaphore_;
    uint64_t frame_;

    // In-flight frame state. beginFrame stashes the drawable + opens the
    // offscreen encoder; endFrame consumes both and clears them.
    NS::SharedPtr<CA::MetalDrawable> current_drawable_;
    NS::SharedPtr<MTL4::RenderCommandEncoder> current_offscreen_encoder_;
};
