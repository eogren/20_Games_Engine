# Configure (Xcode generator):
#   macOS:  cmake -G Xcode -B build/mac
#   iOS:    cmake -G Xcode -B build/ios -DCMAKE_SYSTEM_NAME=iOS
# Then open build/<plat>/Pong.xcodeproj.

cmake_minimum_required(VERSION 3.28)

project(Pong LANGUAGES CXX OBJCXX)

# C++23, not 26: the 26.2 SDK headers on the GHA macos-26 runner emit
# enum-enum arithmetic errors under -std=c++26 (CGImage.h, ColorSyncDeprecated.h).
# Apple fixed those headers in 26.4; revisit when the runner image catches up.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json so clang-tidy (cpp/lint.sh) can find compile flags.
# Xcode generator ignores this; cpp/lint.sh configures a side Ninja build dir.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Metal 4 ships in macOS 26 / iOS 26.
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING "" FORCE)
if(NOT CMAKE_OSX_SYSROOT OR CMAKE_OSX_SYSROOT STREQUAL "macosx")
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_pong_sdk iphoneos)
    else()
        set(_pong_sdk macosx)
    endif()
    execute_process(
        COMMAND xcrun --sdk ${_pong_sdk} --show-sdk-path
        OUTPUT_VARIABLE _pong_sdk_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CMAKE_OSX_SYSROOT "${_pong_sdk_path}" CACHE STRING "" FORCE)
endif()

set(PONG_BUNDLE_ID "com.eogren.Pong" CACHE STRING "App bundle identifier (reverse-DNS)")
set(PONG_TEAM_ID   ""                CACHE STRING "Apple Developer Team ID (10-char) for code signing")

find_library(METAL_FRAMEWORK     Metal      REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

add_executable(Pong MACOSX_BUNDLE
    src/main.mm
    src/math/Math.h
    src/Pong.cpp
    src/Pong.h
    src/PongApplication.mm
    src/PongApplication.h
    src/Platform.mm
)

# Metal shaders: compile src/Pong.metal -> default.metallib and embed in
# the bundle's Resources, where Device::newDefaultLibrary() finds it.
#
# CMake's Xcode generator recognizes .metal as a source type but doesn't
# wire it into the Sources build phase, and there's no MTL language for
# Ninja either. Driving xcrun metal / metallib via custom commands is the
# portable path. Custom commands only fire on a real build, so the lint
# side-build (configure-only Ninja for compile_commands.json) is unaffected.
set(_pong_metal_src ${CMAKE_CURRENT_SOURCE_DIR}/src/Pong.metal)
set(_pong_air       ${CMAKE_CURRENT_BINARY_DIR}/Pong.air)
set(_pong_metallib  ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_pong_metal_sdk iphoneos)
else()
    set(_pong_metal_sdk macosx)
endif()
add_custom_command(
    OUTPUT  ${_pong_air}
    COMMAND xcrun -sdk ${_pong_metal_sdk} metal -c ${_pong_metal_src} -o ${_pong_air}
    DEPENDS ${_pong_metal_src}
    COMMENT "metal -c Pong.metal"
    VERBATIM
)
add_custom_command(
    OUTPUT  ${_pong_metallib}
    COMMAND xcrun -sdk ${_pong_metal_sdk} metallib ${_pong_air} -o ${_pong_metallib}
    DEPENDS ${_pong_air}
    COMMENT "metallib -> default.metallib"
    VERBATIM
)
target_sources(Pong PRIVATE ${_pong_metallib})
set_source_files_properties(${_pong_metallib} PROPERTIES
    GENERATED TRUE
    MACOSX_PACKAGE_LOCATION Resources
)

# SYSTEM so clang-tidy / clang-analyzer don't flag third-party diagnostics.
# metal-cpp's NSSharedPtr destructor calls release() unconditionally on a
# possibly-null receiver — safe under ObjC ([nil release] is a no-op) and
# already guarded with __attribute__((no_sanitize("undefined"))), but the
# analyzer can't see that. SYSTEM treats the headers as opaque.
target_include_directories(Pong SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/metal-cpp
)

target_link_libraries(Pong PRIVATE
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
)

# Enable ARC for Objective-C++ sources under all generators.
# Xcode also reads XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC below, but Ninja
# (used by cpp/lint.sh) needs the explicit -fobjc-arc compile flag.
#
# -fblocks lets C++ sources use Apple block syntax (^{ ... }) — needed for
# metal-cpp callbacks (e.g. CommandBuffer::addCompletedHandler) that take
# blocks. ObjC/ObjC++ has blocks on by default, so the flag is only needed
# for CXX, but applying to OBJCXX too is harmless and keeps things uniform.
target_compile_options(Pong PRIVATE
    $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
    $<$<OR:$<COMPILE_LANGUAGE:CXX>,$<COMPILE_LANGUAGE:OBJCXX>>:-fblocks>
)

set_target_properties(Pong PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME           "Pong"
    MACOSX_BUNDLE_GUI_IDENTIFIER        "${PONG_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION        "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING  "0.1"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PONG_BUNDLE_ID}"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_MetalCaptureEnabled YES
)

