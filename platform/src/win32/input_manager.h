#pragma once
#include "platform/input.h"

#include <bitset>

namespace platform::win32
{
    /**
     * Win32 keyboard polling. Pulls currently-held keys from GetAsyncKeyState,
     * maps the disambiguated Win32 virtual-key code into the engine's KeyCode
     * enum, and returns a bitset snapshot.
     *
     * GetAsyncKeyState is a system-wide query — it reports key state regardless
     * of which window has focus. That's fine for the single-window app shape
     * we have today; if engine-driven games ever want background-input
     * suppression we'd add an HWND foreground check here.
     */
    class InputManager
    {
    public:
        InputManager() = default;
        ~InputManager() = default;

        InputManager(const InputManager&) = delete;
        InputManager& operator=(const InputManager&) = delete;
        InputManager(InputManager&&) = delete;
        InputManager& operator=(InputManager&&) = delete;

        std::bitset<static_cast<size_t>(KeyCode::KeyCount)> pollPressedKeys();
    };
} // namespace platform::win32
