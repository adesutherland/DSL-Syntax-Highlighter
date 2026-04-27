# Standard Parser Adapters

This directory contains parser servers for standard languages. Each adapter is a
normal DSLSH parser process: it keeps the editor-facing protocol in `codebuffer`
and isolates language-specific parsing behind `ParserFunction(CodeBuffer*)`.

Parser adapters are optional build products. Keep adapter dependencies local to
the adapter's CMake file and guard them behind `DSLSH_BUILD_PARSERS` plus a
parser-specific option where useful.

## Markdown

`parsers/markdown` builds `mdp`, a Markdown parser backed by GitHub's
`cmark-gfm` C library. The adapter uses cmark-gfm for block structure and a
small Markdown token pass for complete DSLSH leaf-token coverage.

```bash
./cmake-build-debug/parsers/markdown/mdp --stdio
```

The current adapter maps top-level Markdown blocks to DSLSH structural nodes and
highlights Markdown markers, code spans/fences, comments, numbers, separators,
operators, whitespace, and plain text.
