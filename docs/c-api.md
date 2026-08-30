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
| `le_runtime_create` | `config` may be null; `out_runtime` must not be null; caller owns success result | Safe | Returns status; writes null before failure |
| `le_runtime_destroy` | Null accepted; consumes the handle | Safe for an unshared handle | Existing results remain valid |
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

`language` is a non-null-terminated borrowed BCP-47-compatible byte view. The
generic fallback records it as region metadata but does not interpret it.
Empty language means `und`. This preserves explicit routing information without
putting language behavior in the core.

## Error handling

No exception crosses the ABI. Statuses are stable signed 32-bit values.
Invalid UTF-8 returns `LE_ERROR_INVALID_UTF8`; invalid pointers, structure sizes,
flags, strategies, or normalized values return `LE_ERROR_INVALID_ARGUMENT`.
Allocation failures return `LE_ERROR_OUT_OF_MEMORY`. Use
`le_runtime_last_error` for a thread-local diagnostic intended for humans. The
diagnostic uses fixed thread-local storage so reporting an allocation failure
cannot itself allocate; messages longer than 511 bytes are truncated.
