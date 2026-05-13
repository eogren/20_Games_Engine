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

            /**
             * Main game loop. Never returns until it's time to exit.
             */
            void GameLoop();

        private:
            static LRESULT CALLBACK wndProcThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
            LRESULT wndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

            HWND wnd_ = nullptr;
            bool running_ = true;
            bool minimized_ = false;
        };
    } // namespace win32
} // namespace platform