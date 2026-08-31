# LAE

LAE is an early language-aware typographic emphasis runtime. The current
milestone provides a stable C ABI, a validated and inspectable linguistic IR,
a generic Unicode fallback, statically linked rule-based English, Chinese, and
Japanese providers, a stable C provider plugin ABI, prefix and lexical-core reading
models, explicit mixed-language analysis regions, and binary or
variable-strength presentation. Versioned `.lem` artifacts can now carry model
parameters and capability metadata into the memory-based runtime loader. An
independent standard-library Python package can validate offline datasets,
extract features, optimize a deterministic prefix baseline, evaluate it, and
export the same artifact format. Its strategy-neutral evaluation layer compares
offline plan density/complexity and paired human-study outcomes. LAE still
intentionally has no renderer, runtime training dependency, automatic language
detection, or neural network. Its first released model predicts English word
fixation probability from the CC BY 4.0 Provo eye-tracking corpus and consumes
stable IR features through the same artifact and C processing APIs as
deterministic models. External providers can be registered statically on every
target or loaded from modules when the optional desktop capability is enabled.

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

Install the runtime and consume its relocatable CMake package:

```sh
cmake --install build --prefix "$PWD/install"
```

```cmake
find_package(le 0.15 CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE le::runtime)
```

Point `CMAKE_PREFIX_PATH` at the chosen install prefix when configuring the
consumer. Static packages locate the required `libutf8proc` dependency through
`pkg-config`; consuming C applications should enable both C and C++ languages
because the runtime implementation is C++.
See the [minimal installed-package consumer](examples/cmake-consumer/CMakeLists.txt)
for a complete target.

For static/mobile/WASM builds without operating-system module loading:

```sh
cmake -S . -B build-static -DBUILD_SHARED_LIBS=OFF \
  -DLAE_ENABLE_DYNAMIC_PROVIDERS=OFF
```

Run the minimal Python provider and Markdown renderer against a shared
build:

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
python3 examples/markdown_bold.py \
  "Language-aware emphasis works in Markdown output."
```

The script implements provider ABI v1 through `ctypes`, registers it with the
runtime, and converts the resulting byte spans into Markdown `**bold**` syntax.

Run the released real-data model with its recommended binary threshold:

```sh
printf 'Language-aware emphasis helps readers scan complex documentation.' |
  build/le-cli --artifact models/lae-provo-fixation-v1.lem \
  --language en --threshold 0.60
PYTHONPATH=bindings/python/src python3 examples/provo_markdown.py \
  'Language-aware emphasis helps readers scan complex documentation.'
```

The model was selected with passage-grouped five-fold validation over 2,661
word units from 84 readers. See its [model card](docs/models/provo-fixation-v1.md)
and [complete deterministic report](models/lae-provo-fixation-v1.json).

Use the dependency-free Python runtime binding against the same shared build:

```python
from lae import ProcessOptions, ReadingModel, Runtime

with Runtime("build-shared/lible_runtime.dylib") as runtime:
    plan = runtime.process(
        "unbelievable reading",
        ProcessOptions(language="en", reading_model=ReadingModel.LEXICAL_CORE),
    )
```

Set `PYTHONPATH=bindings/python/src` when using the binding from a source
checkout. See the [Python binding guide](docs/python-binding.md) for model
loading, ownership, and cross-platform library discovery.

The Swift package wraps the same installed C ABI and retains its UTF-8 byte
coordinate system:

```swift
import LAE

let runtime = try Runtime()
defer { runtime.close() }
let plan = try runtime.process("éclair")
let emphasizedText = plan[0].span.substring(in: "éclair")
```

See the [Swift binding guide](docs/swift-binding.md) for Swift Package Manager
integration and native library search-path configuration.

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
printf '食べさせられました' | build/le-cli --language ja --model lexical-core
printf '日本語を研究しています' | build/le-cli --language ja --dump-analysis
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
PYTHONPATH=training/src python3 -m lae_training fit-linear-salience \
  training/tests/fixtures/salience-training.jsonl build/learned.lem --ridge 0
build/le-model inspect build/learned.lem
printf 'unbelievable reading' | build/le-cli \
  --artifact build/learned.lem --language en --dump-signals
```

The CLI prints ordered, non-overlapping UTF-8 byte ranges or the provider's
nodes, features, and language regions as JSON.

## Current architecture

```text
UTF-8 → external or built-in language provider → validated linguistic IR
      → built-in or artifact-loaded reading model → reading signals
      → binary/variable presentation policy → emphasis spans
```

The runtime owns result storage. Callers borrow its contiguous emphasis array
until `le_result_destroy`. Rendering stays in the host application.

See [architecture](docs/architecture.md), [C API](docs/c-api.md),
[model artifact format](docs/model-format.md),
[training/runtime boundary](docs/training-runtime-boundary.md),
[evaluation framework](docs/evaluation.md),
[learned model](docs/learned-model.md),
[Provo fixation v1 model card](docs/models/provo-fixation-v1.md),
[provider plugin ABI](docs/provider-plugin-abi.md),
[Python binding](docs/python-binding.md),
[Swift binding](docs/swift-binding.md),
[English provider](docs/english-provider.md),
[Chinese provider](docs/chinese-provider.md),
[Japanese provider](docs/japanese-provider.md),
[reading and presentation](docs/reading-and-presentation.md), and
[text and offsets](docs/text-and-offsets.md) for the contracts.

## License

LAE is available under the [MIT License](LICENSE).
