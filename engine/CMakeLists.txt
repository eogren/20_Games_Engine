# engine — game logic, simulation, math, input, rendering. Vulkan lives
# here; platform/ owns OS divergence (window, message pump, surface).
#
# See CLAUDE.md > "Layer split" for the direction-of-deps rule:
# engine depends on platform (one-way). The link is PRIVATE because
# engine.h only forward-declares platform::Platform — the include of
# platform/surface.h sits in engine.cpp.

# ---- Third-party libs (vendored under third_party/) -----------------
#
# SYSTEM includes keep their internal warnings out of clang-tidy's view
# of our code, mirroring the pattern from the legacy cpp/ tree.

# Volk — Vulkan function loader. Compiled as a small static lib.
# volk.h includes <vulkan/vulkan.h> with VK_NO_PROTOTYPES defined, so
# anyone using volk also gets Vulkan headers transparently.
add_library(volk STATIC third_party/volk/volk.c)
target_include_directories(volk SYSTEM PUBLIC third_party/volk)
target_link_libraries(volk PUBLIC Vulkan::Headers)
# Enable platform-specific Vulkan declarations (vkCreateWin32SurfaceKHR etc.) in
# both volk.c (where the function-pointer storage is allocated) and consumers
# (where the declarations are needed). PUBLIC so the define propagates.
if(WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
endif()

# VMA — Vulkan Memory Allocator. Header-only at use-site; one TU in
# engine defines VMA_IMPLEMENTATION when VMA actually starts getting used.
# Today this is include-path-only.
add_library(vma INTERFACE)
target_include_directories(vma SYSTEM INTERFACE third_party/vma)
target_link_libraries(vma INTERFACE Vulkan::Headers)

# doctest — header-only test framework. Test executables link this;
# engine itself does not.
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE third_party/doctest)

# ---- Engine library -------------------------------------------------

add_library(engine STATIC
    src/engine.cpp
    src/log/Log.cpp
    src/renderer/renderer.cpp
    src/renderer/renderer_init.cpp
)
target_include_directories(engine PUBLIC src)
target_link_libraries(engine
    PUBLIC  volk vma spdlog::spdlog
    PRIVATE engine_compile_options platform
)

# ---- Tests ----------------------------------------------------------

# smoke_test.cpp defines doctest's main (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN).
# Additional test TUs in this exe just #include "doctest.h" — defining the
# implementation macro in more than one TU breaks the link.
add_executable(engine_tests
    tests/smoke_test.cpp
    tests/MathTests.cpp
    tests/LogTests.cpp
)
target_link_libraries(engine_tests PRIVATE engine doctest engine_compile_options)
add_test(NAME engine_tests COMMAND engine_tests)
