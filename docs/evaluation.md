# Evaluation framework

LAE evaluates emphasis strategies outside the runtime. The framework accepts
arbitrary variant names, so the same machinery can compare plain text,
traditional prefixes, morphological emphasis, and future learned models.
It has two versioned JSON Lines inputs: offline emphasis plans and human-study
observations.

## Offline plan records

Plan schema v1 describes one rendered plan for one example:

```json
{
  "schema_version": 1,
  "example_id": "example-1",
  "variant": "traditional-prefix",
  "grapheme_count": 10,
  "line_count": 2,
  "emphasized_ranges": [{"begin": 0, "end": 2}]
}
```

Plan ranges are half-open grapheme indices, not runtime UTF-8 byte offsets.
Preprocessing must convert runtime spans using the same grapheme segmentation
as the analyzed text. Ranges must be ordered, separated, nonempty, and within
the example. Adjacent ranges must be merged because v1 does not represent
distinct presentation strengths.

The summary reports:

- emphasis density: emphasized graphemes divided by all graphemes;
- text density: graphemes per rendered line;
- fragmentation: emphasis spans per 100 graphemes;
- visual-complexity proxy: emphasis-state transitions per 100 adjacent
  grapheme boundaries.

The transition metric is only an offline proxy for visual disruption. It is
not a substitute for a participant's distraction rating or perceptual study.

## Human-study observations

Study schema v1 records one participant/example/variant observation:

```json
{
  "schema_version": 1,
  "participant_id": "p1",
  "example_id": "example-1",
  "variant": "morphological",
  "duration_ms": 48000,
  "content_unit_count": 100,
  "grapheme_count": 500,
  "line_count": 20,
  "emphasized_graphemes": 150,
  "comprehension_score": 0.88,
  "fixation_duration_ms": 23000,
  "fixation_count": 85,
  "regression_count": 7,
  "preference_score": 0.80,
  "distraction_score": 0.20
}
```

`content_unit_count` is the experiment's reading-speed denominator. A study
must use one consistent definition, such as words, lexical units, or another
predeclared unit. `fixation_duration_ms` is the total duration of all fixations
in the observation and must appear with `fixation_count`. Fixation, regression,
preference, and distraction fields are optional; comprehension and timing are
required. Scores are normalized to `[0,1]`.

Variant summaries include:

- content units per minute;
- comprehension score;
- mean duration per fixation;
- fixations and regressions per 100 content units;
- preference and reported distraction;
- emphasis density and rendered text density.

Counts and durations are pooled for fixation metrics. Reading speed,
comprehension, preference, and distraction are observation means. Missing
optional measurements remain `null`; they are never silently replaced by zero.
Summaries report the observation count for every optional measurement, and A/B
results report the number of valid pairs behind each optional delta.

## Paired A/B comparisons

Comparisons pair only records with the same participant and example for human
studies, or the same example for offline plans. Source grapheme count and
content-unit metadata must agree inside each pair; line count may differ because
rendered text density is itself a measured outcome. Every reported delta is:

```text
candidate - baseline
```

Positive reading-speed, comprehension, and preference deltas usually favor the
candidate. Negative fixation-duration, fixation-count, regression, and
distraction deltas usually favor it. Emphasis-density and plan-complexity
deltas are descriptive and have no universal preferred direction.

The framework reports paired sample counts and deterministic descriptive
statistics. It does not claim statistical significance, population effects, or
causality. Study protocols should predeclare exclusions, counterbalancing,
content-unit definitions, and statistical analysis before collecting data.

## Commands

```sh
PYTHONPATH=training/src python3 -m lae_training \
  summarize-plans plans.jsonl --baseline plain
PYTHONPATH=training/src python3 -m lae_training \
  summarize-study study.jsonl --baseline plain
```

Both commands emit stable-key JSON suitable for experiment reports or a later
statistics layer.
