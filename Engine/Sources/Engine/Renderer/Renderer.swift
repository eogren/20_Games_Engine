import Metal
import MetalKit
@preconcurrency import ModelIO
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
    private let defaultDepthState: MTLDepthStencilState
    private var pipelines: [PipelineKey: MTLRenderPipelineState] = [:]
    /// Identity set of MTLBuffers already added to `residencySet`, so
    /// `register(_:)` can dedup. Keyed by `ObjectIdentifier` because
    /// `MTKMeshBufferAllocator` frequently packs a mesh's vertex and
    /// index data into the same underlying MTLBuffer (distinguished by
    /// offset), and we only want one `addAllocation` call per instance.
    private var knownAllocations: Set<ObjectIdentifier> = []
    
    /// Per-frame upload buffer capacity. 64 KB is overkill for a fullscreen
    /// quad's uniforms but cheap, and gives headroom for the mesh path
    /// (per-draw model matrices etc.) coming next.
    private static let uploadBufferCapacity = 64 * 1024

    // Per-frame state, valid only between begin/endFrame.
    private var encoder: (any MTL4RenderCommandEncoder)?
    private var currentDrawable: CAMetalDrawable?
    private var currentColorFormat: MTLPixelFormat = .invalid
    /// Sample count of the bound color attachment (1 = no MSAA, 4 = 4× MSAA).
    /// PSOs must declare a matching `rasterSampleCount`, so this is part of
    /// the PSO cache key. Read from the pass descriptor's color texture in
    /// `beginFrame` — letting the platform decide MSAA vs not without the
    /// renderer needing a top-level config knob.
    private var currentSampleCount: Int = 1
    private var currentDrawableSize: SIMD2<Float>?
    /// Game-time seconds — the host accumulates `dt` across per-tick
    /// calls and supplies it at `beginFrame`. Game-time, not wall-clock:
    /// if the simulation pauses or slows by passing a smaller `dt`, this
    /// advances in lockstep, so shader animation pauses with the game.
    /// Folded into `MeshGlobalUniform` lazily on the first `drawMesh` of
    /// the frame.
    private var currentTime: Float = 0
    /// View-projection stashed by `setCamera`; nil until set. The lazy
    /// upload in `drawMesh` traps if it sees nil here, which is what
    /// the "call setCamera before drawMesh" contract becomes.
    private var currentViewProjection: simd_float4x4?
    /// GPU address of the per-frame mesh global uniform once it's been
    /// uploaded for this frame. Nil until the first `drawMesh` allocates
    /// it from `currentTime` + `currentViewProjection`. Switching cameras
    /// mid-frame (split-screen, PIP) sets this back to nil so the next
    /// `drawMesh` re-uploads with the new VP — time isn't re-uploaded
    /// redundantly because it lives on the same struct and only changes
    /// frame-to-frame.
    private var meshGlobalUniformsAddr: MTLGPUAddress?

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

        let depthDesc = MTLDepthStencilDescriptor()
        // `.lessEqual` (not `.less`) so the substrate fullscreen quad —
        // which emits z=1 to sit at the far plane — still draws against
        // a `clearDepth = 1.0` buffer.
        depthDesc.depthCompareFunction = .lessEqual
        depthDesc.isDepthWriteEnabled = true
        depthDesc.label = "Renderer.defaultDepthState"
        guard let depthState = device.makeDepthStencilState(descriptor: depthDesc) else {
            fatalError("Renderer: makeDepthStencilState failed")
        }
        self.defaultDepthState = depthState

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
    ///
    /// Camera state is the game's responsibility: call `setCamera(viewProjection:)`
    /// before the first `drawMesh` of the frame. Keeping it out of
    /// `beginFrame` lets the game compose its view-projection however
    /// it wants (and switch cameras mid-frame for split-screen / PIP)
    /// without the renderer needing to know about Transform shape.
    ///
    /// `time` is engine-side game-time (accumulated `dt`, not wall-clock)
    /// and the renderer folds it into the per-frame mesh global uniform
    /// alongside the game-supplied view-projection. Defaults to 0 for
    /// tests that don't care about animation; production callers
    /// (`GameEngine`) pass an accumulator.
    func beginFrame(passDescriptor: MTL4RenderPassDescriptor,
                    drawable: CAMetalDrawable? = nil,
                    time: Float = 0) {
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
        currentSampleCount = target?.sampleCount ?? 1
        // Always-on depth: when the pass has a depth attachment, bind the
        // shared lessEqual + write-enabled state for every draw in this
        // frame. Skip the bind when there's no depth attachment so the
        // existing color-only test passes don't drag in depth state they
        // never asked for.
        if passDescriptor.depthAttachment.texture != nil {
            enc.setDepthStencilState(defaultDepthState)
        }
        // Metal 4 doesn't auto-infer a viewport from the color attachment
        // the way classic Metal did; without this call the rasterizer
        // produces fragments but they don't land on the target texture.
        if let target {
            currentDrawableSize = SIMD2<Float>(Float(target.width), Float(target.height))
            enc.setViewport(MTLViewport(
                originX: 0, originY: 0,
                width: Double(target.width), height: Double(target.height),
                znear: 0, zfar: 1
            ))
        } else {
            currentDrawableSize = nil
        }
        uploadBuffer.clear()
        // Each frame must re-establish camera state via `setCamera`;
        // clearing here means a forgotten call traps in `drawMesh` rather
        // than silently reusing last frame's view-projection.
        self.currentViewProjection = nil
        self.meshGlobalUniformsAddr = nil
        self.currentTime = time
    }

    /// Upload a view-projection matrix and bind it as the per-frame mesh
    /// global uniform. Must be called between `beginFrame` and the first
    /// `drawMesh` of the frame. Calling again replaces the binding, so
    /// switching cameras between draw groups (split-screen, picture-in-
    /// picture) is just calling `setCamera` again before the next group.
    public func setCamera(viewProjection: simd_float4x4) {
        guard encoder != nil else {
            fatalError("Renderer.setCamera called outside begin/endFrame")
        }
        // Stash only — the actual upload happens lazily on the first
        // `drawMesh` that needs it. Invalidate any previously-uploaded
        // global uniform so a mid-frame camera swap re-uploads with the
        // new VP.
        currentViewProjection = viewProjection
        meshGlobalUniformsAddr = nil
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
        self.currentSampleCount = 1
        self.currentDrawableSize = nil

        return FrameCompletion(semaphore: completed)
    }

    /// The current frame's render-target size in pixels, valid between
    /// `beginFrame` and `endFrame`. Use to derive a correctly-aspected
    /// projection matrix:
    /// `let aspect = renderer.drawableSize.x / renderer.drawableSize.y`.
    /// Traps if accessed outside a frame, or during a frame whose pass
    /// descriptor has no color attachment texture (which the renderer's
    /// other paths already disallow in practice).
    public var drawableSize: SIMD2<Float> {
        guard let size = currentDrawableSize else {
            fatalError("Renderer.drawableSize: only valid between beginFrame and endFrame")
        }
        return size
    }

    /// Register a mesh's GPU allocations with the renderer's residency
    /// set so the queue keeps them resident across submissions. Must be
    /// called before the first `drawMesh(_:)` referencing this mesh —
    /// without it, the encoder records draws against a non-resident
    /// allocation and the GPU faults at submission. Idempotent: each
    /// underlying MTLBuffer is added at most once across all `register`
    /// calls for the lifetime of this renderer.
    public func register(_ mesh: MTKMesh) {
        var added = false
        for vbuf in mesh.vertexBuffers {
            if knownAllocations.insert(ObjectIdentifier(vbuf.buffer)).inserted {
                residencySet.addAllocation(vbuf.buffer)
                added = true
            }
        }
        for submesh in mesh.submeshes {
            let buf = submesh.indexBuffer.buffer
            if knownAllocations.insert(ObjectIdentifier(buf)).inserted {
                residencySet.addAllocation(buf)
                added = true
            }
        }
        // Commit only if we actually added something — `commit()` is the
        // expensive step, not `addAllocation`. Re-registering an already-
        // known mesh is a no-op.
        if added { residencySet.commit() }
    }

    /// Draw the given mesh with the appropriate transform. Issues one
    /// indexed draw per `MTKSubmesh` — submeshes share the parent mesh's
    /// vertex buffer but each owns its index buffer and primitive type.
    ///
    /// Follow-ups:
    /// 1. Instanced drawing when multiple copies of the same mesh recur
    /// 2. Game-driven uniforms in a different argument-table slot
    public func drawMesh(_ mesh: MTKMesh, fragmentShader: String, meshTransform: Transform) {
        guard let encoder else {
            fatalError("Renderer.drawMesh called outside begin/endFrame")
        }
        let pso = pipelineStateForMesh(
            fragmentShader: fragmentShader,
            colorFormat: currentColorFormat,
            sampleCount: currentSampleCount
        )
        encoder.setRenderPipelineState(pso)

        // Vertex buffer at index 0. MTKMesh may pack into a parent allocation
        // with a non-zero `offset`; bake it into the GPU address so submesh
        // indices remain 0-based against the bound vertex base.
        let vbuf = mesh.vertexBuffers.first!
        argumentTable.setAddress(vbuf.buffer.gpuAddress + UInt64(vbuf.offset), index: 0)

        // Per-frame global uniforms at buffer 1. Allocated lazily on the
        // first `drawMesh` after each `setCamera` so cameras can swap
        // mid-frame without bundling time re-uploads into setCamera.
        let globalUniformsAddr: MTLGPUAddress
        if let cached = meshGlobalUniformsAddr {
            globalUniformsAddr = cached
        } else {
            guard let viewProjection = currentViewProjection else {
                fatalError("Renderer.drawMesh: call setCamera(viewProjection:) before drawMesh in this frame")
            }
            let uniform = MeshGlobalUniform(
                viewProjectionMatrix: viewProjection,
                time: currentTime
            )
            globalUniformsAddr = uploadBuffer.allocate(uniform)
            meshGlobalUniformsAddr = globalUniformsAddr
        }
        argumentTable.setAddress(globalUniformsAddr, index: 1)

        // Per-mesh model uniforms at buffer 2
        let meshUniforms = MeshModelUniform(modelMatrix: meshTransform.matrix)
        let meshUniformsAddress = uploadBuffer.allocate(meshUniforms)
        argumentTable.setAddress(meshUniformsAddress, index: 2)

        encoder.setArgumentTable(argumentTable, stages: [.vertex, .fragment])

        for submesh in mesh.submeshes {
            let ibuf = submesh.indexBuffer
            encoder.drawIndexedPrimitives(
                primitiveType: submesh.primitiveType,
                indexCount: submesh.indexCount,
                indexType: submesh.indexType,
                indexBuffer: ibuf.buffer.gpuAddress + UInt64(ibuf.offset),
                indexBufferLength: ibuf.length - ibuf.offset
            )
        }
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
        let pso = pipelineStateForFullscreenQuad(
            fragmentShader: fragmentShader,
            colorFormat: currentColorFormat,
            sampleCount: currentSampleCount
        )
        encoder.setRenderPipelineState(pso)

        let address = uploadBuffer.allocate(uniforms)
        argumentTable.setAddress(address, index: 0)
        encoder.setArgumentTable(argumentTable, stages: .fragment)

        // Triangle strip covering NDC: 4 verts, 2 triangles. Vertex shader
        // synthesizes positions from vertex_id.
        encoder.drawPrimitives(primitiveType: .triangleStrip, vertexStart: 0, vertexCount: 4)
    }

    /// PSO for the engine's `fullscreen_vertex` paired with a game-supplied
    /// fragment shader. No vertex buffer / vertex descriptor — the vertex
    /// shader synthesizes corners from `[[vertex_id]]`.
    private func pipelineStateForFullscreenQuad(
        fragmentShader: String, colorFormat: MTLPixelFormat, sampleCount: Int
    ) -> MTLRenderPipelineState {
        let key = PipelineKey(
            vertexFunction: "fullscreen_vertex",
            fragmentFunction: fragmentShader,
            colorPixelFormat: colorFormat,
            sampleCount: sampleCount
        )
        return cachedPipelineState(key: key) {
            let desc = MTL4RenderPipelineDescriptor()
            desc.vertexFunctionDescriptor = libraryFn("fullscreen_vertex", in: engineLibrary)
            desc.fragmentFunctionDescriptor = libraryFn(fragmentShader, in: requireGameLibrary("drawFullscreenQuad"))
            desc.colorAttachments[0].pixelFormat = colorFormat
            desc.rasterSampleCount = sampleCount
            return desc
        }
    }

    /// PSO for the engine's `mesh_vertex` paired with a game-supplied
    /// fragment shader. Vertex layout is fixed: position (float3) + UV
    /// (float2) interleaved in buffer 0, matching `MeshVertexIn` in
    /// `EngineShaderTypes.h`.
    private func pipelineStateForMesh(
        fragmentShader: String, colorFormat: MTLPixelFormat, sampleCount: Int
    ) -> MTLRenderPipelineState {
        let key = PipelineKey(
            vertexFunction: "mesh_vertex",
            fragmentFunction: fragmentShader,
            colorPixelFormat: colorFormat,
            sampleCount: sampleCount
        )
        return cachedPipelineState(key: key) {
            let desc = MTL4RenderPipelineDescriptor()
            desc.vertexFunctionDescriptor = libraryFn("mesh_vertex", in: engineLibrary)
            desc.fragmentFunctionDescriptor = libraryFn(fragmentShader, in: requireGameLibrary("drawMesh"))
            desc.vertexDescriptor = Self.mtlMeshVertexDescriptor()
            desc.colorAttachments[0].pixelFormat = colorFormat
            desc.rasterSampleCount = sampleCount
            return desc
        }
    }

    /// Cache lookup + lazy creation + error handling for PSOs. Single
    /// owner of the cache invariant — every PSO method funnels through
    /// this so the dictionary stays consistent.
    private func cachedPipelineState(
        key: PipelineKey,
        buildDescriptor: () -> MTL4RenderPipelineDescriptor
    ) -> MTLRenderPipelineState {
        if let cached = pipelines[key] { return cached }
        let desc = buildDescriptor()
        do {
            let pso = try compiler.makeRenderPipelineState(descriptor: desc)
            pipelines[key] = pso
            return pso
        } catch {
            fatalError("Renderer: makeRenderPipelineState failed for \(key): \(error)")
        }
    }

    private func libraryFn(_ name: String, in library: MTLLibrary) -> MTL4LibraryFunctionDescriptor {
        let d = MTL4LibraryFunctionDescriptor()
        d.name = name
        d.library = library
        return d
    }

    private func requireGameLibrary(_ caller: String) -> MTLLibrary {
        guard let library = gameLibrary else {
            fatalError("Renderer: \(caller) requires a game shader library, but none was loaded. Add a `.metal` file to the app target.")
        }
        return library
    }

    /// MTL form of the mesh layout, derived from `meshVertexDescriptor()`
    /// via MetalKit. One source of truth — both `MeshLoader` (MDL form)
    /// and the mesh PSO (this MTL form) come from the same definition,
    /// so drift is impossible.
    private static func mtlMeshVertexDescriptor() -> MTLVertexDescriptor {
        guard let mtl = MTKMetalVertexDescriptorFromModelIO(meshVertexDescriptor()) else {
            fatalError("Renderer: MTKMetalVertexDescriptorFromModelIO failed for the mesh vertex layout")
        }
        return mtl
    }
}

