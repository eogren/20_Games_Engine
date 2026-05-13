# Pong — first game on the new engine. Implements engine's Game interface
# (once that exists) and hands the instance to platform's host (once that
# exists). At scaffold time, just a hello-world that links engine + platform
# to prove the dependency graph.

add_executable(Pong
    src/main.cpp
)
target_link_libraries(Pong PRIVATE platform engine_compile_options)
