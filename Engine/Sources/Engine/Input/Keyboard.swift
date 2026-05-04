import GameController

/// Tracks live keyboard state via the `GameController` framework. Works on
/// macOS 14+ and iPadOS 17+ (Mac/iPad share the same `GCKeyboard` API).
///
/// Pattern: GameController fires `keyChangedHandler` on key down/up; we fold
/// each event into a `KeyboardState`. The game loop polls that state on demand
/// and calls `endFrame()` once per tick to clear pressed/released edges.
@MainActor
public final class Keyboard {
    private var state = KeyboardState()

    public init() {
        if let kb = GCKeyboard.coalesced {
            attach(kb)
        }
        NotificationCenter.default.addObserver(
            forName: .GCKeyboardDidConnect,
            object: nil,
            queue: .main
        ) { [weak self] _ in
            // Posted on .main, so we're already on the main thread; re-read
            // `coalesced` inside the isolation boundary to avoid sending the
            // non-Sendable GCKeyboard across actors.
            MainActor.assumeIsolated {
                guard let self, let kb = GCKeyboard.coalesced else { return }
                self.attach(kb)
            }
        }
    }

    public var snapshot: KeyboardState { state }

    public func isDown(_ key: Key) -> Bool { state.isDown(key) }
    public func wasPressed(_ key: Key) -> Bool { state.wasPressed(key) }
    public func wasReleased(_ key: Key) -> Bool { state.wasReleased(key) }

    /// Clears per-frame edge sets. Called by `GameEngine.update` at end of tick.
    public func endFrame() { state.endFrame() }

    private func attach(_ keyboard: GCKeyboard) {
        guard let input = keyboard.keyboardInput else { return }
        // Handler runs on GameController's input queue; hop to MainActor to
        // mutate `state` without crossing isolation boundaries unsafely.
        input.keyChangedHandler = { [weak self] _, _, keyCode, pressed in
            Task { @MainActor in
                guard let self, let key = Key(gcKeyCode: keyCode) else { return }
                self.state.setKey(key, down: pressed)
            }
        }
    }
}

private extension Key {
    init?(gcKeyCode: GCKeyCode) {
        switch gcKeyCode {
        case .keyA: self = .a
        case .keyB: self = .b
        case .keyC: self = .c
        case .keyD: self = .d
        case .keyE: self = .e
        case .keyF: self = .f
        case .keyG: self = .g
        case .keyH: self = .h
        case .keyI: self = .i
        case .keyJ: self = .j
        case .keyK: self = .k
        case .keyL: self = .l
        case .keyM: self = .m
        case .keyN: self = .n
        case .keyO: self = .o
        case .keyP: self = .p
        case .keyQ: self = .q
        case .keyR: self = .r
        case .keyS: self = .s
        case .keyT: self = .t
        case .keyU: self = .u
        case .keyV: self = .v
        case .keyW: self = .w
        case .keyX: self = .x
        case .keyY: self = .y
        case .keyZ: self = .z
        case .zero: self = .num0
        case .one: self = .num1
        case .two: self = .num2
        case .three: self = .num3
        case .four: self = .num4
        case .five: self = .num5
        case .six: self = .num6
        case .seven: self = .num7
        case .eight: self = .num8
        case .nine: self = .num9
        case .spacebar: self = .space
        case .returnOrEnter: self = .returnKey
        case .escape: self = .escape
        case .tab: self = .tab
        case .deleteOrBackspace: self = .delete
        case .leftArrow: self = .leftArrow
        case .rightArrow: self = .rightArrow
        case .upArrow: self = .upArrow
        case .downArrow: self = .downArrow
        case .leftShift: self = .leftShift
        case .rightShift: self = .rightShift
        case .leftControl: self = .leftControl
        case .rightControl: self = .rightControl
        case .leftAlt: self = .leftOption
        case .rightAlt: self = .rightOption
        case .leftGUI: self = .leftCommand
        case .rightGUI: self = .rightCommand
        default: return nil
        }
    }
}
