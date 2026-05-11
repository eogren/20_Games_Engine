#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "Renderer.h"

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <simd/simd.h>

#include <format>

#include "Uniforms.h"
#include "math/Math.h"

constexpr NS::UInteger RENDER_WIDTH = 320;
constexpr NS::UInteger RENDER_HEIGHT = 240;
constexpr ssize_t NUM_2D_PRIMITIVES = 64;

// Pixel format of the offscreen render target the game draws into before
// upscaling to the drawable. Internal — DRAWABLE_PIXEL_FORMAT (in Renderer.h)
// is the externally-visible source of truth.
constexpr MTL::PixelFormat OFFSCREEN_PIXEL_FORMAT = MTL::PixelFormatBGRA8Unorm;

// Argument-table slot the offscreen color target binds at, matching the
// `[[texture(0)]]` attribute on blit_fragment in Pong.metal. Source-of-truth
// so the binding side and the shader side don't drift. The blit's sampler
// is inline (constexpr) in the shader and has no argument-table slot.
constexpr NS::UInteger BLIT_SOURCE_SLOT = 0;

namespace
{
    // MTL::ClearColor's constructor isn't constexpr, so these are inline const
    // rather than inline constexpr.
    const MTL::ClearColor CLEAR_COLOR_BLACK{0.0, 0.0, 0.0, 1.0};
    const MTL::ClearColor CLEAR_COLOR_BLUE{0.0, 0.0, 1.0, 1.0};

    // setLabel takes NS::String*; these fold the std::format + NS::String hop
    // so call sites read like printf. Label property on Metal objects is
    // copy-semantics, so the temp string going out of scope is safe.
    //
    // Two overloads: format-string variant for inline composition, std::string
    // variant for when the same label is reused across multiple objects (build
    // once, label many). The format overload is constrained to >=1 args so the
    // no-args literal case (e.g. "Pong queue") routes unambiguously to the
    // std::string overload instead of being viable for both.
    template <class T, class... Args>
        requires(sizeof...(Args) >= 1)
    void setMetalLabel(T* obj, std::format_string<Args...> fmt, Args&&... args)
    {
        auto s = std::format(fmt, std::forward<Args>(args)...);
        obj->setLabel(NS::String::string(s.c_str(), NS::UTF8StringEncoding));
    }

    template <class T> void setMetalLabel(T* obj, const std::string& label)
    {
        obj->setLabel(NS::String::string(label.c_str(), NS::UTF8StringEncoding));
    }

    NS::SharedPtr<MTL4::RenderPassDescriptor> buildRenderPassDescriptor(MTL::Texture* texture,
                                                                        const MTL::ClearColor& clearColor)
    {
        auto ret = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
        auto* color0 = ret->colorAttachments()->object(0);
        color0->setTexture(texture);
        color0->setLoadAction(MTL::LoadActionClear);
        color0->setStoreAction(MTL::StoreActionStore);
        color0->setClearColor(clearColor);

        return ret;
    }

    NS::SharedPtr<MTL4::RenderPassDescriptor> buildRenderPassDescriptor(CA::MetalDrawable* drawable,
                                                                        const MTL::ClearColor& clearColor)
    {
        return buildRenderPassDescriptor(drawable->texture(), clearColor);
    }

    NS::SharedPtr<MTL::Texture> buildRenderTexture(MTL::Device* device)
    {
        // texture2DDescriptor is a class-method convenience constructor — its
        // selector doesn't start with alloc/new/copy, so it returns +0
        // (autoreleased). RetainPtr is the correct wrapper; TransferPtr would
        // under-retain and over-release once the autorelease pool drains.
        auto desc = NS::RetainPtr(
            MTL::TextureDescriptor::texture2DDescriptor(OFFSCREEN_PIXEL_FORMAT, RENDER_WIDTH, RENDER_HEIGHT, false));
        desc->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
        desc->setStorageMode(MTL::StorageModePrivate);

        auto ret = NS::TransferPtr(device->newTexture(desc.get()));
        assert(ret && "newTexture failed");
        setMetalLabel(ret.get(), "Pong offscreen color {}x{}", RENDER_WIDTH, RENDER_HEIGHT);
        return ret;
    }

