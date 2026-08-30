# Text and offsets

The external encoding is UTF-8. `le_string_view_t` is a byte view and does not
require null termination; embedded U+0000 is valid input. A null pointer is only
valid when the size is zero.

Every `le_text_span_t` is a half-open UTF-8 byte interval `[begin, end)`, with:

```text
begin <= end <= input byte size
```

The implementation decodes the entire input before analysis. Malformed,
overlong, truncated, surrogate, or out-of-range UTF-8 is rejected. Internally,
`ByteOffset` is a strong type rather than an ambiguous integer.

All returned emphasis boundaries are both valid UTF-8 boundaries and extended
grapheme-cluster boundaries. Consequently, prefix processing does not split
combining sequences, emoji modifier sequences, regional-indicator sequences,
emoji ZWJ sequences, or supported Indic conjunct sequences. Grapheme breaking
is provided by the linked `utf8proc` Unicode implementation.

Offsets are never code-point indices, grapheme indices, UTF-16 code units, or
display columns. Bindings for Swift, Java, C#, or DOM APIs must explicitly
translate byte spans to the coordinate system expected by the host.

