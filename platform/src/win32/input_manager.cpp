#include "input_manager.h"
#include "GameInput.h"
#include "win32_check.h"

#include <array>
#include <cstdint>
#include <spdlog/spdlog.h>

using namespace GameInput::v3;

namespace platform
{
    namespace win32
    {
        // Larger than any real keyboard's n-key rollover; the aggregate device
        // is synthetic and doesn't report a maxSimultaneousKeys hint we could
        // size against, so we just pick a safe ceiling and reserve once.
        static constexpr size_t kMaxSimultaneousKeys = 256;

        // Win32 virtual-key (uint8_t) → engine KeyCode. Unmapped slots stay at
        // KeyCode::Unknown (== 0), which is the value-initialized default.
        // Relies on KeyA..KeyZ, Digit0..9, F1..F12, and Numpad0..9 being
        // declared as contiguous runs in the KeyCode enum.
        static constexpr std::array<KeyCode, 256> makeVkLookup()
        {
            std::array<KeyCode, 256> t{};

            // Letters: VK_A..VK_Z are uppercase ASCII (0x41..0x5A)
            for (uint8_t i = 0; i < 26; ++i)
            {
                t[static_cast<size_t>('A' + i)] = static_cast<KeyCode>(static_cast<uint8_t>(KeyCode::KeyA) + i);
            }
            // Top-row digits: VK_0..VK_9 are ASCII (0x30..0x39)
            for (uint8_t i = 0; i < 10; ++i)
            {
                t[static_cast<size_t>('0' + i)] = static_cast<KeyCode>(static_cast<uint8_t>(KeyCode::Digit0) + i);
            }
            // VK_F1 == 0x70
            for (uint8_t i = 0; i < 12; ++i)
            {
                t[0x70 + i] = static_cast<KeyCode>(static_cast<uint8_t>(KeyCode::F1) + i);
            }
            // VK_NUMPAD0 == 0x60
            for (uint8_t i = 0; i < 10; ++i)
            {
                t[0x60 + i] = static_cast<KeyCode>(static_cast<uint8_t>(KeyCode::Numpad0) + i);
            }

            // Arrows
            t[0x25] = KeyCode::ArrowLeft;  // VK_LEFT
            t[0x26] = KeyCode::ArrowUp;    // VK_UP
            t[0x27] = KeyCode::ArrowRight; // VK_RIGHT
            t[0x28] = KeyCode::ArrowDown;  // VK_DOWN

            // Modifiers (GameInput reports L/R disambiguated VKs)
            t[0xA0] = KeyCode::ShiftLeft;    // VK_LSHIFT
            t[0xA1] = KeyCode::ShiftRight;   // VK_RSHIFT
            t[0xA2] = KeyCode::ControlLeft;  // VK_LCONTROL
            t[0xA3] = KeyCode::ControlRight; // VK_RCONTROL
            t[0xA4] = KeyCode::AltLeft;      // VK_LMENU
            t[0xA5] = KeyCode::AltRight;     // VK_RMENU
            t[0x5B] = KeyCode::SuperLeft;    // VK_LWIN
            t[0x5C] = KeyCode::SuperRight;   // VK_RWIN

            // Common
            t[0x20] = KeyCode::Space;     // VK_SPACE
            t[0x0D] = KeyCode::Enter;     // VK_RETURN (numpad-enter shares this VK)
            t[0x09] = KeyCode::Tab;       // VK_TAB
            t[0x1B] = KeyCode::Escape;    // VK_ESCAPE
            t[0x08] = KeyCode::Backspace; // VK_BACK

            // Navigation
            t[0x24] = KeyCode::Home;     // VK_HOME
            t[0x23] = KeyCode::End;      // VK_END
            t[0x21] = KeyCode::PageUp;   // VK_PRIOR
            t[0x22] = KeyCode::PageDown; // VK_NEXT
            t[0x2D] = KeyCode::Insert;   // VK_INSERT
            t[0x2E] = KeyCode::Delete;   // VK_DELETE

            // Numpad operators
            t[0x6B] = KeyCode::NumpadAdd;      // VK_ADD
            t[0x6D] = KeyCode::NumpadSubtract; // VK_SUBTRACT
            t[0x6A] = KeyCode::NumpadMultiply; // VK_MULTIPLY
            t[0x6F] = KeyCode::NumpadDivide;   // VK_DIVIDE
            t[0x6E] = KeyCode::NumpadDecimal;  // VK_DECIMAL

            // Locks / system
            t[0x14] = KeyCode::CapsLock;    // VK_CAPITAL
            t[0x90] = KeyCode::NumLock;     // VK_NUMLOCK
            t[0x91] = KeyCode::ScrollLock;  // VK_SCROLL
            t[0x2C] = KeyCode::PrintScreen; // VK_SNAPSHOT
            t[0x13] = KeyCode::Pause;       // VK_PAUSE

            // Punctuation — VK_OEM_* assignments are US-layout specific.
            t[0xBC] = KeyCode::Comma;        // VK_OEM_COMMA
            t[0xBE] = KeyCode::Period;       // VK_OEM_PERIOD
            t[0xBF] = KeyCode::Slash;        // VK_OEM_2
            t[0xBA] = KeyCode::Semicolon;    // VK_OEM_1
            t[0xDE] = KeyCode::Quote;        // VK_OEM_7
            t[0xDB] = KeyCode::BracketLeft;  // VK_OEM_4
            t[0xDD] = KeyCode::BracketRight; // VK_OEM_6
            t[0xDC] = KeyCode::Backslash;    // VK_OEM_5
            t[0xC0] = KeyCode::Backquote;    // VK_OEM_3
            t[0xBD] = KeyCode::Minus;        // VK_OEM_MINUS
            t[0xBB] = KeyCode::Equal;        // VK_OEM_PLUS

            return t;
        }
        static constexpr auto kVkToKeyCode = makeVkLookup();

        InputManager::InputManager() : keyboardAggDevice_{}
        {
            HR_CHECK(GameInputCreate(&gameInput_));
            HR_CHECK(gameInput_->CreateAggregateDevice(GameInputKindKeyboard, &keyboardAggDevice_));
            keyStates_.resize(kMaxSimultaneousKeys);
        }

        InputManager::~InputManager() = default;

        std::bitset<static_cast<size_t>(KeyCode::KeyCount)> InputManager::pollPressedKeys()
        {
            std::bitset<static_cast<size_t>(KeyCode::KeyCount)> pressedKeys;

            Microsoft::WRL::ComPtr<IGameInputDevice> keyboardInput;
            if (FAILED(gameInput_->FindDeviceFromId(&keyboardAggDevice_, &keyboardInput)))
            {
                return pressedKeys;
            }
            Microsoft::WRL::ComPtr<IGameInputReading> reading;
            if (FAILED(gameInput_->GetCurrentReading(GameInputKindKeyboard, keyboardInput.Get(), &reading)))
            {
                return pressedKeys;
            }

            // GetKeyState returns the number of currently-pressed keys it wrote
            // into the buffer (≤ stateArrayCount).
            const uint32_t count = reading->GetKeyState(static_cast<uint32_t>(keyStates_.size()), keyStates_.data());
            for (uint32_t i = 0; i < count; ++i)
            {
                const KeyCode kc = kVkToKeyCode[keyStates_[i].virtualKey];
                if (kc != KeyCode::Unknown)
                {
                    pressedKeys.set(static_cast<size_t>(kc));
                }
            }

            return pressedKeys;
        }

    } // namespace win32
} // namespace platform