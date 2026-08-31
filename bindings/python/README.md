# LAE Python runtime binding

This package is a dependency-free `ctypes` wrapper around LAE's stable C ABI.
It does not bundle the native runtime and has no dependency on the separate
`lae-training` package.

Build LAE as a shared library, then pass its path explicitly or set
`LAE_RUNTIME`:

```python
from lae import ProcessOptions, ReadingModel, Runtime

with Runtime("build-shared/lible_runtime.dylib") as runtime:
    plan = runtime.process(
        "unbelievable reading",
        ProcessOptions(language="en", reading_model=ReadingModel.LEXICAL_CORE),
    )

for emphasis in plan:
    print(emphasis.span.begin, emphasis.span.end, emphasis.strength)
```

`Runtime.process_regions` accepts explicit `LanguageRegion` values for
high-level mixed-language processing through the same result type.

All spans use half-open UTF-8 byte offsets, matching the C ABI. A loaded model
owns its native handle and must be closed independently of the runtime that
loaded it; both types support context managers.
