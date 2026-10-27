# Optional cREXX parser-side library

The optional `crexx/` component implements the parser side in Level B and uses
Level L for grammar-driven highlighting. The
[BL plan](FUTURE_WORK.md#optional-crexx-bl-library) owns scope and acceptance.
The existing C middleware remains independent; the B editor library, emergency
highlighting and browser host are later work. The selected parser-side increment
has normal native macOS, single-VM qualification; other platform and deployment
claims retain the limits below.

## Configure, build and start a parser

Supply an existing matching cREXX toolchain and SDK. `CREXX_BIN_DIR` must contain
`rxc`, `rxlang`, `rxas`, `rxlink`, `rxbvm`, `library.rxbin` and `rxfnsl.rxbin`.
The SDK must provide `request.capturetrivia()` and `request.trivia()` (introduced
in CREXX `ccfa4aa6f`); an older SDK cannot build this adapter. CREXX is neither
fetched nor built by this project.

For only the optional B/L component and its focused checks:

```sh
cmake -S . -B build-crexx -DDSLSH_BUILD_CREXX=ON \
  -DCREXX_BIN_DIR=/absolute/path/to/CREXX/bin \
  -DDSLSH_BUILD_CORE=OFF -DDSLSH_BUILD_TOOLS=OFF \
  -DDSLSH_BUILD_EXAMPLES=OFF -DDSLSH_BUILD_PARSERS=OFF \
  -DDSLSH_BUILD_TESTS=ON -DBUILD_TESTING=ON
cmake --build build-crexx --target dslsh_crexx_examples dslsh_crexx_contracts --parallel 4
ctest --test-dir build-crexx -L crexx --output-on-failure
```

`dslsh_crexx_core` prepares only the B protocol/session modules;
`dslsh_crexx_adapter` prepares the L adapter. All producers use private import
roots and the selected tools. Add `-DCREXX_SOURCE_DIR=/absolute/path/to/CREXX`
to configure the optional PHP endpoint from that checkout's maintained grammar.

Configure a DSLSH editor to launch the settings parser with this command,
using absolute paths in the editor configuration:

```sh
/absolute/path/to/CREXX/bin/rxbvm --no-autoload \
  /absolute/path/to/build-crexx/crexx/programs/dslsh-settings.rxbin -a --stdio
```

The PHP endpoint substitutes `dslsh-php.rxbin`. These commands read framed DSLSH
requests, not a source filename or unframed text on stdin. The bytecode still
requires RXVM. The external VM/toolchain installation, native platforms and
browser transport are separate from this component's local qualification.

For the optional Classic Rexx example, also set `CREXX_CLASSIC_BUILD_DIR` to an
existing CREXX research build containing the maintained Classic grammar/parser
artifacts and the observation API. DSLSH stages their existing bytecode and
checks that its selected SDK matches; it never recursively builds CREXX.
The [component README](../crexx/README.md#optional-classic-rexx-example)
lists the exact parser-only closure and build commands. Launch
`dslsh-classic.rxbin` with the same `--no-autoload ... -a --stdio` form.

## Ownership and data flow

`dslshmodel`, `dslshwire`, `dslshsession` and `dslshstdio` own the shared values,
protocol, document versions and STDIO endpoint. `dslshlevel` imports the shared
model and the existing cREXX SDK; it does not import a session or transport.
No edited source is executed, compiled or loaded as validation code.

For each immutable source version, the service anchors its source, positions
index and request context through the synchronous parser callback:

```text
source + request identity + UTF-8 boundary index
  -> borrowed parser callback
  -> fresh Level L request over one anchored frozen definition
  -> tokens + observed comment regions + optional successful named tree
  -> explicit profile mappings and optional trusted validation
  -> owned codepoint-positioned DSLSH result
```

The service applies I/D/H messages and controls atomic publication. Hypotheses
use a scratch document; parser callbacks cannot retain their source/context/map
references. A fresh full recognition pass per version is the initial baseline.
Protocol deltas do not imply incremental lexing or parsing.

## Shared callback and result

`.dslshmodel..parser` defines `parse(source: reference .binary,
context: reference .request) -> .result`. Source is strict UTF-8 logical-buffer
data; the initial interoperability profile refuses embedded NUL. Context has
`docid()`, `base()`, `version()`, `overlay()` and `hypothesis()`. When
`haspositions()` is true, borrow `context.positions()` for this call only;
otherwise a direct caller constructs one local `.dslshmodel..positions` index.
The index owns boundaries, not another source copy. Its `byte(point)` and
`point(byte)` queries return -1 for invalid positions or non-boundaries.

The returned result owns its messages and flat tree records. `begin()` and
`close()` delimit containers; `leaf()` adds source leaves; `finish(extent)`
checks bounds, hierarchy and complete ordered coverage. All positions use
zero-based Unicode codepoints, not UTF-8 bytes or display columns. Empty input
uses a zero-length root. A zero-width diagnostic covers the next scalar, or the
preceding scalar at EOF; empty-source diagnostics attach to the root. Zero-length
records do not supply source coverage.

## Grammar-independent profile and validation

`.dslshlevel..profile` binds declared names through `token(name,kind)` and
`rule(name,kind)`, then `finish(reference definition)`. Token kinds are DSLSH
lexical categories; rule kinds are structural categories. They are not colours.
Admission checks names and category kinds once and freezes the profile.
The initial adapter admits scanner-only and nonregional LR1 definitions; other
parser backends refuse explicitly. It does not silently fall back to LL, PEG,
Pratt or Earley, or change the grammar's recognition semantics.
Token zero is valid: the sealed token-category table uses token ID + 1.
Unknown/duplicate names refuse the profile rather than guessing a language.
Named-tree rule labels use the sealed, bounded mapping; projection never
requeries the grammar or reconstructs a definition per node.
Generated LR definitions keep rule metadata in their tables rather than the
optional `definition.grammar()` field. Profile admission reads the existing
tables/production metadata once; these explicit owned setup copies do not occur
during edits or token lookup.

The adapter owns no language-specific keyword table. The settings and PHP
profiles supply their own mappings. Plain grammar parsing does not infer symbol
resolution, variable scopes or types. Native cREXX parser mode remains the
existing full B/G/C semantic-highlighting route; the optional PHP profile covers
only the maintained example subset, and no full B grammar is supplied here.

An optional `.dslshlevel..validator` receives borrowed source, token stream and
successful raw derivation tree with declared labels/root, plus a borrowed output
collector. It runs only
after a clean unique parse. It may add owned byte-ranged diagnostics or refine
an exact lexical token's category/identifier. It cannot mutate or retain those
borrowed owners, change recognition, or print on protocol stdout. AST creation
is optional application work, not a validator prerequisite.

The collector checks ranges, limits and messages; refinements must match exact
token spans. Nonzero diagnostic ranges split leaves at their exact boundaries
while retaining lexical category and identifier. Overlapping messages are joined
in collector order with their codes; the first code and greatest severity are
the displayed attributes. Adapter projection meters work and merges a bounded
ordered boundary list without an unbounded token-by-diagnostic cross product.
Stable diagnostic codes belong
to the profile/validator. Callback failure discards its partial refinements;
arbitrary callback work must be bounded/cooperative because parser budgets do
not preempt trusted B code.

## Failed input and trivia

Level L positions are original UTF-8 byte ranges with an exclusive end. Convert
each final range through the one snapshot boundary index before serialization.
Line/column diagnostic formatting is separate and must not substitute for this
global coordinate conversion.

The SDK's `request.capturetrivia(limit)` selects outer-comment observations
before lexing/parsing; `trivia()` returns one owned `.llcomments..regions` value.
The existing comment scan supplies rule/start/finish/complete records, including
an unfinished outer comment ending at EOF. No adapter delimiter lexer or second
comment pass is used. Capture is disabled by default and bounded by its selected
limit and `tree.entries`; exhaustion is a resource failure, not truncated success.

Extract tokens/trivia at most once per request. Failed lexing can retain an
accepted token prefix; source preparation can fail before any token exists.
Render observed comments and tokens, retain located errors, and use unknown
coverage for the remaining input. Only actual whitespace bytes may become
whitespace leaves. Never call strict `ast()`/`projection()` on failed or forest
results and never label an internal partial arena as a recovered tree.
Existing flat-record recovery is a distinct capability; PHP token stages and
the contextual cREXX frontend do not acquire it automatically.
Recognition status 1 is invalid source and can publish this lexical/error
snapshot. Resource exhaustion (2) and invalid contract/configuration (3) instead
return a failed result; the service leaves its committed document/version
unchanged. The client must correct limits/configuration and resynchronize rather
than assuming that a refused delta advanced the server mirror.

`dslshlevel.project()` exposes that same synchronous renderer for already
observed token/comment streams, using a sealed live profile and one exact source
position index. A root of zero emits only the flat lexical file tree. The
Classic adapter selects `llclanguage.prepared()` once, seals explicit category
mappings and obtains one bounded `llcparse.parsed` observation per source
snapshot. Classic preparation remains the sole owner of contextual keywords,
source options, continuation filtering, Unicode and comment policy. Authored
continuation commas remain visible in observations. Failed input retains
lexical/comment/unknown coverage with its original offset/error, without
requesting or presenting a partial AST. Active SRCMAP is refused before
projection because edited-buffer ranges must still name original bytes.

## Reuse, limits and qualification boundary

Create the generated definition once in stable worker storage and keep it live
and unchanged through every borrowed request. Frozen LFP1 data is trusted build
output, not a format for accepting editor-supplied binary images. Requests and
results are separate per source version. Public value getters can copy, so
extract a tree/token value once and traverse IDs/scoped borrows thereafter.

Frame/source/token/message/region and recognition-work limits are explicit.
Profile mappings are bounded and prepared once; no global semantic cache or
runtime index construction is required. Source styles remain independent of UI
colours and display widths. Selected normal SDK, grammar/validator and C-protocol
interoperability checks qualify this parser-side profile only; full language,
installed/platform/browser, performance and full B-library claims remain open.

## Maintained example profiles

`crexx/examples/settings.levell` is an independent small LR1 grammar with names,
integers, quoted strings, assignments, comments and separators. Its `settings.crexx`
endpoint supplies the explicit category mappings and a trusted user validator:

```text
unknown = 1
limit = 100
```

The first setting produces `SET_UNKNOWN`; the second produces `SET_RANGE` because
the example policy permits `limit` from 0 through 99. Known setting names also
receive an explicit constant-identifier refinement. These checks belong to the
example, not Level L or DSLSH. Invalid syntax receives lexical/error output and
does not invoke that validator.

The optional `php.crexx` endpoint reads the maintained grammar at
`CREXX_SOURCE_DIR/examples/php/grammar/php.levell`; no second PHP grammar is
maintained here. It maps the supported example's procedural, function, array and
selected built-in-call syntax. It does not translate or execute PHP, supply PHP
name/type validation, or claim complete PHP compatibility. Both endpoints accept
`--syntaxhighlight`, `--stdio` and the parser tester's `-d` compatibility flag.
