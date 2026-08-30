# Language provider guide

Language providers answer “what structure and features exist in this text?”
They must not emit bold spans, font weights, HTML, or presentation styles.

The built-in `LanguageProvider` contract accepts validated immutable text plus
advisory language metadata. Providers declare whether they support a language
tag and return a core-owned `Analysis`. The internal router currently chooses
English for `en` / `en-*`, Chinese for `zh` / `zh-*`, then falls back to generic
analysis.
Provider output must obey these invariants:

- every span uses half-open UTF-8 byte offsets into the original input;
- node spans are valid and do not split UTF-8 sequences;
- child identifiers resolve within the same analysis;
- emitted facts use extensible numeric feature identifiers;
- output is deterministic for identical inputs and provider configuration.

The core enforces these rules after every provider call. Providers may build
their analysis however they prefer, but invalid graphs never reach reading
models or the public analysis handle.

The generic provider is always available as fallback and makes no morphology
claims. Concrete implementations live under `runtime/providers`; English keeps
token and affix rules there, while Chinese keeps Han ranges, segmentation, and
its compact lexicon there. External NLP libraries must likewise be wrapped
inside concrete providers; the runtime core may not depend on them.

English and Chinese exercise different structures through this contract. The
public provider ABI v1 now exposes an equivalent C-only, sink-based boundary.
The runtime owns sink storage and validates the completed graph before it
reaches a model. Providers can be registered statically on every target or
loaded from modules when that optional build capability is enabled. See the
[provider plugin ABI](provider-plugin-abi.md) for callbacks, ownership, and
compatibility rules.
