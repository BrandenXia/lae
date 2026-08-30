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
target variance is nonzero. These are training-set diagnostics, not evidence of
generalization. Real experiments should use held-out data and the independent
evaluation framework.

## Runtime boundary

The exporter writes a `LE_MODEL_LINEAR_SALIENCE` `.lem` artifact. The runtime
loads it through `le_model_load` and executes it through
`le_generate_model_signals` or `le_process_with_model`; no public function or
ownership rule changes for learned models. Runtime builds do not contain the
solver, dataset reader, Python, or checkpoints.

The baseline deliberately excludes feature transforms, interactions, neural
layers, and online adaptation. Future learned models can add new validated
artifact types while preserving the same C processing APIs.
