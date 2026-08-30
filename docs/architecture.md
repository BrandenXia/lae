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

The language router selects statically linked providers for explicit `en` /
`en-*` and `zh` / `zh-*` tags and otherwise selects the generic provider. The
core owns the router and provider-neutral contract, but contains no English or
Chinese analysis rules.

The generic provider groups maximal runs of grapheme clusters separated by
Unicode separator, control, or punctuation categories. This is only a fallback
segmentation strategy. It is deliberately not whitespace tokenization and does
not claim linguistic word boundaries.

The English provider emits document, sentence, unit, and subunit structure. A
small deterministic ruleset labels lexical cores and grammatical or
derivational affixes. It is a framework-validation baseline, not a general
morphological analyzer; its limitations are documented separately.

The Chinese provider emits sentence nodes, lexically segmented unit nodes, and
grapheme-level subunits without depending on whitespace. A deterministic
coverage-based segmenter uses a compact built-in lexicon and falls back to
individual Han characters. Script and segmentation-confidence features make
the distinction explicit without adding a language-specific node kind.

Provider output is validated before it reaches a reading model. Validation
checks the document root, dense node identifiers, single-parent tree structure,
ordered non-overlapping siblings, contained grapheme-safe spans, finite unique
features, and ordered language regions with normalized confidence.

The prefix reading model chooses a fixed count or proportion of graphemes from
each unit and emits per-grapheme fixation-salience signals. The lexical-core
model consumes only stable IR feature IDs and emits lexical/fixation signals for
marked nodes; English marks morphological subunits while Chinese marks lexical
units. Presentation is a separate
stage: the binary policy thresholds and merges signals at one strength, while
the variable policy interpolates strength from salience. Output spans remain
ordered and non-overlapping.

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
and language regions. Dynamic providers, a stable plugin ABI, artifact loading,
mixed-language routing, and streaming remain deferred. English and Chinese now
exercise structurally different uses of the same IR; Japanese remains a future
third provider.
