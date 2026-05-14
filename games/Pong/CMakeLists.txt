# Pong — first game on the new engine. Implements engine's Game interface
# (once that exists) and hands the instance to platform's host (once that
# exists). At scaffold time, just a hello-world that links engine + platform
# to prove the dependency graph.

add_executable(Pong
    src/main.cpp
)
# Pong sees both engine and platform directly: it constructs a Platform,
# hands it to Engine, then runs the loop. The dep on platform used to be
# transitive (when platform PUBLIC-linked engine); after the flip to
# engine -> platform, Pong has to name both explicitly.
target_link_libraries(Pong PRIVATE engine platform engine_compile_options)
