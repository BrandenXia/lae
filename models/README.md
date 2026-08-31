# Released models

## Provo fixation v1

`lae-provo-fixation-v1.lem` is LAE's first real-data model. It predicts the
probability that an English word attracts a fixation, then lets the host choose
how that score becomes typographic emphasis. The recommended binary threshold
is `0.60`.

```sh
printf 'Language-aware emphasis helps readers scan complex documentation.' |
  build/le-cli --artifact models/lae-provo-fixation-v1.lem \
  --language en --threshold 0.60
```

For a Markdown result through the Python binding:

```sh
PYTHONPATH=bindings/python/src python3 examples/provo_markdown.py \
  'Language-aware emphasis helps readers scan complex documentation.'
```

Files:

- `lae-provo-fixation-v1.lem`: 100-byte runtime artifact, requiring ABI 1.11.
- `lae-provo-fixation-v1.json`: complete source, selection, metric, parameter,
  and checksum report.
- `lae-provo-fixation-v1-NOTICE.txt`: portable data attribution and
  transformation notice.
- [Model card](../docs/models/provo-fixation-v1.md): intended use, validation,
  provenance, and limitations.

The raw corpus is deliberately not redistributed. The trainer verifies the
canonical OSF file checksum before it reads any observations.
