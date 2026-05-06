import Metal
import QuartzCore

/// Issues GPU work for a single frame using Metal 4. Owns the command
/// queue, a reusable command buffer + allocator, the engine's substrate
/// shader library, the game's shader library, a `MTL4Compiler` for
/// pipeline creation, an `MTL4ArgumentTable` for resource binding, a
/// `MTLResidencySet` keeping the upload buffer resident on the queue,
/// and a pipeline-state cache keyed by the function/format tuple.
///
/// Frame lifecycle is driven by `GameEngine.update(dt:drawable:passDescriptor:)`:
/// the host hands the engine a drawable + render-pass descriptor, the
/// engine calls `beginFrame` to open an encoder, the game's `update`
/// issues draws against `ctx.renderer`, and the engine calls `endFrame`
/// to close, schedule presentation, and commit.
///
/// Concurrency: a single in-flight frame for v1 — the inflight
/// semaphore gates `beginFrame` so we never overwrite the upload buffer
/// or reset the allocator while the GPU is still reading. Triple-buffer
/// (3 allocators / 3 upload buffers / sema=3) is a follow-up if CPU-GPU
/// pipelining ever becomes a measurable bottleneck.
@MainActor
public final class Renderer {
    private let device: MTLDevice
    private let queue: any MTL4CommandQueue
    private let allocator: any MTL4CommandAllocator
    private let commandBuffer: any MTL4CommandBuffer
    private let compiler: any MTL4Compiler
    private let argumentTable: any MTL4ArgumentTable
    private let residencySet: any MTLResidencySet
    private let uploadBuffer: UploadBuffer
    private let inflightSemaphore = DispatchSemaphore(value: 1)
    private let engineLibrary: MTLLibrary
    private let gameLibrary: MTLLibrary?
    private var pipelines: [PipelineKey: MTLRenderPipelineState] = [:]

    /// Per-frame upload buffer capacity. 64 KB is overkill for a fullscreen
    /// quad's uniforms but cheap, and gives headroom for the mesh path
    /// (per-draw model matrices etc.) coming next.
    private static let uploadBufferCapacity = 64 * 1024

    // Per-frame state, valid only between begin/endFrame.
    private var encoder: (any MTL4RenderCommandEncoder)?
    private var currentDrawable: CAMetalDrawable?
    private var currentColorFormat: MTLPixelFormat = .invalid

    /// Boot-time failures (no command queue, no engine metallib, etc.) are
    /// fatal — nothing useful can happen without these and recovery isn't
    /// meaningful. `gameLibrary` is allowed to be nil for games that don't
    /// yet ship `.metal` files; draws that need it will trap with a clear
    /// message.
    public init(device: MTLDevice, gameLibrary: MTLLibrary?) {
        self.device = device
        self.gameLibrary = gameLibrary

        // `makeMTL4CommandQueue` raises an uncatchable Obj-C NSException
        // (despite the header documenting a nullable return) when the
        // device doesn't support Metal 4 — common on virtualized GPUs.
        // Preflight with `supportsFamily(.metal4)` so we trap with a
        // clean message.
        guard device.supportsFamily(.metal4) else {
            fatalError("Renderer: \(device.name) does not support MTLGPUFamilyMetal4. Virtualized GPUs (e.g., CI runners) typically lack support.")
        }

        guard let queue = device.makeMTL4CommandQueue() else {
            fatalError("Renderer: device.makeMTL4CommandQueue() returned nil")
        }
        self.queue = queue

        let allocDesc = MTL4CommandAllocatorDescriptor()
        allocDesc.label = "Renderer.allocator"
        guard let allocator = try? device.makeCommandAllocator(descriptor: allocDesc) else {
            fatalError("Renderer: makeCommandAllocator failed")
        }
        self.allocator = allocator

        guard let cb = device.makeCommandBuffer() else {
            fatalError("Renderer: device.makeCommandBuffer() (MTL4) returned nil")
        }
        cb.label = "Renderer.commandBuffer"
        self.commandBuffer = cb

        let compilerDesc = MTL4CompilerDescriptor()
        compilerDesc.label = "Renderer.compiler"
        guard let compiler = try? device.makeCompiler(descriptor: compilerDesc) else {
            fatalError("Renderer: makeCompiler failed")
        }
        self.compiler = compiler

        // Argument table sized for v1's needs: a small handful of buffer
        // slots, no textures or samplers yet. Bump when phase 1's mesh
        // path adds vertex/fragment textures.
        let argDesc = MTL4ArgumentTableDescriptor()
        argDesc.maxBufferBindCount = 8
        argDesc.maxTextureBindCount = 0
        argDesc.maxSamplerStateBindCount = 0
        argDesc.initializeBindings = true
        argDesc.label = "Renderer.argumentTable"
        guard let argTable = try? device.makeArgumentTable(descriptor: argDesc) else {
            fatalError("Renderer: makeArgumentTable failed")
        }
        self.argumentTable = argTable

        // Single shared upload buffer for inline uniforms. With the v1
        // single-in-flight gate, no risk of overlap. `.storageModeShared`
        // gives us a CPU-writable pointer with no blit needed on Apple
        // Silicon's unified memory.
        self.uploadBuffer = UploadBuffer(
            device: device,
            length: Self.uploadBufferCapacity,
            label: "Renderer.uploadBuffer"
        )

        let resDesc = MTLResidencySetDescriptor()
        resDesc.label = "Renderer.residencySet"
        resDesc.initialCapacity = 4
        guard let resSet = try? device.makeResidencySet(descriptor: resDesc) else {
            fatalError("Renderer: makeResidencySet failed")
        }
        self.residencySet = resSet

        guard let engineLib = try? device.makeDefaultLibrary(bundle: .module) else {
            fatalError("Renderer: failed to load engine metallib from Bundle.module. Ensure the Metal toolchain is installed (xcodebuild -downloadComponent MetalToolchain) and the app is built via Xcode.")
        }
        self.engineLibrary = engineLib

        // Resources must be added + the set committed before the queue
        // can use them. The queue holds the residency set across all
        // submissions until `removeResidencySet` is called.
        residencySet.addAllocation(uploadBuffer.buffer)
        residencySet.commit()
        queue.addResidencySet(residencySet)
    }

