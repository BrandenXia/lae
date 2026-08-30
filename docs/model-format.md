# Runtime model artifact format v1

LAE model artifacts use the `.lem` extension and are compiled runtime inputs,
not Python, PyTorch, JAX, or training checkpoints. The runtime loads them from
an immutable memory buffer and owns the parsed model; no filesystem API is
required by the C ABI.

All integers are unsigned little-endian. Floating-point parameters are IEEE-754
binary32 values stored as their little-endian 32-bit representation. Offsets are
absolute byte offsets from the beginning of the artifact. The maximum accepted
artifact size is 16 MiB.

## Header

Format v1 uses this fixed 64-byte header:

| Offset | Size | Field | v1 rule |
|---:|---:|---|---|
| 0 | 8 | magic | bytes `4c 41 45 4d 4f 44 4c 00` (`LAEMODL\0`) |
| 8 | 2 | format major | `1` |
| 10 | 2 | format minor | `0` |
| 12 | 4 | header size | `64` |
| 16 | 4 | total size | exact input-buffer size |
| 20 | 4 | checksum | CRC-32/ISO-HDLC over the entire artifact with these four bytes treated as zero |
| 24 | 4 | minimum ABI | packed `(major << 16) | minor` |
| 28 | 4 | model type | `1` prefix, `2` lexical core, `3` linear salience |
| 32 | 4 | model version | nonzero producer-defined version |
| 36 | 4 | language count | at most 64 |
| 40 | 4 | required-feature count | at most 256 |
| 44 | 4 | language-table offset | exactly `64` in v1 |
| 48 | 4 | feature-table offset | exact end of language table |
| 52 | 4 | parameter-table offset | exact end of feature table |
| 56 | 4 | parameter word count | model-type-specific |
| 60 | 4 | flags | zero in v1 |

CRC-32 uses the reflected polynomial `0xedb88320`, initial value
`0xffffffff`, and final XOR `0xffffffff`. There is no padding between tables.

## Metadata tables

Each language entry is a 16-bit byte length followed by that many non-null-
terminated ASCII bytes. Tags follow the runtime's BCP-47-compatible syntax and
are compared case-insensitively. A primary tag such as `en` supports regional
tags such as `en-US`. Zero language entries means unrestricted.

Required features are packed 32-bit `le_feature_id_t` values. Unknown required
features make a model incompatible rather than corrupt. Duplicate language or
feature entries are invalid. Required features describe runtime capabilities;
they need not appear in every individual analysis, such as empty input.

## Model parameters

Prefix models contain exactly three 32-bit words:

1. prefix strategy (`LE_PREFIX_PROPORTIONAL` or `LE_PREFIX_FIXED`);
2. fixed grapheme count;
3. proportional value as IEEE-754 binary32 bits.

Lexical-core models contain no parameter words and must declare
`LE_FEATURE_LEXICAL_CORE` as a required feature.

Linear-salience models contain `2 + 2N` words:

1. bias as IEEE-754 binary32 bits;
2. weight count `N`, from 1 through 256;
3. `N` pairs of feature ID and IEEE-754 binary32 weight bits.

Every weighted feature must be unique, supported, and present in the artifact's
required-feature table. Bias and weights must be finite. Linear artifacts must
declare minimum ABI 1.6 or newer. The runtime evaluates
the ordered binary32 weights against `unit` node features with a binary64
accumulator and clamps the sum to `[0,1]`.

## Validation and compatibility

Loading verifies magic, format version, exact sizes and offsets, count limits,
CRC-32, flags, language syntax and uniqueness, required feature support, model
type, parameter shape and ranges, and minimum runtime ABI. Malformed artifacts
return `LE_ERROR_MODEL_INVALID`; well-formed artifacts requiring a newer format,
ABI, model type, flag, or feature return `LE_ERROR_MODEL_INCOMPATIBLE`.

The loader copies parsed metadata and parameters, so the caller may release the
source bytes after `le_model_load` returns. Model handles are immutable and can
outlive the runtime used to load them.

## Tooling

`le-model` uses the same encoder/parser implementation as the runtime:

```sh
le-model compile-prefix prefix.lem --fixed 2 --language en
le-model compile-lexical-core lexical.lem --language en --language zh
le-model compile-linear-salience learned.lem --bias 0.1 --weight 2:0.2 --language en
le-model inspect prefix.lem
```

The independent Python training exporter implements this written contract
rather than linking the runtime encoder. Its output is checked against the same
golden bytes and loaded by `le-model` and the C runtime in integration tests.

Encoding is deterministic. Tests pin header layout, encoded sizes, a golden
checksum, round trips, corruption handling, future-version rejection, unknown
feature rejection, training/runtime agreement, and CLI interoperability.
