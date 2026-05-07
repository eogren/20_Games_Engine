/// Cross-platform pointer edge: mouse click on macOS, single tap on iOS.
///
/// Shape mirrors `Keyboard` (passive sink polled by game code) but the
/// event source diverges per OS, so Engine can't drive it directly the
/// way `GCKeyboard` lets it for keys. The platform layer owns the
/// OS-specific event hook (`mouseDown(with:)`, `touchesBegan(_:with:)`)
/// and feeds taps in via `recordTap()`. Engine still owns the type so
/// games never reach into Platform.
@MainActor
public final class Pointer {
    private var state = PointerState()

    public init() {}

    public var snapshot: PointerState { state }

    public var tappedThisFrame: Bool { state.tappedThisFrame }

    /// Called by the platform layer from its OS event hook.
    public func recordTap() { state.recordTap() }

    /// Clears the per-frame edge. Called by `GameEngine.update` at end of tick.
    public func endFrame() { state.endFrame() }
}
