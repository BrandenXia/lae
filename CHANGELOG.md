# Changelog

All notable changes to LAE are documented here. The project follows semantic
versioning for the runtime package and separately versions its public ABI and
model artifact format.

## [Unreleased]

No changes yet.

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
