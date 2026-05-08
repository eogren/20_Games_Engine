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
    src/PongApplication.mm
    src/PongApplication.h
)

target_link_libraries(Pong PRIVATE
    ${METAL_FRAMEWORK}
    ${QUARTZCORE_FRAMEWORK}
    ${FOUNDATION_FRAMEWORK}
)

set_target_properties(Pong PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME           "Pong"
    MACOSX_BUNDLE_GUI_IDENTIFIER        "${PONG_BUNDLE_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION        "1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING  "0.1"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PONG_BUNDLE_ID}"
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
