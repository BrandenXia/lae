# Provider plugin ABI v1

LAE 0.10 stabilizes a C-only provider ABI for adding linguistic analyzers
without coupling the runtime to an NLP library or a C++ toolchain. The public
contract is `include/le/provider.h`; it is independent of the runtime's internal
`LanguageProvider` class.

## Descriptor and discovery

A provider supplies one `le_provider_v1_t` descriptor. Its `supports` callback
claims BCP-47-compatible language tags and its `analyze` callback emits an
analysis through the runtime-owned `le_analysis_sink_v1_t`. Provider names must
be nonempty, at most 255 bytes, and unique within one runtime.

Statically linked providers pass the descriptor to
`le_runtime_register_provider`. A successful call copies the descriptor and
transfers its context lifecycle to the runtime. The optional `destroy` callback
runs exactly once during runtime destruction. The descriptor object itself may
be released after registration.

Dynamic modules export this exact symbol with default visibility:

```c
LE_PROVIDER_EXPORT const le_provider_v1_t* le_provider_entry_v1(void);
```

`le_runtime_load_provider` opens a caller-supplied module path, finds that entry
point, validates the returned descriptor, and registers it. The runtime keeps
the module loaded until after the provider context is destroyed. It does not
scan ambient search paths or load modules implicitly. Hosts can enumerate
successfully discovered external providers with `le_runtime_provider_count`
and `le_runtime_provider_name_at`.

## Compatibility checks

The runtime rejects a provider with `LE_ERROR_PLUGIN_INCOMPATIBLE` when:

- `struct_size` is smaller than `LE_PROVIDER_V1_SIZE`;
- `abi_version` is not `LE_PROVIDER_ABI_VERSION` (1.0);
- an unknown flag or nonzero reserved field is present;
- the name or required callbacks are invalid; or
- the provider name is already registered.

Larger descriptors are accepted so fields may be appended in a future ABI.
Existing v1 fields, callback signatures, constants, and ownership rules will
not be reordered or reinterpreted. A module that cannot be opened or whose
analysis callback fails returns `LE_ERROR_PLUGIN_FAILURE`; malformed emitted IR
also becomes a plugin failure. Allocation failures remain
`LE_ERROR_OUT_OF_MEMORY`.

## Sink protocol

The sink is borrowed only for the `analyze` callback. A provider must:

1. emit nodes with dense identifiers beginning at zero;
2. emit a document node zero spanning the complete source;
3. emit a node before attaching its features or referencing it from an edge;
4. emit child edges in non-overlapping source order;
5. use half-open UTF-8 byte spans aligned to grapheme boundaries;
6. use finite feature values and unique feature IDs per node; and
7. emit ordered, non-overlapping regions with valid language tags and
   confidence in `[0, 1]`.

Every sink function returns `le_status_t`. Providers must stop and return that
status when a sink call fails. The runtime copies language bytes and all graph
data during each call; a provider must not retain the text, language, or sink.
The completed graph is run through the same validator used for built-in
English, Chinese, and generic providers.

Registered providers are queried in registration order before built-ins. If no
external provider claims the requested language, normal built-in routing is
unchanged. `LE_PROVIDER_FLAG_THREAD_SAFE` declares that `supports` and
`analyze` may run concurrently for the same context. Without it, the runtime
serializes calls to that provider. Callbacks are C ABI boundaries: they must not
throw exceptions, retain borrowed inputs, or unwind into the runtime.

## Optional dynamic loading

`LAE_ENABLE_DYNAMIC_PROVIDERS` controls filesystem-backed module loading. It is
on by default for supported desktop targets and off by default for Emscripten,
iOS, tvOS, and watchOS. With it off, the loader and operating-system dynamic
linker are not linked into the runtime; `le_runtime_load_provider` returns
`LE_ERROR_UNSUPPORTED`, while static registration remains fully available.
This is the supported configuration for WASM, iOS, and embedded/static builds.
`le_runtime_dynamic_providers_enabled` lets a host inspect the build capability
without attempting a load.