if(PONG_TEAM_ID)
    set_target_properties(Pong PROPERTIES
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${PONG_TEAM_ID}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE  "Automatic"
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    find_library(UIKIT_FRAMEWORK UIKit REQUIRED)
    target_link_libraries(Pong PRIVATE ${UIKIT_FRAMEWORK})
    set_target_properties(Pong PROPERTIES
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY      "1,2"  # iPhone + iPad
        XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET  "26.0"
    )
else()
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    target_link_libraries(Pong PRIVATE ${APPKIT_FRAMEWORK})
endif()

# Opt-in sanitizers. Use a separate build dir so toggles don't fight cache:
#   cmake -G Xcode -B build/asan  -DPONG_SANITIZE=address
#   cmake -G Ninja -B build/ubsan -DPONG_SANITIZE=undefined
#   cmake -G Xcode -B build/san   -DPONG_SANITIZE=address,undefined
# LSan (leak detection) does not work on Apple Silicon; ASan still catches
# use-after-free, OOB, double-free.
set(PONG_SANITIZE "" CACHE STRING "Sanitizers to enable: address, undefined, or address,undefined. Empty = off.")

# Tests. doctest is vendored single-header at third_party/doctest/doctest.h.
# PongTests is a plain CLI executable (no MACOSX_BUNDLE, no Metal/AppKit deps)
# so the math/test surface stays light. Run with:  cd cpp && ./test.sh
enable_testing()

add_executable(PongTests
    tests/MathTests.cpp
)

target_include_directories(PongTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# SYSTEM keeps doctest's own diagnostics out of clang-tidy's view, mirroring
# how metal-cpp is included in the main target.
target_include_directories(PongTests SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# doctest's own headers trip a few warnings under modern clang/c++23. They're
# third-party code we don't want to patch — silence at the test target only.
target_compile_options(PongTests PRIVATE
    -Wno-deprecated-declarations
)

add_test(NAME MathTests COMMAND PongTests)

if(PONG_SANITIZE)
    string(REPLACE "," ";" _pong_san_list "${PONG_SANITIZE}")
    foreach(_san IN LISTS _pong_san_list)
        if(NOT _san MATCHES "^(address|undefined)$")
            message(FATAL_ERROR "PONG_SANITIZE: unknown sanitizer '${_san}' (supported: address, undefined)")
        endif()
    endforeach()

    if(CMAKE_GENERATOR STREQUAL "Xcode")
        # Xcode build settings — applied per-target. The plain compile flags
        # below would also work under Xcode, but using the dedicated build
        # settings keeps the scheme's "Diagnostics" tab consistent.
        if("address" IN_LIST _pong_san_list)
            set_target_properties(Pong PongTests PROPERTIES XCODE_ATTRIBUTE_ENABLE_ADDRESS_SANITIZER YES)
        endif()
        if("undefined" IN_LIST _pong_san_list)
            set_target_properties(Pong PongTests PROPERTIES XCODE_ATTRIBUTE_ENABLE_UNDEFINED_BEHAVIOR_SANITIZER YES)
        endif()
    else()
        foreach(_t IN ITEMS Pong PongTests)
            target_compile_options(${_t} PRIVATE -fsanitize=${PONG_SANITIZE} -fno-omit-frame-pointer)
            target_link_options(${_t} PRIVATE -fsanitize=${PONG_SANITIZE})
        endforeach()
    endif()

    message(STATUS "Pong: sanitizers enabled — ${PONG_SANITIZE}")
endif()
