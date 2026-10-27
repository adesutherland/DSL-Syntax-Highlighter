# Optional cREXX parser library

The first B/L increment implements the parser side of DSLSH. The full optional
B library remains the goal in [the owning plan](../docs/FUTURE_WORK.md): editor
synchronization, emergency highlighting, sockets, browser/WASM hosts and complete
language profiles retain their open criteria. The native C middleware and its
default build remain independent of the optional cREXX toolchain/SDK.
The [integration guide](../docs/CREXX_INTEGRATION.md) describes the L adapter,
language profiles, validation and the supported build/runtime boundary.

## B parser core

`lib/dslshmodel.crexx` owns the shared contracts. `dslshwire.crexx` owns the
existing text/hex codec; `dslshsession.crexx` owns versioned document mirrors;
`dslshstdio.crexx` owns synchronous framing. The core imports Level B facilities
only. The L adapter imports the model, with no dependency on the transport.

A parser implements `dslshmodel.parser.parse(source = reference .binary,
context = reference .dslshmodel..request) -> .dslshmodel..result`. Both arguments
are immutable borrows valid only during that callback. The provider must not
mutate them, retain their references, execute the edited program or publish its
own transport output. Request getters are `docid`, `base`, `version`, `overlay`
and `hypothesis`. `haspositions()` guards `positions()`, a borrowed reference to
the exact source index. Directly constructed requests may omit that index;
such adapters construct one locally rather than retaining a source reference.

`dslshmodel.positions(reference source, limit, work)` is the single byte/span
authority used by the core and L adapter. It validates strict UTF-8 and refuses
embedded NUL, which the C middleware's terminating-NUL strings cannot transport
faithfully. `count()` is the codepoint extent, `bytes()` is the byte extent,
`byte(point)` and `point(byte)` accept exact boundaries including the end and
return `-1` for invalid/non-boundary positions. ASCII needs no offsets storage.
The index retains owned boundary integers, never a source reference.

Construct an owned `result`, use `begin(type,pos,length,identifier,severity,
code,message)` for containers, `leaf(...)` for tokens and `close()` for each
container. Severity is numeric 0–3 (none/information/warning/error), independent
of colours. `finish(codepoint_length)` performs a bounded linear stack check:
one file root (71), ordered nested extents and leaves covering the exact source
without overlap or gaps. Only a final EOF leaf (34) may have zero length; an
empty document uses a zero-length root. Attach EOF diagnostics to the nearest
real leaf, or to that empty root. A sealed result owns all records/messages;
later mutation is refused and repeated `finish` must name the same extent.
Malformed/recovered source is still a successful callback when it returns a
complete admitted lexical/unknown tree with located diagnostics.

`dslshsession.service(provider,policy)` exposes `handle(reference payload)` for
transport-independent use. Inspect `status()`/`error()` for refused requests;
`documents()`, `version(id)` and owned `source(id)` support host inspection.
`dslshstdio.run(reference service,maximum)` runs that service over standard
handles; return codes are 0 for clean EOF, 2 for ingress/service failure and 3
for output failure. Standard handles remain caller-owned.

## Wire and state contract

Frames have exactly eight hexadecimal ASCII header bytes counting payload
bytes. Messages are consumed and answered in order. `I` creates/replaces the
named session; version-2 `D` requires the current base and the next version;
`H` applies the same edits to a private candidate and never commits it. An
accepted edit whose source has syntax errors still commits its text/version.
Every request, edit, callback result and serialized reply is admitted before
session publication. A complete invalid request returns the existing empty
token stream `0` and leaves committed state unchanged. The service exposes its
reason locally; the legacy reply has no revision/identity or error envelope.
An exceptional native allocation/signal failure during final publication poisons
the service: it serves no further document state and the endpoint exits nonzero.
This terminal path makes no allocation-failure rollback claim and does not copy
unrelated documents for every edit.
Malformed, oversized or incomplete framing terminates the endpoint because safe
resynchronization cannot be established. Clean EOF releases process-owned state.

The actual transaction codes are `a` add-line, `b` delete-line, `c` add-codepoints,
`d` delete-codepoints, `e` join-lines, `f` split-line. Lines/columns are zero-based
Unicode codepoints. Add-line/add-codepoints content cannot contain LF: use the
explicit line operations. CR remains an ordinary preserved codepoint. Logical
source has LF after each stored line, including the final line, as in C DSLSH.
Initial line counts must agree with decoded content. Legacy unversioned deltas
are refused; current C clients emit version 2. Unlike the current C delta
implementation, this service also checks the declared base version.

The `C|EP` request (or legacy empty-command `C|`) receives the empty `C|`
configuration response. This keeps source/message
traffic entirely ASCII with hex-encoded strings; no raw-newline emergency-rule
configuration or general Windows binary-CRT contract is claimed. STDOUT is
reserved for protocol bytes. Diagnostics go to STDERR.

Default policy bounds are 8 MiB per frame, 2 MiB source per document, 16 documents,
8 MiB aggregate retained source, 4,096 transactions, 200,000 result records,
1 MiB decoded result messages and 20,000,000 candidate-edit work units. Index
passes and L recognition/projection have their own bounded phase allowances;
these are not one aggregate process-fuel counter. They are explicit admission
limits, not RSS, latency or production-performance claims.
Edits use private line state, charge copied/scanned work and join source once
per parse; response bytes grow through the maintained geometric binary append
path. No per-token whole-source or definition copy is required.

## Configure and package

