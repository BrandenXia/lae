# Provo fixation v1 model card

## Summary

`lae-provo-fixation-v1` is a compact English unit-salience model trained on
human eye-tracking observations. It predicts the probability that a reader
fixates a word. LAE can render words whose predicted probability crosses a host
selected threshold; `0.60` is the recommended starting point for binary bold.

This model is intended to make a first useful, reproducible baseline available,
not to claim that fixation probability is the same thing as semantic importance
or reading comprehension.

## Data and attribution

Training uses *The Provo Corpus: A Large Eye-Tracking Corpus with Predictability
Norms* by Steven G. Luke and Kiel Christianson:

- official project: <https://osf.io/sjefs/>;
- canonical eye-tracking CSV: <https://osf.io/download/a32be/>;
- paper: <https://doi.org/10.3758/s13428-017-0908-4>;
- source license: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/);
- source SHA-256:
  `38aedcb29bc9171009916eb2bcc2375729f104a2a1005c64a563da94b611b9e7`.

The released artifact is an adapted, derived representation of those data.
This card preserves attribution, identifies the transformation, links the
licensed material, and states the source license. LAE does not redistribute
raw participant rows or passage text.

## Training target and features

For every labeled word interest area, the target is:

```text
number of readers who did not skip the word / number of recorded readers
```

The preprocessing command asks the matching `le-cli` build to analyze every
compatible token. That snapshots the exact features available at inference
time and prevents a separate Python tokenizer or word-class list from drifting
away from the runtime.

The final ridge-linear score is:

```text
clamp(0.35674667
      + 0.05543682 * grapheme_count
      - 0.14409699 * function_unit,
      0, 1)
```

`function_unit` is `1` for a runtime-classified English function word and `0`
otherwise. Candidate feature sets and ridge values were selected solely by
held-out root mean square error.

## Evaluation

Passages—not individual words—are assigned to five deterministic folds. Each
held-out word is therefore evaluated by a model that did not train on any word
from its passage.

| Measure | Mean-only baseline | Selected model |
| --- | ---: | ---: |
| Units | 2,661 | 2,661 |
| MAE | 0.19766 | 0.09126 |
| RMSE | 0.22738 | 0.11719 |
| R² | -0.00095 | 0.73414 |

The selected model reduces held-out RMSE by 48.46% relative to the fold-specific
mean-only baseline. The final model is then refit on all 2,661 compatible units
from 55 passages and 84 participants. Twenty-five labeled corpus tokens are
excluded because the current English provider maps them to zero or multiple
units, primarily numbers, hyphenated forms, or malformed punctuation.

The full 18-candidate evaluation and all-data fit metrics are preserved in
[`models/lae-provo-fixation-v1.json`](../../models/lae-provo-fixation-v1.json).

## Intended use

- English prose processed by LAE runtime ABI 1.11 or newer.
- Ranking or thresholding whole-word emphasis using predicted fixation
  probability.
- A reproducible baseline for future semantic, lexical-frequency, contextual,
  and multilingual models.

For binary bold, begin with a threshold of `0.60`. Lower values emphasize more
words; higher values emphasize fewer. Consumers that can render variable weight
may use the raw score as a strength input instead.

## Limitations

- Fixation likelihood is not a direct label for importance, comprehension, or
  accessibility benefit.
- The model uses only word length and LAE's function/content classification. It
  has no sentence context, frequency, surprisal, domain, or reader adaptation.
- Provo participants were adult native speakers of American English reading 55
  short naturalistic passages. Results should not be generalized to children,
  second-language readers, clinical populations, or other languages without
  new evaluation.
- Cross-validation estimates prediction of the corpus target. It does not prove
  that bold presentation improves reading outcomes. LAE's paired human-study
  evaluation contract remains the appropriate next validation step.

## Reproduction

After downloading the canonical CSV and building `le-cli` from this release:

```sh
PYTHONPATH=training/src python3 -m lae_training train-provo \
  Provo_Corpus-Eyetracking_Data.csv \
  models/lae-provo-fixation-v1.lem \
  --analyzer build/le-cli \
  --report models/lae-provo-fixation-v1.json
```

The command rejects a source file whose SHA-256 differs from the canonical OSF
version and deterministically reproduces the model artifact and report.
