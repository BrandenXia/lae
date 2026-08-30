# LAE

LAE is an early language-aware typographic emphasis runtime. The current
milestone provides a stable C ABI, a validated and inspectable linguistic IR,
a generic Unicode fallback, a statically linked rule-based English provider,
prefix and lexical-core reading models, and binary or variable-strength
presentation. It intentionally has no renderer, training dependency, model
loader, automatic language detection, or dynamic plugin system yet.

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
printf 'progressive emphasis' | build/le-cli --fixed 3 --dump-signals
printf 'progressive emphasis' | build/le-cli --fixed 3 --policy variable --min-strength 0.2
printf 'unbelievable reading' | build/le-cli --language en --model lexical-core
printf 'unbelievable reading' | build/le-cli --language en --dump-analysis
```

The CLI prints ordered, non-overlapping UTF-8 byte ranges or the provider's
nodes, features, and language regions as JSON.

## Current architecture

```text
UTF-8 → language router → generic or English provider → linguistic IR
      → prefix or lexical-core reading model → reading signals
      → binary/variable presentation policy → emphasis spans
```

The runtime owns result storage. Callers borrow its contiguous emphasis array
until `le_result_destroy`. Rendering stays in the host application.

See [architecture](docs/architecture.md), [C API](docs/c-api.md),
[English provider](docs/english-provider.md),
[reading and presentation](docs/reading-and-presentation.md), and
[text and offsets](docs/text-and-offsets.md) for the contracts.
