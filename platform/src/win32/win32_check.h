#pragma once

#include <windows.h>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>

namespace platform::win32
{
    inline std::string formatLastError(DWORD err)
    {
        LPSTR buf = nullptr;
        const DWORD len = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buf), 0, nullptr);
        if (len == 0 || buf == nullptr)
        {
            return "(no message)";
        }
        std::string msg(buf, len);
        ::LocalFree(buf);
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' '))
        {
            msg.pop_back();
        }
        return msg;
    }

    [[noreturn]] inline void win32CheckFail(const char* expr, const char* file, int line)
    {
        // Capture before anything else — spdlog sinks may call Win32 APIs
        // that clobber the thread-local last-error.
        const DWORD err = ::GetLastError();
        spdlog::critical("[win32] {} failed: {} ({}) at {}:{}", expr, formatLastError(err), err, file, line);
        spdlog::default_logger()->flush();
        std::abort();
    }

    [[noreturn]] inline void hrCheckFail(HRESULT hr, const char* expr, const char* file, int line)
    {
        spdlog::critical("[win32] {} failed: {} (0x{:08x}) at {}:{}", expr, formatLastError(static_cast<DWORD>(hr)),
                         static_cast<uint32_t>(hr), file, line);
        spdlog::default_logger()->flush();
        std::abort();
    }
} // namespace platform::win32

// Win32 functions overwhelmingly signal failure with a zero return (NULL HWND,
// zero ATOM, FALSE BOOL) and set GetLastError(). WIN32_CHECK aborts on that
// signal after logging the formatted system message — use it at boundary calls
// where there's no meaningful recovery path. A WIN32_TRY/std::expected variant
// for fallible sites is planned but not yet written.
#define WIN32_CHECK(expr)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            ::platform::win32::win32CheckFail(#expr, __FILE__, __LINE__);                                              \
        }                                                                                                              \
    } while (0)

// COM-style APIs (GameInput, D3D, WIC, etc.) return HRESULT instead of
// populating GetLastError(). HR_CHECK branches on FAILED(hr) and logs the
// HRESULT directly — use it at boundary calls whose signature is
// `HRESULT foo(...)`. Same abort-on-failure contract as WIN32_CHECK.
#define HR_CHECK(expr)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        const HRESULT _hr = (expr);                                                                                    \
        if (FAILED(_hr))                                                                                               \
        {                                                                                                              \
            ::platform::win32::hrCheckFail(_hr, #expr, __FILE__, __LINE__);                                            \
        }                                                                                                              \
    } while (0)
