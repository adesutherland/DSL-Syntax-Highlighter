# Gemini CLI: DSLSH Project Guide

## Project Context
The **DSL Syntax Highlighter (DSLSH)** is a C library providing a platform for synchronized syntax highlighting between an editor and a language parser.

## Directory Structure
- `codebuffer/`: Core library (the "Platform"). Handles synchronization, threading, and transport.
- `toyeditor/`: Example ncurses-based editor (the "Client").
- `toyparser/`: Example language parser (the "Server").
- `docs/`: Consolidated documentation and integration guides.

## Key Architectural Patterns
1. **Deltas & Transactions**: Edits are captured as atomic transactions and bundled into deltas for synchronization.
2. **Double Buffering**: Both client and server maintain a `CodeBuffer`.
3. **Emergency Parsing**: The library performs heuristic tree shifts on the client to provide zero-latency feedback.
4. **Hex-Encoded Protocol**: Sockets use a text-based protocol with hex-encoded strings to avoid delimiter collisions.

## Common Operations
### Build the Project
```bash
mkdir -p cmake-build-debug && cd cmake-build-debug && cmake .. && cmake --build .
```

### Run Tests
```bash
ctest
```

### Run Client/Server
```bash
# Default (STDIO mode - Editor launches Parser automatically)
./toyeditor/te toyeditor/test.toy

# Specify Parser location and args (e.g. slow mode)
./toyeditor/te --parser ./toyparser/tp --parser-args "-s" toyeditor/test.toy

# Socket mode
# Terminal 1
./toyparser/tp -d 8080
# Terminal 2
./toyeditor/te --socket --port 8080 toyeditor/test.toy
```

## Maintenance Rules
- **No exit()**: Library functions in `codebuffer/` must not call `exit()` or `panic()`. Use `LOG()` and safe returns.
- **Strict Separation**: Concerns of the editor (UI) and parser (AST logic) must not leak into the core `codebuffer/` library.
- **Thread Safety**: All access to the shared `CodeBuffer` in the editor must be wrapped in `enter_codeblock_critical_section()`.
