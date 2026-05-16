#pragma once
#include "platform/input.h"

#include <GameInput.h>
#include <bitset>
#include <vector>
#include <wrl.h>

namespace GameInput
{
    namespace v3
    {
        struct IGameInput;
    }
} // namespace GameInput

namespace platform
{
    namespace win32
    {
        class InputManager
        {
        public:
            InputManager();
            ~InputManager();

            std::bitset<static_cast<size_t>(KeyCode::KeyCount)> pollPressedKeys();

        private:
            Microsoft::WRL::ComPtr<GameInput::v3::IGameInput> gameInput_;
            std::vector<GameInput::v3::GameInputKeyState> keyStates_;
            APP_LOCAL_DEVICE_ID keyboardAggDevice_{};
        };
    } // namespace win32
} // namespace platform