The opt-in component requires CMake 3.24 and an existing matching CREXX build or
installation. Set `CREXX_BIN_DIR` explicitly to the directory containing `rxc`,
`rxlang`, `rxas`, `rxlink`, `rxbvm`, `library.rxbin` and `rxfnsl.rxbin`. Enabling
this option never builds or fetches CREXX. The default C-only configuration
does not inspect that directory or add the dependency.
The matching SDK must expose `request.capturetrivia()` and `trivia()`; the
supporting source contract is CREXX `ccfa4aa6f` or a compatible later revision.

For the B library without the native C middleware/examples/adapters:

```sh
cmake -S . -B build-crexx -DDSLSH_BUILD_CREXX=ON \
  -DCREXX_BIN_DIR=/absolute/path/to/crexx/bin \
  -DDSLSH_BUILD_CORE=OFF -DDSLSH_BUILD_TOOLS=OFF \
  -DDSLSH_BUILD_EXAMPLES=OFF -DDSLSH_BUILD_PARSERS=OFF
cmake --build build-crexx --target dslsh_crexx_core
```

Each producer owns a private source copy and an import directory containing only
its declared, completed inputs. Compilation uses `--no-exe-import`; linked test
drivers and other in-progress modules are not import search roots. Assembly and
linking finish privately before the resulting bytecode is published. B-only
drivers link the Level B library; only L consumers depend on `rxfnsl.rxbin`.
The native `rxlang` tool embeds its complete generator image and is invoked with
`--no-autoload`, without loading the separate callable `rxlanglib.rxbin`.

With tests enabled, `dslsh_crexx_contracts` prepares the maintained independent
fixtures. CTest uses the configured `rxbvm` with explicit bytecode and the `-a`
argument marker. `crexx_stdio` exercises framed input without a native C build;
it also exercises C interoperability when `parser_tester` is already a selected
target or `DSLSH_CREXX_PARSER_TESTER` explicitly names a prepared executable.

When `DSLSH_INSTALL` is enabled, the B sources and standalone modules install
under `share/dslsyntax/crexx`. The external CREXX VM and SDK remain separately
supplied dependencies. Cross builds and browser/WASM deployment are not qualified
by this native host-tools configuration.

## Optional Classic Rexx example

`dslshclassic.adapter` implements the same parser callback using CREXX's
maintained `levelc.rxl` grammar and `llcparse.parsed` preparation. It asks for
bounded `observedtokens()`/`observedtrivia()` values once per request, then
uses `dslshlevel.project()` with root zero. The result contains a flat file
root, lexical categories, observed comments, unknown coverage and original
located parser diagnostics; it does not claim a recovered or semantic AST.
No edited Rexx source is executed. Active `OPTIONS ... SRCMAP` is refused
because these editor spans require exact original bytes.

This example requires a prepared CREXX research build with the default-off
Classic observation API (`parsed(..., observations)`, `observedtokens()`,
`observedtrivia()` and `mapped()`). Add the build root explicitly:

```sh
cmake -S . -B build-crexx -DDSLSH_BUILD_CREXX=ON \
  -DCREXX_BIN_DIR=/absolute/path/to/CREXX/build/bin \
  -DCREXX_CLASSIC_BUILD_DIR=/absolute/path/to/CREXX/build \
  -DDSLSH_BUILD_CORE=OFF -DDSLSH_BUILD_TOOLS=OFF \
  -DDSLSH_BUILD_EXAMPLES=OFF -DDSLSH_BUILD_PARSERS=OFF \
  -DDSLSH_BUILD_TESTS=ON -DBUILD_TESTING=ON
cmake --build build-crexx --target dslsh_crexx_examples dslsh_crexx_contracts --parallel 4
ctest --test-dir build-crexx -L classic --output-on-failure
```

Configuration checks five existing artifacts under that build root:

- `research/level-l/compilers/frontends/llcgrammar.rxbin`
- `research/level-l/compilers/levelc/opt/llclanguage.rxbin`
- `research/level-l/compilers/levelc/opt/llcsrcmap.rxbin`
- `research/level-l/compilers/levelc/opt/llcparse.rxbin`
- `lib/rxfnsc/members/RexxClassicUnicodeProperties/RexxClassicUnicodeProperties.rxbin`

They are copied into private imports without rebuilding CREXX or regenerating
the grammar. The selected `library.rxbin` and `rxfnsl.rxbin` must match that
build's SDK byte for byte. An empty `CREXX_CLASSIC_BUILD_DIR` adds no Classic
build dependency. The default C-only configuration remains independent.

The standalone `programs/dslsh-classic.rxbin` links those five parser modules,
`library`, `rxfnsl`, the four DSLSH core modules, `dslshlevel` and `dslshclassic`.
The selected closure also includes generic Level L SDK transitive modules,
including `llir` and `llirservices`. It excludes the Classic C frontend,
`llcir`, `llcdynamic`, Classic executor and RXAS selector modules. Inspect
`rxlink -p` for each build to audit the actual selected modules and external
RXPA provider requirements. The installed endpoint contains its bytecode closure
and requires the external matching VM/native runtime. The separately installed
`dslshclassic` module and five parser modules support relinking; consumers must
additionally supply the matching SDK and DSLSH model/projection modules.

Launch the endpoint with:

```sh
/absolute/path/to/CREXX/build/bin/rxbvm --no-autoload \
  /absolute/path/to/build-crexx/crexx/programs/dslsh-classic.rxbin -a --stdio
```

`examples/classic.rexx` is representative edited text, including Unicode,
control flow, a call/label routine, nested comments and continuation punctuation.
Send its contents through framed initial-load traffic; it is not an endpoint
command-line argument. Classic syntax admission and its original diagnostics
come from the maintained parser, with lexical fallback for invalid source.
The source-byte and observation limits bound recognition through that parser;
the adapter's `work` argument bounds indexing/projection, not an aggregate
whole-process fuel counter. Browser/WASM, platform qualification and full Classic
semantic analysis remain outside this example.
