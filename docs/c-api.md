# C API

`include/le/api.h` is the canonical public interface. It is compiled as C11,
C17, and C++ in the test suite. Public structs contain only C-compatible types;
long-lived objects use opaque handles. Exceptions are caught at every ABI
operation that can execute C++ code.

## Versioning and structures

`LE_ABI_VERSION_MAJOR` and `LE_ABI_VERSION_MINOR` version the ABI. Public input
structs begin with `struct_size`; initialize them with their corresponding
initializer. This runtime accepts larger structures and ignores appended bytes,
but rejects structures smaller than the v1 size. Existing fields must never be
reordered or reinterpreted within ABI major version 1.

ABI 1.5 adds the opaque model lifecycle, metadata discovery,
artifact-driven signal generation, and high-level model processing functions.
ABI 1.6 adds the `LE_MODEL_LINEAR_SALIENCE` model type without changing any
function signature or public structure layout.
ABI 1.7 adds provider ABI v1, runtime-local provider discovery and registration,
and optional dynamic module loading.
ABI 1.8 adds stable Hiragana and Katakana script feature identifiers and the
built-in Japanese provider without changing public structure layouts.
ABI 1.9 adds `le_analyze_regions` for explicit mixed-language provider routing.
ABI 1.10 adds `le_process_regions` and `le_process_regions_with_model` for
high-level explicit-region processing.
ABI 1.11 adds the stable `LE_FEATURE_FUNCTION_UNIT` identifier without changing
any public structure layout or function signature.
ABI 1.12 adds the `LE_MODEL_SEGMENTAL_SALIENCE` artifact type and stable unit
position, sentence progress, and sentence unit count features without changing
any public function or structure layout.

## Function contracts

