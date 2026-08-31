# LAE Swift binding

This Swift package wraps LAE's stable C ABI with native value types and explicit
runtime/model ownership. It preserves UTF-8 byte offsets while providing safe
conversion helpers for Swift `String` values.

The package does not bundle LAE. Install or build the shared runtime and make
its headers and library visible when Swift builds the package:

```sh
swift build --package-path bindings/swift \
  -Xcc -I/path/to/lae/include \
  -Xlinker -L/path/to/lae/lib
```

Add `bindings/swift` as a local package dependency during development, or use
the repository URL and a tagged release. Applications import the `LAE` product:

```swift
import LAE

let runtime = try Runtime()
defer { runtime.close() }

let options = ProcessOptions(language: "en", readingModel: .lexicalCore)
let plan = try runtime.process("unbelievable reading", options: options)
```

The `regions:` overload accepts explicit `LanguageRegion` values for
high-level mixed-language processing through the same `[Emphasis]` result.

`Runtime` and `Model` close idempotently and also release their native handles
on deinitialization. A model may outlive the runtime that loaded it, matching the
C ABI. Both types are sendable for concurrent immutable operations. Do not call
`close()` concurrently with another operation on the same object.

The `LAEIntegrationTests` executable verifies Unicode span conversion,
language-aware processing, artifact metadata and lifetime, native diagnostics,
and closed-handle behavior without requiring XCTest:

```sh
swift run --package-path bindings/swift \
  -Xcc -I/path/to/lae/include \
  -Xlinker -L/path/to/lae/lib \
  -Xlinker -rpath -Xlinker /path/to/lae/lib \
  LAEIntegrationTests
```
