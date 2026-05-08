#!/usr/bin/env bash
# iOS build. Defaults to iPhone simulator (no signing required).
# Override via env: IOS_SDK=iphoneos for device, PONG_BUNDLE_ID, PONG_TEAM_ID, CONFIG.
set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="build/ios"
CONFIG="${CONFIG:-Debug}"
SDK="${IOS_SDK:-iphonesimulator}"

CONFIGURE_ARGS=(-G Xcode -B "$BUILD_DIR" -DCMAKE_SYSTEM_NAME=iOS)
[[ -n "${PONG_BUNDLE_ID:-}" ]] && CONFIGURE_ARGS+=(-DPONG_BUNDLE_ID="$PONG_BUNDLE_ID")
[[ -n "${PONG_TEAM_ID:-}"   ]] && CONFIGURE_ARGS+=(-DPONG_TEAM_ID="$PONG_TEAM_ID")

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake "${CONFIGURE_ARGS[@]}"
fi

XCODE_ARGS=(-sdk "$SDK")
if [[ -z "${PONG_TEAM_ID:-}" ]]; then
    XCODE_ARGS+=(
        CODE_SIGNING_ALLOWED=NO
        CODE_SIGNING_REQUIRED=NO
        CODE_SIGN_IDENTITY=""
        DEVELOPMENT_TEAM=""
    )
fi

cmake --build "$BUILD_DIR" --config "$CONFIG" -- "${XCODE_ARGS[@]}" "$@"
