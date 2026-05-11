# 20 Games Engine

An Apple-only, Metal-backed game engine, built alongside the games it
runs. The first game is a synthwave tunnel-runner take on Flappy Bird;
the engine grows from concrete game needs rather than speculative
abstractions (see [`CLAUDE.md`](CLAUDE.md)).

Targets macOS 14+ and iPadOS 17+. Vulkan / DX12 / WebGPU are explicitly
out of scope.

## Layout

```
Engine/      Swift package — game logic, simulation, math, input, rendering.
Platform/    Swift package — OS-divergence layer (window/view, app lifecycle,
             display link, touch vs. mouse). Depends on Engine.
FlappyBird/  Xcode app — first game, implements Engine's `Game` protocol
             and hands the instance to Platform's `Host`.
```

Engine never imports Platform. Cross-Apple input (keyboard, gamepad,
GCMouse) lives in Engine; OS-divergent input (touch, NSResponder mouse)
lives in Platform.

## Build & run

The FlappyBird app is the canonical way to bring up the engine — it's
the only target that pulls in compiled `.metal` shaders.

```sh
open FlappyBird/FlappyBird.xcodeproj
# Pick the FlappyBird scheme; Run on a Mac, an iPad, or the iOS simulator.
```

iOS device builds can also be driven from the command line:

```sh
Platform/build-ios.sh
```

## Tests

Each package owns its own `test.sh`:

```sh
Engine/test.sh                                   # full Engine suite (xcodebuild)
Engine/test.sh -only-testing:EngineTests/RendererSmokeTests
```

`xcodebuild` is required because SwiftPM's CLI doesn't invoke the Metal
compiler — renderer pixel-readback tests skip under `swift test` but run
under `xcodebuild`.

## Documentation

- [`CLAUDE.md`](CLAUDE.md) — architecture, design influences, layer
  split, frame ordering, workflow conventions.
- [`TASKS.md`](TASKS.md) — engineering plan: phased substrate work
  driven by the current game's needs.
- [`INSPIRATION.md`](INSPIRATION.md) — art and music direction:
  references, palette, shape language, post-processing.

## License

Licensed under the [Apache License, Version 2.0](LICENSE).

Vendored third-party code under `cpp/third_party/` retains its original
license: `metal-cpp` (Apache-2.0) and `doctest` (MIT). See each
directory's `LICENSE.txt`.
