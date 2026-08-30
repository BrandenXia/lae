# Rule-based English provider

The statically linked English provider is selected only when the caller supplies
`en` or an `en-*` language tag. It emits:

- document and punctuation-delimited sentence nodes;
- Unicode-letter unit nodes, including internal straight or curly apostrophes;
- subunits labeled as lexical core, derivational affix, or grammatical affix;
- grapheme-count and boundary-strength features.

The provider preserves original UTF-8 byte spans and casing. Its small built-in
rules recognize a fixed list of common English prefixes, suffixes, and
contractions. Normal prefix/suffix decomposition is currently limited to ASCII
tokens; a token containing non-ASCII bytes remains one lexical-core subunit.
This keeps byte slicing deterministic and grapheme-safe while the provider
contract is exercised.

## Deliberate limits

This milestone is not a dictionary-backed or statistically trained
morphological analyzer. It does not provide part-of-speech tagging, lemmatizing,
frequency estimates, syntactic parsing, named entities, semantic density, or
automatic language identification. Rules may miss genuine affixes or retain an
ambiguous form as a lexical core. A small ambiguity guard prevents `read*` forms
from being treated as the prefix `re-`.

The rules stay inside `runtime/providers/english`. The language-independent
lexical-core reading model sees only stable feature IDs and therefore works with
any future provider that supplies equivalent facts.

Use the provider and model together through the CLI:

```sh
printf 'unbelievable reading' | le-cli --language en --model lexical-core
printf 'unbelievable reading' | le-cli --language en --dump-analysis
```
