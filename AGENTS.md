# Repository Instructions

`AGENTS.md` is the canonical repository instruction file. Keep repository-specific
agent guidance here. `GEMINI.md` should only refer back to this file so
instructions do not drift.

## Project Context

The **DSL Syntax Highlighter (DSLSH)** is a C library providing a platform for
synchronized syntax highlighting between an editor and a language parser.

## Directory Structure

- `codebuffer/`: Core library (the "Platform"). Handles synchronization,
  threading, and transport.
- `toyeditor/`: Example ncurses-based editor (the "Client").
- `toyparser/`: Example language parser (the "Server").
- `docs/`: Consolidated documentation and integration guides.

## Key Architectural Patterns

1. **Deltas & Transactions**: Edits are captured as atomic transactions and
   bundled into deltas for synchronization.
2. **Double Buffering**: Both client and server maintain a `CodeBuffer`.
3. **Emergency Parsing**: The library performs heuristic tree shifts on the
   client to provide zero-latency feedback.
4. **Hex-Encoded Protocol**: Sockets use a text-based protocol with hex-encoded
   strings to avoid delimiter collisions.

## Common Operations

### Build the Project

```bash
cmake -B cmake-build-debug
cmake --build cmake-build-debug
```

### Run Tests

```bash
cd cmake-build-debug
ctest
```

### Run ASan/LSan Tests

Use `tools/asan-run.sh` for sanitizer runs and consult
`docs/ASAN_TESTING.md`. Do not hand-run broad ASan builds or CTests unless the
runner itself is broken.

### Run Client/Server (from project root)

**Linux / macOS:**

```bash
# Default (STDIO mode - Editor launches Parser automatically)
./cmake-build-debug/toyeditor/te toyeditor/test.toy

# Specify Parser location and args (e.g. slow mode)
./cmake-build-debug/toyeditor/te --parser ./cmake-build-debug/toyparser/tp --parser-args "-s" toyeditor/test.toy

# Socket mode
# Terminal 1
./cmake-build-debug/toyparser/tp -d 8080
# Terminal 2
./cmake-build-debug/toyeditor/te --socket --port 8080 toyeditor/test.toy
```

**Windows (PowerShell):**

```powershell
# Default (STDIO mode - Editor launches Parser automatically)
.\cmake-build-debug\toyeditor\te.exe toyeditor\test.toy

# Specify Parser location and args (e.g. slow mode)
.\cmake-build-debug\toyeditor\te.exe --parser .\cmake-build-debug\toyparser\tp.exe --parser-args "-s" toyeditor\test.toy

# Socket mode
# Terminal 1
.\cmake-build-debug\toyparser\tp.exe -d 8080
# Terminal 2
.\cmake-build-debug\toyeditor\te.exe --socket --port 8080 toyeditor\test.toy
```

## Maintenance Rules

- **No exit()**: Library functions in `codebuffer/` must not call `exit()` or
  `panic()`. Use `LOG()` and safe returns.
- **Strict Separation**: Concerns of the editor (UI) and parser (AST logic) must
  not leak into the core `codebuffer/` library.
- **Thread Safety**: All access to the shared `CodeBuffer` in the editor must be
  wrapped in `enter_codeblock_critical_section()`.
- **Release-safe tests**: Test setup must not rely on side effects inside
  `assert()`. Release builds may define `NDEBUG`, so calls such as `snprintf()`,
  `mkdtemp()`, `realpath()`, `symlink()`, or parser launch setup must execute
  before assertions check their results.
