# platform — OS-divergence layer. Owns the Win32 window + message pump
# and the single vkCreateWin32SurfaceKHR call. Depends on engine; engine
# never imports platform (see CLAUDE.md > "Layer split").

add_library(platform STATIC
        src/platform.cpp
        src/win32/window.cpp
)
target_include_directories(platform PUBLIC src)
target_link_libraries(platform
        PUBLIC engine
        PRIVATE engine_compile_options
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
