# Changelog

All notable changes to LAE are documented here. The project follows semantic
versioning for the runtime package and separately versions its public ABI and
model artifact format.

## [Unreleased]

No changes yet.

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
