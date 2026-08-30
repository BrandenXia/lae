# Linguistic IR

The core IR is a validated hierarchical representation. The C ABI exposes a
flattened immutable projection without exposing C++ containers or ownership.

An analysis contains hierarchical nodes, language regions, and extensible
numeric features. A node has a strong `NodeId`, source `TextSpan`, generic
`NodeKind`, child identifiers, and features keyed by a 32-bit `FeatureId`.
The implemented generic provider emits one document root and zero or more unit
children. It emits generic grapheme-count and boundary-strength features, but
no language-specific morphology.

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
