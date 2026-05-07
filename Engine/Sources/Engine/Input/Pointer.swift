/// Cross-platform pointer edge: mouse click on macOS, single tap on iOS.
///
/// Passive sink polled by game code. The platform layer owns the
/// OS-specific event hook (`mouseDown(with:)`, `touchesBegan(_:with:)`)
/// and feeds taps in via `recordTap()`. Engine still owns the type so
/// games never reach into Platform.
///
/// v1 carries no position — `mouseDown` / `touchesBegan` reduce to a
/// single per-frame edge. Multiple taps in one frame collapse to the
/// same edge (game polls once per tick). Position-aware tap can fold
/// in later (likely as `takeTap() -> Tap?`) without breaking callers
/// that just want "did the user activate this frame."
@MainActor
public final class Pointer {
    public private(set) var tappedThisFrame: Bool = false

    public init() {}

    /// Called by the platform layer from its OS event hook.
    public func recordTap() { tappedThisFrame = true }

    /// Clears the per-frame edge. Called by `GameEngine.update` at end of tick.
    public func endFrame() { tappedThisFrame = false }
}
