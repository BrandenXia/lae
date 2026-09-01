# Rule-based English provider

The statically linked English provider is selected only when the caller supplies
`en` or an `en-*` language tag. It emits:

- document and punctuation-delimited sentence nodes;
- Unicode-letter unit nodes, including internal straight or curly apostrophes;
- subunits labeled as lexical core, derivational affix, or grammatical affix;
- grapheme-count, segmentation-confidence, unit-position, sentence-progress,
  sentence-unit-count, and boundary-strength features;
- provider-neutral content-unit or function-unit classification.

The provider preserves original UTF-8 byte spans and casing. Its small built-in
rules recognize layered common English prefixes, suffixes,
contractions, and closed-class function words. Suffix rules can operate around
non-ASCII lexical content, and contractions recognize both straight and curly
apostrophes without normalizing the returned spans. For example, `can’t`
becomes lexical `can` plus grammatical `’t` and its unit is marked as a function
unit.

Layered analysis preserves each surface segment independently. For example,
`readers` becomes lexical `read`, derivational/grammatical `er`, and grammatical
`s`; `internationalization` becomes `inter + nation + al + ization`. A compact
exception inventory prevents common coincidental endings such as those in
`business`, `document`, and `nation` from being stripped recursively.

## Deliberate limits

This milestone is not a dictionary-backed or statistically trained
morphological analyzer. It does not provide part-of-speech tagging, lemmatizing,
frequency estimates, syntactic parsing, named entities, semantic density, or
automatic language identification. Its function-word inventory is deliberately
compact. Rules may miss genuine affixes or retain an ambiguous form as a lexical
core. A small ambiguity guard prevents `read*` forms from being treated as the
prefix `re-`.

The rules stay inside `runtime/providers/english`. The language-independent
lexical-core reading model sees only stable feature IDs and therefore works with
any future provider that supplies equivalent facts.

Use the provider and model together through the CLI:

```sh
printf 'unbelievable reading' | le-cli --language en --model lexical-core
printf 'unbelievable reading' | le-cli --language en --dump-analysis
```
