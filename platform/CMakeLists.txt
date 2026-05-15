# platform — OS-divergence layer. Owns the Win32 window + message pump
# and the single vkCreateWin32SurfaceKHR call. Engine depends on platform
# (one-way; see CLAUDE.md > "Layer split"). Platform itself only needs
# volk because surface.h speaks VkSurfaceKHR / VkInstance.

# ---- Win32-only third-party (vendored under third_party/) -----------
#
# GameInput — Microsoft's modern unified input API (keyboards, mice,
# gamepads, sensors, haptics). Strictly a Win32 concern: it sits behind
# platform's input source path and never reaches engine headers. iPad
# would use a different input API entirely, so this is vendored under
# platform/third_party/ rather than the shared engine/third_party/.
# See platform/third_party/VENDORING.md for version pinning and the
# re-download command. The MSBuild .targets file from the NuGet pkg
# defines USING_GAMEINPUT for all consumers; mirror that here so any
# config keyed off the macro inside GameInput.h behaves identically.

if(WIN32)
    add_library(gameinput INTERFACE)
    target_include_directories(gameinput SYSTEM INTERFACE
            third_party/gameinput/include
    )
    target_link_libraries(gameinput INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gameinput/lib/x64/GameInput.lib
    )
    target_compile_definitions(gameinput INTERFACE USING_GAMEINPUT)
endif()

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
        # gameinput is PRIVATE — engine consumes input through platform's
        # portable event/state interface, never through GameInput.h.
        PUBLIC volk
        PRIVATE engine_compile_options spdlog::spdlog
)
if(WIN32)
    target_link_libraries(platform PRIVATE gameinput)
endif()

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
