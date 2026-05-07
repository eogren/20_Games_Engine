public enum Key: Hashable, Sendable, CaseIterable {
    case a, b, c, d, e, f, g, h, i, j, k, l, m
    case n, o, p, q, r, s, t, u, v, w, x, y, z
    case num0, num1, num2, num3, num4, num5, num6, num7, num8, num9
    case space, returnKey, escape, tab, delete
    case leftArrow, rightArrow, upArrow, downArrow
    case leftShift, rightShift
    case leftControl, rightControl
    case leftOption, rightOption
    case leftCommand, rightCommand
}

/// Per-frame keyboard snapshot. The platform layer drives `down` via
/// `setKey(_:down:)`; `pressed`/`released` are edge sets cleared by `endFrame()`.
public struct KeyboardState: Sendable {
    public private(set) var down: Set<Key> = []
    public private(set) var pressed: Set<Key> = []
    public private(set) var released: Set<Key> = []

    public init() {}

    public func isDown(_ key: Key) -> Bool { down.contains(key) }
    public func wasPressed(_ key: Key) -> Bool { pressed.contains(key) }
    public func wasReleased(_ key: Key) -> Bool { released.contains(key) }

    public mutating func setKey(_ key: Key, down isDown: Bool) {
        if isDown {
            if down.insert(key).inserted { pressed.insert(key) }
        } else {
            if down.remove(key) != nil { released.insert(key) }
        }
    }

    /// Call at the end of each frame to clear edge sets. `down` persists.
    public mutating func endFrame() {
        pressed.removeAll(keepingCapacity: true)
        released.removeAll(keepingCapacity: true)
    }
}

/// Per-frame pointer snapshot. The platform layer drives `tappedThisFrame`
/// via `recordTap()` from its OS event hook; the game polls the boolean
/// and `endFrame()` clears it. v1 carries no position — `mouseDown` /
/// `touchesBegan` reduce to a single per-frame edge. Position-aware tap
/// can fold in later (likely as `takeTap() -> Tap?`) without breaking
/// callers that just want "did the user activate this frame."
public struct PointerState: Sendable {
    public private(set) var tappedThisFrame: Bool = false

    public init() {}

    /// Multiple taps in one frame collapse to a single edge — the game
    /// loop polls once per tick, so finer granularity has no consumer.
    public mutating func recordTap() { tappedThisFrame = true }

    /// Call at the end of each frame to clear the edge.
    public mutating func endFrame() { tappedThisFrame = false }
}
