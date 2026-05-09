#!/usr/bin/env bash
# Build and run the doctest unit suite (PongTests target).
#
# Uses a separate Ninja build dir from the main app build (build/mac is Xcode
# and slower to iterate on). Pass-through args go to ctest, e.g.:
#   ./test.sh --output-on-failure
#   ./test.sh -R Angle
#   ./test.sh --rerun-failed
#
# Override sanitizer or config via env: PONG_SANITIZE=address, CONFIG=Release.
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja not found on PATH. brew install ninja" >&2
    exit 127
fi

BUILD_DIR="${BUILD_DIR:-build/test}"
CONFIG="${CONFIG:-Debug}"

CONFIGURE_ARGS=(-G Ninja -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG")
[[ -n "${PONG_SANITIZE:-}" ]] && CONFIGURE_ARGS+=(-DPONG_SANITIZE="$PONG_SANITIZE")

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake "${CONFIGURE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR" --target PongTests
ctest --test-dir "$BUILD_DIR" --output-on-failure "$@"