    /// Open a render pass for the frame. `passDescriptor` carries the
    /// load/store actions and clear color; the encoder applies them when
    /// it begins. `drawable` is optional — pass nil for offscreen render
    /// passes (tests, render-to-texture). Internal — only `GameEngine`
    /// (or test code in this module) should call this.
    func beginFrame(passDescriptor: MTL4RenderPassDescriptor, drawable: CAMetalDrawable? = nil) {
        // Block if a previous frame is still in flight on the GPU. After
        // this returns, the upload buffer + allocator memory is safe to
        // reuse. `addCompletedHandler` on commit signals the semaphore
        // back to 1.
        inflightSemaphore.wait()

        // Reset the allocator's memory pool, then bind the cb to it for
        // a fresh recording. This is the key MTL4 lifecycle change —
        // command buffers are reusable, allocators own the backing memory.
        allocator.reset()
        commandBuffer.beginCommandBuffer(allocator: allocator)

        // Tell the queue not to start GPU work that targets `drawable`
        // until the drawable is acquired and ready. In Metal 4 drawable
        // sync is explicit; the implicit pre-render wait that classic
        // Metal did is gone.
        if let drawable {
            queue.waitForDrawable(drawable)
        }

        guard let enc = commandBuffer.makeRenderCommandEncoder(descriptor: passDescriptor) else {
            fatalError("Renderer: makeRenderCommandEncoder returned nil")
        }
        encoder = enc
        currentDrawable = drawable
        // PSOs are tied to the color attachment's pixel format. Cache by
        // it so the same fragment shader gets distinct PSOs across formats.
        let target = passDescriptor.colorAttachments[0].texture
        currentColorFormat = target?.pixelFormat ?? .invalid
        // Metal 4 doesn't auto-infer a viewport from the color attachment
        // the way classic Metal did; without this call the rasterizer
        // produces fragments but they don't land on the target texture.
        if let target {
            enc.setViewport(MTLViewport(
                originX: 0, originY: 0,
                width: Double(target.width), height: Double(target.height),
                znear: 0, zfar: 1
            ))
        }
        uploadBuffer.clear()
    }

