# Architecture

## Dependency direction

The runtime is split into a C ABI facade and a language-independent C++ core:

```text
host / binding / CLI
        ↓
public C ABI (`include/le`)
        ↓
ABI facade (`runtime/abi`)
        ↓
text → provider → IR → reading model → signals → policy
```

The core uses abstract nodes named `document`, `unit`, and `subunit`; it does
not define a language-independent “word.” Nodes refer to the immutable source
text with UTF-8 byte spans. Analysis produces facts and structure, the reading
model produces numeric signals, and the presentation policy alone produces
emphasis. Rendering is outside the library.

## Milestone implementation

The generic provider groups maximal runs of grapheme clusters separated by
Unicode separator, control, or punctuation categories. This is only a fallback
segmentation strategy. It is deliberately not whitespace tokenization and does
not claim linguistic word boundaries. A Chinese or Japanese provider can later
emit different units without changing the pipeline or C processing API.

The prefix reading model chooses a fixed count or proportion of graphemes from
each generic unit and emits fixation-salience signals. The binary policy maps
positive fixation salience to normalized emphasis strength. Output spans are
ordered and non-overlapping.

## Runtime properties

- Input is borrowed and immutable for the duration of `le_process`.
- Results are immutable and own one contiguous emphasis array.
- A result remains valid after its creating runtime is destroyed.
- Processing has no global mutable model or provider state and is deterministic.
- Diagnostics are thread-local; each thread observes its own most recent error.
- `utf8proc` supplies standards-based UTF-8 decoding and extended grapheme
  boundary behavior without introducing language-specific rules.

Dynamic providers, stable plugin ABI, artifact loading, streaming, and advanced
IR access are intentionally deferred until multiple structurally different
providers validate their abstractions.

