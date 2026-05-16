#include "platform/platform.h"

#include <windows.h>

#include <vector>

namespace platform
{
    std::filesystem::path executableDir()
    {
        // GetModuleFileNameW(NULL, ...) returns the running EXE's full path.
        // MAX_PATH (260) is the historic limit, but long-path support (registry
        // LongPathsEnabled, manifest opt-in, \\?\ prefix) lifts it to ~32k.
        // Grow on truncation rather than baking in a hard cap.
        std::vector<wchar_t> buf(MAX_PATH);
        for (;;)
        {
            const DWORD got = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
            if (got == 0) return {};
            if (got < buf.size())
            {
                return std::filesystem::path(std::wstring(buf.data(), got)).parent_path();
            }
            // got == buf.size(): name didn't fit. Retry with double the room.
            buf.resize(buf.size() * 2);
        }
    }
} // namespace platform
