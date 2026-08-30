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
- plan and study evaluators compare arbitrary named strategies without changing
  the runtime or artifact contract.

Dataset preprocessing owns linguistic units and true extended-grapheme
boundaries. Storing absolute boundaries avoids silently reimplementing runtime
segmentation in Python. Schema v1 records a target prefix-grapheme count per
unit; later schemas can add fixation, comprehension, preference, or other
targets without changing the runtime.

The Python exporter implements the documented byte format independently. A
golden checksum test detects encoder drift, while integration tests inspect and
execute its output with the C++ model tool and C ABI runtime. This is a contract
test across the boundary, not a runtime dependency on training.

Milestone 8 adds two evaluation inputs. Offline plan records measure emphasis
density, text density, fragmentation, and emphasis transitions. Human-study
records measure reading speed, comprehension, fixation duration/count,
regressions, preference, reported distraction, and density. A/B results use
paired examples and report candidate-minus-baseline descriptive deltas.

Milestone 9 adds a deterministic ridge-regression trainer for a sparse linear
salience model. Training consumes labeled snapshots of stable runtime feature
IDs, quantizes fitted parameters to binary32, and exports a model artifact. The
runtime only evaluates the compiled bias and weights; it does not contain the
solver or dataset code.

There is no neural network, framework checkpoint, experiment database, runtime
Python bridge, online adaptation, or claim of statistical significance.
