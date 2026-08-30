# Runtime model artifact format

No model artifact is implemented in this milestone. Deterministic prefix
parameters are passed through process options so the text/provider/model/policy
pipeline can be validated before a binary format is frozen.

The future artifact is a compiled runtime input, not a Python, PyTorch, JAX, or
training checkpoint. Its planned envelope will include a magic value, format
version, minimum runtime/ABI capability, model type and version, supported
languages, required feature identifiers, parameters or lookup data, and an
integrity checksum.

Loading will be memory-based at the core ABI so mobile, WASM, embedded, and
packaged-resource hosts do not require filesystem access. Validation must occur
before any artifact data is trusted. The model inspection tool and exporter
will share a written binary specification and golden fixtures.

