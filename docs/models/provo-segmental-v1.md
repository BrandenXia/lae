# LAE Provo segmental v1

`lae-provo-segmental-v1.lem` is a compact English reading model that predicts
both word-level fixation salience and a within-word first-fixation anchor. It
is the first LAE model to emit partial-word emphasis from real eye-tracking
targets.

## Intended use

Use the model to produce presentation-neutral spans for English prose. With
the recommended binary threshold of `0.60`, consumers can render those spans
as Markdown strong emphasis, variable font weight, attributed text, or another
host-native treatment.

The model is suitable for experimentation, interface prototyping, and
controlled reading studies. It is not evidence that typographic emphasis
improves reading speed, comprehension, accessibility, or health outcomes.

## Runtime behavior

For each English `unit`, the model predicts fixation probability from stable
IR features. If the analyzer exposes a strict `lexical_core` subunit, the
signal is projected onto that subunit. Otherwise, the model predicts normalized
first-fixation progress and emits a grapheme-safe prefix ending at that anchor.
Multi-grapheme roots remain partial by default.

Examples of the segment projection include:

```text
unbelievable         → un[believ]able
readers              → [read]ers
reread               → re[read]
internationalization → inter[nation]alization
documentation        → [document]ation
```

Square brackets show the output span; typography is chosen by the caller.
Every boundary is a validated UTF-8 grapheme boundary.

## Training data

The source is the CC BY 4.0 Provo Corpus by Steven G. Luke and Kiel
Christianson:

- 84 adult native speakers of American English
- 55 passages
- 230,412 participant/word rows
- 2,661 runtime-compatible word units after 25 exclusions
- 124,158 valid first-fixation landing observations for compatible units

The canonical source is checksum-pinned to:

```text
38aedcb29bc9171009916eb2bcc2375729f104a2a1005c64a563da94b611b9e7
```

The raw data is not included in LAE. See the bundled notice for attribution
and links.

## Targets and evaluation

Both regressors use passage-grouped five-fold cross-validation for feature and
ridge selection. No word from a held-out passage contributes to its fitted
fold.

| Target | Held-out MAE | Held-out RMSE | R² | RMSE reduction vs mean |
| --- | ---: | ---: | ---: | ---: |
| Word fixation probability | 0.09129 | 0.11717 | 0.73423 | 48.47% |
| Mean first-fixation progress | 0.06805 | 0.09146 | 0.12718 | 6.59% |

First-fixation progress is `(x - word_left) / (word_right - word_left)`. It is
an aggregate landing-position target, not a direct annotation of the best span
to bold. Lexical-core projection is therefore a transparent runtime policy
over the learned anchor, not an experimentally validated typographic optimum.

## Selected parameters

Fixation salience selected length, function-unit status, and sentence progress
with ridge `10.0`. The landing predictor selected length and function-unit
status with ridge `10.0`. The complete candidate table, binary32 parameters,
data counts, source metadata, and checksums are in
`models/lae-provo-segmental-v1.json`.

Artifact details:

- Type: `LE_MODEL_SEGMENTAL_SALIENCE`
- Model version: 1
- Model artifact format: 1.0
- Minimum runtime ABI: 1.12
- Size: 148 bytes
- SHA-256: `0d0edd3f3c1011463650a0ecba499bfb98c991c079c2184a1e7922a5d91ba4c4`

## Limitations

- The eye-tracking target comes from one English corpus and one participant
  population; it should not be generalized to other languages or populations.
- Landing position explains modest held-out variance (`R² = 0.12718`).
- English morphology remains a deterministic, conservative analyzer rather
  than a lexical database or neural parser. Unknown and ambiguous forms can be
  segmented imperfectly.
- The model does not use frequency, surprisal, syntax, font metrics, display
  size, or reader-specific calibration.
- Markdown support is an adapter example. Some Markdown renderers differ in
  how they display strong emphasis embedded inside a word.

## Reproduction

```sh
PYTHONPATH=training/src python3 -m lae_training train-provo-segmental \
  /path/to/Provo_Corpus-Eyetracking_Data.csv \
  models/lae-provo-segmental-v1.lem \
  --analyzer build-shared/le-cli \
  --report models/lae-provo-segmental-v1.json \
  --folds 5
```

The matching runtime analyzer is deliberately part of training so feature and
segmentation drift cannot silently change the fitted model.
