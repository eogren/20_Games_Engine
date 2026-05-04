// swift-tools-version: 6.1
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "Engine",
    platforms: [
        .macOS(.v14),
        .iOS(SupportedPlatform.IOSVersion.v17)
    ],
    products: [
        // Products define the executables and libraries a package produces, making them visible to other packages.
        .library(
            name: "Engine",
            targets: ["Engine"]
        ),
    ],
    targets: [
        // Targets are the basic building blocks of a package, defining a module or a test suite.
        // Targets can depend on other targets in this package and products from dependencies.
        .target(
            name: "Engine",
            resources: [
                .process("Shaders"),
            ]
        ),
        .testTarget(
            name: "EngineTests",
            dependencies: ["Engine"]
        ),
    ],
    swiftLanguageModes: [.v6]
)
