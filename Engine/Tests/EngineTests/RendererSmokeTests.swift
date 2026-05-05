import Metal
import Testing
@testable import Engine

/// Pixel-level smoke tests for the renderer. Render to an offscreen
/// `MTLTexture` (no window, no drawable), commit, wait, read back bytes,
/// assert. Lives here rather than in a screen-capture / window-driven
/// test because the offscreen path is deterministic, headless, and runs
/// on CI without a display server.
///
/// Requires the engine `.metallib` to be available in `Bundle.module`,
/// which means these tests must run via `xcodebuild test` (the SwiftPM
/// CLI doesn't invoke the `metal` compiler).
@Suite struct RendererSmokeTests {
    /// True only when the engine `.metallib` is in `Bundle.module`. False
    /// under `swift test` (CLI doesn't invoke the Metal compiler) and
    /// true under `xcodebuild test` (which does). Tests that construct a
    /// `Renderer` are gated on this so `swift test` stays green.
    static let engineMetalLibraryAvailable: Bool =
        Bundle.module.url(forResource: "default", withExtension: "metallib") != nil

    /// True iff the system device supports Metal 4. Real Apple Silicon
    /// hardware passes; virtualized GPUs (CI VMs) don't. See `Renderer.init`
    /// for why we preflight instead of trusting the documented nil.
    static let metal4Supported: Bool = {
        guard let device = MTLCreateSystemDefaultDevice() else { return false }
        return device.supportsFamily(.metal4)
    }()

    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs engine metallib in Bundle.module (xcodebuild test) and a Metal-4-capable GPU (real hardware, not a CI VM)"))
    @MainActor func clearToBlackFillsTargetWithBlackPixels() throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon CI runner expected")
        let renderer = Renderer(device: device, gameLibrary: nil)

