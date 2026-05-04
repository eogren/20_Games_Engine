#if os(macOS)
import AppKit
import Metal
import QuartzCore

/// AppKit view backed by a `CAMetalLayer`. Owns the layer's pixel format
/// (a Platform-level decision since `CAMetalLayer` is what cares) and
/// keeps `drawableSize` in sync with the view's bounds × backing scale.
/// The renderer pulls drawables from `metalLayer` each frame.
@MainActor
final class MetalView: NSView {
    static let pixelFormat: MTLPixelFormat = .bgra8Unorm

    let metalLayer: CAMetalLayer

    init(device: MTLDevice) {
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
        metalLayer.drawableSize = CGSize(
            width: max(1, bounds.width * scale),
            height: max(1, bounds.height * scale)
        )
    }
}
#endif
