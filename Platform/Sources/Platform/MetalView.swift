#if os(macOS)
import AppKit
import Engine
import Metal
import QuartzCore
#elseif os(iOS)
import Engine
import Metal
import QuartzCore
import UIKit
#endif

/// Allocate a 2D multisample render-target texture with `MetalView.sampleCount`.
/// Storage is `.memoryless` because both attachments are transient: the MSAA
/// color is consumed by an on-chip resolve into the drawable, and depth is
/// `storeAction = .dontCare`. Memoryless backings live entirely in tile memory
/// on Apple Silicon — zero physical allocation. Shared by the macOS and iOS
/// `MetalView` definitions, which each define their own `MetalView` class
/// inside their respective `#if` branch.
@MainActor
private func makeMultisampleAttachment(
    device: MTLDevice,
    pixelFormat: MTLPixelFormat,
    width: Int, height: Int,
    label: String
) -> MTLTexture {
    let desc = MTLTextureDescriptor()
    desc.textureType = .type2DMultisample
    desc.pixelFormat = pixelFormat
    desc.width = width
    desc.height = height
    desc.sampleCount = MetalView.sampleCount
    desc.usage = .renderTarget
    desc.storageMode = .memoryless
    guard let tex = device.makeTexture(descriptor: desc) else {
        fatalError("MetalView: failed to allocate \(label) (\(width)×\(height) ×\(MetalView.sampleCount))")
    }
    tex.label = label
    return tex
}

#if os(macOS)

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
    /// 4× MSAA: the standard Apple Silicon sweet spot. Samples live in
    /// tile memory and resolve on-chip, so the only cost is a slightly
    /// smaller tile and per-sample ROP work — a few percent on typical
    /// scenes. The drawable stays single-sampled; we render into
    /// `msaaColorTexture` and resolve out.
    static let sampleCount: Int = 4

    let metalLayer: CAMetalLayer
    private let device: MTLDevice
    private let pointer: Pointer
    private(set) var depthTexture: MTLTexture?
    private(set) var msaaColorTexture: MTLTexture?

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
        rebuildAttachmentsIfNeeded(size: newSize)
    }

    private func rebuildAttachmentsIfNeeded(size: CGSize) {
        let width = Int(size.width)
        let height = Int(size.height)
        // Skip if existing textures already match; setFrameSize fires
        // for every layout pass and most of them don't actually resize.
        if let existing = depthTexture, existing.width == width, existing.height == height {
            return
        }
        depthTexture = makeMultisampleAttachment(
            device: device,
            pixelFormat: MetalView.depthFormat,
            width: width, height: height,
            label: "MetalView.depthTexture"
        )
        msaaColorTexture = makeMultisampleAttachment(
            device: device,
            pixelFormat: MetalView.pixelFormat,
            width: width, height: height,
            label: "MetalView.msaaColorTexture"
        )
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
    /// 4× MSAA: the standard Apple Silicon sweet spot. Samples live in
    /// tile memory and resolve on-chip, so the only cost is a slightly
    /// smaller tile and per-sample ROP work — a few percent on typical
    /// scenes. The drawable stays single-sampled; we render into
    /// `msaaColorTexture` and resolve out.
    static let sampleCount: Int = 4

    // The view's backing layer IS the Metal layer. Cleaner than the
    // macOS `wantsLayer + makeBackingLayer` recipe — UIKit picks up the
    // layer class via this hook before the layer is created.
    override class var layerClass: AnyClass { CAMetalLayer.self }
    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    private let device: MTLDevice
    private let pointer: Pointer
    private(set) var depthTexture: MTLTexture?
    private(set) var msaaColorTexture: MTLTexture?

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
        rebuildAttachmentsIfNeeded(size: newSize)
    }

    private func rebuildAttachmentsIfNeeded(size: CGSize) {
        let width = Int(size.width)
        let height = Int(size.height)
        if let existing = depthTexture, existing.width == width, existing.height == height {
            return
        }
        depthTexture = makeMultisampleAttachment(
            device: device,
            pixelFormat: MetalView.depthFormat,
            width: width, height: height,
            label: "MetalView.depthTexture"
        )
        msaaColorTexture = makeMultisampleAttachment(
            device: device,
            pixelFormat: MetalView.pixelFormat,
            width: width, height: height,
            label: "MetalView.msaaColorTexture"
        )
    }
}
#endif