    NS::SharedPtr<MTL::ResidencySet> buildResidencySet(MTL::Device* device)
    {
        auto desc = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong residency set");
        desc->setInitialCapacity(4);

        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(device->newResidencySet(desc.get(), &err));
        assert(ret && "newResidencySet failed");
        return ret;
    }

    NS::SharedPtr<MTL4::ArgumentTable> buildBlitArgumentTable(MTL::Device* device)
    {
        auto desc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong blit argument table");
        // One texture at slot 0 — matches [[texture(0)]] on blit_fragment.
        // The sampler is inline (constexpr) in the shader, so no sampler
        // bindings live in this table.
        desc->setMaxTextureBindCount(1);

        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(device->newArgumentTable(desc.get(), &err));
        assert(ret && "newArgumentTable failed");
        return ret;
    }

    NS::SharedPtr<MTL4::ArgumentTable> buildPrimitive2dArgumentTable(MTL::Device* device)
    {
        auto desc = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong primitive2d argument table");
        desc->setMaxBufferBindCount(2);

        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(device->newArgumentTable(desc.get(), &err));
        assert(ret && "newArgumentTable failed");
        return ret;
    }

    NS::SharedPtr<MTL4::Compiler> buildCompiler(MTL::Device* device)
    {
        auto desc = NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong compiler");
        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(device->newCompiler(desc.get(), &err));
        assert(ret && "newCompiler failed");
        return ret;
    }

