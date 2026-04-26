# Future Work

This document records design directions that are not current DSLSH behavior.

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