extension Renderer {
    /// `device.supportsFamily(.metal4)`, exposed for test gating. Real
    /// Apple Silicon hardware returns true; virtualized GPUs (CI VMs)
    /// return false.
    public static func isMetal4Supported(on device: MTLDevice) -> Bool {
        device.supportsFamily(.metal4)
    }

    /// Canonical mesh vertex layout: position (float3) at offset 0 + UV
    /// (float2) at offset 12, interleaved in buffer 0, stride 20. The
    /// contract between `MeshLoader` (which reshapes incoming assets to
    /// it via ModelIO) and the renderer's mesh PSO (which converts to
    /// MTL form for the pipeline descriptor). Both consumers derive
    /// from this — one source of truth, no drift.
    ///
    /// `nonisolated` so non-`@MainActor` callers (`MeshLoader`, tests
    /// running off the main actor) can read it without hop boilerplate.
    /// The builder is pure value construction — no shared state to race.
    public nonisolated static func meshVertexDescriptor() -> MDLVertexDescriptor {
        let d = MDLVertexDescriptor()
        d.attributes[0] = MDLVertexAttribute(
            name: MDLVertexAttributePosition,
            format: .float3,
            offset: 0,
            bufferIndex: 0)
        d.attributes[1] = MDLVertexAttribute(
            name: MDLVertexAttributeTextureCoordinate,
            format: .float2,
            offset: 12,
            bufferIndex: 0)
        d.layouts[0] = MDLVertexBufferLayout(stride: 20)
        return d
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

/// PSO identity: vertex + fragment + color format + sample count. Vertex
/// function name must be in the key now that the renderer ships more than
/// one substrate vertex shader (`fullscreen_vertex`, `mesh_vertex`) — same
/// fragment + format with different vertex shaders is two distinct PSOs.
/// Sample count must be in the key because PSOs encode `rasterSampleCount`
/// and that must match the bound color attachment at draw time, so 1×
/// (e.g., test offscreen targets) and 4× (the platform's MSAA pass) need
/// distinct PSOs.
private struct PipelineKey: Hashable {
    let vertexFunction: String
    let fragmentFunction: String
    let colorPixelFormat: MTLPixelFormat
    let sampleCount: Int
}

/// Mirrors of the Shaders/EngineShaderTypes.h definition
private struct MeshGlobalUniform {
    var viewProjectionMatrix: simd_float4x4
    var time: Float
}

private struct MeshModelUniform {
    var modelMatrix: simd_float4x4
}
