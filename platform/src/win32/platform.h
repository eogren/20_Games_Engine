#pragma once

#include <windows.h>

#include <string>

namespace platform
{
    namespace win32
    {
        class Platform
        {
        public:
            Platform(const std::string& windowName);
            ~Platform();

        private:
            static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
            LRESULT wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

            HWND wnd_ = nullptr;
        };
    } // namespace win32
} // namespace platform