# Language provider guide

Language providers answer “what structure and features exist in this text?”
They must not emit bold spans, font weights, HTML, or presentation styles.

The internal `LanguageProvider` contract currently accepts validated immutable
text plus advisory language metadata and returns a core-owned `Analysis`.
Provider output must obey these invariants:

- every span uses half-open UTF-8 byte offsets into the original input;
- node spans are valid and do not split UTF-8 sequences;
- child identifiers resolve within the same analysis;
- emitted facts use extensible numeric feature identifiers;
- output is deterministic for identical inputs and provider configuration.

The generic provider is always available as fallback and makes no morphology
claims. External NLP libraries must be wrapped inside concrete providers; the
runtime core may not depend on them.

A sink-based plugin ABI and dynamic loading are postponed until the abstraction
has been exercised by the generic provider, English, and Chinese or Japanese.
At that point, the core should validate sink events and own the resulting graph
and storage. Static provider registration must remain possible for iOS, WASM,
and embedded builds.

