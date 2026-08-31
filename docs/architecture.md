# Architecture

## Dependency direction

The project has an offline training side and an embeddable runtime, joined only
by the artifact contract:

```text
dataset → preprocessing → features → optimization/evaluation → `.lem`
                                                               ↓
host / binding / CLI
        ↓
public C ABI (`include/le`)
        ↓
ABI facade (`runtime/abi`)
        ↓
text → provider → IR → built-in/artifact model → signals → policy
```

The core uses abstract nodes such as `document`, `unit`, and `subunit`; it does
not define a language-independent “word.” Nodes refer to the immutable source
text with UTF-8 byte spans. Analysis produces facts and structure, the reading
model produces numeric signals, and the presentation policy alone produces
emphasis. Rendering is outside the library.

## Milestone implementation

The language router first queries external providers in deterministic
registration order, then selects statically linked providers for explicit `en`
/ `en-*`, `zh` / `zh-*`, and `ja` / `ja-*` tags, and otherwise selects the generic provider.
External providers use a stable C sink ABI; they never expose their allocator or
C++ types to the core. The core owns the resulting storage and provider-neutral
contract, but contains no English, Chinese, or Japanese analysis rules.

Advanced callers can provide a contiguous, grapheme-aligned language-region
partition. Each slice is routed independently, then its provider-neutral nodes
are shifted into document coordinates and merged under one validated root.
Region detection remains outside the runtime.

The generic provider groups maximal runs of grapheme clusters separated by
Unicode separator, control, or punctuation categories. This is only a fallback
segmentation strategy. It is deliberately not whitespace tokenization and does
not claim linguistic word boundaries.

The English provider emits document, sentence, unit, and subunit structure. A
small deterministic ruleset labels lexical cores and grammatical or
derivational affixes. It is a framework-validation baseline, not a general
morphological analyzer; its limitations are documented separately.

The Chinese provider emits sentence nodes, lexically segmented unit nodes, and
grapheme-level subunits without depending on whitespace. A deterministic
coverage-based segmenter uses a compact built-in lexicon and falls back to
individual Han characters. Script and segmentation-confidence features make
the distinction explicit without adding a language-specific node kind.

The Japanese provider groups mixed-script content with following okurigana or
particles and applies a compact longest-suffix morphology baseline. Lexical,
grammatical, derivational, Han, Hiragana, Katakana, and Latin facts remain
ordinary features on generic unit and subunit nodes.

Provider output is validated before it reaches a reading model. Validation
checks the document root, dense node identifiers, single-parent tree structure,
ordered non-overlapping siblings, contained grapheme-safe spans, finite unique
features, and ordered language regions with normalized confidence.

The prefix reading model chooses a fixed count or proportion of graphemes from
each unit and emits per-grapheme fixation-salience signals. The lexical-core
model consumes only stable IR feature IDs and emits lexical/fixation signals for
marked nodes; English and Japanese mark morphological subunits while Chinese
marks lexical units. Presentation is a separate stage: the binary policy thresholds and
merges signals at one strength, while
the variable policy interpolates strength from salience. Output spans remain
ordered and non-overlapping.

The model subsystem parses immutable bytes supplied by the host into an owned
runtime model. Its format layer has no provider, text, filesystem, or training
dependency. Loaded prefix artifacts supply strategy parameters; lexical-core
artifacts declare their required feature ID. Both feed the same reading-signal
and presentation stages as the compatibility APIs.

Linear-salience artifacts add a bias and sparse weights keyed by stable IR
feature IDs. The model scores `unit` nodes, treats missing features as zero,
clamps predictions to normalized salience, and feeds the existing presentation
stage. Training owns regression and export; runtime code only validates and
evaluates compiled parameters.

The independent `training/` Python package consumes versioned JSON Lines data.
Preprocessing records provider-neutral unit spans, grapheme boundaries, and
target prefix counts as UTF-8 byte coordinates. Feature extraction and a small
deterministic optimizer can fit the prefix baseline, measure offline error, and
export `.lem` v1 without importing or linking the runtime. Cross-system tests
then load that artifact through the production C ABI.

Evaluation remains offline and strategy-neutral. One schema summarizes rendered
plans using grapheme-index ranges; another records timing, comprehension,
eye-tracking, preference, distraction, and density measurements. Comparisons
pair the same example—or participant and example—before reporting
candidate-minus-baseline deltas. No evaluation code is linked into the runtime.

## Runtime properties

- Input is borrowed and immutable for the duration of `le_process`.
- A long-lived analysis owns one immutable source snapshot for later stages.
- Results are immutable and own one contiguous emphasis array.
- Analyses, signals, and results remain valid after their runtime is destroyed.
- Loaded models own parsed metadata and remain valid after runtime destruction.
- Processing has no global mutable model or provider state and is deterministic.
- Non-thread-safe plugin callbacks are serialized per registered provider;
  providers may opt into concurrent callbacks explicitly.
- Runtime targets do not import or link any source under `training/`.
- Diagnostics are thread-local; each thread observes its own most recent error.
- `utf8proc` supplies standards-based UTF-8 decoding and extended grapheme
  boundary behavior without introducing language-specific rules.

The C ABI exposes immutable flat views of nodes, child identifiers, features,
and language regions. Provider ABI v1 supports static registration everywhere
and optional module loading on supported hosts. Automatic language detection and
streaming remain deferred. English, Chinese, and Japanese exercise structurally
different uses of the same provider-neutral IR.

The first supported host-language binding lives under `bindings/python`. It is
a dependency-free wrapper over the shared C ABI, copies immutable emphasis
plans into Python values, and preserves UTF-8 byte offsets. It neither accesses
C++ internals nor imports the offline training package.

The companion Swift package imports the same public headers through a system
library target. It wraps opaque handles in deterministic reference types,
copies emphasis plans into Swift values, and converts validated UTF-8 byte
spans to `String.Index` ranges only when the offsets form valid Swift string
boundaries. This Python/Swift pair exercises two substantially different FFI
and string models without adding a privileged internal API.
