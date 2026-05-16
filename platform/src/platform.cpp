#include "platform/platform.h"

#include "platform_impl.h"

namespace platform
{
    std::bitset<static_cast<size_t>(KeyCode::KeyCount)> Platform::pollPressedKeys()
    {
        return impl_->inputManager.pollPressedKeys();
    }

    const char* version()
    {
        return "0.0.1";
    }

} // namespace platform