    // Builds the PSO for the 2D primitive pass: primitive2d_vertex +
    // primitive2d_fragment, writing into the offscreen color target.
    // Standard alpha blending is enabled so the circle's smoothstep
    // edge composites against the background instead of clobbering it.
    NS::SharedPtr<MTL::RenderPipelineState> buildPrimitive2DPipeline(MTL4::Compiler* compiler, MTL::Library* library)
    {
        auto desc = NS::TransferPtr(MTL4::RenderPipelineDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong primitive2d pipeline");

        auto vfd = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        vfd->setLibrary(library);
        vfd->setName(NS::String::string("primitive2d_vertex", NS::UTF8StringEncoding));
        desc->setVertexFunctionDescriptor(vfd.get());

        auto ffd = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        ffd->setLibrary(library);
        ffd->setName(NS::String::string("primitive2d_fragment", NS::UTF8StringEncoding));
        desc->setFragmentFunctionDescriptor(ffd.get());

        auto color = desc->colorAttachments()->object(0);
        color->setPixelFormat(OFFSCREEN_PIXEL_FORMAT);
        color->setBlendingState(MTL4::BlendStateEnabled);

        // Standard (non-premultiplied) alpha blend:
        //   out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
        //   out.a   = src.a         + dst.a   * (1 - src.a)
        // The fragment shader outputs unpremultiplied rgb plus the
        // smoothstep coverage in alpha; this equation composites it.
        color->setRgbBlendOperation(MTL::BlendOperationAdd);
        color->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
        color->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

        color->setAlphaBlendOperation(MTL::BlendOperationAdd);
        color->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
        color->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

        auto opts = NS::TransferPtr(MTL4::CompilerTaskOptions::alloc()->init());
        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(compiler->newRenderPipelineState(desc.get(), opts.get(), &err));
        assert(ret && "newRenderPipelineState failed");
        return ret;
    }

    // Builds the PSO for the blit pass: fullscreen_vertex + blit_fragment,
    // writing to the drawable's color attachment.
    NS::SharedPtr<MTL::RenderPipelineState> buildBlitPipeline(MTL4::Compiler* compiler, MTL::Library* library)
    {
        auto desc = NS::TransferPtr(MTL4::RenderPipelineDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong blit pipeline");

        auto vfd = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        vfd->setLibrary(library);
        vfd->setName(NS::String::string("fullscreen_vertex", NS::UTF8StringEncoding));
        desc->setVertexFunctionDescriptor(vfd.get());

        auto ffd = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        ffd->setLibrary(library);
        ffd->setName(NS::String::string("blit_fragment", NS::UTF8StringEncoding));
        desc->setFragmentFunctionDescriptor(ffd.get());

        desc->colorAttachments()->object(0)->setPixelFormat(DRAWABLE_PIXEL_FORMAT);

        auto opts = NS::TransferPtr(MTL4::CompilerTaskOptions::alloc()->init());
        NS::Error* err = nullptr;
        auto ret = NS::TransferPtr(compiler->newRenderPipelineState(desc.get(), opts.get(), &err));
        assert(ret && "newRenderPipelineState failed");
        return ret;
    }

    MTL::Viewport viewport(NS::UInteger width, NS::UInteger height)
    {
        // {originX, originY, width, height, znear, zfar}
        return {0.0, 0.0, (double)width, (double)height, 0.0, 1.0};
    }
} // namespace

Renderer::Renderer(MTL::Device* device)
    : device_(NS::RetainPtr(device)), allocator_semaphore_(FRAMES_IN_FLIGHT), frame_(0)
{
    assert(device_.get() != nullptr);

    {
        auto desc = NS::TransferPtr(MTL4::CommandQueueDescriptor::alloc()->init());
        setMetalLabel(desc.get(), "Pong queue");
        NS::Error* err = nullptr;
        command_queue_ = NS::TransferPtr(device_->newMTL4CommandQueue(desc.get(), &err));
        assert(command_queue_ && "newMTL4CommandQueue failed");
    }

    command_buffer_ = NS::TransferPtr(device_->newCommandBuffer());
    assert(command_buffer_ && "newCommandBuffer failed");
    setMetalLabel(command_buffer_.get(), "Pong command buffer");

    render_texture_ = buildRenderTexture(device_.get());

    residency_set_ = buildResidencySet(device_.get());
    residency_set_->addAllocation(render_texture_.get());
    residency_set_->commit();
    command_queue_->addResidencySet(residency_set_.get());

    render_pass_descriptor_ = buildRenderPassDescriptor(render_texture_.get(), CLEAR_COLOR_BLUE);

    default_library_ = NS::TransferPtr(device_->newDefaultLibrary());
    assert(default_library_ && "newDefaultLibrary failed — is default.metallib in the bundle?");
    setMetalLabel(default_library_.get(), "Pong default library");

    compiler_ = buildCompiler(device_.get());
    blit_pipeline_ = buildBlitPipeline(compiler_.get(), default_library_.get());
    primitive2d_pipeline_ = buildPrimitive2DPipeline(compiler_.get(), default_library_.get());

    blit_argument_table_ = buildBlitArgumentTable(device_.get());
    blit_argument_table_->setTexture(render_texture_->gpuResourceID(), BLIT_SOURCE_SLOT);
    primitive2d_argument_table_ = buildPrimitive2dArgumentTable(device_.get());

    // Each slot needs its own buffer + allocator — std::array::fill would copy
    // one PerFrame into every slot, and the NS::SharedPtr members would alias
    // the same Metal resources across all in-flight frames.
    for (auto& pf : per_frame_)
    {
        pf = PerFrame(device_.get(), NUM_2D_PRIMITIVES);
        pf.addToResidencySet(residency_set_.get());
    }

    capture_scope_ =
        NS::TransferPtr(MTL::CaptureManager::sharedCaptureManager()->newCaptureScope(command_queue_.get()));
    setMetalLabel(capture_scope_.get(), "Pong frame");
    MTL::CaptureManager::sharedCaptureManager()->setDefaultCaptureScope(capture_scope_.get());
}

void Renderer::beginFrame(CA::MetalDrawable* drawable, const OrthoView2D& view, float time)
{
    assert(!current_drawable_ && "beginFrame called without matching endFrame");

    // Z range is renderer-internal: Pong.metal emits geometry at z=0, and a
    // symmetric [-1, 1] z window puts that at the middle of the depth slice
    // rather than clipping it against a positive near plane.
    Uniforms uniforms;
    uniforms.view_proj = ortho_rh(view.left, view.right, view.bottom, view.top, -1.0f, 1.0f);
    uniforms.time = time;

    capture_scope_->beginScope();

    frame_++;
    auto label = std::format("Pong frame {}", frame_);
    auto frameIdx = frame_ % FRAMES_IN_FLIGHT;
    allocator_semaphore_.wait();
    auto& frame = per_frame_[frameIdx];
    frame.command_allocator()->reset();

    command_queue_->wait(drawable);
    command_buffer_->beginCommandBuffer(frame.command_allocator());
    setMetalLabel(command_buffer_.get(), label);

    current_drawable_ = NS::RetainPtr(drawable);
    current_offscreen_encoder_ = NS::RetainPtr(command_buffer_->renderCommandEncoder(render_pass_descriptor_.get()));
    setMetalLabel(current_offscreen_encoder_.get(), "Offscreen encoder");
    current_offscreen_encoder_->setViewport(viewport(RENDER_WIDTH, RENDER_HEIGHT));
    frame.beginFrame(uniforms, primitive2d_argument_table_.get());
    current_offscreen_encoder_->setArgumentTable(primitive2d_argument_table_.get(), MTL::RenderStageVertex);
}

void Renderer::endFrame()
{
    assert(current_drawable_ && "endFrame called without matching beginFrame");

    // Flush the frame's accumulated primitives in one instanced draw. 6 verts
    // is the unit-quad triangle list baked into primitive2d_vertex's
    // quad_corners[]; instance count is whatever drawRect/drawCircle pushed
    // into the slot's buffer. When the count is zero we skip the draw
    // entirely — the offscreen pass still runs the clear, and the blit
    // pulls a clean background.
    auto& frame = per_frame_[frame_ % FRAMES_IN_FLIGHT];
    if (frame.num_primitives_used() > 0)
    {
        current_offscreen_encoder_->setRenderPipelineState(primitive2d_pipeline_.get());
        current_offscreen_encoder_->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6),
                                                   NS::UInteger(frame.num_primitives_used()));
    }

