#!/usr/bin/env bash
# Build the Platform package for iOS as a cross-compile gate.
#
# We target `generic/platform=iOS` (real device, arm64 only) rather than
# the iOS Simulator. The iOS Simulator SDK does NOT ship the MTL4 types
# the renderer uses (e.g. MTL4RenderPassDescriptor, MTL4CommandQueue) —
# they exist only in the iPhoneOS device SDK and the macOS SDK. So a
# simulator build of Engine fails to find those symbols regardless of
# arch. Building for a generic iOS device validates that the iOS-side
# code compiles for the platform it'll actually deploy to; revisit if
# Apple ships MTL4 in the simulator SDK.
#
# Pass-through: any extra args go to xcodebuild.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"
exec xcodebuild build \
    -scheme Platform \
    -destination 'generic/platform=iOS' \
    -configuration Debug \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO \
    CODE_SIGN_IDENTITY="" \
    DEVELOPMENT_TEAM="" \
    "$@"
