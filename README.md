# DSL Syntax Highlighter (SDSLH)

**The Bridge between Language Parsers and Modern Editors.**

SDSLH is a high-performance C-based platform that decouples language intelligence from text editing. It provides the essential "plumbing"—synchronization, threading, and IPC—required to build responsive, IDE-grade syntax highlighting and structural analysis.

## 🚀 Key Capabilities
- **Zero-Latency Visuals**: Our **Emergency Parsing** engine provides instant heuristic highlighting on the client while the full parser works in the background.
- **Efficient Synchronization**: Captures edits as atomic transactions and syncs only the differences (**Deltas**) via a robust, hex-encoded socket protocol.
- **Rich Hierarchy**: Supports complex, nested syntax trees with built-in utilities for tree manipulation, sorting, and gap-filling.
- **Differential Sync**: Built-in **LCS (Myers' Diff)** engine automatically handles bulk updates (copy-paste) with minimal network overhead.
- **Platform Integrity**: A strictly non-panicking library architecture designed for host-process stability.

## 📁 Project Structure
- **`codebuffer/`**: The core platform library. Handles all synchronization, transport, and data structures.
- **`toyeditor/`**: A reference `ncurses` implementation of a client editor.
- **`toyparser/`**: A reference language parser server implementing a custom DSL.
- **`docs/`**: Deep-dive technical documentation and integration guides.

## 📚 Documentation
- **[Architecture & Design](docs/ARCHITECTURE.md)**: The "Why" and "How" of the system.
- **[Emergency Parsing Rules](docs/EMERGENCY_PARSING.md)**: How SDSLH maintains visual accuracy during edits.
- **[Protocol Specification](docs/PROTOCOL.md)**: Details on the socket-based message format.
- **[Editor Integration Guide](docs/EDITOR_INTEGRATION.md)**: How to connect your editor to the platform.
- **[Parser Integration Guide](docs/PARSER_INTEGRATION.md)**: How to build a language server.
- **[AI Agent Guide (Gemini)](GEMINI.md)**: Guidance for AI-assisted maintenance.

## 🛠️ Quick Start

### 1. Build the Project
```bash
mkdir -p cmake-build-debug && cd cmake-build-debug
cmake ..
cmake --build .
```

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
