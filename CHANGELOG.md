# Changelog

All notable changes to LAE are documented here. The project follows semantic
versioning for the runtime package and separately versions its public ABI and
model artifact format.

## [Unreleased]

No changes yet.

## [0.14.0] - 2026-08-31

### Added

- Stable `LE_FEATURE_FUNCTION_UNIT` semantic feature for provider-neutral
  content/function classification.
- English closed-class classification, Chinese function-unit lexicon entries,
  and Japanese standalone grammatical-unit classification.
- Runtime and training artifact validation for models that consume the new
  feature, including an ABI 1.11 minimum-version requirement.

### Improved

- English morphology now recognizes contractions containing straight or curly
  apostrophes while preserving their original UTF-8 spans.

### Compatibility

- Runtime version: 0.14.0.
- C ABI: 1.11.
- Provider ABI: 1.0 (unchanged).
- Model artifact format: 1.0 (unchanged).
- Python runtime binding: 0.14.0.
- Python training package: 0.4.1.

## [0.13.1] - 2026-08-31

### Improved

- Japanese analysis now decomposes recognized causative, passive, progressive,
  polite, negative, and past endings into stable grammatical subunits.
- Productive `やすい` and `にくい` forms now distinguish their derivational
  suffix from following adjective inflections.

### Compatibility

- Runtime version: 0.13.1.
- C ABI: 1.10 (unchanged).
- Provider ABI: 1.0 (unchanged).
- Model artifact format: 1.0 (unchanged).
- Python runtime binding: 0.13.1.
- Python training package: 0.4.0 (unchanged).

## [0.13.0] - 2026-08-31

### Added

- High-level explicit-region processing through `le_process_regions` and
  `le_process_regions_with_model`.
- Mixed-language `LanguageRegion` processing in the Python and Swift bindings.

### Compatibility

- Runtime version: 0.13.0.
- C ABI: 1.10.
- Provider ABI: 1.0.
- Model artifact format: 1.0.
- Python runtime binding: 0.13.0.
- Python training package: 0.4.0.

## [0.12.0] - 2026-08-30

### Added

- Explicit mixed-language analysis through `le_analyze_regions`, with strict
  contiguous coverage, grapheme-aligned boundaries, per-region provider
  routing, and one merged provider-neutral IR.

### Compatibility

- Runtime version: 0.12.0.
- C ABI: 1.9.
- Provider ABI: 1.0.
- Model artifact format: 1.0.
- Python runtime binding: 0.12.0.
- Python training package: 0.4.0.

## [0.11.0] - 2026-08-30

### Added

- Dependency-free Python runtime binding for high-level processing, model
  loading, metadata discovery, native diagnostics, and explicit ownership.
- Swift package with safe runtime/model ownership, artifact processing, native
  diagnostics, and UTF-8-byte-to-`String.Index` span conversion.
- Rule-based Japanese provider with mixed-script units, deterministic
  particle/auxiliary morphology, and lexical-core processing.
- Stable Hiragana and Katakana script feature identifiers.

### Compatibility

- Runtime version: 0.11.0.
- C ABI: 1.8.
- Provider ABI: 1.0.
- Model artifact format: 1.0.
- Python runtime binding: 0.11.0.
- Python training package: 0.4.0.

## [0.10.0] - 2026-08-30

### Added

- Stable C runtime ABI 1.7 with immutable analysis, signal, model, and result
  handles.
- Unicode and grapheme-safe text processing backed by `utf8proc`.
- Generic, rule-based English, and rule-based Chinese language providers.
- Prefix, lexical-core, and learned linear-salience reading models.
- Portable, checksummed `.lem` model artifacts and inspection/compiler tools.
- Binary and variable-strength presentation policies.
- Provider plugin ABI v1 with static registration, discovery, compatibility
  checks, optional dynamic loading, and runtime-owned sink storage.
- Independent Python training and strategy-neutral evaluation package 0.4.0.
- Relocatable CMake package exposing the installed `le::runtime` target.
- Minimal Markdown plugin and installed-package consumer examples.

### Compatibility

- Runtime version: 0.10.0.
- C ABI: 1.7.
- Provider ABI: 1.0.
- Model artifact format: 1.0.
- Python training package: 0.4.0.
