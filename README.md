# DSL Syntax Highlighter (DSLSH)

**The Bridge between Language Parsers and Modern Editors.**

DSLSH is a high-performance C-based platform that decouples language intelligence from text editing. It provides the essential "plumbing"—synchronization, threading, and IPC—required to build responsive, IDE-grade syntax highlighting and structural analysis.

## 🚀 Key Capabilities
- **Zero-Latency Visuals**: Our **Emergency Parsing** engine provides instant heuristic highlighting on the client while the full parser works in the background.
- **Efficient Synchronization**: Captures edits as atomic transactions and syncs only the differences (**Deltas**) via a robust, hex-encoded socket protocol.
- **Rich Hierarchy**: Supports complex, nested syntax trees with built-in utilities for tree manipulation, sorting, and gap-filling.
- **Differential Sync**: Built-in **LCS (Myers' Diff)** engine automatically handles bulk updates (copy-paste) with minimal network overhead.
- **Platform Integrity**: A strictly non-panicking library architecture designed for host-process stability.

## 📁 Project Structure
- **`codebuffer/`**: The core platform library. Handles all synchronization, transport, and data structures.
- **`parsers/`**: Standard language parser adapters for C, Python, JavaScript, and Markdown.
- **`toyeditor/`**: A reference `ncurses` implementation of a client editor.
- **`toyparser/`**: A reference language parser server implementing a custom DSL.
- **`docs/`**: Deep-dive technical documentation and integration guides.

## 📚 Documentation
- **[Architecture & Design](docs/ARCHITECTURE.md)**: The "Why" and "How" of the system.
- **[Emergency Parsing Rules](docs/EMERGENCY_PARSING.md)**: How DSLSH maintains visual accuracy during edits.
- **[Protocol Specification](docs/PROTOCOL.md)**: Details on the socket-based message format.
- **[Editor Integration Guide](docs/EDITOR_INTEGRATION.md)**: How to connect your editor to the platform.
- **[Parser Integration Guide](docs/PARSER_INTEGRATION.md)**: How to build a language server.
- **[Future Work](docs/FUTURE_WORK.md)**: Proposed directions such as code completion facilities.
- **[AI Agent Guide (Gemini)](GEMINI.md)**: Guidance for AI-assisted maintenance.

## 🛠️ Quick Start

### 1. Build the Project
```bash
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

By default, a standalone DSLSH build includes the middleware, examples, tools,
tests, and bundled parser adapters. Downstream consumers such as CREXX and THE
should build only the middleware surface they need.

Useful CMake options:

```bash
-DDSLSH_BUILD_CORE=ON       # dslsyntax_editor and dslsyntax_parser
-DDSLSH_BUILD_TOOLS=ON      # parser_tester and middleware tools
-DDSLSH_BUILD_EXAMPLES=OFF  # toy editor/parser programs
-DDSLSH_BUILD_PARSERS=OFF   # bundled language parser adapters
-DDSLSH_BUILD_TESTS=OFF     # DSLSH's own tests
-DDSLSH_INSTALL=OFF         # install DSLSH products from this build
```

Parser adapters under `parsers/` are optional. Their third-party dependencies
must stay behind `DSLSH_BUILD_PARSERS` and parser-specific options, so editor
and compiler consumers do not inherit those dependencies.

Bundled parser adapters can also be controlled individually:

```bash
-DDSLSH_BUILD_C=ON           # C parser, builds parsers/c/dslsh-c
-DDSLSH_BUILD_PYTHON=ON      # Python parser, builds parsers/python/pyp
-DDSLSH_BUILD_JAVASCRIPT=ON  # JavaScript parser, builds parsers/javascript/jsp
-DDSLSH_BUILD_MARKDOWN=ON    # Markdown parser, builds parsers/markdown/mdp
```

Installed executables are placed in `bin/` and the DSLSH shared libraries in
`lib/`. Parser adapter internals are linked into their executables, so the
installed C, Python, JavaScript, and Markdown parsers do not require separate
adapter-specific shared libraries.

On Windows, `toyeditor` is only built when a vendored `PDCursesMod` checkout is
present at `third_party/PDCursesMod`. When present, it is linked statically
using the `wincon` backend. Use a current release and keep the default source
layout intact.

### 2. Run Automated Tests
```bash
ctest
```

### 3. Launch the Demo
The editor defaults to **STDIO mode**, automatically launching the parser child process.

```bash
# Basic launch (using default parser path)
./toyeditor/te toyeditor/test.toy

# Specify a custom parser location and pass arguments (like slow mode)
./toyeditor/te --parser ./toyparser/tp --parser-args "-s" toyeditor/test.toy

# Enable debug logging (writes to editor.log and parser.log)
./toyeditor/te -d toyeditor/test.toy
```

## ⚖️ License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
