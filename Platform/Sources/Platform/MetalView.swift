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

#elseif os(iOS)
import Engine
import Metal
import QuartzCore
import UIKit

/// UIKit view whose backing layer IS a `CAMetalLayer`. Same role as the
/// macOS `MetalView`: owns the color pixel format, syncs `drawableSize`
/// to bounds × scale, and allocates the matching depth texture (depth
/// is the platform's job since `CAMetalLayer` only manages color
/// drawables). Depth-texture lifecycle is intentionally duplicated
/// across the macOS / iOS branches — extract a shared helper if a
/// third platform-glue surface ever appears.
@MainActor
final class MetalView: UIView {
    static let pixelFormat: MTLPixelFormat = .bgra8Unorm
    static let depthFormat: MTLPixelFormat = .depth32Float

    // The view's backing layer IS the Metal layer. Cleaner than the
    // macOS `wantsLayer + makeBackingLayer` recipe — UIKit picks up the
    // layer class via this hook before the layer is created.
    override class var layerClass: AnyClass { CAMetalLayer.self }
    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    private let device: MTLDevice
    private let pointer: Pointer
    private(set) var depthTexture: MTLTexture?

    init(device: MTLDevice, pointer: Pointer) {
        self.device = device
        self.pointer = pointer
        super.init(frame: .zero)
        metalLayer.device = device
        metalLayer.pixelFormat = MetalView.pixelFormat
        metalLayer.framebufferOnly = true
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("MetalView is code-only") }

    // Single-tap-anywhere → one Pointer edge per frame. Multi-touch /
    // location-aware substrate waits for a real consumer.
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        pointer.recordTap()
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        updateDrawableSize()
    }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        // displayScale on the trait collection is the modern path —
        // works without poking at UIScreen, which is deprecated for
        // direct lookup in iOS 16+.
        if window != nil {
            let scale = traitCollection.displayScale
            contentScaleFactor = scale
            metalLayer.contentsScale = scale
            updateDrawableSize()
        }
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
        if let existing = depthTexture, existing.width == width, existing.height == height {
            return
        }
        let desc = MTLTextureDescriptor.texture2DDescriptor(
            pixelFormat: MetalView.depthFormat,
            width: width,
            height: height,
            mipmapped: false
        )
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
