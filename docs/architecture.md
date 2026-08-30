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

The core uses abstract nodes such as `document`, `unit`, and `subunit`; it does
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

Provider output is validated before it reaches a reading model. Validation
checks the document root, dense node identifiers, single-parent tree structure,
ordered non-overlapping siblings, contained grapheme-safe spans, finite unique
features, and ordered language regions with normalized confidence.

The prefix reading model chooses a fixed count or proportion of graphemes from
each generic unit and emits per-grapheme fixation-salience signals. Presentation
is a separate stage: the binary policy thresholds and merges signals at one
strength, while the variable policy interpolates strength from salience. Output
spans remain ordered and non-overlapping.

## Runtime properties

- Input is borrowed and immutable for the duration of `le_process`.
- A long-lived analysis owns one immutable source snapshot for later stages.
- Results are immutable and own one contiguous emphasis array.
- Analyses, signals, and results remain valid after their runtime is destroyed.
- Processing has no global mutable model or provider state and is deterministic.
- Diagnostics are thread-local; each thread observes its own most recent error.
- `utf8proc` supplies standards-based UTF-8 decoding and extended grapheme
  boundary behavior without introducing language-specific rules.

The C ABI exposes immutable flat views of nodes, child identifiers, features,
and language regions. Dynamic providers, stable plugin ABI, artifact loading,
and streaming remain deferred until multiple structurally different providers
validate their abstractions.
