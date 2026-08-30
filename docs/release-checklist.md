# Release checklist

Use this checklist for a tagged LAE release.

## Metadata

- [ ] Runtime version agrees across `CMakeLists.txt` and `include/le/version.h`.
- [ ] Training version in `training/pyproject.toml` matches the intended
  independently versioned package release.
- [ ] `CHANGELOG.md` moves completed work out of `Unreleased` and records the
  release date.
- [ ] MIT license files are present in the repository, installed runtime
  documentation, Python wheel, and source distribution.

## Verification

- [ ] Strict static build passes with warnings treated as errors.
- [ ] Shared release build and full test suite pass.
- [ ] ASan/UBSan build and full test suite pass.
- [ ] Dynamic-provider-disabled runtime-only build succeeds without Python.
- [ ] Relocated static and shared CMake package consumers configure, build, and
  run through `find_package(le CONFIG REQUIRED)`.
- [ ] Python wheel and source distribution contain the declared version,
  console entry point, and MIT license.
- [ ] Shared-library symbol audit exposes only the documented C ABI.
- [ ] `git diff --check` passes and the release worktree is clean.

## Publication

- [ ] Create an annotated `vMAJOR.MINOR.PATCH` tag from the verified commit.
- [ ] Push the tag and publish release notes from `CHANGELOG.md`.
- [ ] Build release artifacts from the tag rather than an uncommitted worktree.
- [ ] Verify a fresh consumer against the published artifacts.
