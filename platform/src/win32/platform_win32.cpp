#include "platform_impl.h"
#include "win32_check.h"

#include <string>

namespace platform
{
    namespace
    {
        LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            if (uMsg == WM_NCCREATE)
            {
                // Win32 hands us lParam as LPARAM (integer); CREATESTRUCT* lives there by ABI contract.
                // NOLINTNEXTLINE(performance-no-int-to-ptr)
                auto createInfo = reinterpret_cast<CREATESTRUCT*>(lParam);
                ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
            }

            // GetWindowLongPtr returns LONG_PTR; we stored a Platform::Impl* there above.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            auto impl = reinterpret_cast<Platform::Impl*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (impl == nullptr)
            {
                return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            }

            switch (uMsg)
            {
            case WM_DESTROY:
                ::PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                impl->minimized = (wParam == SIZE_MINIMIZED);
                return 0;
            default:
                return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
        }
    } // namespace

    Platform::Platform(std::string_view windowName) : impl_(std::make_unique<Impl>())
    {
        WNDCLASSEX wndClass{
            .cbSize = sizeof(WNDCLASSEX),
            .lpfnWndProc = &wndProcThunk,
            .hInstance = ::GetModuleHandle(nullptr),
            .lpszClassName = "GameWindowClass",
        };

        auto windowClass = ::RegisterClassEx(&wndClass);
        WIN32_CHECK(windowClass);

        // CreateWindow needs a null-terminated string; string_view doesn't promise one.
        std::string nameZ(windowName);
        impl_->wnd =
            ::CreateWindow(MAKEINTATOM(windowClass), nameZ.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           800, 600, nullptr, nullptr, ::GetModuleHandle(nullptr), static_cast<LPVOID>(impl_.get()));
        WIN32_CHECK(impl_->wnd);
    }

    Platform::~Platform()
    {
        if (impl_->wnd != nullptr)
        {
            ::DestroyWindow(impl_->wnd);
        }
    }

    void Platform::GameLoop()
    {
        ::ShowWindow(impl_->wnd, SW_SHOW);
        ::UpdateWindow(impl_->wnd);

        MSG msg;
        while (impl_->running)
        {
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    impl_->running = false;
                }
                else
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }
        }
    }

} // namespace platform
