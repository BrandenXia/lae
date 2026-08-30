# Training/runtime boundary

Training and runtime are separate systems with one directional contract:

```text
datasets → preprocessing → features → training/evaluation
         → model compiler → versioned artifact → runtime loader
```

The runtime must never import Python, PyTorch, JAX, experiment frameworks,
datasets, or raw checkpoints. Training may depend on the artifact specification
and export validation tools; runtime code may not depend on the training tree.

No `training/` package is created yet because this milestone has no executable
training work. When artifact v1 is designed, training can be introduced as an
independent top-level package with dataset, feature, evaluation, and export
interfaces. The exporter should behave like a compiler: validate feature and
language compatibility, optimize runtime representation, write versioned
metadata, and compute integrity information.

