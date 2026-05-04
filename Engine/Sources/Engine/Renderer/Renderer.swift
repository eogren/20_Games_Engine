import Metal
import QuartzCore

/// Issues GPU work for a single frame. Owns the command queue, the engine's
/// substrate shader library (loaded from this package's resource bundle),
/// the game's shader library (passed in at boot), and a pipeline-state
/// cache keyed by the function/format tuple.
///
/// Frame lifecycle is driven by `GameEngine.update(dt:)`: the host hands
/// the engine a drawable + render-pass descriptor, the engine calls
/// `beginFrame` to open an encoder, the game's `update` issues draws
/// against `ctx.renderer`, and the engine calls `endFrame` to close,
/// present, and commit.
@MainActor
public final class Renderer {
    private let device: MTLDevice
    private let queue: MTLCommandQueue
    private let engineLibrary: MTLLibrary
    private let gameLibrary: MTLLibrary?
    private var pipelines: [PipelineKey: MTLRenderPipelineState] = [:]

    // Per-frame state, valid only between begin/endFrame.
    private var commandBuffer: MTLCommandBuffer?
    private var encoder: MTLRenderCommandEncoder?
    private var currentDrawable: CAMetalDrawable?
    private var currentColorFormat: MTLPixelFormat = .invalid

    /// Boot-time failures (no command queue, no engine metallib) are fatal —
    /// nothing useful can happen without these and recovery isn't meaningful.
    /// `gameLibrary` is allowed to be nil for games that don't yet ship
    /// `.metal` files; draws that need it will trap with a clear message.
    public init(device: MTLDevice, gameLibrary: MTLLibrary?) {
        self.device = device
        self.gameLibrary = gameLibrary

        guard let queue = device.makeCommandQueue() else {
            fatalError("Renderer: device.makeCommandQueue() returned nil")
        }
        self.queue = queue

        do {
            self.engineLibrary = try device.makeDefaultLibrary(bundle: .module)
        } catch {
            fatalError("Renderer: failed to load engine metallib from Bundle.module — \(error). Ensure the Metal toolchain is installed (xcodebuild -downloadComponent MetalToolchain) and the app is built via Xcode.")
        }
    }

    /// Open a render pass for the frame. `passDescriptor` carries the
    /// load/store actions and clear color; the encoder applies them when
    /// it begins. `drawable` is optional — pass nil for offscreen render
    /// passes (tests, render-to-texture). Internal — only `GameEngine`
    /// (or test code in this module) should call this.
    func beginFrame(passDescriptor: MTLRenderPassDescriptor, drawable: CAMetalDrawable? = nil) {
        guard let cb = queue.makeCommandBuffer() else {
            fatalError("Renderer: makeCommandBuffer returned nil")
        }
        guard let enc = cb.makeRenderCommandEncoder(descriptor: passDescriptor) else {
            fatalError("Renderer: makeRenderCommandEncoder returned nil")
        }
        commandBuffer = cb
        encoder = enc
        currentDrawable = drawable
        // PSOs are tied to the color attachment's pixel format. Cache by
        // it so the same fragment shader gets distinct PSOs across formats.
        currentColorFormat = passDescriptor.colorAttachments[0].texture?.pixelFormat ?? .invalid
    }

    /// Close the encoder, schedule presentation if a drawable was bound,
    /// and submit the frame. Returns the committed command buffer so
    /// callers (tests) can `waitUntilCompleted()` before reading back
    /// pixels. Production callers can ignore the return value.
    @discardableResult
    func endFrame() -> MTLCommandBuffer {
        guard let commandBuffer, let encoder else {
            fatalError("Renderer.endFrame called without a matching beginFrame")
        }
        encoder.endEncoding()
        if let drawable = currentDrawable {
            commandBuffer.present(drawable)
        }
        commandBuffer.commit()
        self.commandBuffer = nil
        self.encoder = nil
        self.currentDrawable = nil
        self.currentColorFormat = .invalid
        return commandBuffer
    }

    /// Issues a single fullscreen draw using the engine's `fullscreen_vertex`
    /// shader paired with a game-supplied fragment shader. The vertex shader
    /// synthesizes corners from `[[vertex_id]]`, so no vertex buffer is
    /// bound. Uniforms are passed inline via `setFragmentBytes` at buffer
    /// index 0.
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

        // Inline uniform path — fine for ≤4KB structs. Larger uniforms
        // would want a buffer ring; we don't have one yet.
        var copy = uniforms
        encoder.setFragmentBytes(&copy, length: MemoryLayout<U>.stride, index: 0)

        // Triangle strip covering NDC: 4 verts, 2 triangles. Vertex shader
        // synthesizes positions from vertex_id.
        encoder.drawPrimitives(type: .triangleStrip, vertexStart: 0, vertexCount: 4)
    }

    private func pipelineState(forFragment name: String, colorFormat: MTLPixelFormat) -> MTLRenderPipelineState {
        let key = PipelineKey(fragmentFunction: name, colorPixelFormat: colorFormat)
        if let cached = pipelines[key] { return cached }

        guard let vertexFn = engineLibrary.makeFunction(name: "fullscreen_vertex") else {
            fatalError("Renderer: engine library is missing 'fullscreen_vertex' — engine metallib is broken")
        }
        guard let library = gameLibrary else {
            fatalError("Renderer: drawFullscreenQuad('\(name)') requires a game shader library, but none was loaded. Add a `.metal` file to the app target.")
        }
        guard let fragmentFn = library.makeFunction(name: name) else {
            fatalError("Renderer: game library has no fragment function named '\(name)'")
        }

        let desc = MTLRenderPipelineDescriptor()
        desc.vertexFunction = vertexFn
        desc.fragmentFunction = fragmentFn
        desc.colorAttachments[0].pixelFormat = colorFormat

        let pso: MTLRenderPipelineState
        do {
            pso = try device.makeRenderPipelineState(descriptor: desc)
        } catch {
            fatalError("Renderer: makeRenderPipelineState failed for '\(name)' @ \(colorFormat): \(error)")
        }
        pipelines[key] = pso
        return pso
    }
}

/// PSO identity. Vertex function is fixed at `fullscreen_vertex` for v1 —
/// add it to the key when a second substrate vertex shader appears.
private struct PipelineKey: Hashable {
    let fragmentFunction: String
    let colorPixelFormat: MTLPixelFormat
}
