# Rule-based Chinese provider

The statically linked Chinese provider is selected for explicit `zh` and `zh-*`
language tags, including `zh-Hans` and `zh-Hant`. It demonstrates that the IR
does not depend on whitespace, English morphology, or a universal word node.

The provider emits:

- sentence nodes delimited by Chinese or ASCII terminal punctuation;
- segmented lexical unit nodes carrying `LE_FEATURE_LEXICAL_CORE`;
- grapheme-level subunits that distinguish characters from lexical units;
- Han and Latin script features;
- normalized segmentation-confidence features.

Contiguous Han text is segmented with deterministic dynamic programming over a
small built-in simplified/traditional lexicon. The objective first maximizes
dictionary-covered graphemes, then minimizes unit count, then prefers the
longer current match. This resolves `研究生命` as `研究 + 生命` instead of
`研究生 + 命`. Unknown Han text falls back to one-character units with lower
confidence. Adjacent non-Han letters or numbers form a separate unit.

## Deliberate limits

This is an architecture-validation baseline, not a production Chinese word
segmenter. Its lexicon is intentionally compact; it has no statistical model,
part-of-speech tagging, named entities, normalization, dialect detection, or
automatic simplified/traditional conversion. Ambiguous text outside the small
lexicon may fall back to individual characters.

Han ranges and segmentation logic stay under `runtime/providers/chinese`. The
core lexical reading model sees only stable features, selecting Chinese unit
nodes through the same API that selects English morphological subunits.

```sh
printf '中华人民共和国' | le-cli --language zh-Hans --model lexical-core
printf '研究生命起源' | le-cli --language zh-Hans --dump-analysis
```
