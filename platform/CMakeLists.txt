# platform — OS-divergence layer. Owns the Win32 window + message pump
# and the single vkCreateWin32SurfaceKHR call. Engine depends on platform
# (one-way; see CLAUDE.md > "Layer split"). Platform itself only needs
# volk because surface.h speaks VkSurfaceKHR / VkInstance.

add_library(platform STATIC
        src/platform.cpp
        src/win32/platform_win32.cpp
        src/win32/surface_win32.cpp
)
target_include_directories(platform
        PUBLIC include
        PRIVATE src src/win32
)
target_link_libraries(platform
        # volk is PUBLIC because surface.h speaks VkSurfaceKHR / VkInstance to
        # callers. spdlog is PRIVATE — platform uses it only inside
        # win32_check.h (WIN32_CHECK / win32CheckFail) for internal logging.
        PUBLIC volk
        PRIVATE engine_compile_options spdlog::spdlog
)

# Win32-specific link bits (user32, gdi32) land here once the window /
# message-pump code does. Today the platform lib has no Win32 surface
# yet — it's just the scaffolded target.

# UTF-8 process code page (Windows 10 1903+). Propagated as an INTERFACE
# source so any executable linking platform embeds the manifest at link
# time — MSVC's mt.exe merges it with the auto-generated manifest.
# Lets the engine stay on std::string + UTF-8 across the Win32 boundary
# instead of dragging wchar_t through the codebase. See CLAUDE.md and
# platform.manifest for the why.
if(WIN32)
    target_sources(platform INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform.manifest
    )
endif()
