# Future Work

This document records planned capabilities and their explicitly qualified
increments. Open items are not current guarantees.

## Optional cREXX B/L library

The goal is an optional Level B implementation of the DSLSH library, with a
Level L grammar-to-highlighter facility. The first agreed increment is the
parser side. Editor synchronization, emergency highlighting and the remainder
of a full B library remain later work; THE continues to own editor/UI policy.

The parser-side component belongs to this project and is disabled by default.
It uses an explicitly selected existing cREXX toolchain and SDK, without making
them prerequisites of the C middleware or fetching/building CREXX recursively.
The B session/protocol layer owns document mirrors, atomic edits, versions,
hypothesis isolation, bounded framing and ordered replies. The L adapter owns
grammar/profile admission, lexical/structural projection and trusted optional
user validation. Generic Level L keeps ownership of parsing, source preparation
and comment recognition; highlighting must not introduce a second language
lexer or execute the edited program.

Generated, prevalidated grammar definitions stay anchored and are reused.
Each changed source snapshot initially receives a bounded full parse; deltas
do not imply incremental recognition. Results own their output values and use
zero-based Unicode-codepoint wire spans derived from original UTF-8 byte
boundaries. Token categories are separate from editor colours. Invalid source
must retain useful lexical/unknown coverage and located diagnostics without
claiming a successful partial AST. Language-specific semantic checks belong to
the selected profile/validator, not the generic protocol or SDK.

### Acceptance

| ID | Observable condition | Status |
| --- | --- | --- |
| **BL-AC01** | An opt-in component builds with an existing CREXX toolchain; the default C-only build has no new dependency. Source/library/runtime packaging and usage are documented. The staged installed settings image runs with autoload disabled. | done; selected native profile |
| **BL-AC02** | The B parser endpoint interoperates with the actual C framing/token protocol for initial loads, all six edit operations and noncommitting hypotheses. Multiple documents, revision refusals, invalid requests, limits and EOF cleanup preserve committed state. | done; synchronous version-2 STDIO |
| **BL-AC03** | One generic L adapter serves a small independent grammar and the maintained PHP subset through explicit mappings. Optional user validation adds owned located diagnostics/refinements; it does not execute source. | done; scanner/LR1 profile |
| **BL-AC04** | Clean and damaged inputs retain ordered source coverage, correct Unicode/CRLF/end-of-input positions and deterministic diagnostic display. Malformed ingress and resource refusal have explicit safe outcomes. | done; selected native profile |
| **BL-AC05** | Source and representative RXAS review show reused frozen definitions, bounded work/storage and no per-token whole-definition/tree snapshots. Selected normal integration checks pass on one VM; unverified platform/browser limits are explicit. | done; selected native profile |
| **BL-AC06** | Both editor and parser sides of the B library, including synchronization and emergency highlighting, meet their independently agreed compatibility checks. | open; later increment |

### Steps

| ID | Work and dependencies | Status |
| --- | --- | --- |
| **BL-STEP01** | Record scope and internally review shared result/provider interfaces, exact wire behavior, limits and the minimal L snapshot/trivia seam before product edits; AC01–05. | done |
| **BL-STEP02** | Implement the B parser protocol/session component and L mapping/validation adapter against that reviewed interface; AC02–04. | done |
| **BL-STEP03** | Add opt-in build/install, maintained small examples and focused independent/C interoperability checks. Review first representative RXAS early, batch repairs, then freeze; AC01–05. | done |
| **BL-STEP04** | Complete selected checks, enduring documentation and local delivery. Keep full-library, complete PHP/cREXX coverage and browser/WASM qualification separate; AC01–06. | done; parser-side local increment |

The initial endpoint uses synchronous STDIO and exposes a transport-independent
request service for later in-process/browser hosts. Socket transport, concurrent
requests, richer response envelopes, incremental recognition and full-language
semantic profiles are not selected by this increment. Native cREXX parser mode
remains available while optional L profiles retain their stated language limits.

## Code Completion Facilities

DSLSH now has the protocol foundation for non-committing hypothesis parses, but
it does not yet provide a complete code-completion API. A future completion layer
could make DSLSH the editor-facing broker for lightweight language intelligence
while still keeping language-specific knowledge inside the parser where possible.

Potential generic facilities:

- Completion requests addressed by document id, base version, cursor position,
  trigger character, prefix, replacement range, and optional hypothesis edits.
- Structured completion results with label, kind, insert text, replacement
  range, detail, documentation, sort key, confidence, and commit characters.
- Cursor context queries for current token, parser node, lexical state, and
  enclosing scope.
- Per-document symbol caches for locals, globals, labels, routines, imports,
  built-ins, and parser-supplied symbols.
- Signature-help requests for active call and parameter index.
- Hover, definition, and reference lookups when the parser can supply enough
  symbol metadata.
- Cancellation and stale-result rejection keyed by document id and base version.

With limited or parameterized language details, DSLSH can still provide useful
completion by consuming parser-advertised configuration such as:

- Identifier character rules and case-sensitivity.
- Keywords grouped by statement, declaration, expression, or control-flow use.
- Trigger characters for calls, member access, labels, modules, or paths.
- Comment and string delimiters.
- Scope open/close tokens or parse-tree node types.
- Built-in functions, commands, variables, and constants.

A conservative generic completion algorithm would be:

1. Receive a completion request with document id, base version, cursor, prefix,
   trigger, and optional hypothesis transactions.
2. Reject or mark stale requests whose base version no longer matches the
   parser session.
3. Determine lexical context: prefix, replacement range, previous token, trigger
   character, and whether the cursor is inside code, string, or comment text.
4. Determine syntax context from the parse tree: current node, enclosing scope,
   and any parser-supplied expected construct.
5. Generate candidates from configured keywords, in-scope symbols, document
   symbols, imported symbols, built-ins, snippets, and file or module names.
6. Filter candidates by prefix, trigger, lexical state, and expected construct.
7. Rank local and exact-prefix matches first, then parser-preferred candidates,
   recent selections, globals, built-ins, and broad keyword fallbacks.
8. Return structured items with explicit replacement ranges so the editor does
   not need to guess what text should be replaced.

For CREXX/THE, a good first implementation would combine CREXX keywords and
built-ins, identifiers discovered in the current document, parser-known imports,
and DSLSH hypothesis parses to rank candidates by whether the inserted text
keeps the surrounding syntax valid. Richer symbol resolution can be added later
without changing the basic editor-facing request/response shape.
