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

    @Test(.enabled(if: engineMetalLibraryAvailable,
                   "engine metallib not in Bundle.module — run via xcodebuild test"))
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
}
