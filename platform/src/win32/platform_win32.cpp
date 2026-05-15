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
        HINSTANCE hInstance = ::GetModuleHandle(nullptr);
        HICON appIcon = ::LoadIcon(hInstance, "AppIcon");

        WNDCLASSEX wndClass{
            .cbSize = sizeof(WNDCLASSEX),
            .lpfnWndProc = &wndProcThunk,
            .hInstance = hInstance,
            .hIcon = appIcon,
            // Without this the launch-time wait cursor sticks over the client
            // area until another window forces a change.
            .hCursor = ::LoadCursor(nullptr, IDC_ARROW),
            .lpszClassName = "GameWindowClass",
            .hIconSm = appIcon,
        };

        auto windowClass = ::RegisterClassEx(&wndClass);
        WIN32_CHECK(windowClass);

        // CreateWindow needs a null-terminated string; string_view doesn't promise one.
        std::string nameZ(windowName);
        impl_->wnd = ::CreateWindow(MAKEINTATOM(windowClass), nameZ.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                    CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance,
                                    static_cast<LPVOID>(impl_.get()));
        WIN32_CHECK(impl_->wnd);
    }

    void Platform::show()
    {
        // Deferred from the constructor so Vulkan init (hundreds of ms) doesn't
        // run with the empty client area on screen — the user would see a white
        // flash before the first present. Engine::run() calls this just before
        // entering its loop.
        ::ShowWindow(impl_->wnd, SW_SHOW);
        ::UpdateWindow(impl_->wnd);
    }

    Platform::~Platform()
    {
        if (impl_->wnd != nullptr)
        {
            ::DestroyWindow(impl_->wnd);
        }
    }

    void Platform::pollEvents()
    {
        MSG msg;
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

    bool Platform::shouldClose() const noexcept
    {
        return !impl_->running;
    }

} // namespace platform
