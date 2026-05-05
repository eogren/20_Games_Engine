#!/usr/bin/env bash
# Run the Engine package's test suite via xcodebuild on macOS.
#
# We use xcodebuild rather than `swift test` because the renderer tests
# need a compiled metallib in the test bundle, and the SwiftPM CLI does
# not invoke the Metal compiler. xcodebuild does (as part of the normal
# Xcode build pipeline), so it's the only path that runs the renderer
# pixel-readback tests rather than skipping them.
#
# Pass-through: any extra args go to xcodebuild, e.g.
#   ./test.sh -only-testing:EngineTests/RendererSmokeTests

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

cd "$repo_root/Engine"
exec xcodebuild test \
    -scheme Engine \
    -destination 'platform=macOS' \
    "$@"
