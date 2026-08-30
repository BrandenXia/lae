# Training/runtime boundary

Training and runtime are separate systems with one directional contract:

```text
datasets → preprocessing → features → training/evaluation
         → model compiler → versioned artifact → runtime loader
```

The runtime never imports Python, PyTorch, JAX, experiment frameworks, datasets,
or raw checkpoints. Training may depend on the artifact specification and
export validation tools; runtime code may not depend on the training tree.

Milestone 7 introduces `training/` as an independent, standard-library Python
package. It provides four explicit boundaries:

- `Dataset` yields immutable examples with versioned UTF-8 byte annotations.
- `extract_features` converts provider-neutral units into scalar feature rows.
- `evaluate_prefix` and `fit_prefix` measure and optimize a deterministic
  runtime-compatible baseline.
- the artifact exporter compiles fitted parameters directly to `.lem` v1.

Dataset preprocessing owns linguistic units and true extended-grapheme
boundaries. Storing absolute boundaries avoids silently reimplementing runtime
segmentation in Python. Schema v1 records a target prefix-grapheme count per
unit; later schemas can add fixation, comprehension, preference, or other
targets without changing the runtime.

The Python exporter implements the documented byte format independently. A
golden checksum test detects encoder drift, while integration tests inspect and
execute its output with the C++ model tool and C ABI runtime. This is a contract
test across the boundary, not a runtime dependency on training.

There is no neural model, framework checkpoint, experiment database, or runtime
Python bridge. Formal multi-strategy evaluation and learned salience models
remain Milestones 8 and 9.
