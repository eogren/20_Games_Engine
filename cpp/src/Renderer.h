#pragma once
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include <array>

#include "Semaphore.h"

// Pixel format the CAMetalLayer / drawable presents at. Source of truth —
// PongApplication's layer setup and the Renderer's blit pipeline both read
// this. Numeric value matches the ObjC MTLPixelFormatBGRA8Unorm, so
// PongApplication can cast:
//     _metalLayer.pixelFormat = (MTLPixelFormat)DRAWABLE_PIXEL_FORMAT;
inline constexpr MTL::PixelFormat DRAWABLE_PIXEL_FORMAT = MTL::PixelFormatBGRA8Unorm;

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
    // endFrame upscales to the drawable.
    void beginFrame(CA::MetalDrawable* drawable);

    // Ends the offscreen pass, blits the offscreen color target to the
    // drawable, commits, and presents.
    void endFrame();

private:
    static constexpr long FRAMES_IN_FLIGHT = 2;

    NS::SharedPtr<MTL::Device> device_;
    NS::SharedPtr<MTL4::CommandQueue> command_queue_;
    NS::SharedPtr<MTL4::CommandBuffer> command_buffer_;
    std::array<NS::SharedPtr<MTL4::CommandAllocator>, FRAMES_IN_FLIGHT> command_allocators_;
    NS::SharedPtr<MTL::Texture> render_texture_;
    NS::SharedPtr<MTL::ResidencySet> residency_set_;
    NS::SharedPtr<MTL4::RenderPassDescriptor> render_pass_descriptor_;
    NS::SharedPtr<MTL4::ArgumentTable> argument_table_;
    NS::SharedPtr<MTL::Library> default_library_;
    NS::SharedPtr<MTL4::Compiler> compiler_;
    NS::SharedPtr<MTL::SamplerState> blit_sampler_;
    NS::SharedPtr<MTL::RenderPipelineState> blit_pipeline_;
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
