# Swift runtime binding

`bindings/swift` is a Swift Package Manager library that wraps the canonical
LAE C ABI. The package imports installed public headers through the `CLAE`
system-library target and links `le_runtime`; it does not compile or reach into
the C++ implementation.

## Package integration

During development, add the package by local path:

```swift
.package(path: "/path/to/lae/bindings/swift")
```

Depend on the `LAE` product from the application target. Build or install LAE
as a shared library first, then provide its prefix to Swift:

```sh
swift build \
  -Xcc -I/path/to/prefix/include \
  -Xlinker -L/path/to/prefix/lib
```

The application must also be able to locate the shared library at launch. Use
the platform's normal application-bundling mechanism or an appropriate runtime
search path. The binding does not perform filesystem-backed dynamic loading.

## Processing and spans

```swift
import LAE

let runtime = try Runtime()
defer { runtime.close() }

let options = ProcessOptions(language: "en", readingModel: .lexicalCore)
let plan = try runtime.process("unbelievable reading", options: options)
```

`Runtime.process` returns copied `[Emphasis]` values. Each `TextSpan` uses the
C ABI's half-open UTF-8 byte coordinates. `range(in:)` and `substring(in:)`
convert those offsets into Swift indices only when both boundaries are valid
for the supplied string. This avoids treating Swift's native string indices,
UTF-16 offsets, Unicode-scalar positions, and UTF-8 byte offsets as
interchangeable.

## Artifact models and ownership

Load `.lem` bytes or a file through `Runtime`:

```swift
let model = try runtime.loadModel(contentsOf: modelURL)
defer { model.close() }

let plan = try runtime.process(
    "artifact-driven emphasis",
    options: ProcessOptions(language: "en"),
    model: model
)
```

`Model` exposes the model type, producer version, minimum ABI, supported
languages, and required feature IDs. Models and runtimes own separate opaque
handles: a model remains valid after its loading runtime closes. `close()` is
idempotent, and deinitialization is a fallback. `Runtime` and `Model` are
sendable for concurrent immutable operations, matching the C ABI. Closing an
object concurrently with an operation on that same object is unsupported.

Native failures become `LAEError` values containing the stable numeric status,
symbolic name, and thread-local diagnostic.

## Verification harness

The `LAEIntegrationTests` executable uses no third-party package or Apple test
framework. It checks grapheme-safe processing, UTF-8-to-Swift span conversion,
English provider routing, valid and invalid artifact loading, model lifetime,
diagnostics, and closed-handle rejection.

```sh
swift run --package-path bindings/swift \
  -Xcc -I/path/to/prefix/include \
  -Xlinker -L/path/to/prefix/lib \
  -Xlinker -rpath -Xlinker /path/to/prefix/lib \
  LAEIntegrationTests
```
