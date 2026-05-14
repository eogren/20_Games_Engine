cmake_minimum_required(VERSION 3.28)

project(20GamesEngine LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# C is for volk.c. C11 is more than enough for it.
set(CMAKE_C_STANDARD 11)

# Emit compile_commands.json for clangd / clang-tidy. Multi-config generators
# (Visual Studio, Xcode) ignore this; Ninja honors it.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20 modules are out of scope (substrate-first; no `import` anywhere in the
# tree). With scanning enabled, CMake + Ninja + MSVC emits per-TU response-file
# refs (`@.../foo.cpp.obj.modmap`) into compile_commands.json that only exist
# after a build, so clang-tidy errors out reading them. Turning scanning off
# strips those response-file flags entirely, and shaves a small dyndep step
# off the build path we'd never use.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Vulkan SDK provides headers + slangc. Volk does runtime function loading,
# so engine links against Vulkan::Headers (header-only) rather than
# Vulkan::Vulkan (the loader stub library).
find_package(Vulkan 1.3 REQUIRED)

# ---- Build options --------------------------------------------------
#
# Sanitizers + MSVC /analyze ride on top of a normal Debug build. The
# CMakePresets.json file at the repo root flips these on for the
# `asan`, `ubsan`, and `analyze` presets; nothing else needs to know
# about them.

option(ENGINE_ENABLE_ASAN "Build with /fsanitize=address (MSVC or clang-cl)." OFF)
option(ENGINE_ENABLE_UBSAN "Build with -fsanitize=undefined (clang-cl only — MSVC does not ship UBSan)." OFF)
option(ENGINE_ENABLE_MSVC_ANALYZE "Build with MSVC /analyze. Slow — intended for a dedicated CI lane." OFF)

# /RTC1 (default in MSVC Debug) is incompatible with /fsanitize=address.
# CMake bakes /RTC1 into CMAKE_<LANG>_FLAGS_DEBUG via its platform module,
# so we strip it globally when ASan is on. Done before any target is
# defined so every TU agrees on the flag set.
if(ENGINE_ENABLE_ASAN AND MSVC)
    foreach(flag_var
        CMAKE_C_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        string(REGEX REPLACE "/RTC[1csu]+" "" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

# ---- engine_compile_options -----------------------------------------
#
# INTERFACE library carrying our compile flags. Linked PRIVATE to
# engine, platform, and per-game targets so the flags apply to *our*
# code only — third_party libs (volk, vma) are left alone with their
# own warning posture. The MSVC variable is also TRUE under clang-cl
# (it emulates MSVC's driver), so the flags below cover both compilers
# the build supports today.

add_library(engine_compile_options INTERFACE)

if(MSVC)
    target_compile_options(engine_compile_options INTERFACE
        # Warnings
        /W4
        /WX
        # External-header posture. CMake's SYSTEM include keyword maps
        # to /external:I on MSVC 16.10+, so headers reached via SYSTEM
        # (volk, vma, spdlog, doctest) get marked external. /external:W0
        # silences regular warnings inside them — third-party warnings
        # are not ours to fix, and /WX would otherwise turn them into
        # build failures (e.g. spdlog/fmt's bundled base.h trips C6239
        # under /analyze).
        /external:W0
        # Conformance — the MSVC defaults predate C++17.
        /permissive-       # strict standard name lookup, two-phase template lookup, etc.
        /Zc:preprocessor   # conforming preprocessor (legacy MSVC one mangles __VA_ARGS__)
        /Zc:__cplusplus    # __cplusplus reflects the actual standard, not 199711L
        /utf-8             # source + execution charset; dodges smart-quote-in-literal traps
    )

    if(ENGINE_ENABLE_ASAN)
        target_compile_options(engine_compile_options INTERFACE
            /fsanitize=address
            /Zi  # ensure debug info for ASan symbolication
        )
        # No explicit link flag needed: the MSVC driver auto-links
        # clang_rt.asan_dynamic-x86_64.lib via /INFERASANLIBS. The
        # matching DLL must be on PATH at runtime — vcvarsall (and
        # ilammy/msvc-dev-cmd in CI) takes care of that.
    endif()

    if(ENGINE_ENABLE_MSVC_ANALYZE)
        # Treat /analyze warnings (C6xxx) as errors via the /WX already
        # set above. If a false positive surfaces in our code, prefer a
        # localized /wd6XXX or a __pragma annotation over weakening the
        # lane. /analyze:external- skips analysis of third-party headers
        # marked via /external:I (CMake SYSTEM include dirs) — their
        # warnings aren't ours to fix.
        target_compile_options(engine_compile_options INTERFACE
            /analyze
            /analyze:external-
        )
    endif()
endif()

if(ENGINE_ENABLE_UBSAN)
    # UBSan on Windows requires clang-cl (CMake sees CMAKE_CXX_COMPILER_ID
    # = "Clang" for both clang and clang-cl). MSVC's cl.exe does not ship
    # a UBSan runtime — error early instead of producing a build that
    # silently has no UBSan instrumentation.
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR
            "ENGINE_ENABLE_UBSAN requires clang-cl. "
            "Configure with the `ubsan` preset, which sets CMAKE_CXX_COMPILER=clang-cl. "
            "Got compiler id: ${CMAKE_CXX_COMPILER_ID}")
    endif()
    target_compile_options(engine_compile_options INTERFACE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined  # abort on UB instead of just logging
    )
    target_link_options(engine_compile_options INTERFACE -fsanitize=undefined)
endif()

# ---- Third-party deps fetched at configure time ---------------------
#
# Single-header / single-file deps stay vendored under
# engine/third_party/ (see engine/third_party/VENDORING.md). spdlog is
# a whole source tree, so it lives here under FetchContent instead —
# version-pinned in source, reproducible, no submodule mechanics. When
# a 2nd non-trivial dep arrives the policy is to introduce vcpkg; until
# then FetchContent is the right call for a one-off.
#
# SYSTEM marks spdlog's include dirs as system headers so its own
# warnings stay out of /W4 + clang-tidy on our code. EXCLUDE_FROM_ALL
# keeps spdlog out of the default build target — it's pulled in only
# when engine links against it.

include(FetchContent)

set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(spdlog)

# MSVC's STL emits a per-TU '__annotate_string' / '__annotate_vector' sentinel
# whose value depends on whether the TU was compiled with /fsanitize=address.
# Mixing ASan and non-ASan TUs that touch std::string/std::vector in the same
# link fails with LNK2038. spdlog is built from source here, so rebuild it
# under the same sanitizer flag — engine_compile_options is intentionally
# scoped to our code (warning posture), but ASan is a link-time contract that
# every TU sharing STL types must agree on.
if(ENGINE_ENABLE_ASAN AND MSVC AND TARGET spdlog)
    target_compile_options(spdlog PRIVATE /fsanitize=address)
endif()

enable_testing()

add_subdirectory(engine)
add_subdirectory(platform)
add_subdirectory(games/Pong)
