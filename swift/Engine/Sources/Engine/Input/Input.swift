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
