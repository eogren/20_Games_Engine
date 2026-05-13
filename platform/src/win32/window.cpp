#include "win32/platform.h"
#include <windows.h>

#include "spdlog/spdlog.h"
#include "win32/win32_check.h"

namespace platform
{
    namespace win32
    {

        Platform::Platform(const std::string& windowName)
        {
            WNDCLASSEX wndClass{
                .cbSize = sizeof(WNDCLASSEX),
                .lpfnWndProc = &Platform::wndProcThunk,
                .hInstance = GetModuleHandle(nullptr),
                .lpszClassName = "GameWindowClass",
            };

            auto windowClass = ::RegisterClassEx(&wndClass);
            WIN32_CHECK(windowClass);

            wnd_ = ::CreateWindow(MAKEINTATOM(windowClass), windowName.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                  CW_USEDEFAULT, 800, 600, nullptr, nullptr, ::GetModuleHandle(nullptr),
                                  static_cast<LPVOID>(this));
            WIN32_CHECK(wnd_);
        }

        Platform::~Platform()
        {
            if (wnd_ != nullptr)
            {
                ::DestroyWindow(wnd_);
            }
        }

        void Platform::GameLoop()
        {
            ::ShowWindow(wnd_, SW_SHOW);
            ::UpdateWindow(wnd_);

            MSG msg;
            while (running_)
            {
                while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
                {
                    if (msg.message == WM_QUIT)
                    {
                        running_ = false;
                    }
                    else
                    {
                        ::TranslateMessage(&msg);
                        ::DispatchMessage(&msg);
                    }
                }
            }
        }

        LRESULT CALLBACK Platform::wndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            if (uMsg == WM_NCCREATE)
            {
                // Win32 hands us lParam as LPARAM (integer); CREATESTRUCT* lives there by ABI contract.
                // NOLINTNEXTLINE(performance-no-int-to-ptr)
                auto createInfo = reinterpret_cast<CREATESTRUCT*>(lParam);
                ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createInfo->lpCreateParams));
            }

            // GetWindowLongPtr returns LONG_PTR; we stored a Platform* there above.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            auto self = reinterpret_cast<Platform*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (self == nullptr)
            {
                return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            return self->wndProc(hwnd, uMsg, wParam, lParam);
        }

        LRESULT Platform::wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
        {
            switch (uMsg)
            {
            case WM_DESTROY:
                ::PostQuitMessage(0);
                return 0;
            case WM_SIZE:
                minimized_ = (wParam == SIZE_MINIMIZED);
                return 0;
            default:
                return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
        }

    } // namespace win32
} // namespace platform