# Linguistic IR

The current internal IR proves the dependency boundary without exposing an
unstable graph layout through ABI v1.

An analysis contains hierarchical nodes, language regions, and extensible
numeric features. A node has a strong `NodeId`, source `TextSpan`, generic
`NodeKind`, child identifiers, and features keyed by a 32-bit `FeatureId`.
The implemented generic provider emits one document root and zero or more unit
children. It emits no language-specific morphology.

The vocabulary intentionally omits a universal `word` node. Future providers
may represent Chinese segmentations, Japanese mixed-script morphology, Semitic
root-pattern analyses, or other structures with units and subunits. Providers
may disagree about unit boundaries while downstream stages continue to consume
the same span/node/feature concepts.

Advanced ABI access to analyses is deferred. Stabilizing it before English and
a structurally different provider exist would risk freezing accidental
English-centric assumptions.