    current_offscreen_encoder_->endEncoding();
    current_offscreen_encoder_.reset();

    auto label = std::format("Pong frame {}", frame_);
    auto descriptor = buildRenderPassDescriptor(current_drawable_.get(), CLEAR_COLOR_BLACK);
    auto blit_encoder = NS::RetainPtr(command_buffer_->renderCommandEncoder(descriptor.get()));
    setMetalLabel(blit_encoder.get(), label);
    // MTL4 has no implicit hazard tracking between encoders. The offscreen
    // pass writes render_texture_ at the fragment stage (the clear store)
    // and the blit reads it at the fragment stage — flush + wait so the
    // sample sees committed pixels.
    blit_encoder->barrierAfterQueueStages(MTL::StageFragment, MTL::StageFragment, MTL4::VisibilityOptionDevice);
    blit_encoder->setRenderPipelineState(blit_pipeline_.get());
    blit_encoder->setArgumentTable(blit_argument_table_.get(), MTL::RenderStageFragment);
    blit_encoder->setViewport(viewport(current_drawable_->texture()->width(), current_drawable_->texture()->height()));
    blit_encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    blit_encoder->endEncoding();

    command_buffer_->endCommandBuffer();

    // Feedback handler fires after the GPU finishes this commit; signaling
    // the allocator semaphore there is what frees the slot for reuse N
    // frames later. Capturing &allocator_semaphore_ is safe — Renderer
    // outlives any in-flight frame.
    auto opts = NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    auto* sem = &allocator_semaphore_;
    opts->addFeedbackHandler(^(MTL4::CommitFeedback*) { sem->signal(); });

