#pragma once
#include <cstdint>

namespace platform
{
    /**
     * Keyboard key identifiers, named after Bevy's KeyCode. Letters, digits,
     * function keys, and numpad numbers are kept in contiguous blocks so a
     * platform layer can fill its lookup table with simple arithmetic.
     *
     * `Unknown` (== 0) is the sentinel returned by platform lookups when a
     * native key code has no engine mapping. Game code shouldn't query it.
     *
     * `KeyCount` is the size sentinel — keep it last. Used to size bitsets
     * keyed by KeyCode.
     */
    enum class KeyCode : std::uint8_t
    {
        Unknown = 0,

        // Letters
        KeyA, KeyB, KeyC, KeyD, KeyE, KeyF, KeyG, KeyH, KeyI, KeyJ,
        KeyK, KeyL, KeyM, KeyN, KeyO, KeyP, KeyQ, KeyR, KeyS, KeyT,
        KeyU, KeyV, KeyW, KeyX, KeyY, KeyZ,

        // Top-row digits
        Digit0, Digit1, Digit2, Digit3, Digit4,
        Digit5, Digit6, Digit7, Digit8, Digit9,

        // Function keys
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        // Arrow keys
        ArrowUp, ArrowDown, ArrowLeft, ArrowRight,

        // Modifiers (left/right disambiguated)
        ShiftLeft, ShiftRight,
        ControlLeft, ControlRight,
        AltLeft, AltRight,
        SuperLeft, SuperRight,

        // Common
        Space, Enter, Tab, Escape, Backspace,

        // Navigation block
        Home, End, PageUp, PageDown, Insert, Delete,

        // Numpad
        Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
        Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
        NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide, NumpadDecimal,

        // Locks / system
        CapsLock, NumLock, ScrollLock, PrintScreen, Pause,

        // Punctuation (US layout)
        Comma, Period, Slash, Semicolon, Quote,
        BracketLeft, BracketRight, Backslash, Backquote,
        Minus, Equal,

        KeyCount
    };

} // namespace platform