        let width = 64
        let height = 64
        let texDesc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .bgra8Unorm,
            width: width,
            height: height,
            mipmapped: false
        )
        texDesc.usage = [.renderTarget, .shaderRead]
        // .shared is fine on Apple Silicon (unified memory) and lets us
        // call getBytes without a synchronize blit.
        texDesc.storageMode = .shared
        let texture = try #require(device.makeTexture(descriptor: texDesc))

        let pass = MTL4RenderPassDescriptor()
        let color = pass.colorAttachments[0]!
        color.texture = texture
        color.loadAction = .clear
        color.storeAction = .store
        color.clearColor = MTLClearColorMake(0, 0, 0, 1)

        renderer.beginFrame(passDescriptor: pass)
        renderer.endFrame().waitUntilCompleted()

        let bytesPerRow = width * 4
        var pixels = [UInt8](repeating: 0xFF, count: width * height * 4)
        let region = MTLRegion(
            origin: MTLOrigin(x: 0, y: 0, z: 0),
            size: MTLSize(width: width, height: height, depth: 1)
        )
        texture.getBytes(&pixels, bytesPerRow: bytesPerRow, from: region, mipmapLevel: 0)

        // .bgra8Unorm with clearColor (R=0,G=0,B=0,A=1) → bytes (0,0,0,255).
        for y in 0..<height {
            for x in 0..<width {
                let off = (y * width + x) * 4
                #expect(pixels[off] == 0,     "B at (\(x),\(y))")
                #expect(pixels[off + 1] == 0, "G at (\(x),\(y))")
                #expect(pixels[off + 2] == 0, "R at (\(x),\(y))")
                #expect(pixels[off + 3] == 255, "A at (\(x),\(y))")
            }
        }
    }

    /// Catches viewport / rasterizer-shaped bugs that the clear-only test
    /// can't see. A missing `setViewport` in `beginFrame` doesn't affect
    /// `loadAction = .clear` (clears bypass the rasterizer), but it does
    /// stop draws from landing on the target — exactly the regression
    /// that motivated this test. We render a fullscreen quad whose
    /// fragment emits the interpolated uv into red/green and assert the
    /// four corners disagree from each other in the expected directions:
    /// red increases left→right, green increases bottom→top. Any failure
    /// mode (no rasterization, wrong viewport, transposed axes, uniform
    /// color) breaks at least one of those inequalities.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs test metallib + engine metallib in their bundles (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func fullscreenDrawCoversTargetWithUVGradient() throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon hardware expected")

        // The test fragment lives in the test bundle's metallib. Loaded
        // and handed to the Renderer as the "game" library.
        let testLibrary = try #require(
            try? device.makeDefaultLibrary(bundle: .module),
            "couldn't load test metallib from Bundle.module — run via xcodebuild test"
        )
        let renderer = Renderer(device: device, gameLibrary: testLibrary)

        let width = 64
        let height = 64
        let texDesc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: .bgra8Unorm,
            width: width,
            height: height,
            mipmapped: false
        )
        texDesc.usage = [.renderTarget, .shaderRead]
        texDesc.storageMode = .shared
        let texture = try #require(device.makeTexture(descriptor: texDesc))

        let pass = MTL4RenderPassDescriptor()
        let color = pass.colorAttachments[0]!
        color.texture = texture
        color.loadAction = .clear
        color.storeAction = .store
        // Magenta clear, distinguishable from anything the gradient emits.
        // If a corner reads back as clear-color the draw didn't cover it.
        color.clearColor = MTLClearColorMake(1, 0, 1, 1)

        struct UVGradientUniforms { var dummy: Float = 0 }

        renderer.beginFrame(passDescriptor: pass)
        renderer.drawFullscreenQuad(fragmentShader: "uv_gradient",
                                    uniforms: UVGradientUniforms())
        renderer.endFrame().waitUntilCompleted()

        let bytesPerRow = width * 4
        var pixels = [UInt8](repeating: 0xFF, count: width * height * 4)
        let region = MTLRegion(
            origin: MTLOrigin(x: 0, y: 0, z: 0),
            size: MTLSize(width: width, height: height, depth: 1)
        )
        texture.getBytes(&pixels, bytesPerRow: bytesPerRow, from: region, mipmapLevel: 0)

        // Sample one pixel in from each corner to dodge edge-rasterization
        // anomalies. .bgra8Unorm byte order is (B, G, R, A).
        func sample(_ x: Int, _ y: Int) -> (b: UInt8, g: UInt8, r: UInt8, a: UInt8) {
            let off = (y * width + x) * 4
            return (pixels[off], pixels[off + 1], pixels[off + 2], pixels[off + 3])
        }
        // Framebuffer row 0 is the top of the screen. The fullscreen
        // vertex shader emits uv=clip*0.5+0.5 with clip.y=+1 at top, so
        // top-of-framebuffer corresponds to uv.y≈1 (green high).
        let topLeft     = sample(1,         1)
        let topRight    = sample(width - 2, 1)
        let bottomLeft  = sample(1,         height - 2)
        let bottomRight = sample(width - 2, height - 2)

        // Sanity: the draw covered every corner (no clear-color leakage,
        // blue stays 0 from the shader, alpha stays 255).
        for (name, p) in [("top-left", topLeft), ("top-right", topRight),
                          ("bottom-left", bottomLeft), ("bottom-right", bottomRight)] {
            #expect(p.b == 0, "blue at \(name) (clear color leaked through?)")
            #expect(p.a == 255, "alpha at \(name)")
        }

        // Red increases left→right.
        #expect(topLeft.r < 32,      "top-left red should be near 0, got \(topLeft.r)")
        #expect(topRight.r > 223,    "top-right red should be near 255, got \(topRight.r)")
        #expect(bottomLeft.r < 32,   "bottom-left red should be near 0, got \(bottomLeft.r)")
        #expect(bottomRight.r > 223, "bottom-right red should be near 255, got \(bottomRight.r)")

        // Green increases bottom→top.
        #expect(topLeft.g > 223,    "top-left green should be near 255, got \(topLeft.g)")
        #expect(topRight.g > 223,   "top-right green should be near 255, got \(topRight.g)")
        #expect(bottomLeft.g < 32,  "bottom-left green should be near 0, got \(bottomLeft.g)")
        #expect(bottomRight.g < 32, "bottom-right green should be near 0, got \(bottomRight.g)")

        // The cross-pair inequality TASKS.md called out — top-right and
        // bottom-left should be maximally different (one is yellow, the
        // other is black). A bug that emits a uniform color anywhere
        // (e.g., shader ran but uv interpolation broke) trips this.
        #expect(topRight.r > bottomLeft.r + 192,
                "top-right.r (\(topRight.r)) should dominate bottom-left.r (\(bottomLeft.r))")
        #expect(topRight.g > bottomLeft.g + 192,
                "top-right.g (\(topRight.g)) should dominate bottom-left.g (\(bottomLeft.g))")
    }
}
