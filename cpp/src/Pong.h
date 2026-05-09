#pragma once
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <dispatch/dispatch.h>

#include <array>

// Pixel format the CAMetalLayer / drawable presents at. Source of truth —
// PongApplication's layer setup and any of Pong's render targets that write
// to the drawable both read this. Numeric value matches the ObjC
// MTLPixelFormatBGRA8Unorm, so PongApplication can cast:
//     _metalLayer.pixelFormat = (MTLPixelFormat)DRAWABLE_PIXEL_FORMAT;
inline constexpr MTL::PixelFormat DRAWABLE_PIXEL_FORMAT = MTL::PixelFormatBGRA8Unorm;

// MTL::ClearColor's constructor isn't constexpr, so these are inline const
// rather than inline constexpr.
inline const MTL::ClearColor CLEAR_COLOR_BLACK{0.0, 0.0, 0.0, 1.0};
inline const MTL::ClearColor CLEAR_COLOR_BLUE{0.0, 0.0, 1.0, 1.0};

// Owns a dispatch_semaphore_t. Destructor is out-of-line because dispatch_release
// is unavailable under OS_OBJECT_USE_OBJC=1 (i.e. ARC-enabled .mm translation
// units). Keeping ~Semaphore in Pong.cpp confines the release call to a pure-C++
// TU where OS_OBJECT_USE_OBJC=0.
class Semaphore
{
public:
    explicit Semaphore(long initialValue);
    ~Semaphore();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&&) = delete;
    Semaphore& operator=(Semaphore&&) = delete;

    void wait()
    {
        dispatch_semaphore_wait(handle_, DISPATCH_TIME_FOREVER);
    }
    void signal()
    {
        dispatch_semaphore_signal(handle_);
    }

private:
    dispatch_semaphore_t handle_;
};

class Pong
{
public:
    Pong(MTL::Device* device);

    void tick(CA::MetalDrawable* drawable, double targetTimestamp);

private:
    void do_step(double dt);
    void render_frame(CA::MetalDrawable* drawable, double alpha);

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

    /**
     * Simulation clock. Monotonic seconds-since-boot, same base as
     * CAMetalDisplayLinkUpdate.targetPresentationTimestamp.
     */
    double time_;

    /**
     * Accumulated time
     */
    double accumulator_;

    /**
     * False until the first display-link callback seeds time_ from the
     * presentation timestamp. Avoids a large initial dt spike from
     * (constructor wall time) -> (first frame).
     */
    bool seeded_;

    /**
     * Current frame
     */
    uint64_t frame_;

    /**
     * Semaphore that guards the allocator
     */
    Semaphore allocator_semaphore_;
};
