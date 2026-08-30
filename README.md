# LAE

LAE is an early language-aware typographic emphasis runtime. The current
milestone provides a stable C ABI, a validated and inspectable linguistic IR,
Unicode-safe generic analysis, and fixed or proportional grapheme-prefix
baselines. It intentionally has no
language-specific morphology, renderer, training dependency, model loader, or
dynamic plugin system yet.

## Build and test

LAE requires CMake 3.20+, a C11/C++20 compiler, `pkg-config`, and
[`utf8proc`](https://github.com/JuliaStrings/utf8proc).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Try the C-ABI-only CLI:

```sh
printf 'Reading 世界 👩‍🚀' | build/le-cli --proportion 0.5
printf '我在研究 structured concurrency' | build/le-cli --language zh-Hans --dump-analysis
```

The CLI prints ordered, non-overlapping UTF-8 byte ranges or the provider's
nodes, features, and language regions as JSON.

## Current architecture

```text
UTF-8 → generic provider → linguistic IR → prefix reading model
      → reading signals → binary presentation policy → emphasis spans
```

The runtime owns result storage. Callers borrow its contiguous emphasis array
until `le_result_destroy`. Rendering stays in the host application.

See [architecture](docs/architecture.md), [C API](docs/c-api.md), and
[text and offsets](docs/text-and-offsets.md) for the contracts.
