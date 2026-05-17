# Pong — first game on the new engine. Implements engine's Game interface
# (once that exists) and hands the instance to platform's host (once that
# exists). At scaffold time, just a hello-world that links engine + platform
# to prove the dependency graph.

set(PONG_SOURCES src/main.cpp src/physics.cpp)

# On Windows, ship the embedded app icon. CMake recognises .rc and runs
# MSVC's rc.exe to compile it; the resulting .res is linked into Pong.exe,
# giving Explorer/taskbar the icon and letting the platform layer LoadIcon
# it by name for the window class.
if(WIN32)
    list(APPEND PONG_SOURCES resources/AppIcon.rc)
endif()

add_executable(Pong ${PONG_SOURCES})
target_include_directories(Pong PRIVATE src)
target_link_libraries(Pong PRIVATE engine platform engine_compile_options)

stage_engine_shaders_for(Pong)

# ---- Tests ----------------------------------------------------------

add_executable(pong_tests
        tests/test_main.cpp
        tests/physics_tests.cpp
        src/physics.cpp
)
target_include_directories(pong_tests PRIVATE src)
target_link_libraries(pong_tests PRIVATE engine doctest engine_compile_options)
add_test(NAME pong_tests COMMAND pong_tests)
