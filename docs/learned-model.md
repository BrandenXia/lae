# Linear salience model

Milestone 9 introduces LAE's first learned reading model. It is intentionally a
small linear baseline, not a neural network:

```text
unit IR features → bias + weighted sum → clamp to [0,1] → reading signal
```

The model operates on provider-neutral `le_feature_id_t` values attached to
`unit` nodes. Missing features contribute zero. Each positive prediction emits
one signal over the unit span with fixation and lexical salience set to the
predicted value. Presentation remains independent: callers choose thresholds,
binary emphasis, or variable strength through the existing policy options.

## Training data

Salience dataset schema v1 is JSON Lines. A record groups labeled unit feature
snapshots by source example:

```json
{
  "schema_version": 1,
  "id": "sample-1",
  "language": "en",
  "units": [
    {
      "features": [{"id": 2, "value": 4.0}],
      "target_salience": 0.8
    }
  ]
}
```

Feature values are quantized to IEEE-754 binary32, matching the runtime IR.
Targets are normalized to `[0,1]`. Feature IDs must be stable identifiers known
to the runtime; duplicate or unknown IDs are rejected.

ABI 1.11 adds `LE_FEATURE_FUNCTION_UNIT`, allowing a model to learn a distinct
weight for grammatical and closed-class units across providers. Any artifact
that declares this feature must set its minimum runtime ABI to at least 1.11;
older runtimes therefore reject it before inference rather than silently
treating the unavailable feature as zero.

## Fitting

The standard-library trainer solves ridge-regularized least squares using
normal equations and partial-pivot Gaussian elimination. The intercept is not
regularized. Selected features are sorted by numeric ID unless explicitly
provided, and fitted parameters are quantized to binary32 before metrics are
calculated or the artifact is written. Numerical residue smaller than `1e-12`
is canonicalized to zero before quantization.

Inference multiplies binary32 features and weights using a binary64 accumulator
before clamping. This prevents finite artifact values from overflowing an
intermediate binary32 sum and keeps the Python metrics aligned with the runtime.

```sh
PYTHONPATH=training/src python3 -m lae_training fit-linear-salience \
  salience.jsonl learned.lem --ridge 0.000001
```

The report includes mean absolute error, root mean square error, and `R²` when
target variance is nonzero. For the generic command these are training-set
diagnostics, not evidence of generalization. Real experiments should use
held-out data and the independent evaluation framework.

## Released real-data model

Version 0.15 adds `train-provo`, a specialized reproducible path for the CC BY
4.0 Provo eye-tracking corpus. It verifies the canonical source checksum,
aggregates per-word fixation probability, and delegates feature discovery to a
matching `le-cli` so training and inference cannot silently disagree about
segmentation or function-word classification.

Model candidates are evaluated with passage-grouped five-fold cross-validation
and selected by held-out RMSE before the winner is refit on all compatible
units. The released length-plus-function model achieves held-out MAE 0.09126,
RMSE 0.11719, and R² 0.73414 across 2,661 units, reducing RMSE by 48.46% from a
fold-specific mean-only baseline. See the
[Provo fixation v1 model card](models/provo-fixation-v1.md) for provenance,
attribution, intended use, and limitations.

## Runtime boundary

The exporter writes a `LE_MODEL_LINEAR_SALIENCE` `.lem` artifact. The runtime
loads it through `le_model_load` and executes it through
`le_generate_model_signals` or `le_process_with_model`; no public function or
ownership rule changes for learned models. Runtime builds do not contain the
solver, dataset reader, Python, or checkpoints.

The baseline deliberately excludes feature transforms, interactions, neural
layers, and online adaptation. Future learned models can add new validated
artifact types while preserving the same C processing APIs.

## Segmental real-data model

Version 0.16 adds `train-provo-segmental` and
`LE_MODEL_SEGMENTAL_SALIENCE`. The trainer uses the Provo first-fixation X
coordinate and word bounding box to learn normalized landing position in
addition to word fixation probability. Runtime ABI 1.12 projects those outputs
onto English lexical-core subunits or a learned partial prefix. See the
[Provo segmental v1 model card](models/provo-segmental-v1.md) for held-out
results and limitations.
