# Rule-based Japanese provider

The statically linked Japanese provider is selected for explicit `ja` and
`ja-*` language tags. It demonstrates mixed-script morphology without adding
Japanese-specific node kinds, offsets, or processing operations to the core.

The provider emits:

- sentence nodes delimited by Japanese or ASCII terminal punctuation;
- units formed from script runs plus following Hiragana okurigana or particles;
- lexical-core, grammatical-affix, and derivational-affix subunits;
- Han, Hiragana, Katakana, and Latin script features;
- normalized segmentation-confidence and content-unit features.

A deterministic longest-suffix ruleset recognizes a compact baseline of
particles, polite forms, auxiliaries, inflections, and the productive endings
`やすい` and `にくい`. Recognized inflection chains are decomposed rather than
reported as opaque suffixes. For example:

- `食べさせられました` becomes lexical `食べ` plus grammatical `させ`, `られ`,
  `まし`, and `た`;
- `研究しています` becomes lexical `研究` plus grammatical `し`, `て`, `い`,
  and `ます`;
- `読みやすかった` becomes lexical `読み`, derivational `やす`, and
  grammatical `かっ` plus `た`;
- `日本語を` becomes lexical `日本語` plus grammatical `を`.

The feature vocabulary remains language-neutral: the provider emits only the
shared lexical-core, derivational-affix, and grammatical-affix facts. Katakana
and embedded Latin content use the same unit/subunit contract.

## Deliberate limits

This is an architecture-validation baseline, not a general Japanese
morphological analyzer. It has no dictionary, part-of-speech tagger,
normalization, reading generation, named entities, a complete conjugation
lattice, or statistical disambiguation. Hiragana-only passages and forms outside
the compact suffix list may remain coarse lexical units. Script and morphology
rules stay under `runtime/providers/japanese`; downstream models see only stable
features.

```sh
printf '食べさせられました' | le-cli --language ja --model lexical-core
printf '日本語を研究しています' | le-cli --language ja --dump-analysis
```
