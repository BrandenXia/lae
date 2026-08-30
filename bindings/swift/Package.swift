// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "LAE",
    platforms: [
        .macOS(.v11),
    ],
    products: [
        .library(name: "LAE", targets: ["LAE"]),
    ],
    targets: [
        .systemLibrary(name: "CLAE"),
        .target(name: "LAE", dependencies: ["CLAE"]),
        .executableTarget(
            name: "LAEIntegrationTests",
            dependencies: ["LAE"],
            path: "Tests/LAEIntegrationTests"
        ),
    ]
)
