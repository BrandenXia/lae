# Linguistic IR

The core IR is a validated hierarchical representation. The C ABI exposes a
flattened immutable projection without exposing C++ containers or ownership.

An analysis contains hierarchical nodes, language regions, and extensible
numeric features. A node has a strong `NodeId`, source `TextSpan`, generic
`NodeKind`, child identifiers, and features keyed by a 32-bit `FeatureId`.
The generic provider emits one document root and zero or more unit children. It
emits generic grapheme-count and boundary-strength features, but no
language-specific morphology. The English provider additionally emits sentence
and morphological subunit nodes. The Chinese provider emits segmented lexical
units containing grapheme-level character subunits. The Japanese provider emits
mixed-script morphological units with lexical and grammatical subunits.

The vocabulary intentionally omits a universal `word` node. Future providers
may represent Chinese segmentations, Japanese mixed-script morphology, Semitic
root-pattern analyses, or other structures with units and subunits. Providers
may disagree about unit boundaries while downstream stages continue to consume
the same span/node/feature concepts.

The ABI node vocabulary is deliberately limited to generic structural kinds:
document, block, paragraph, sentence, unit, and subunit. Child and feature
ranges index analysis-owned contiguous arrays. New linguistic meaning should
normally use feature identifiers rather than expanding a closed node-kind list.

Provider results are rejected if identifiers are not dense, the graph is not a
single-parent tree, sibling spans overlap, spans split graphemes, features are
duplicated or non-finite, or language regions overlap or have invalid
confidence. This ensures downstream models consume one coherent contract.

The stable features defined through ABI 1.8 are:

| Identifier | Meaning |
|---|---|
| `LE_FEATURE_BOUNDARY_STRENGTH` | normalized structural boundary signal |
| `LE_FEATURE_GRAPHEME_COUNT` | node grapheme count |
| `LE_FEATURE_SEGMENTATION_CONFIDENCE` | provider confidence in a segmentation decision |
| `LE_FEATURE_LEXICAL_CORE` | provider identifies a node as a lexical core |
| `LE_FEATURE_DERIVATIONAL_AFFIX` | provider identifies a derivational affix |
| `LE_FEATURE_GRAMMATICAL_AFFIX` | provider identifies a grammatical affix |
| `LE_FEATURE_CONTENT_UNIT` | provider identifies a content-bearing unit |
| `LE_FEATURE_SCRIPT_HAN` | node is represented in Han script |
| `LE_FEATURE_SCRIPT_LATIN` | node is represented in Latin script |
| `LE_FEATURE_SCRIPT_HIRAGANA` | node contains Hiragana script |
| `LE_FEATURE_SCRIPT_KATAKANA` | node contains Katakana script |

Reading models consume these numeric facts rather than provider classes or
language tags. Unknown feature IDs remain valid and must be ignored or
preserved by consumers that do not understand them.
