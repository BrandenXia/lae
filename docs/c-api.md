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

## Function contracts

| Function | Nullability and ownership | Thread safety | Errors / lifetime |
|---|---|---|---|
| `le_runtime_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_process_options_init` | `options` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_prefix_model_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_presentation_config_init` | `config` may be null; otherwise caller-owned | Safe for distinct objects | No failure |
| `le_runtime_create` | `config` may be null; `out_runtime` must not be null; caller owns success result | Safe | Returns status; writes null before failure |
| `le_runtime_destroy` | Null accepted; consumes the handle | Safe for an unshared handle | Existing analyses, signals, and results remain valid |
| `le_analyze` | Runtime/output required; text and language borrowed during call; caller owns analysis | Concurrent calls are safe | Validates UTF-8 and provider output; writes null before failure |
| `le_analysis_*_count` | Null returns zero; analysis borrowed | Safe | Analysis must still be alive |
| `le_analysis_*_data` | Null/empty returns null; arrays and language bytes are borrowed | Safe | Views last until analysis destruction |
| `le_analysis_destroy` | Null accepted; consumes analysis and nested storage | Safe for an unshared handle | No failure |
| `le_generate_prefix_signals` | Runtime, analysis, and output required; config may be null | Concurrent calls are safe | Caller owns signal result; writes null before failure |
| `le_generate_lexical_core_signals` | Runtime, analysis, and output required | Concurrent calls are safe | Caller owns signal result; writes null before failure |
| `le_signal_result_*` | Null counts as empty; returned array is borrowed | Safe | Views last until signal-result destruction |
| `le_generate_emphasis` | Runtime, signals, and output required; config may be null | Concurrent calls are safe | Caller owns result; writes null before failure |
| `le_process` | Runtime and output pointer required; text/options borrowed only during call; caller owns result | Concurrent calls are safe | Returns status; writes null before failure |
| `le_runtime_last_error` | Runtime required for a nonempty answer; returned bytes are borrowed | Thread-local | View lasts until the same thread records another error |
| `le_status_string` | No owned inputs or output | Safe | Returned null-terminated string is static |
| `le_result_emphasis_count` | Null returns zero; result borrowed | Safe | Result must still be alive |
| `le_result_emphasis_data` | Null/empty returns null; array is borrowed | Safe | Array lasts until result destruction |
| `le_result_destroy` | Null accepted; consumes result and all nested storage | Safe for an unshared handle | No failure |

Destroying or using the same handle concurrently is not supported. A caller may
process concurrently through one runtime because the current runtime and its
provider/model objects have no mutable processing state.

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
router selects English for `en` / `en-*` and Chinese for `zh` / `zh-*`; the
generic fallback records all other tags as region metadata but does not
interpret them. Empty language means `und`. This preserves explicit routing
information without putting language behavior in the core. There is no
automatic or mixed-language detection in this version.

## Analysis representation

`le_analyze` runs the routed provider independently of reading and presentation
stages. An analysis owns an immutable source-text snapshot plus four contiguous
arrays:

- nodes, with stable node identifiers and UTF-8 byte spans;
- flattened child identifiers addressed by each node's child range;
- flattened features addressed by each node's feature range;
- language regions whose language byte views are analysis-owned.

Node kinds are generic structural categories. Feature IDs are extensible
32-bit identifiers. ABI 1.3 defined boundary strength, grapheme count, lexical
core, derivational affix, grammatical affix, and content-unit features. ABI 1.4
adds segmentation confidence plus Han and Latin script features. Unknown feature
IDs must be preserved or ignored rather than treated as errors by consumers.

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

`le_generate_emphasis` consumes those immutable signals with either
`LE_POLICY_BINARY` or `LE_POLICY_VARIABLE_STRENGTH`. Binary policy emits the
configured maximum strength for signals meeting the threshold and merges
adjacent equal spans. Variable policy maps salience linearly between configured
minimum and maximum strengths. Both return the same `le_result_t` plan used by
high-level processing.

## Error handling

No exception crosses the ABI. Statuses are stable signed 32-bit values.
Invalid UTF-8 returns `LE_ERROR_INVALID_UTF8`; invalid pointers, structure sizes,
flags, strategies, or normalized values return `LE_ERROR_INVALID_ARGUMENT`.
Allocation failures return `LE_ERROR_OUT_OF_MEMORY`. Use
`le_runtime_last_error` for a thread-local diagnostic intended for humans. The
diagnostic uses fixed thread-local storage so reporting an allocation failure
cannot itself allocate; messages longer than 511 bytes are truncated.