    /// Close the encoder, schedule presentation if a drawable was bound,
    /// and submit the frame. Returns a `FrameCompletion` token that lets
    /// callers (tests) block until the GPU finishes; production callers
    /// can ignore the return value.
    @discardableResult
    func endFrame() -> FrameCompletion {
        guard let encoder else {
            fatalError("Renderer.endFrame called without a matching beginFrame")
        }
        encoder.endEncoding()
        commandBuffer.endCommandBuffer()

        // Two semaphores: `inflightSemaphore` is the engine's frame gate
        // (released so the *next* `beginFrame` can proceed); `completed`
        // is exposed to the caller for explicit waits (test path). The
        // feedback handler fires once on the queue's feedback dispatch
        // queue after the GPU finishes the submitted command buffers.
        let completed = DispatchSemaphore(value: 0)
        let opts = MTL4CommitOptions()
        opts.addFeedbackHandler { [inflightSemaphore] _ in
            inflightSemaphore.signal()
            completed.signal()
        }

        queue.commit([commandBuffer], options: opts)

        // Metal 4 splits what classic `MTLCommandBuffer.presentDrawable(_:)`
        // did in one call: `signalDrawable` tells the queue "rendering
        // targeting this drawable is complete," and only then is it legal
        // to call `drawable.present()` to actually schedule presentation.
        // Skipping the present() call leaves the drawable rendered-to but
        // never displayed — symptom is a blank window.
        if let drawable = currentDrawable {
            queue.signalDrawable(drawable)
            drawable.present()
        }

        self.encoder = nil
        self.currentDrawable = nil
        self.currentColorFormat = .invalid

        return FrameCompletion(semaphore: completed)
    }

    /// Issues a single fullscreen draw using the engine's `fullscreen_vertex`
    /// shader paired with a game-supplied fragment shader. The vertex shader
    /// synthesizes corners from `[[vertex_id]]`, so no vertex buffer is
    /// bound. Uniforms are uploaded into the per-frame upload buffer and
    /// bound via the argument table at fragment buffer index 0.
    ///
    /// `U`'s memory layout must match the fragment shader's
    /// `constant U& [[buffer(0)]]` — Swift can't verify the cross-language
    /// layout, so it's a contract.
    public func drawFullscreenQuad<U: BitwiseCopyable>(fragmentShader: String, uniforms: U) {
        guard let encoder else {
            fatalError("Renderer.drawFullscreenQuad called outside begin/endFrame")
        }
        let pso = pipelineState(forFragment: fragmentShader, colorFormat: currentColorFormat)
        encoder.setRenderPipelineState(pso)

        let address = uploadBuffer.allocate(uniforms)
        argumentTable.setAddress(address, index: 0)
        encoder.setArgumentTable(argumentTable, stages: .fragment)

        // Triangle strip covering NDC: 4 verts, 2 triangles. Vertex shader
        // synthesizes positions from vertex_id.
        encoder.drawPrimitives(primitiveType: .triangleStrip, vertexStart: 0, vertexCount: 4)
    }

    private func pipelineState(forFragment name: String, colorFormat: MTLPixelFormat) -> MTLRenderPipelineState {
        let key = PipelineKey(fragmentFunction: name, colorPixelFormat: colorFormat)
        if let cached = pipelines[key] { return cached }

        guard let library = gameLibrary else {
            fatalError("Renderer: drawFullscreenQuad('\(name)') requires a game shader library, but none was loaded. Add a `.metal` file to the app target.")
        }

        let vertexFnDesc = MTL4LibraryFunctionDescriptor()
        vertexFnDesc.name = "fullscreen_vertex"
        vertexFnDesc.library = engineLibrary

        let fragmentFnDesc = MTL4LibraryFunctionDescriptor()
        fragmentFnDesc.name = name
        fragmentFnDesc.library = library

        let desc = MTL4RenderPipelineDescriptor()
        desc.vertexFunctionDescriptor = vertexFnDesc
        desc.fragmentFunctionDescriptor = fragmentFnDesc
        desc.colorAttachments[0].pixelFormat = colorFormat

        let pso: MTLRenderPipelineState
        do {
            pso = try compiler.makeRenderPipelineState(descriptor: desc)
        } catch {
            fatalError("Renderer: makeRenderPipelineState failed for '\(name)' @ \(colorFormat): \(error)")
        }
        pipelines[key] = pso
        return pso
    }
}

extension Renderer {
    /// `device.supportsFamily(.metal4)`, exposed for test gating. Real
    /// Apple Silicon hardware returns true; virtualized GPUs (CI VMs)
    /// return false.
    public static func isMetal4Supported(on device: MTLDevice) -> Bool {
        device.supportsFamily(.metal4)
    }
}

/// Returned by `Renderer.endFrame()`. Production callers ignore it; tests
/// call `waitUntilCompleted()` to block until the GPU finishes the frame
/// before reading back render-target pixels.
public struct FrameCompletion {
    let semaphore: DispatchSemaphore

    public func waitUntilCompleted() {
        semaphore.wait()
    }
}

/// PSO identity. Vertex function is fixed at `fullscreen_vertex` for v1 —
/// add it to the key when a second substrate vertex shader appears.
private struct PipelineKey: Hashable {
    let fragmentFunction: String
    let colorPixelFormat: MTLPixelFormat
}
