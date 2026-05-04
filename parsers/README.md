# Standard Parser Adapters

This directory contains parser servers for standard languages. Each adapter is a
normal DSLSH parser process: it keeps the editor-facing protocol in `codebuffer`
and isolates language-specific parsing behind `ParserFunction(CodeBuffer*)`.

Parser adapters are optional build products. Keep adapter dependencies local to
the adapter's CMake file and guard them behind `DSLSH_BUILD_PARSERS` plus a
parser-specific option where useful.

## Tree-sitter Adapters

`parsers/c`, `parsers/python`, and `parsers/javascript` build `dslsh-c`, `pyp`,
and `jsp`. They share the Tree-sitter runtime and a common DSLSH tree
conversion layer, while each adapter owns its language grammar dependency and
stdio entry point.

```bash
cmake -DDSLSH_BUILD_C=ON -DDSLSH_BUILD_PYTHON=ON -DDSLSH_BUILD_JAVASCRIPT=ON ..
```

Each grammar is fetched by CMake and pinned in the adapter CMake file. Disable
one adapter by setting its option to `OFF`; the shared Tree-sitter runtime is
only built when at least one Tree-sitter-backed adapter is enabled.

```bash
./cmake-build-debug/parsers/c/dslsh-c --stdio
./cmake-build-debug/parsers/python/pyp --stdio
./cmake-build-debug/parsers/javascript/jsp --stdio
```

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
