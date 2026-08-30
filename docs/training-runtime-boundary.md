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
training work. Artifact format v1 is now implemented, along with a deterministic
model compiler used to validate the runtime boundary. The next milestone can
introduce training as an independent top-level package with dataset, feature,
evaluation, and export interfaces. Its exporter should target the written v1
format or a backward-compatible extension rather than linking training code
into the runtime.
