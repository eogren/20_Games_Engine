#!/usr/bin/env bash
# macOS build. Pass extra args through to xcodebuild.
# Override bundle ID / team via env: PONG_BUNDLE_ID, PONG_TEAM_ID, CONFIG.
# Enable sanitizers via PONG_SANITIZE=address|undefined|address,undefined.
# Sanitizers are sticky in the cache — to switch, use a separate BUILD_DIR
# (e.g. BUILD_DIR=build/mac-asan PONG_SANITIZE=address ./build.sh) or
# delete build/mac first.
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build/mac}"
CONFIG="${CONFIG:-Debug}"

CONFIGURE_ARGS=(-G Xcode -B "$BUILD_DIR")
[[ -n "${PONG_BUNDLE_ID:-}" ]] && CONFIGURE_ARGS+=(-DPONG_BUNDLE_ID="$PONG_BUNDLE_ID")
[[ -n "${PONG_TEAM_ID:-}"   ]] && CONFIGURE_ARGS+=(-DPONG_TEAM_ID="$PONG_TEAM_ID")
[[ -n "${PONG_SANITIZE:-}"  ]] && CONFIGURE_ARGS+=(-DPONG_SANITIZE="$PONG_SANITIZE")

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake "${CONFIGURE_ARGS[@]}"
fi

XCODE_ARGS=()
if [[ -z "${PONG_TEAM_ID:-}" ]]; then
    XCODE_ARGS+=(
        CODE_SIGNING_ALLOWED=NO
        CODE_SIGNING_REQUIRED=NO
        CODE_SIGN_IDENTITY=""
        DEVELOPMENT_TEAM=""
    )
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" -- "${XCODE_ARGS[@]}" "$@"
