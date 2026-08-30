# LAE

LAE is an early language-aware typographic emphasis runtime. The current
milestone provides a stable C ABI, a validated and inspectable linguistic IR,
a generic Unicode fallback, statically linked rule-based English and Chinese
providers, prefix and lexical-core reading models, and binary or
variable-strength presentation. Versioned `.lem` artifacts can now carry model
parameters and capability metadata into the memory-based runtime loader. An
independent standard-library Python package can validate offline datasets,
extract features, optimize a deterministic prefix baseline, evaluate it, and
export the same artifact format. Its strategy-neutral evaluation layer compares
offline plan density/complexity and paired human-study outcomes. LAE still
intentionally has no renderer, runtime training dependency, automatic language
detection, neural model, or dynamic plugin system.

## Build and test

LAE requires CMake 3.20+, a C11/C++20 compiler, `pkg-config`, and
[`utf8proc`](https://github.com/JuliaStrings/utf8proc). The complete test suite
also requires Python 3.10+; runtime-only builds with `LAE_BUILD_TESTS=OFF` do
not require Python.

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
printf '中华人民共和国' | build/le-cli --language zh-Hans --model lexical-core
printf '研究生命起源' | build/le-cli --language zh-Hans --dump-analysis
build/le-model compile-prefix build/prefix.lem --fixed 2 --language en
build/le-model inspect build/prefix.lem
printf 'artifact driven' | build/le-cli --artifact build/prefix.lem --language en
PYTHONPATH=training/src python3 -m lae_training fit-prefix \
  training/tests/fixtures/prefix-training.jsonl build/trained-prefix.lem
build/le-model inspect build/trained-prefix.lem
PYTHONPATH=training/src python3 -m lae_training \
  summarize-plans training/tests/fixtures/plans.jsonl --baseline plain
PYTHONPATH=training/src python3 -m lae_training \
  summarize-study training/tests/fixtures/study.jsonl --baseline plain
```

The CLI prints ordered, non-overlapping UTF-8 byte ranges or the provider's
nodes, features, and language regions as JSON.

## Current architecture

```text
UTF-8 → language router → generic, English, or Chinese provider → linguistic IR
      → built-in or artifact-loaded reading model → reading signals
      → binary/variable presentation policy → emphasis spans
```

The runtime owns result storage. Callers borrow its contiguous emphasis array
until `le_result_destroy`. Rendering stays in the host application.

See [architecture](docs/architecture.md), [C API](docs/c-api.md),
[model artifact format](docs/model-format.md),
[training/runtime boundary](docs/training-runtime-boundary.md),
[evaluation framework](docs/evaluation.md),
[English provider](docs/english-provider.md),
[Chinese provider](docs/chinese-provider.md),
[reading and presentation](docs/reading-and-presentation.md), and
[text and offsets](docs/text-and-offsets.md) for the contracts.
