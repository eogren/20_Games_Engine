import Metal

/// Test-only helper that bundles an offscreen color texture with the
/// boilerplate to render into it and read pixels back. Exists so smoke
/// tests can talk about *what* they're rendering without re-deriving
/// "build an MTL4RenderPassDescriptor, getBytes into a [UInt8], decode
/// the pixel format" each time.
struct OffscreenColorTarget {
    let texture: MTLTexture
    let width: Int
    let height: Int

    /// `.bgra8Unorm` matches what `CAMetalLayer` hands the renderer in
    /// production, so the offscreen path tests the same color pipeline
    /// the on-screen path uses.
    init(device: MTLDevice, width: Int, height: Int,
         pixelFormat: MTLPixelFormat = .bgra8Unorm) throws {
        let desc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: pixelFormat,
            width: width,
            height: height,
            mipmapped: false
        )
        desc.usage = [.renderTarget, .shaderRead]
        // .shared is fine on Apple Silicon (unified memory) and lets us
        // call getBytes without a synchronize blit.
        desc.storageMode = .shared
        guard let tex = device.makeTexture(descriptor: desc) else {
            throw OffscreenError.textureAllocationFailed
        }
        self.texture = tex
        self.width = width
        self.height = height
    }

    /// Pass descriptor that targets this texture with `loadAction = .clear`
    /// at the given clear color. Each call returns a fresh descriptor —
    /// MTL4 descriptors are cheap and reusing one across frames is a
    /// bug-magnet. Optionally attaches a depth target with `loadAction =
    /// .clear` at `clearDepth = 1.0` and `storeAction = .dontCare`, so
    /// depth-aware tests get the same one-call ergonomics.
    func clearPass(_ clearColor: MTLClearColor,
                   depth: OffscreenDepthTarget? = nil) -> MTL4RenderPassDescriptor {
        let pass = MTL4RenderPassDescriptor()
        let color = pass.colorAttachments[0]!
        color.texture = texture
        color.loadAction = .clear
        color.storeAction = .store
        color.clearColor = clearColor
        if let depth {
            let d = pass.depthAttachment!
            d.texture = depth.texture
            d.loadAction = .clear
            d.storeAction = .dontCare
            d.clearDepth = 1.0
        }
        return pass
    }

    /// Snapshot the texture's current contents. Call after the GPU has
    /// finished (e.g., after `FrameCompletion.waitUntilCompleted()`); the
    /// returned `Pixels` is a value, decoupled from any further GPU work.
    func readback() -> Pixels {
        let bytesPerRow = width * 4
        var bytes = [UInt8](repeating: 0, count: width * height * 4)
        let region = MTLRegion(
            origin: MTLOrigin(x: 0, y: 0, z: 0),
            size: MTLSize(width: width, height: height, depth: 1)
        )
        texture.getBytes(&bytes, bytesPerRow: bytesPerRow, from: region, mipmapLevel: 0)
        return Pixels(bytes: bytes, width: width, height: height)
    }

    enum OffscreenError: Error {
        case textureAllocationFailed
    }
}

/// CPU-side snapshot of a `.bgra8Unorm` render target. Index by `(x, y)`;
/// row 0 is the top of the framebuffer (`getBytes` returns rows in
/// scan order from the top, matching how Metal's framebuffer y maps to
/// texture y).
struct Pixels {
    let width: Int
    let height: Int
    private let bytes: [UInt8]

    init(bytes: [UInt8], width: Int, height: Int) {
        self.bytes = bytes
        self.width = width
        self.height = height
    }

    subscript(x: Int, y: Int) -> BGRA {
        let off = (y * width + x) * 4
        return BGRA(b: bytes[off], g: bytes[off + 1], r: bytes[off + 2], a: bytes[off + 3])
    }
}

/// One pixel from a `.bgra8Unorm` readback. Channel order matches the
/// in-memory byte order so `pixel.r` reads what you'd expect without
/// the test having to remember "B comes first."
struct BGRA: Equatable {
    let b: UInt8
    let g: UInt8
    let r: UInt8
    let a: UInt8

    static let opaqueBlack = BGRA(b: 0, g: 0, r: 0, a: 255)
}

/// Companion to `OffscreenColorTarget`: a depth render target sized to
/// match its color partner. `.depth32Float` matches what the platform
/// host will use; `.private` storage is fine because tests don't read
/// depth values back, only assert what survives in the *color* target
/// after the depth test runs.
struct OffscreenDepthTarget {
    let texture: MTLTexture
    let width: Int
    let height: Int

    init(device: MTLDevice, width: Int, height: Int,
         pixelFormat: MTLPixelFormat = .depth32Float) throws {
        let desc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: pixelFormat,
            width: width,
            height: height,
            mipmapped: false
        )
        desc.usage = .renderTarget
        desc.storageMode = .private
        guard let tex = device.makeTexture(descriptor: desc) else {
            throw OffscreenColorTarget.OffscreenError.textureAllocationFailed
        }
        self.texture = tex
        self.width = width
        self.height = height
    }
}
