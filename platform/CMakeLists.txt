# platform — OS-divergence layer. Owns the Win32 window + message pump
# and the single vkCreateWin32SurfaceKHR call. Depends on engine; engine
# never imports platform (see CLAUDE.md > "Layer split").

add_library(platform STATIC
    src/platform.cpp
)
target_include_directories(platform PUBLIC src)
target_link_libraries(platform PUBLIC engine)

# Win32-specific link bits (user32, gdi32) land here once the window /
# message-pump code does. Today the platform lib has no Win32 surface
# yet — it's just the scaffolded target.
