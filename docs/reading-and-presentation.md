# Reading signals and presentation

Reading and presentation are independent runtime stages:

```text
validated analysis + text → reading model → numerical signals
numerical signals + policy → normalized emphasis plan
```

## Signals

`le_reading_signal_t` contains a grapheme-safe UTF-8 byte span and independent
normalized channels for fixation salience, lexical salience, and reading
difficulty. The deterministic prefix model populates fixation salience only. It
emits one signal per selected grapheme, declining linearly
from `1.0` to `0.5` across a multi-grapheme prefix. This is a baseline heuristic,
not a typography decision or a learned prediction.

The lexical-core model selects non-document nodes carrying
`LE_FEATURE_LEXICAL_CORE` and emits one whole-span signal with fixation and
lexical salience set to `1.0`. It does not require one structural level: English
marks morphological subunits and Chinese marks segmented units. The model is
language-independent, and the generic fallback produces no lexical-core facts.

The learned linear-salience model scores each `unit` from stable IR feature IDs.
It emits the clamped prediction in both fixation and lexical channels and skips
zero predictions. It does not inspect language-specific feature meanings;
language metadata on the artifact limits where fitted weights may run.

The same two models can be loaded from `.lem` artifacts. Prefix artifacts move
strategy, fixed-count, and proportion parameters out of the runtime call site.
Lexical-core artifacts declare `LE_FEATURE_LEXICAL_CORE` as a required runtime
capability. Model language metadata is checked before signal generation. The
offline training skeleton can optimize the prefix parameters against annotated
unit targets and export them without becoming a runtime dependency.

Signal results are immutable and own contiguous output storage. An advanced
analysis handle owns one immutable snapshot of its source text, exposed as a
borrowed view by `le_analysis_text`; signal generation therefore needs only the
analysis and model configuration. High-level `le_process` remains zero-copy and
does not construct this long-lived snapshot.

## Policies

Both policies first discard signals below `salience_threshold`:

- Binary policy assigns `maximum_strength` and merges adjacent spans with the
  same style and strength. This preserves the original prefix-baseline output.
- Variable-strength policy clamps fixation salience to `[0,1]` and maps it
  linearly into `[minimum_strength, maximum_strength]`.

Policies do not inspect linguistic node kinds, feature identifiers, languages,
or source substrings. They only consume numerical signals. Renderers remain
outside the runtime and decide how normalized strength maps to font weight,
contrast, decoration, or another host-specific presentation.

## Configuration compatibility

Dedicated prefix-model and presentation configurations are independently
versioned with `struct_size`. High-level `le_process_options_t` v2 appended the
policy fields to its v1 layout; v3 appends reading-model selection. ABI 1.4
accepts all three exact legacy boundaries, defaults older callers to the prefix
model, and never reads appended fields from a smaller buffer.
