#pragma once

#include "input_manager.h"
#include "platform/platform.h"

#include <windows.h>

namespace platform
{
    struct Platform::Impl
    {
        HWND wnd = nullptr;
        bool running = true;
        bool minimized = false;
        win32::InputManager inputManager;
    };

    // The single access point for backend TUs that need to read Platform's
    // private impl (today: surface_win32.cpp). Keeps impl_ private from the
    // rest of the engine without forcing a public nativeWindow() accessor.
    struct PlatformInternals
    {
        static Platform::Impl& get(Platform& p) noexcept
        {
            return *p.impl_;
        }
        static const Platform::Impl& get(const Platform& p) noexcept
        {
            return *p.impl_;
        }
    };
} // namespace platform
