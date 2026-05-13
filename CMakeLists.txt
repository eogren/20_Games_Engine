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

# Vulkan SDK provides headers + slangc. Volk does runtime function loading,
# so engine links against Vulkan::Headers (header-only) rather than
# Vulkan::Vulkan (the loader stub library).
find_package(Vulkan 1.3 REQUIRED)

enable_testing()

add_subdirectory(engine)
add_subdirectory(platform)
add_subdirectory(games/Pong)