| Function | Nullability and ownership | Thread safety | Errors / lifetime |
|---|---|---|---|
| `le_runtime_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_process_options_init` | `options` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_prefix_model_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_presentation_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_runtime_create` | `config` may be null; `out_runtime` must not be null; caller owns success result | Safe | Returns status; writes null before failure |
| `le_runtime_destroy` | Null accepted; consumes the handle | Safe for an unshared handle | Existing models, analyses, signals, and results remain valid |
| `le_runtime_register_provider` | Runtime and v1 descriptor required; runtime takes context lifecycle on success | Safe; briefly blocks concurrent routing | Validates compatibility and unique name |
| `le_runtime_load_provider` | Runtime and path required; path borrowed for call | Safe; briefly blocks concurrent routing | Optional build capability; module retained by runtime |
| `le_runtime_provider_*` | Runtime borrowed; null/out-of-range returns empty | Safe | Names remain borrowed until runtime destruction |
| `le_runtime_dynamic_providers_enabled` | No arguments | Safe | Returns a build capability, not module availability |
| `le_model_load` | Runtime, bytes, and output required; bytes borrowed during call; caller owns model | Concurrent calls are safe | Fully validates artifact; writes null before failure |
| `le_model_destroy` | Null accepted; consumes model | Safe for an unshared handle | No failure |
| `le_model_*` metadata accessors | Null returns zero/empty; model borrowed | Safe | Views last until model destruction |
| `le_analyze` | Runtime/output required; text and language borrowed during call; caller owns analysis | Concurrent calls are safe | Validates UTF-8 and provider output; writes null before failure |
| `le_analyze_regions` | Runtime/output required; region array, language views, and text borrowed during call | Concurrent calls are safe | Requires exact grapheme-aligned coverage; routes and validates each provider; writes null before failure |
| `le_analysis_*_count` | Null returns zero; analysis borrowed | Safe | Analysis must still be alive |
| `le_analysis_*_data` | Null/empty returns null; arrays and language bytes are borrowed | Safe | Views last until analysis destruction |
| `le_analysis_destroy` | Null accepted; consumes analysis and nested storage | Safe for an unshared handle | No failure |
| `le_generate_prefix_signals` | Runtime, analysis, and output required; config may be null | Concurrent calls are safe | Caller owns signal result; writes null before failure |
| `le_generate_lexical_core_signals` | Runtime, analysis, and output required | Concurrent calls are safe | Caller owns signal result; writes null before failure |
| `le_generate_model_signals` | Runtime, analysis, model, and output required | Concurrent calls are safe | Checks model language metadata; caller owns result |
| `le_signal_result_*` | Null counts as empty; returned array is borrowed | Safe | Views last until signal-result destruction |
| `le_generate_emphasis` | Runtime, signals, and output required; config may be null | Concurrent calls are safe | Caller owns result; writes null before failure |
| `le_process` | Runtime and output pointer required; text/options borrowed only during call; caller owns result | Concurrent calls are safe | Returns status; writes null before failure |
| `le_process_with_model` | Runtime, model, and output required; text/options borrowed during call | Concurrent calls are safe | Uses model parameters and option presentation fields |
| `le_process_regions` | Runtime/output required; text, regions, and options borrowed during call | Concurrent calls are safe | Uses explicit routing with configured built-in reading and presentation stages |
| `le_process_regions_with_model` | Runtime/model/output required; text, regions, and options borrowed during call | Concurrent calls are safe | Checks every region against model language metadata |
| `le_runtime_last_error` | Runtime required for a nonempty answer; returned bytes are borrowed | Thread-local | View lasts until the same thread records another error |
| `le_status_string` | No owned inputs or output | Safe | Returned null-terminated string is static |
| `le_result_emphasis_count` | Null returns zero; result borrowed | Safe | Result must still be alive |
| `le_result_emphasis_data` | Null/empty returns null; array is borrowed | Safe | Array lasts until result destruction |
| `le_result_destroy` | Null accepted; consumes result and all nested storage | Safe for an unshared handle | No failure |

Destroying a runtime concurrently with another operation is not supported. A
caller may process concurrently through one runtime. External callbacks are
concurrent only when their descriptor declares `LE_PROVIDER_FLAG_THREAD_SAFE`;
the runtime serializes other provider contexts.

## Model lifecycle and metadata

`le_model_load` accepts `.lem` bytes from memory and never performs filesystem
access. The loader checks integrity, format and ABI compatibility, model type,
language tags, required feature IDs, and parameters before returning an
immutable handle. It owns parsed strings and arrays, so input bytes may be
released immediately and the model may outlive its loading runtime.

Model accessors expose type, producer-defined model version, minimum ABI,
supported languages, and required feature identifiers. Zero languages means
unrestricted. `le_model_supports_language` matches tags case-insensitively and
treats a primary capability such as `en` as supporting `en-US`.

`LE_MODEL_LINEAR_SALIENCE` evaluates artifact weights against stable features
on each `LE_NODE_UNIT`. Missing features contribute zero; positive clamped
predictions become fixation and lexical salience signals over the unit span.
`LE_MODEL_SEGMENTAL_SALIENCE` adds a learned within-unit anchor predictor and
projects positive salience onto a strict lexical core or a grapheme-safe partial
prefix. The result still uses the same immutable signal and emphasis arrays.

## Processing options

With null options, processing uses generic analysis, a proportional prefix of
`0.5`, and emphasis strength `1.0`. `LE_PREFIX_FIXED` uses
`fixed_graphemes`; `LE_PREFIX_PROPORTIONAL` uses a finite proportion in `[0,1]`.
Strength must also be finite and normalized to `[0,1]`.

Process options v2 appended presentation policy, minimum strength, and salience
threshold fields. V3 appends `reading_model`. `LE_PROCESS_OPTIONS_V1_SIZE` and
`LE_PROCESS_OPTIONS_V2_SIZE` remain their historical boundaries; the runtime
never reads fields beyond the caller's declared version. The initializer emits
the latest v3 size. Binary policy and `LE_READING_MODEL_PREFIX` are compatibility
defaults. On 64-bit targets, an explicit reserved field occupies v2's historical
tail padding so the v2 and v3 size boundaries remain distinguishable.

`language` is a non-null-terminated borrowed BCP-47-compatible byte view. The
router queries registered external providers first, then selects English for
`en` / `en-*`, Chinese for `zh` / `zh-*`, and Japanese for `ja` / `ja-*`; the
generic fallback records all other tags as region metadata but does not interpret
them. Empty language means `und`. This preserves explicit routing information
without putting language behavior in the core. `le_analyze_regions` accepts a
grapheme-aligned, contiguous partition of non-empty text, routes each region
independently, and merges provider output into one document-relative analysis.
Region confidence and language tags are preserved. Empty text requires an empty
partition. Automatic language detection is not implemented. The complete
provider contract is documented in the
[provider plugin ABI](provider-plugin-abi.md).

## Analysis representation

`le_analyze` runs one routed provider independently of reading and presentation
stages. `le_analyze_regions` performs the same operation for each explicit
language region and merges the results. An analysis owns an immutable
source-text snapshot plus four contiguous arrays:

- nodes, with stable node identifiers and UTF-8 byte spans;
- flattened child identifiers addressed by each node's child range;
- flattened features addressed by each node's feature range;
- language regions whose language byte views are analysis-owned.

Node kinds are generic structural categories. Feature IDs are extensible
32-bit identifiers. ABI 1.3 defined boundary strength, grapheme count, lexical
core, derivational affix, grammatical affix, and content-unit features. ABI 1.4
adds segmentation confidence plus Han and Latin script features. Unknown feature
IDs must be preserved or ignored rather than treated as errors by consumers.

ABI 1.8 adds stable Hiragana and Katakana script features.
ABI 1.11 adds the complementary function-unit semantic feature; providers may
omit both content and function classification when confidence is insufficient.
ABI 1.12 adds unit position (one-based within its sentence), normalized sentence
progress, and sentence unit count.

Feature namespaces begin at `0x00000000` (core), `0x00010000` (morphology),
`0x00020000` (syntax), `0x00030000` (semantic), `0x00040000` (script), and
`0x80000000` (vendor/plugin-specific).

## Reading and presentation stages

`le_generate_prefix_signals` consumes an analysis and returns normalized
numerical signals without choosing typography. The analysis owns an immutable
source-text snapshot, available through `le_analysis_text`, so bindings do not
need to preserve or resupply the input buffer. The signal result owns a
contiguous `le_reading_signal_t` array.

`le_generate_lexical_core_signals` emits one normalized signal for each
non-document node carrying `LE_FEATURE_LEXICAL_CORE`. The same selection is
available to high-level processing through `LE_READING_MODEL_LEXICAL_CORE`;
older option layouts and null options use `LE_READING_MODEL_PREFIX`.

`le_generate_model_signals` selects the reading implementation and parameters
from a loaded artifact. `le_process_with_model` performs routed analysis,
artifact-driven signal generation, and the presentation policy in one call.
Its process-option prefix/model-selector fields are compatibility fields; the
artifact selects the reading model, while language and presentation fields
remain active.

`le_generate_emphasis` consumes those immutable signals with either
`LE_POLICY_BINARY` or `LE_POLICY_VARIABLE_STRENGTH`. Binary policy emits the
configured maximum strength for signals meeting the threshold and merges
adjacent equal spans. Variable policy maps salience linearly between configured
minimum and maximum strengths. Both return the same `le_result_t` plan used by
high-level processing.

`le_process_regions` and `le_process_regions_with_model` provide the same
high-level pipeline over an explicit partition. `options->language` must be
empty because the region array is the routing authority. The former uses the
configured built-in reading model; the latter uses its artifact model and
rejects the request unless that model supports every region language.

## Error handling

No exception crosses the ABI. Statuses are stable signed 32-bit values.
Invalid UTF-8 returns `LE_ERROR_INVALID_UTF8`; invalid pointers, structure sizes,
flags, strategies, or normalized values return `LE_ERROR_INVALID_ARGUMENT`.
Malformed artifacts return `LE_ERROR_MODEL_INVALID`; valid artifacts requiring
unsupported ABI, format, features, flags, or model types return
`LE_ERROR_MODEL_INCOMPATIBLE`. Language capability mismatches return
`LE_ERROR_UNSUPPORTED_LANGUAGE`. Provider descriptor mismatches return
`LE_ERROR_PLUGIN_INCOMPATIBLE`; callback, module, and emitted-IR failures return
`LE_ERROR_PLUGIN_FAILURE`. A capability compiled out of the runtime returns
`LE_ERROR_UNSUPPORTED`. Allocation failures return `LE_ERROR_OUT_OF_MEMORY`. Use
`le_runtime_last_error` for a thread-local diagnostic intended for humans. The
diagnostic uses fixed thread-local storage so reporting an allocation failure
cannot itself allocate; messages longer than 511 bytes are truncated.