    MTL4::CommandBuffer* bufs[] = {command_buffer_.get()};
    command_queue_->commit(bufs, 1, opts.get());

    // Under CAMetalDisplayLink, presentAtTime() raises — the slot is already
    // baked into the drawable.
    command_queue_->signalDrawable(current_drawable_.get());
    current_drawable_->present();

    current_drawable_.reset();

    capture_scope_->endScope();
}

Renderer::PerFrame::PerFrame(MTL::Device* device, ssize_t num2dPrimitives)
    : num2dPrimitives_used_(0), num2dPrimitives_capacity_(num2dPrimitives)
{
    auto desc = NS::TransferPtr(MTL4::CommandAllocatorDescriptor::alloc()->init());
    setMetalLabel(desc.get(), "Pong per-frame allocator");
    NS::Error* err = nullptr;
    command_allocator_ = NS::TransferPtr(device->newCommandAllocator(desc.get(), &err));
    assert(command_allocator_ && "newCommandAllocator failed");

    static_assert(sizeof(Uniforms) % alignof(Primitive2D) == 0);

    buffer_ = NS::TransferPtr(
        device->newBuffer(sizeof(Uniforms) + num2dPrimitives * sizeof(Primitive2D), MTL::StorageModeShared));
}

void Renderer::drawRect(simd::float2 center, simd::float2 half_extents, simd::float4 color)
{
    assert(current_drawable_ && "drawRect called outside beginFrame/endFrame");
    Primitive2D p{
        .center = center,
        .half_extents = half_extents,
        .color_rgba = pack_rgba(color),
        .type_and_flags = static_cast<uint32_t>(ShapeType::Rect),
    };
    per_frame_[frame_ % FRAMES_IN_FLIGHT].addPrimitive(p);
}

void Renderer::PerFrame::addPrimitive(const Primitive2D& primitive)
{
    assert(num2dPrimitives_used_ < num2dPrimitives_capacity_ && "primitive2d buffer full — bump NUM_2D_PRIMITIVES");

    // Layout in the shared buffer: [Uniforms][Primitive2D[0]][Primitive2D[1]]...
    // The static_assert in PerFrame's ctor guarantees the Primitive2D array
    // starts at a properly aligned offset, so the cast is safe.
    auto* base = static_cast<std::byte*>(buffer_->contents());
    auto* dst = base + sizeof(Uniforms) + num2dPrimitives_used_ * sizeof(Primitive2D);
    *reinterpret_cast<Primitive2D*>(dst) = primitive;

    num2dPrimitives_used_++;
}

void Renderer::PerFrame::beginFrame(const Uniforms& uniforms, MTL4::ArgumentTable* argument_table)
{
    // Reset the primitive counter so writes from the previous frame on this
    // slot are overwritten in-place rather than appended past them.
    num2dPrimitives_used_ = 0;

    // Uniforms sits at offset 0 — see PerFrame's ctor for the layout. The
    // buffer is StorageModeShared so the CPU write is visible to the GPU as
    // soon as the command buffer encodes a read against it.
    *static_cast<Uniforms*>(buffer_->contents()) = uniforms;

    // gpuAddress is stable for the buffer's lifetime, but the argument table
    // is shared across slots — re-point its slots at *this* slot's buffer.
    auto base = buffer_->gpuAddress();
    argument_table->setAddress(base, UNIFORMS_BUFFER_SLOT);
    argument_table->setAddress(base + sizeof(Uniforms), PRIMITIVE2D_BUFFER_SLOT);
}

void Renderer::PerFrame::addToResidencySet(MTL::ResidencySet* residency_set) const
{
    residency_set->addAllocation(buffer_.get());
}
