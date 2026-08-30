# LAE training

This package is the offline side of LAE's artifact boundary. It uses only the
Python standard library and is never linked or imported by the runtime.

Milestone 7 intentionally provides a deterministic baseline rather than a
neural model:

- a versioned JSON Lines dataset interface;
- validated UTF-8 byte spans and externally supplied grapheme boundaries;
- language-neutral scalar feature extraction;
- prefix-candidate evaluation and grid-search optimization;
- a pure-Python `.lem` v1 artifact exporter.
- strategy-neutral offline plan metrics and paired human-study comparisons.
- a deterministic ridge-regression trainer for linear unit salience.

Preprocessing owns linguistic-unit and grapheme discovery. Recording those
boundaries in the dataset prevents training from silently using segmentation
rules that differ from the runtime.

## Dataset schema v1

Every JSON Lines record contains one text example:

```json
{
  "schema_version": 1,
  "id": "example-1",
  "text": "read complex",
  "language": "en",
  "units": [
    {
      "begin": 0,
      "end": 4,
      "grapheme_boundaries": [0, 1, 2, 3, 4],
      "target_prefix_graphemes": 2
    }
  ]
}
```

All positions are absolute, half-open UTF-8 byte offsets. Unit spans must be
ordered and nonoverlapping. Grapheme boundaries must cover their unit, be
strictly increasing, and fall on UTF-8 boundaries. A preprocessing pipeline is
responsible for producing true extended-grapheme boundaries.

## Commands

From the repository root:

```sh
PYTHONPATH=training/src python3 -m lae_training extract-features DATASET.jsonl
PYTHONPATH=training/src python3 -m lae_training evaluate-prefix DATASET.jsonl --fixed 2
PYTHONPATH=training/src python3 -m lae_training fit-prefix DATASET.jsonl model.lem
PYTHONPATH=training/src python3 -m lae_training summarize-plans PLANS.jsonl
PYTHONPATH=training/src python3 -m lae_training summarize-study STUDY.jsonl
PYTHONPATH=training/src python3 -m lae_training \
  fit-linear-salience SALIENCE.jsonl learned.lem
```

`fit-prefix` emits a JSON report and a runtime-loadable artifact. With no
explicit `--language`, the exporter records the sorted set of dataset language
tags. The optimizer evaluates fixed counts from zero through the longest unit
and proportional values from 0.00 through 1.00 in 0.05 steps. Ties are resolved
deterministically.

Plan evaluation accepts arbitrary variant labels and reports emphasis density,
text density, fragmentation, and emphasis-transition rate. Study evaluation
aggregates reading speed, comprehension, fixation duration/count, regressions,
preference, distraction, and density. Comparisons are paired and always report
candidate minus baseline. See [the evaluation contract](../docs/evaluation.md)
for both schemas and metric semantics.

Salience-training records contain a language tag and labeled units whose inputs
are stable numeric IR feature IDs. The trainer fits a bias and sparse feature
weights, quantizes them to runtime binary32 values, reports MAE/RMSE/`R²`, and
exports `LE_MODEL_LINEAR_SALIENCE`. See
[the learned-model contract](../docs/learned-model.md).
