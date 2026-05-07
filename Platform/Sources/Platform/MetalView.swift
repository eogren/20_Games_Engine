#if os(macOS)
import AppKit
import Engine
import Metal
import QuartzCore

/// AppKit view backed by a `CAMetalLayer`. Owns the layer's pixel format
/// (a Platform-level decision since `CAMetalLayer` is what cares) and
/// keeps `drawableSize` in sync with the view's bounds × backing scale.
/// Also owns the depth texture — `CAMetalLayer` only manages color
/// drawables, so the matching depth attachment is the platform's job
/// to allocate, resize, and hand to the renderer each tick.
@MainActor
final class MetalView: NSView {
    static let pixelFormat: MTLPixelFormat = .bgra8Unorm
    static let depthFormat: MTLPixelFormat = .depth32Float

    let metalLayer: CAMetalLayer
    private let device: MTLDevice
    private let pointer: Pointer
    private(set) var depthTexture: MTLTexture?

    init(device: MTLDevice, pointer: Pointer) {
        self.device = device
        self.pointer = pointer
        let layer = CAMetalLayer()
        layer.device = device
        layer.pixelFormat = MetalView.pixelFormat
        layer.framebufferOnly = true
        self.metalLayer = layer
        super.init(frame: .zero)
        // wantsLayer + makeBackingLayer override is the canonical recipe
        // for installing a custom CALayer subclass on an NSView.
        wantsLayer = true
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("MetalView is code-only") }

    // Single-click-anywhere → one Pointer edge per frame. Position-aware /
    // multi-touch substrate waits for a real consumer.
    override func mouseDown(with event: NSEvent) {
        pointer.recordTap()
    }

    // Keys flow through `Keyboard` (GameController), not the AppKit responder
    // chain. But AppKit still dispatches keyDown to the responder chain in
    // parallel; if no one consumes it, NSWindow falls back to NSBeep. Become
    // first responder and swallow keyDown so the system stays quiet — game
    // code reads `Keyboard` for the actual state.
    override var acceptsFirstResponder: Bool { true }
    override func keyDown(with event: NSEvent) {}

    override func makeBackingLayer() -> CALayer { metalLayer }

    override func setFrameSize(_ newSize: NSSize) {
        super.setFrameSize(newSize)
        updateDrawableSize()
    }

    override func viewDidChangeBackingProperties() {
        super.viewDidChangeBackingProperties()
        if let scale = window?.backingScaleFactor {
            metalLayer.contentsScale = scale
        }
        updateDrawableSize()
    }

    private func updateDrawableSize() {
        let scale = metalLayer.contentsScale
        // CAMetalLayer rejects zero-size drawables; clamp to 1×1.
        let newSize = CGSize(
            width: max(1, bounds.width * scale),
            height: max(1, bounds.height * scale)
        )
        metalLayer.drawableSize = newSize
        rebuildDepthTextureIfNeeded(size: newSize)
    }

    private func rebuildDepthTextureIfNeeded(size: CGSize) {
        let width = Int(size.width)
        let height = Int(size.height)
        // Skip if the existing texture already matches; setFrameSize fires
        // for every layout pass and most of them don't actually resize.
        if let existing = depthTexture, existing.width == width, existing.height == height {
            return
        }
        let desc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: MetalView.depthFormat,
            width: width,
            height: height,
            mipmapped: false
        )
        // .private: the GPU is the only reader. .renderTarget alone is
        // enough — no shaderRead, no readback path on this allocation.
        desc.usage = .renderTarget
        desc.storageMode = .private
        guard let tex = device.makeTexture(descriptor: desc) else {
            fatalError("MetalView: failed to allocate depth texture (\(width)×\(height))")
        }
        tex.label = "MetalView.depthTexture"
        depthTexture = tex
    }
}
#endif
