# Python runtime binding

The `bindings/python` package is the first supported host-language wrapper for
LAE. It calls only the public C ABI through Python's standard-library `ctypes`
module. The package does not bundle a native library and does not depend on the
offline `lae-training` package.

## Loading the runtime

Build LAE with `BUILD_SHARED_LIBS=ON`. Pass the resulting library path to
`Runtime`, set the `LAE_RUNTIME` environment variable, or install the shared
library where the operating system's library discovery can find it.

```python
from lae import Runtime

with Runtime("/path/to/lible_runtime.so") as runtime:
    plan = runtime.process("Language-aware emphasis")
```

The filename is normally `lible_runtime.dylib` on macOS,
`lible_runtime.so` on Linux, and `le_runtime.dll` on Windows. Explicit paths
are the most predictable choice for applications that package the runtime.

## Processing

`Runtime.process` returns an immutable tuple of `Emphasis` values. Spans remain
half-open UTF-8 byte offsets; they are deliberately not translated to Python
character indexes.

```python
from lae import ProcessOptions, ReadingModel, Runtime

options = ProcessOptions(language="en", reading_model=ReadingModel.LEXICAL_CORE)
with Runtime("/path/to/lible_runtime.so") as runtime:
    plan = runtime.process("unbelievable reading", options)
```

`ProcessOptions` also exposes prefix strategy, fixed and proportional prefix
parameters, binary or variable-strength presentation, salience threshold, and
emphasis strength. Invalid combinations are validated by the canonical native
runtime so every binding observes the same contract.

Explicit language regions use the same processing and result types. Region
spans are UTF-8 byte offsets and must form a contiguous, grapheme-aligned
partition of the input:

```python
from lae import LanguageRegion, ProcessOptions, ReadingModel, Runtime, TextSpan

text = "unbelievable 日本語を 研究"
regions = [
    LanguageRegion(TextSpan(0, 13), "en"),
    LanguageRegion(TextSpan(13, 26), "ja"),
    LanguageRegion(TextSpan(26, 32), "zh-Hans"),
]
options = ProcessOptions(reading_model=ReadingModel.LEXICAL_CORE)
with Runtime("/path/to/lible_runtime.so") as runtime:
    plan = runtime.process_regions(text, regions, options)
```

Leave `ProcessOptions.language` empty when supplying regions. The optional
`model=` argument works the same way as it does for `Runtime.process` and the
native runtime checks that the artifact supports every region language.

## Model artifacts

Load compiled `.lem` bytes or a file through the runtime. A model exposes its
type, producer version, minimum ABI, supported languages, and required feature
IDs.

```python
from lae import ProcessOptions, Runtime

with Runtime("/path/to/lible_runtime.so") as runtime:
    with runtime.load_model_file("reading.lem") as model:
        plan = runtime.process(
            "artifact-driven emphasis",
            ProcessOptions(language="en"),
            model=model,
        )
```

The native ABI permits a model to outlive the runtime that loaded it, and the
binding preserves that rule. Close models and runtimes independently, either
explicitly or with context managers. Emphasis values are copied before the
native result is destroyed, so returned plans remain ordinary Python data.

## Errors and concurrency

Native failures raise `LaeError` with the stable numeric status, symbolic status
name, and the runtime's thread-local diagnostic. A `Runtime` supports concurrent
processing according to the C contract. Closing a runtime while another thread
is using it is not supported.
